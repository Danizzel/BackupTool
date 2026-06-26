#pragma once
#include "Archiver.h"
#include "Config.h"
#include "S3Storage.h"
#include <ctime>
#include <string>
#include <vector>

//Helfer Struct zum Auflisten der Backups in den S3 Bucket und zur Wiederherstellung eines Backups
struct HelferBlock{
    std::string key; //zeitstempel zb 2026-06-19_112500
    std::string zeit; // vollständige S3 Kez -> "backups/bilder/urlaubfoto_2026-06-19_112500_full oder inc.tar.gz
    std::string typ; //full oder inc
};

class BackupService {
public:
    //Referenzen der Objekte übergeben und Config anlegen
    BackupService(S3Storage& storage, Archiver& archiver, const Config& config);

    Result fullBackup(const std::string& quellOrdner, const std::string& setName);


    Result incrementalBackup(const std::string& quellOrdner, const std::string& setName);

    Result restoreVersion(const std::string& setName, const std::string& zielZeit);

    std::vector<HelferBlock> listVersion(const std::string& setName);


private:
    S3Storage& storage;
    Archiver& archiver;
    Config config;

    //Format: "%Y-%m-%d_%H%M%S"
    std::string zeitStempel() const;

    /**
     * baut Key für den Backup Block
     * setName = zb bilder
     * zeitStempel
     * typ = zb full oder inc
     *
     * davor aber Config.basePrefix + rest...
     */
    std::string blockKey(const std::string& setName, const std::string& zeitStempel, const std::string& typ) const;

    /**
     *baut den Snapshot Key (.snar)
     *- nur SetName kein ZeitStemepl
     *=> config.basePrefix + setName + / + setName + .snar
     */
    std::string snapKey(const std::string& setName) const;


    HelferBlock parseBlock (const std::string& key) const;


    /**
     *1. jüngsten full Backup Block im Bucket unter den setName (zb pvl) && <= zielZeit
     *2. full block holen und danach:
     *3. alle Blöcke mit inc die auch Blockzeit <= zielZeit haben (der Reihe nach vom neuesten zum ältesten inc Block)
     *
     *Beispiel Bucket:
     *V1 1011 full
     *V2 1109 inc
     *V3 1220 inc  <-- Zielzeit vom Nutzer gewählt
     *V4 1300 inc
     *V5 1600 inc
     *V6 1800 full
     *
     *Kette = V1 (full) + V2 (inc) + V3 (inc) rest wird nicht beachtet
     */

    std::vector<HelferBlock> findeKette(const std::string& setName, const std::string& zielZeit);

    std::string dateiname (const std::string& key) const;

};