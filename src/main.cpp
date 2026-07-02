

#include "Archiver.h"
#include "CliApp.h"
#include "Config.h"
#include "AwsSdkSession.h"
#include "BackupService.h"
#include "S3Storage.h"

int main(int argc, char* argv[]) {

    AwsSdkSession session;

    Config config;

    /**
     *Wenn genung Argumente übergeben werden -> lese Credentials daraus
     *hier check auf 3 oder 4 Argumente da das erste Argument argv[0] immer der Name der ausführbaren Datei ist
     */
    if (argc >= 3) {
        config.accessKey = argv[1];
        config.secretKey = argv[2];

        if (argc>= 4) {
            config.region = argv[3]; //Region kommt als 3 Argument
        }
        std::cout << "Credentials aus Programmargumenten übernommen" << std::endl;
    }else {
        std::cout << "Keine Credential-Argumente -> nutze ~/.aws/credentials" << std::endl;
    }

    S3Storage storage(config);

    Archiver archiver;

    BackupService backupService(storage, archiver, config);


    CliApp app(backupService);
    app.run();

}
