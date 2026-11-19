#include "commands.hpp"
#include "version.hpp"
#include <iostream>
#include <string_view>

struct CommandEntry {
    std::string_view name;
    int (*handler)(int argc, char* argv[]);
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mikit <command> [args...]\n";
        return 1;
    }

    if (mikit::handle_version_flag(argc, argv)) return 0;

    constexpr CommandEntry commands[] = {
        {"init",     mikit::cmd_init},
        {"add",      mikit::cmd_add},
        {"status",   mikit::cmd_status},
        {"commit",   mikit::cmd_commit},
        {"cat-file", mikit::cmd_cat_file},
        {"log",      mikit::cmd_log},
        {"config",   mikit::cmd_config},
    };

    std::string_view cmd = argv[1];
    for (const auto& entry : commands) {
        if (entry.name == cmd) {
            return entry.handler(argc, argv);
        }
    }

    std::cerr << "mikit: '" << cmd << "' is not a mikit command.\n";
    return 1;
}