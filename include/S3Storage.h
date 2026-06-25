#pragma once
#include "Result.h"
#include "Config.h"

#include <string>
#include <vector>
#include <aws/s3/S3Client.h>
#include <memory> // -> für den unique pointer


/**
 * Klasse die mit Aws spricht
 */
class S3Storage {
public:

    //Hier im Konstruktor Client bauen mit der Region aus config + BucketNamen
    //einmaliger bau von S3Client
    explicit S3Storage(const Config& config);


    Result uploadeObject(const Aws::String& fileName, const Aws::String& objectKey);

    Result downloadObject(const Aws::String& objectKey, const Aws::String& saveAsName);

    std::vector<std::string> listAllObjects(const Aws::String& bucketName);

    Result deleteObject(const Aws::String& objectKey);

    bool objectExists(const Aws::String& objectKey);

private:
    //Config hält hier BucketName und Region
    Config config;

    /**
     * hier unique pointer da Client genau einen Besitzer hat -> dieses S3Storage Object
     * -> dieser lebt und stirbt mit diesem
     *
     * ein normalen Client im Konstruktor bauen geht auch nicht da dieser hier als in der Initialisierungsliste gebaut wird
     * -> bevor der Konstruktor Rumpf überhaupt läuft -> würde man im Rump client= schreiben wäre client schon vorher mit default konstruiert und man würde ihn nur
     * überschreiben
     */
    std::unique_ptr<Aws::S3::S3Client> client;


};

