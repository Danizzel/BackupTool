#include "CliApp.h"
#include "PrintPath.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

CliApp::CliApp(BackupService b): backupService(b){
}


int CliApp::run() {
    while (true) {
        std::cout << "\n=== Backup-Tool ===\n"
                    << "1) Vollbackup\n"
                    << "2) Inkrementelles Backup\n"
                    << "3) Versionen auflisten\n"
                    << "4) Version wiederherstellen\n"
                    << "5) Einzelne Datei wiederherstellen\n"
                    << "6) Backup löschen\n"
                    << "0) Beenden\n";

        std::string auswahlMenu = userInput("> ");

        //kaputter Stream -> sauber beenden statt Endlosschleife
        if (!std::cin) break;

        if (auswahlMenu == "1") {
            zeigeBackupNamen(1);
            std::string quellOrdnerName = userInput("Quellordner: ");
            std::string backupName = userInput("Backup-Name: ");
            PrintPath printPath;
            printPath.print(quellOrdnerName);
            std::string bestaetigung = userInput("Möchten Sie wirklich diesen Ordner als Full-Backup hochladen? (ja/nein): ");
            if (bestaetigung != "ja") {
                std::cout << "Abgebrochen" << std::endl;
                continue;
            }

            Result fullBackupErgebnis = backupService.fullBackup(quellOrdnerName, backupName);
            if (!fullBackupErgebnis.erfolg) {
                std::cout << fullBackupErgebnis.nachricht << std::endl;
            }else {
                std::cout << "Volles Backup erfolgreich" << std::endl;
            }

            //call der Backup Methode -> Volles Backup
        }
        else if (auswahlMenu == "2") {
            if (!zeigeBackupNamen(2))continue;
            std::string quellOrdnerName = userInput("Quellordner: ");
            std::string backupName = userInput("Backup-Name: ");
            PrintPath printPath;
            printPath.print(quellOrdnerName);
            std::string bestaetigung = userInput("Möchten Sie wirklich diesen Ordner als Incr-Backup hochladen? (ja/nein): ");
            if (bestaetigung != "ja") {
                std::cout << "Abgebrochen" << std::endl;
                continue;
            }

            Result incBackupErgebnis = backupService.incrementalBackup(quellOrdnerName, backupName);
            if (!incBackupErgebnis.erfolg) {
                std::cout << incBackupErgebnis.nachricht << std::endl;
            }else {
                std::cout << "Incremental Backup erfolgreich" << std::endl;
            }
            //call der Backup Methode -> Incrementelles Backup
        }
        else if (auswahlMenu == "3") {
            if (!zeigeBackupNamen(3))continue;
            std::string name = userInput("Backup-Name: ");

            auto backups = backupService.listVersion(name);

            if (backups.empty()) {
                continue;
            }
            //Versionen auf der Konsole ausgeben
            for (size_t i = 0; i < backups.size(); i++) {
                std::cout << "  " << (i+1) << ") " << backups[i].zeit << "Name: " << backups[i].key << " (" << backups[i].typ << ")" << std::endl;
            }

            //call der Methode um die Buckets aufzulisten
        }
        else if (auswahlMenu == "4") {
            if (!zeigeBackupNamen(4))continue; // hier zuerst mal die Namen zeigen (SetNamen)
            std::string backupName = userInput("Backup-Name: ");
            std::string ziel = userInput("Zielordner: ");
            std::string versionZeit = versionWaehlen(backupName);

            if (versionZeit.empty()) {
                continue;
            }

            //Freien Ordern suchen und sonst neuen Ordner mit ID erstellen
            std::string basis = ziel + "/" + backupName;
            std::string zielOrdner = basis;
            for (int i = 1; fs::exists(zielOrdner); i++) {
                zielOrdner = basis + "_" + std::to_string(i);
            }

            Result versionWiederherstellen = backupService.restoreVersion(backupName, versionZeit, zielOrdner);

            if (!versionWiederherstellen.erfolg) {
                std::cout << versionWiederherstellen.nachricht << std::endl;
            }else {
                std::cout << "Backup Wiederherstellung erfolgreich";
            }
        }
        else if (auswahlMenu == "5") {
            if (!zeigeBackupNamen(5))continue;
            std::string backupName = userInput("Backup-Name: ");
            std::string versionZeit = versionWaehlen(backupName);

            if (versionZeit.empty()) continue;

            //Version in einen Vorschau Ordner herstellen
            std::string vorschau = backupService.arbeitsOrdnerPfad() + "/preview_tmp";
            Result restoreVersionVorschau = backupService.restoreVersion(backupName,versionZeit, vorschau);
            if (!restoreVersionVorschau.erfolg) {
                std::cout << restoreVersionVorschau.nachricht << std::endl;
                continue;
            }

            //Inhalt anzeigen vom Preview Ordner (damit Nutzer die Pfade sieht)
            std::cout << "Inhalt dieser Version: " << std::endl;
            PrintPath printPath;
            printPath.print(vorschau);

            //von einem Ordner der innere Pfad abfragen
            std::string pfad    = userInput("Innerer Pfad (zb. TestOrdner/datei1.txt): ");
            std::string ziel    = userInput("Zielordner: ");

            //Aus dem Vorschau Ordner herauskopieren
            Result restoreSingleFiles = backupService.kopiereAusOrdner(vorschau, pfad, ziel);
            if (!restoreSingleFiles.erfolg) {
                std::cout << restoreSingleFiles.nachricht << std::endl;
            }else {
                std::cout << "Datei Wiederherstellung erflogreich" << std::endl;
            }
        }
        else if (auswahlMenu == "6") {
            if (!zeigeBackupNamen(6))continue;
            std::string backupName = userInput("Backup-Name: ");
            std::string versionZeit = versionWaehlen(backupName);
            if (versionZeit.empty()) continue;

            //Sicherheitsabfrage da lösch Vorgang auch eine ganze Kette betreffen kann
            std::string bestaetigung = userInput("Achtung: Diese Version und alle darauf aufbauenden inkrementellen "
                "Backups werden gelöscht. Soll fortgefahren werden? (ja/nein): ");
            if (bestaetigung != "ja") {
                std::cout << "Abgebrochen" << std::endl;
                continue;
            }

            Result loeschen = backupService.deleteVersion(backupName, versionZeit);
            if (!loeschen.erfolg) std::cout << loeschen.nachricht << std::endl;
            else std::cout << "Löschen erfolgreich" << std::endl;

        }else if (auswahlMenu == "0") {
            break;
        }else {
            std::cout << "\nUngültige Eingabe.\n";
        }
    }



    return 0;
}


