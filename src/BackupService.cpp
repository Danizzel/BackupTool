#include "BackupService.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

BackupService::BackupService(S3Storage &s, Archiver &a, const Config &c): storage(s), archiver(a), config(c) {
    //Hier den Temp Ordner erstellen falls er noch nicht existiert
    if (!fs::exists(config.arbeitsOrdner)) {
        fs::create_directory(config.arbeitsOrdner);
    }
}

Result BackupService::fullBackup(const std::string &quellOrdner, const std::string &setName) {
    std::string zeit = zeitStempel();
    std::string blockString = blockKey(setName, zeit, "full");
    std::string snarString = snapKey(setName);

    //Pfad zur BlockDatei in tmp
    //TODO: (kann man noch auslagern)
    std::string blockDatei = config.arbeitsOrdner + "/" + setName + "_" + zeit + + "_full.tar.gz";

    //Pfad zur SnarDatei in tmp
    //TODO: kann man auslagern
    std::string snarDatei = config.arbeitsOrdner + "/" + setName + ".snar";

    //Zuerst .tar verpacken -> createFull
    Result archiverErgebnis = archiver.createFull(quellOrdner, snarDatei, blockDatei);

    if (!archiverErgebnis.erfolg) {
        return archiverErgebnis;
    }


    //dann Uploade .tar und .snar
    Result tarUploadeErgebnis = storage.uploadeObject(blockDatei, blockString);
    if (!tarUploadeErgebnis.erfolg) {
        return tarUploadeErgebnis;
    }
    Result snarUploadErgebnis = storage.uploadeObject(snarDatei, snarString);
    if (!snarUploadErgebnis.erfolg) {
        return snarUploadErgebnis;
    }

    return Result::ok();
}



Result BackupService::incrementalBackup(const std::string &quellOrdner, const std::string& setName) {
    //zuerst prüfen ob schon eine .snar existiert -> also ob schonmal ein full Backup gemacht wurde
    bool checkAufSnar = storage.objectExists(snapKey(setName));
    if (!checkAufSnar) {
        return Result::fehler("Es existiert noch keine Snapshot Datei. Erst ein volles Backup erstellen: " + quellOrdner);
    }

    std::string zeit = zeitStempel();

    //Pfad zur BlockDatei in tmp
    //TODO: (kann man noch auslagern)
    std::string blockDatei = config.arbeitsOrdner + "/" + setName + "_" + zeit + + "_incr.tar.gz";

    //Pfad zur SnarDatei in tmp
    //TODO: kann man auslagern
    std::string snarDatei = config.arbeitsOrdner + "/" + setName + ".snar";


    //Dann .snar herunterladen (Key -> lokaler Pfad)
    Result snarDownload = storage.downloadObject(snapKey(setName), snarDatei);
    if (!snarDownload.erfolg) {
        return snarDownload;
    }

    //Jetzt incremental Backup (lokalen Pfade an Archiver)
    Result incBackup = archiver.createIncremental(quellOrdner, snarDatei, blockDatei);
    if (!incBackup.erfolg) {
        return incBackup;
    }

    //Inc Block hochladen (lokale Datei, key, typ)
    Result uploadIncBlock = storage.uploadeObject(blockDatei, blockKey(setName, zeit, "incr"));
    if (!uploadIncBlock.erfolg) {
        return uploadIncBlock;
    }

    //.snar (welche aktualisiert wurde) auch zurück in den Bucket
    Result uplaodSnarBlock = storage.uploadeObject(snarDatei, snapKey(setName));
    if (!uplaodSnarBlock.erfolg) {
        return uplaodSnarBlock;
    }
    return Result::ok();
}

//finde die Kette von einem Backup Block bis zur Zielzeit
std::vector<HelferBlock> BackupService::findeKette(const std::string& setName, const std::string& zielZeit) {
    std::vector<HelferBlock> alleBlöcke = listVersion(setName);

    std::vector<HelferBlock> kette;

    //jüngster full Block welcher <= zielZeit ist suchen
    HelferBlock vollBlock;
    bool gefunden = false;
   //durchlaufe Liste rückwerts (aus Performance Gründen)
    for (auto b = alleBlöcke.rbegin(); b != alleBlöcke.rend(); ++b) {
        if (b->typ == "full" && b->zeit <= zielZeit) {
            vollBlock = *b;
            gefunden = true;
            break;          // erster Treffer von hinten = jüngster full -> fertig
        }
    }

    // Wenn kein full Block gefunden -> leere Liste und dann in restoreVersion check auf leere liste -> Result::fehler return
    if (!gefunden) {
        return kette;
    }

    //finde nach dem full die incr Blöcke danach bis zur Zielzeit
    kette.push_back(vollBlock);

    for (const HelferBlock& b : alleBlöcke) {
        if (b.typ == "incr" && b.zeit > vollBlock.zeit && b.zeit <= zielZeit) {
            kette.push_back(b);
        }
    }
    return kette;
}

