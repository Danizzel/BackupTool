

#include "Archiver.h"
#include "CliApp.h"
#include "Config.h"
#include "AwsSdkSession.h"
#include "BackupService.h"
#include "S3Storage.h"

int main() {

    AwsSdkSession session;

    Config config;

    S3Storage storage(config);

    Archiver archiver;

    BackupService backupService(storage, archiver, config);


    CliApp app(backupService);
    app.run();

}
