#include "util_win.hpp"

#include <windows.h>

#include <ctime>

namespace tracker {

std::string to_utf8(const wchar_t* wide) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int src_len = static_cast<int>(utf8.size());
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), src_len, nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), src_len, out.data(), len);
    return out;
}

std::wstring data_dir() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L".";
    std::wstring dir = std::wstring(buf) + L"\\PortfolioTracker";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::string data_path(const wchar_t* file) {
    return to_utf8((data_dir() + L"\\" + file).c_str());
}

std::wstring exe_path() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, n);
}

std::int64_t now_minute_utc() {
    return static_cast<std::int64_t>(std::time(nullptr)) / 60;
}

std::int64_t local_midnight_minute_utc() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    local.tm_hour = local.tm_min = local.tm_sec = 0;
    return static_cast<std::int64_t>(std::mktime(&local)) / 60;
}

}  // namespace tracker
