#include "tracker/cli.hpp"

#include "tracker/version.hpp"

namespace tracker {

ParseResult parse_args(const std::vector<std::string>& args) {
    Options opts;
    bool command_set = false;

    for (const auto& arg : args) {
        std::optional<Command> command;

        if (arg == "--help" || arg == "-h") {
            command = Command::Help;
        } else if (arg == "--version" || arg == "-v") {
            command = Command::Version;
        } else if (arg == "--console") {
            opts.console = true;
        } else {
            return ParseResult{std::nullopt, "unknown option: " + arg};
        }

        if (command) {
            if (command_set) {
                return ParseResult{std::nullopt, "conflicting option: " + arg};
            }
            opts.command = *command;
            command_set = true;
        }
    }

    return ParseResult{opts, {}};
}

std::string usage() {
    return std::string(kName) + " " + kVersion +
           "\n"
           "Tracks PC activity (counts only) and publishes stats to a portfolio page.\n"
           "\n"
           "Usage: tracker.exe [options]\n"
           "\n"
           "Options:\n"
           "  -h, --help       Show this help and exit\n"
           "  -v, --version    Show version and exit\n"
           "      --console    Print debug output to the launching terminal\n";
}

}  // namespace tracker
