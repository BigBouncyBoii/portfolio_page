#pragma once

#include <cstdint>
#include <map>
#include <string>

struct sqlite3;

namespace tracker {

using AppSeconds = std::map<std::string, std::uint64_t>;

struct KeyStats {
    std::uint64_t total = 0;
    std::uint64_t active_minutes = 0;  // minutes with at least one keystroke
    std::uint64_t peak_minute = 0;     // most keystrokes in a single minute
};

// Per-minute buckets in a local SQLite database (WAL mode).
//   input_minute(minute_utc, keys)
//   app_minute(minute_utc, app, seconds)
// minute_utc = unix time / 60. Flushes are additive, so flushing the same
// minute twice (e.g. after a restart) just adds to it.
class Storage {
public:
    Storage() = default;
    ~Storage();
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    bool open(const std::string& path_utf8);  // creates file + tables if needed
    bool flush(std::int64_t minute_utc, std::uint64_t keys, const AppSeconds& apps);

    std::uint64_t keys_since(std::int64_t minute_utc);
    KeyStats key_stats_since(std::int64_t minute_utc);
    AppSeconds app_seconds_since(std::int64_t minute_utc);

    const std::string& error() const { return error_; }

private:
    bool exec(const char* sql);
    bool fail(const char* what);

    sqlite3* db_ = nullptr;
    std::string error_;
};

}  // namespace tracker
