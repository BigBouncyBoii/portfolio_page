#include "tracker/cli.hpp"

#include "tracker/version.hpp"

namespace tracker {

ParseResult parse_args(const std::vector<std::string>& args) {
    Options opts;
    bool command_set = false;

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        std::optional<Command> command;

        if (arg == "--help" || arg == "-h") {
            command = Command::Help;
        } else if (arg == "--version" || arg == "-v") {
            command = Command::Version;
        } else if (arg == "--install") {
            command = Command::Install;
        } else if (arg == "--uninstall") {
            command = Command::Uninstall;
        } else if (arg == "--publish-now") {
            command = Command::PublishNow;
        } else if (arg == "--set-token") {
            if (i + 1 >= args.size()) return ParseResult{std::nullopt, "--set-token needs a value"};
            command = Command::SetToken;
            opts.token = args[++i];
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
           "  -h, --help         Show this help and exit\n"
           "  -v, --version      Show version and exit\n"
           "      --console      Run with a debug console window showing live counts\n"
           "      --install      Copy to %LOCALAPPDATA%\\PortfolioTracker, start at login, launch\n"
           "      --uninstall    Stop the tracker and remove auto-start (data is kept)\n"
           "      --publish-now  Ask the running tracker to publish stats now\n"
           "\n"
           "With no options, runs in the background with a tray icon.\n";
}

}  // namespace tracker
