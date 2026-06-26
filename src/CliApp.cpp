#include "CliApp.h"
#include "PrintPath.h"
#include <iostream>

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
                  << "0) Beenden\n";

        std::string auswahlMenu = userInput("> ");

        //kaputter Stream -> sauber beenden statt Endlosschleife
        if (!std::cin) break;

        if (auswahlMenu == "1") {
            std::string quellOrdnerName = userInput("Quellordner: ");
            //std::string backupName = userInput("Backup-Name: ");
            std::string backupName = getLastFolderName(quellOrdnerName);
            PrintPath printPath;
            printPath.print(quellOrdnerName);

            Result fullBackupErgebnis = backupService.fullBackup(quellOrdnerName, backupName);
            if (!fullBackupErgebnis.erfolg) {
                std::cout << fullBackupErgebnis.nachricht << std::endl;
            }else {
                std::cout << "Volles Backup erfolgreich" << std::endl;
            }

            //call der Backup Methode -> Volles Backup
        }
        else if (auswahlMenu == "2") {
            std::string quellOrdnerName = userInput("Quellordner: ");
            std::string backupName = userInput("Backup-Name: ");

            Result incBackupErgebnis = backupService.incrementalBackup(quellOrdnerName, backupName);
            if (!incBackupErgebnis.erfolg) {
                std::cout << incBackupErgebnis.nachricht << std::endl;
            }else {
                std::cout << "Incremental Backup erfolgreich" << std::endl;
            }
            //call der Backup Methode -> Incrementelles Backup
        }
        else if (auswahlMenu == "3") {
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
            std::string backupName = userInput("Backup-Name: ");
            std::string versionZeit = versionWaehlen(backupName);
            if (versionZeit.empty()) {
                continue;
            }

            Result versionWiederherstellen = backupService.restoreVersion(backupName, versionZeit);

            if (!versionWiederherstellen.erfolg) {
                std::cout << versionWiederherstellen.nachricht << std::endl;
            }else {
                std::cout << "Backup Wiederherstellung erfolgreich";
            }
        }
        else if (auswahlMenu == "5") {
            std::string backupName = userInput("Backup-Name: ");
            std::string version = userInput("Version: ");
            //von einem Ordner der innere Pfad
            std::string pfad    = userInput("Innerer Pfad: ");
            std::string ziel    = userInput("Zielordner: ");
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


