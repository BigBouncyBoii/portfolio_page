#pragma once

#include <chrono>
#include <future>
#include <map>
#include <string>
#include <vector>

#include "session_win.hpp"
#include "tracker/config.hpp"
#include "tracker/logger.hpp"

namespace tracker {

// GitHub token in Windows Credential Manager (generic credential). An empty
// token deletes it.
bool save_token(const std::string& token, std::string& message);

// Builds the sanitised JSON every `publish_interval_min` and uploads changed
// files to GitHub (Contents API) on a background thread. A copy of every file
// is also written to %LOCALAPPDATA%\PortfolioTracker\publish\.
// Call tick() from the same thread as the Session.
class Publisher {
public:
    Publisher(const Config& config, Logger& log) : config_(config), log_(log) {}
    ~Publisher();  // waits for an upload in flight (bounded by HTTP timeouts)

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

    void tick(Session& session);  // call every few seconds; publishes when due
    void publish_now(Session& session);

    // Latest outcome ("published 3 files", ...), cleared once read.
    std::string take_status();

private:
    using Clock = std::chrono::steady_clock;
    using Files = std::vector<std::pair<std::string, std::string>>;  // repo path, content

    void collect_result();
    void set_status(const std::string& status);

    Config config_;
    Logger& log_;
    Clock::time_point next_{};  // first tick publishes
    unsigned failures_ = 0;
    std::future<bool> upload_;
    Files in_flight_;
    std::map<std::string, std::string> published_;  // repo path -> last uploaded content
    std::string status_;
};

}  // namespace tracker
