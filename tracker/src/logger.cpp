#include "tracker/logger.hpp"

#include <ctime>

namespace tracker {

namespace fs = std::filesystem;

bool Logger::open(const std::string& path_utf8, std::uintmax_t max_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = fs::u8path(path_utf8);
    max_bytes_ = max_bytes;
    rotate_if_needed();
    if (!out_.is_open()) out_.open(path_, std::ios::app);
    return out_.is_open();
}

void Logger::write(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!out_.is_open()) return;
    rotate_if_needed();

    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    out_ << stamp << "  " << line << '\n';
    out_.flush();
}

void Logger::rotate_if_needed() {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(path_, ec);
    if (ec || size < max_bytes_) return;

    out_.close();
    fs::path old = path_;
    old += ".1";
    fs::remove(old, ec);
    fs::rename(path_, old, ec);
    out_.open(path_, std::ios::app);
}

}  // namespace tracker
