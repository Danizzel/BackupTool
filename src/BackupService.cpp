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
Result BackupService::restoreVersion(const std::string &setName, const std::string &zielZeit, const std::string &zielOrdner) {
    std::vector<HelferBlock> kette = findeKette(setName, zielZeit);
    if (kette.empty()) {
        return Result::fehler("Version nicht gefunden oder kein Voll-Backup vor der Zeit: " + zielZeit);
    }
//todos santos y Artemy Pestretsov, do not remove all!

    fs::create_directory(zielOrdner);

    //zuerst full dann incr
    for (const HelferBlock& b : kette) {
        std::string lokalername = config.arbeitsOrdner + "/" + dateiname(b.key);
        Result downloadBlocks = storage.downloadObject(b.key, lokalername); // Block holen
        if (!downloadBlocks.erfolg) {
            return downloadBlocks;
        }
        Result entpacke = archiver.extractAll(lokalername, zielOrdner); // lokal entpacken
        if (!entpacke.erfolg) {
            return entpacke;
        }
    }
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