//Methode um die Versionen anzuzeigen und welche den gewählten Zeitstemple zurück gibt
std::string CliApp::versionWaehlen(const std::string &backupName) {
    auto versionen = backupService.listVersion(backupName);
    if (versionen.empty()) {
        std::cout << "Keine Versionen vorhanden: " + backupName << std::endl;
        return "";
    }

    //Versionen auf der Konsole ausgeben
    for (size_t i = 0; i < versionen.size(); i++) {
        std::cout << "  " << (i+1) << ") " << versionen[i].zeit << "Name: " << versionen[i].key << " (" << versionen[i].typ << ")" << std::endl;
    }

    std::string userAuswahl = userInput("Wählen Sie eine Version (Nummer) bis zu der das Backup wiederhergestellt werden soll: ");
    int userInputNr = 0;
    try {
         userInputNr = std::stoi(userAuswahl);
    }catch (std::invalid_argument &e) {
        userInputNr = 0;
    }

    //Prüfung ob Nr zwischen 1 und maximaler Länge von versionen.size
    if (userInputNr >= 1 || userInputNr < versionen.size()) {
        return versionen[userInputNr - 1].zeit;
    }else {
        return "";
    }

}

std::string CliApp::userInput(const std::string& prompt) {
    std::cout << prompt;
    std::string inputZeile;
    std::getline(std::cin, inputZeile);
    return inputZeile;
}

std::string CliApp::getLastFolderName(std::string quellOrdner) const {


    //wenn String nicht leer ist dann schaue am Ende des Strings nach ob er mit einem / endet ? lösche letztes zeichen : mache nichts
    if (!quellOrdner.empty() && quellOrdner.back() == '/') {
        quellOrdner.pop_back();
    }

    char trennzeichen = '/';
    //Letzte Position des Zeichens suchen
    size_t letztePosition = quellOrdner.rfind(trennzeichen);
    //prüfe ob das Zeichen überhaupt gefunden wurde
    if (letztePosition != std::string::npos) {
        quellOrdner = quellOrdner.substr(letztePosition + 1);
    }

    return quellOrdner;

}

bool CliApp::zeigeBackupNamen(int optionenAufruf) {
    auto namen = backupService.listSetName();
    if (namen.empty()) {
        if (optionenAufruf != 1)std::cout<<"Es gibt keine Backups" << std::endl;
        return false;
    }


    std::cout << "Vorhandene Backups: " << std::endl;
    for (const std::string& name : namen) {
        std::cout << "  - " << name << std::endl;
    }
    return true;
}


