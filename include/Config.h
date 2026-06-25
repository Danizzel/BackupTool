/**
 *Config File für die AWS SDK -> ist quasi Daten Box für Bucketname, Region, Arbeitsordner
 */
#pragma once
#include <string>

struct Config {
    std::string bucketName = "amzn-s3-bucket-daniel-363786804928-eu-north-1-an";
    std::string region = "eu-north-1";

    //Credentials werden leer gelassen -> SDK nutzt dann automatisch die Default Credential
    std::string basePrefix = "backups/";
    std::string arbeitsOrdner = "/tmp/s3backup";
};