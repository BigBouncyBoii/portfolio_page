#pragma once

#include <map>
#include <string>

namespace tracker {

// Settings read from config.json (next to the database). Edit the file and
// restart the tracker to apply changes.
struct Config {
    unsigned idle_threshold_s = 120;   // no input for this long = idle
    unsigned publish_interval_min = 30;  // how often stats are pushed to GitHub
    std::string publish_repo = "BigBouncyBoii/portfolio_page";  // "owner/repo"; empty = local only
    std::string publish_branch = "data";
    // exe name -> public label; anything not listed is published as "Other".
    std::map<std::string, std::string> app_allowlist;
};

Config default_config();

// Reads `path`. If the file doesn't exist, writes the defaults there first.
// On a parse error, returns the defaults and sets `error`.
Config load_config(const std::string& path_utf8, std::string& error);

}  // namespace tracker
