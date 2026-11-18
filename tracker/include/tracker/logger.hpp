#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace tracker {

// Appends timestamped lines to a log file. When the file grows past
// `max_bytes` it is renamed to "<name>.1" (replacing the previous one) and a
// fresh file is started, so at most ~2 x max_bytes is kept on disk.
class Logger {
public:
    bool open(const std::string& path_utf8, std::uintmax_t max_bytes = 1024 * 1024);
    void write(const std::string& line);  // thread-safe; no-op if not open

private:
    void rotate_if_needed();

    std::mutex mutex_;
    std::ofstream out_;
    std::filesystem::path path_;
    std::uintmax_t max_bytes_ = 0;
};

}  // namespace tracker
