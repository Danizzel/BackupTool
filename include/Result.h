#pragma once
#include <string>

// Rückgabetyp ob eine Operation funktioniert
struct Result {
    bool erfolg;
    std::string nachricht;

    static Result ok() {
        return {true,  ""};
    }
    static Result fehler(const std::string& m) {
        return {false, m};
    }
};