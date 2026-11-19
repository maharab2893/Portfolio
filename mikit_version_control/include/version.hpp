#pragma once

#include <string_view>

namespace mikit {

extern const std::string_view NAME;
extern const std::string_view VERSION;

void print_version();
bool handle_version_flag(int argc, char* argv[]);

}