/**
 *Config File für die AWS SDK -> ist quasi Daten Box für Bucketname, Region, Arbeitsordner
 */
#pragma once
#include <cstring>
#include <string>

struct Config {
    std::string bucketName = "amzn-s3-bucket-daniel-363786804928-eu-north-1-an";
    std::string region = "eu-north-1";


    /**
     *Wenn diese Paramter leer bleiben nutzt die SDK automatisch den Weg ~/.aws/credentials
     *aber wenn sie gegeben sind -> an S3Client weiter geben
     */
    //Credentials werden leer gelassen -> SDK nutzt dann automatisch die Default Credential (weg über aws cli)

    std::string accessKey;
    std::string secretKey;

    std::string basePrefix = "backups/";
    std::string arbeitsOrdner;
    std::string restoreOrdner;


    Config() {
        //Home Verzeichnis des aktuellen Nutzers holen (Pointer und Char da getenv aus C)
        const char* home = getenv("HOME");
        std::string homedir = (home!= nullptr) ? std::string(home) : ".";

        arbeitsOrdner = homedir + "/s3backups_tmp";
        restoreOrdner = homedir + "/restore";
    }
};