#include <string>
#include <filesystem>

class PrintPath {
    public:
    void print(std::string);

private:
    void structurePrint(const std::filesystem::path&, int ebene = 0);

};
