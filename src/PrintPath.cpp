
#include "PrintPath.h"

#include <iostream>
#include <string>

void PrintPath::print(std::string path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Pfad existiert nicht: " << path << std::endl;
    }else {
        structurePrint(path);
    }

}
void PrintPath::structurePrint(const std::filesystem::path& path, int ebene) {

    try {
        for (const auto& eintrag : std::filesystem::directory_iterator(path)) {
            //für Baumoptik in der Konsole
            for (int i = 0; i < ebene; i++) {
                std::cout << "  ";
            }

            //Dateisymbol
            std::cout << "|--";

            //Dateiname
            std::cout << eintrag.path().filename().string() << std::endl;

            if (std::filesystem::is_directory(eintrag.status())) {
                structurePrint(eintrag.path(), ebene + 1);
            }
        }
    }catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Fehler: "<< e.what() << std::endl;
    }

}
