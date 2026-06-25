#include "S3Storage.h"
#include <aws/core/Aws.h>                        // Aws::FStream, Aws::MakeShared
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <fstream>

/**
 * Folgendes Muster gilt bei jedem SDK aufruf der AWS S3 buckets:
 * - Request-Objekt bauen (hier setzt man Bucket und den Key)
 * - client->PutObject/GetObject...(request) aufrufen -> dann kommt ein "outcome" zurück
 * - outcome prüfen mit outcome.IsSuccess() bei Fehler outcome.GetError().GetMessage() die Fehlermeldung holen und in Result schreiben
 *
 * .c_str() wird benötigt da aws Aws::String benötigt -> die umwandlung von std::string zu Aws::String und umgekehrt geht damit
 */

//Konstruktor
S3Storage::S3Storage(const Config &cfg) {
    config = cfg; //Config zuweisen

    Aws::Client::ClientConfiguration clientConfig; //SDK-Box
    clientConfig.region = config.region.c_str();
    client = std::make_unique<Aws::S3::S3Client>(clientConfig);
}

//Uplaod eines Objects
Result S3Storage::uploadeObject(const Aws::String &fileName, const Aws::String &objectKey) {
    Aws::S3::Model::PutObjectRequest request;

    request.SetBucket(config.bucketName.c_str());
    request.SetKey(objectKey.c_str());

    /**
     *hier die lokale Datei als Stream öffnen und an den Request geben
     * - auto = key word damit der Compiler quasi selber herausfindet was das für ein Typ ist
     * - makeshared = gibt smartpointer zurück
     * - zum öffnen einer Datei fürs Betriebssystem flags: std::ios_base::in = Input (Datei nur zum lesen geöffnet)
     *                                                      std::ios_bas::binary = öffnet Datei im Binärtmodus -> C++ ließt datei exakzt so Byte für Byte ohne Inhalt zu interpretieren
     *
     */
    auto datei = Aws::MakeShared<Aws::FStream>("uploadStream" , fileName.c_str(), std::ios_base::in | std::ios_base::binary);

    //hier wird "->" benötigt da datei ein Pointer ist (auf das Datei Objekt im Speicher)
    //good = Standardfunktion für Datei Streams in C++ -> gibt true zurück wenn all
    if (!datei -> good()) {
        return Result::fehler("Datei kann nicht gelesen werden: " + fileName);
    }
    request.SetBody(datei);

    auto outcome = client -> PutObject(request);
    if (outcome.IsSuccess()) return Result::ok();
    //Wenn was beim Upload nicht funktioniert hat dann:
    return Result::fehler("Upload fehlgeschlagen " + std::string(outcome.GetError().GetMessage().c_str()));
}

//Downlaod
Result S3Storage::downloadObject(const Aws::String &objectKey, const Aws::String &saveAsName) {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(config.bucketName.c_str());
    request.SetKey(objectKey.c_str());

    //Hier quasi der Downlaod Befehl an AWS senden
    //outcome ist hier aber noch nicht der Downlaod sondern enthält zuerst mal nur Metadaten und eine offenen Stream
    auto outcome = client -> GetObject(request);
    //Hier prüfen wir auf Fehler die auftreten können (zb Access Denied oder not found)
    if (!outcome.IsSuccess()) return Result::fehler("Download fehlgeschlagen " + std::string(outcome.GetError().GetMessage().c_str()));

    //Der heruntergeladene Inhalt (Stream) sonst in einen lokale Datei speichern (of-> Output File Stream)
    //std::ios::binary wichtig da sonst vom os bestimmte Bytes wie Zeilenumbrüche selbstständig umgewandelt werden könnten (macht runtergeladenes Bild oder PDF dann unlesbar)
    std::ofstream ziel(saveAsName, std::ios::binary);
    if (!ziel) return Result::fehler("Zieldatei kann nicht geschrieben werden " + saveAsName);

    /**
     *Hier schreiben wir die Datei nicht zuerst in den Arbeitsspeicher und dann auf die Festpaltte sonder:
     *- .GetBody() = holt den Datenstrom der direkt vom AWS Server kommt
     *- .rdbuf() = (Read Buffer) greift auf den internen Speicherbuffer des Streams zu
     *- und dann schieben wir das per << Operator in die lokale Datei
     *
     *---> Damit landen die Daten direkt auf der Festplatte und das Programm verbraucht weniger Arbeitsspeicher (auch bei großen Dateien)
     */
    ziel << outcome.GetResult().GetBody().rdbuf();
    return Result::ok();
}

//hier geben wir eine Liste zurück die alle gefundenen Dateinamen enthält
//Prefix kann hier sowas wie "bilder/2023/" zb sein -> S3 sucht dann nur Dateien die mit dem Prefix beginnen
//----> Damit "simuliert" S3 eine Ordnerstruktur
std::vector<std::string> S3Storage::listAllObjects(const std::string &prefix) {
    std::vector<std::string> keys;

    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(config.bucketName.c_str());
    //hier übergeben wir das Prefix -> aws gibt dann auch nur Dateien zurück die in dem Ordner liegen
    request.SetPrefix(prefix.c_str());

    /**
     *S3 leifert hier maximal 1000 Objekte je Antwort. Daher am besten in einer Schleife alle "Pages" holen:
     *-> solange Antwort nicht abgeschnitten ist -> mit dem Continuation Token die näcste Page holen
     */

    bool weitereSeiten = true;
    while (weitereSeiten) {
        auto outcome = client -> ListObjectsV2(request);
        if (!outcome.IsSuccess()) break; //bei fehler geben wir einfach zurück was wir bis jetzt haben (kann zb bei fehlschlagen wenn die Verbindung abbricht)

        //hier sammeln wir die Ergebnisse (auto& sorgt dafür dass die Objekte nicht in den Speicher kopiert werden (effizienter)
        //pushBack = fügt den Dateinamen (getKey) hinten an die keys Liste an
        for (const auto& objekt: outcome.GetResult().GetContents()) {
            keys.push_back(objekt.GetKey().c_str());
        }

        /**
         *Hier fragen wir aws ob es noch mehr Dateien gibt (1000 Limit)
         *--> falls ja -> holen wir mit GetNextContinuationToken die quasi
         */
        if (outcome.GetResult().GetIsTruncated()) {
            request.SetContinuationToken(outcome.GetResult().GetNextContinuationToken());
        }else {
            weitereSeiten = false;
        }
    }
    return keys;
}


bool S3Storage::objectExists(const Aws::String &objectKey) {
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(config.bucketName.c_str());
    request.SetKey(objectKey.c_str());

    //Head läd nichts herunter es fragt nur ab (um zb zu prüfen ob es eine .snar / Block gibt)
    auto outcome = client->HeadObject(request);
    return outcome.IsSuccess(); // ist true wenn es existiert
}

Result S3Storage::deleteObject(const std::string &objectKey) {
    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(config.bucketName.c_str());
    request.SetKey(objectKey.c_str());

    auto outcome = client->DeleteObject(request);
    if (outcome.IsSuccess()) return Result::ok();
    return Result::fehler("Löschen fehlgeschlagen: " + std::string(outcome.GetError().GetMessage().c_str()));
}