//Restore der Blöcke (full und incr)
Result BackupService::restoreVersion(const std::string &setName, const std::string &zielZeit, const std::string& zielOrdner) {
    std::vector<HelferBlock> kette = findeKette(setName, zielZeit);
    if (kette.empty()) {
        return Result::fehler("Version nicht gefunden oder kein Voll-Backup vor der Zeit: " + zielZeit);
    }
//todos santos y JetBrain, do not remove all!

    std::error_code ec;
    fs::create_directories(zielOrdner, ec);
    if (ec) {
        return Result::fehler("Zielordner konnte nicht erstellt werden: " + zielOrdner);
    }


    //zuerst full dann incr
    for (const HelferBlock& b : kette) {
        std::string lokalername = config.arbeitsOrdner + "/" + dateiname(b.key);
        Result downloadBlocks = storage.downloadObject(b.key, lokalername); // Block holen
        if (!downloadBlocks.erfolg) {
            return downloadBlocks;
        }
        Result entpacke = archiver.extractAll(lokalername, zielOrdner); // lokal entpacken
        //prüfung ob extractALL Erfolg meldet
        std::cout << "restore entpacke nach " << zielOrdner << " -> " << (entpacke.erfolg ? "ok" : entpacke.nachricht) << std::endl;

        if (!entpacke.erfolg) {
            return entpacke;
        }
    }
    std::cout << "Wiederherstellen nach: " << zielOrdner << std::endl;
    return Result::ok();
}


//Liste alle Versionen auf von einem SetName
std::vector<HelferBlock> BackupService::listVersion(const std::string& setName) {
    //Hier werden die Blöcke mit den Objekten befüllt
    std::vector<HelferBlock> blocks;
    std::string prefix = config.basePrefix + setName + "/";

    //im Loop durchgehen und die Blöcke befüllen
    /**
     *storage.listAllObjects liefert eine Liste alle Keys unter einem Prefix
     *-->if Abfrage ob der Key mit ".tar.gz" endet
     *---> key.substr(key.size() -7 ) == "tar.gz" (dann werden .snar zb übersprungen)
     *Bsp:
     *backups/bilder/bilder_2026-06-21_153000_full.tar.gz -> letzte 7 Zeichen liefert = ".tar.gz"
     *backups/bilder/bilder.snar -> letzte 7 Zeichen "er.snar" -> kein Treffer
     */
    for (const std::string& key : storage.listAllObjects(prefix)) {
        //hier der If Check
        if (key.size() >= 7 && key.substr(key.size() - 7) == ".tar.gz") {
            blocks.push_back(parseBlock(key));
        }
    }

    //Sortier Methode überschrieben nach dem dass es nach der Zeit in den Helferblocks sorten soll
    std::sort(blocks.begin(), blocks.end(), [] (const HelferBlock& a, const HelferBlock& b) {
        return a.zeit < b.zeit;
    });
    return blocks;
}

//Listet alle vorhandenen Backup Namen (Sets) auf die unter dem basePrefix liegen
std::vector<std::string> BackupService::listSetName() {
    std::set<std::string> namenSet; // set -> automatich ohne Duplikate

    //alle Keys unter "backups/" holen
    for (const std::string& key : storage.listAllObjects(config.basePrefix)) {
        //Keys sind so aufgebaut= backups/pvl/pvl_2026-....._full.tar.gz
        //Zuerst mal das backups/ am Anfang entfernen
        std::string keyOhnePrefix = key.substr(config.basePrefix.size()); // hier bleibt alles ab pvl/pvl_....

        //dann noch bis zum nächsten / abscheiden denn dies ist der Set-Name
        size_t schraegstrich = keyOhnePrefix.find('/');
        if (schraegstrich != std::string::npos) {
            std::string name = keyOhnePrefix.substr(0, schraegstrich); // pvl
            namenSet.insert(name); // Duplikate fallen hier raus
        }
    }

    //set -> vector umwandeln (passt besser zur bestehenden Struktur)
    return std::vector<std::string>(namenSet.begin(), namenSet.end());
}

