#pragma once

#include <cstdint>
#include <string>

namespace tracker {

std::string to_utf8(const wchar_t* wide);
std::wstring to_wide(const std::string& utf8);

std::wstring data_dir();                     // %LOCALAPPDATA%\PortfolioTracker (created if missing)
std::string data_path(const wchar_t* file);  // UTF-8 path of `file` inside data_dir()
std::wstring exe_path();                     // full path of the running tracker.exe

std::int64_t now_minute_utc();             // unix time / 60
std::int64_t local_midnight_minute_utc();  // start of today (local time), unix minutes

}  // namespace tracker
