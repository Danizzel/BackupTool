

#pragma once
#include "Result.h"
#include <string>


class Archiver {
public:

    /**
     * - quellOrdner = Ordner der per tar als Backup verwendet werden soll
     * - snapshotPfad = Pfad zur .snar Datei (was im /tmp Ordner liegt zb) -> bei vollem Backup zuerst gelöscht -> bein incrementel wiederverwendet
     * - blockPfad = Pfad wohin die fertige .tar geschrieben werden soll (auch hier im /tmp Verzeichnis)
     */

    //Volles Backup: Snapshot-Datei löschen und dann tarErstellen
    Result createFull(const std::string& quellOrdner, const std::string& snapshotPfad, const std::string& blockPfad);

    //Incremental: Prüfung ob Snapshotdatei existiert (sonst Error) wenn ja -> tarErstellen() (aber nur die Änderungen)
    Result createIncremental(const std::string& quellOrdner, const std::string& snapshotPfad, const std::string& blockPfad);

    //Ganzen Block entpacken
    Result extractAll(const std::string& blockPfad, const std::string& zielOrdner);

    //Einzelne Datei oder Unterordner entpacken
    Result extractSingle(const std::string& blockPfad, const std::string& zielOrdner, const std::string& innererPfad);


private:

    //Methode um eine tar zu erstellen -> genutzt von Volles und incrementelles Backup
    Result tarErstellen(const std::string& quellOrdner, const std::string& snapshotPfad, const std::string& blockPfad);

    Result executeTarBefehl(const std::string& befehl);
};