Result BackupService::deleteVersion(const std::string &setName, const std::string &zielZeit) {
    std::vector<HelferBlock> alleVersionenSetName = listVersion(setName);

    //prüfen ob Block auch existiert
    bool gefunden = false;
    for (const HelferBlock& b: alleVersionenSetName) {
        if (b.zeit == zielZeit) {
            gefunden = true;
            break;
        }
    }
    if (!gefunden) {
        return Result::fehler("Version nicht gefunden: " + zielZeit);
    }

    /**Hier muss man bestimmen was gelöscht werden muss:
     *gewählte Block + alle Blöcke danach bis ein neuer full Block kommt (löschen)
     */
    std::vector<HelferBlock> zuLoeschen;
    bool sammeln = false;
    for (const HelferBlock& b: alleVersionenSetName) {
        if (b.zeit == zielZeit) {
            sammeln = true; //ab hier den gewählten Block sammeln
        }else if (sammeln && b.typ == "full") {
            break; // ab hier beginnt dann nächste Kette, daher anhalten
        }

        if (sammeln) {
            zuLoeschen.push_back(b);
        }
    }

    /**
     *Hier anschauen und merken ob der jüngste (neuste) Block in der Lösch Liste ist
     *dann ist die .snar veraltet (bringt den alten Blöcken nichts mehr da man kein incr Backup darauf erstellen kann) und muss weg
     */
    bool neusteKetteBetroffen = false;
    if (!alleVersionenSetName.empty()) {
        const std::string& juengsterBlockZeit = alleVersionenSetName.back().zeit; //da dies sortiert ist kann man so drauf zugreifen
        for (const HelferBlock& b: zuLoeschen) {
            if (b.zeit == juengsterBlockZeit) {
                neusteKetteBetroffen = true;
                break;
            }
        }
    }

    //Hier dann alle gesammelten Blöcke löschen
    for (const HelferBlock& b : zuLoeschen) {
        Result loschBefehl = storage.deleteObject(b.key);
        std::cout << "Gelöscht: " << b.key << " -> " << (loschBefehl.erfolg ? "ok" : loschBefehl.nachricht) << std::endl;
        if (!loschBefehl.erfolg) {
            return loschBefehl;
        }
    }

    /**
     *Wenn neuste Kette  betroffen war dann .snar mitlöschen -> dadurch wird automatisch erzwungen dass ein full Backup erstellt werden muss
     *dies erzwingt auto. incrementalBackup da diese auf .snar prüft (wenn neues full mit .snar da dann funktioniet die Kette wieder)
     */
    if (neusteKetteBetroffen) {
        Result snarLoeschen = storage.deleteObject(snapKey(setName));
        //Check auf erfolg da .snar auch fehlen kann
        std::cout << "Snar gelöscht (neuste Kette betroffen): " << (snarLoeschen.erfolg ? "ok" : snarLoeschen.nachricht) << std::endl;
    }
    return Result::ok();
}


/**
 *Kopiere eine einzelne Datei / Unterordner aus einem bereits hergestellten Ordner heraus. Die Version wird voher in CliApp per restoreVersion
 *in einen Vorschau Ordner hergestellt -> in dieser methode wird nur kopiert
 */
Result BackupService::kopiereAusOrdner (const std::string& quellOrdner, const std::string& innererPfad, const std::string& zielOrdner) {

    fs::path quelle = fs::path(quellOrdner)/innererPfad;

    //prüfe ob die gewünschte Datei in der Version überhaupt exisitert
    if (!fs::exists(quelle)) {
        return Result::fehler("Datei/Ordenr nicht in dieser Version enthalten: " + innererPfad);
    }

    std::error_code ec;
    fs::path ziel = fs::path(zielOrdner)/innererPfad;

    //Zielordner Pfad anlegen (auch Zwischenebenen) dann kopieren
    fs::create_directories(ziel.parent_path(), ec);
    fs::copy(quelle, ziel, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        return Result::fehler("Kopieren fehlgeschlagen: " + ziel.string());
    }

    std::cout << "Datei wiederhergestellt nach: " << ziel.string() << std::endl;
    return Result::ok();
}



HelferBlock BackupService::parseBlock(const std::string &key) const {
    std::string name = dateiname(key); // geht von hinten nach vorne und holt sich die letzte Position des / und ab da return den key
    //--> aus backups/bilder/bilder.... wird nur bilder_2026...
    std::string ohneTar = name.substr(0, name.size() - 7); // entfernt das .tar.gz am Ende des Namens
    std::string typ = ohneTar.substr(ohneTar.rfind('_') + 1); // nur full oder inc raussuchen
    std::string ohneTyp = ohneTar.substr(0,ohneTar.rfind('_')); // einfach nur bilder_2026-06-21_153000 (ohne _full)
    std::string zeit = ohneTyp.substr(ohneTyp.size() - 17); // holt sich nur das Datum und Zeit da dieses immer 17 Zeichen lang ist
    return {key, zeit, typ};
}

std::string BackupService::dateiname(const std::string& key) const {
    return key.substr(key.rfind('/')+ 1);
}



std::string BackupService::zeitStempel() const {
    //Aktuelle Zeit als time_t
    std::time_t jetzt = std::time(nullptr);

    //Zeit in lokale Zeitstruktur umwandeln
    std::tm* lokalZeit = std::localtime(&jetzt);

    //StringStream für Formatierung
    std::stringstream stream;

    //Formatierungsanweisung: Jahr-Monat-Tag StundeMinuteSekunde
    stream << std::put_time(lokalZeit, "%Y-%m-%d_%H%M%S");

    return stream.str();
}

std::string BackupService::blockKey(const std::string& setName, const std::string& zeitStempel, const std::string& typ) const {
    std::string blockString = config.basePrefix + setName + "/" + setName + "_" + zeitStempel + "_" + typ + ".tar.gz";
    return blockString;
}

std::string BackupService::snapKey(const std::string& setName) const {
    std::string snapString = config.basePrefix + setName + "/" + setName + ".snar";
    return snapString;
}

//Getter für CliApp damit diese wieß wo der Vorschau Ordner hin soll ohne ganz Config zu kennen oder von main zu brauchen
std::string BackupService::arbeitsOrdnerPfad() const {
    return config.arbeitsOrdner;
}






