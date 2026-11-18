#pragma once

#include <optional>
#include <string>
#include <vector>

namespace tracker {

enum class Command {
    Run,      // default: start tracking
    Help,     // --help / -h
    Version,  // --version / -v
    Install,     // --install: copy to %LOCALAPPDATA%, auto-start at login, launch
    Uninstall,   // --uninstall: stop, remove auto-start (data is kept)
    PublishNow,  // --publish-now: ask the running tracker to publish
};

struct Options {
    Command command = Command::Run;
    bool console = false;  // --console: show debug output in a terminal
};

struct ParseResult {
    std::optional<Options> options;  // set on success
    std::string error;               // set on failure
};

// Parses command-line arguments. `args` must NOT include the program name.
ParseResult parse_args(const std::vector<std::string>& args);

// Human-readable usage text for --help.
std::string usage();

}  // namespace tracker
