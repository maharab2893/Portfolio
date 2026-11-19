#include "version.hpp"
#include <iostream>

namespace mikit {

const std::string_view NAME = "MIKIT";
const std::string_view VERSION = "1.0.0.0";

void print_version() {
    std::cout << NAME << " " << VERSION << "\n";
}

bool handle_version_flag(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            print_version();
            return true;
        }
    }
    return false;
}

}