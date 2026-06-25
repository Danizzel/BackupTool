#pragma once
#include <string>
#include "BackupService.h"

class CliApp {
public:

    CliApp(BackupService backupService);
    int run();

private:
    BackupService backupService;
    std::string userInput(const std::string& prompt);

    std::string getLastFolderName(std::string quellOrdner) const;

    std::string versionWaehlen(const std::string& backupName);

};

