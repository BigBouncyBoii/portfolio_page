#include "tracker/config.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace tracker {

namespace fs = std::filesystem;

Config default_config() {
    Config c;
    c.app_allowlist = {
        {"Code.exe", "VS Code"},
        {"devenv.exe", "Visual Studio"},
        {"WindowsTerminal.exe", "Terminal"},
        {"chrome.exe", "Chrome"},
        {"msedge.exe", "Edge"},
        {"firefox.exe", "Firefox"},
        {"explorer.exe", "File Explorer"},
    };
    return c;
}

Config load_config(const std::string& path_utf8, std::string& error) {
    const Config defaults = default_config();
    const fs::path path = fs::u8path(path_utf8);

    if (!fs::exists(path)) {
        const nlohmann::json j = {
            {"idle_threshold_s", defaults.idle_threshold_s},
            {"publish_interval_min", defaults.publish_interval_min},
            {"publish_repo", defaults.publish_repo},
            {"publish_branch", defaults.publish_branch},
            {"app_allowlist", defaults.app_allowlist},
        };
        std::ofstream(path) << j.dump(2) << '\n';
        return defaults;
    }

    try {
        std::ifstream in(path);
        const nlohmann::json j = nlohmann::json::parse(in);
        Config c;
        c.idle_threshold_s = j.value("idle_threshold_s", defaults.idle_threshold_s);
        c.publish_interval_min = j.value("publish_interval_min", defaults.publish_interval_min);
        c.publish_repo = j.value("publish_repo", defaults.publish_repo);
        c.publish_branch = j.value("publish_branch", defaults.publish_branch);
        c.app_allowlist = j.value("app_allowlist", defaults.app_allowlist);
        if (c.idle_threshold_s == 0) c.idle_threshold_s = defaults.idle_threshold_s;
        if (c.publish_interval_min == 0) c.publish_interval_min = defaults.publish_interval_min;
        return c;
    } catch (const nlohmann::json::exception& e) {
        error = std::string("config.json: ") + e.what();
        return defaults;
    }
}

}  // namespace tracker
