#include "focus_sampler_win.hpp"

#include <utility>

namespace tracker {
namespace {

std::string to_utf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int src_len = static_cast<int>(wide.size());
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), src_len, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), src_len, out.data(), len, nullptr, nullptr);
    return out;
}

unsigned seconds_since_last_input() {
    LASTINPUTINFO info{sizeof(LASTINPUTINFO), 0};
    if (!GetLastInputInfo(&info)) return 0;
    return (GetTickCount() - info.dwTime) / 1000;  // unsigned wrap-around is fine
}

}  // namespace

void FocusSampler::start() {
    if (thread_.joinable()) return;
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    thread_ = std::thread([this] { run(); });
}

void FocusSampler::stop() {
    if (!thread_.joinable()) return;
    SetEvent(stop_event_);
    thread_.join();
    CloseHandle(stop_event_);
    stop_event_ = nullptr;
}

std::map<std::string, std::uint64_t> FocusSampler::take() {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::exchange(seconds_, {});
}

std::string FocusSampler::last_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_state_;
}

void FocusSampler::run() {
    int ticks = 0;
    while (WaitForSingleObject(stop_event_, 1000) == WAIT_TIMEOUT) {
        // PIDs get reused, so drop the cache every 10 minutes.
        if (++ticks % 600 == 0) name_cache_.clear();

        std::string state;
        HWND hwnd = GetForegroundWindow();
        DWORD pid = 0;
        if (hwnd != nullptr) GetWindowThreadProcessId(hwnd, &pid);

        if (seconds_since_last_input() >= idle_threshold_s_) {
            state = "idle";
        } else if (pid == 0) {
            state = "locked";
        } else {
            state = exe_name_for(pid);
            if (state == "LockApp.exe") state = "locked";
        }

        std::lock_guard<std::mutex> lock(mutex_);
        last_state_ = state;
        if (state != "idle" && state != "locked") ++seconds_[state];
    }
}

std::string FocusSampler::exe_name_for(DWORD pid) {
    if (auto it = name_cache_.find(pid); it != name_cache_.end()) return it->second;

    std::string name = "Unknown";
    if (HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(proc, 0, path, &size)) {
            std::wstring full(path, size);
            name = to_utf8(full.substr(full.find_last_of(L'\\') + 1));
        }
        CloseHandle(proc);
    }
    name_cache_[pid] = name;
    return name;
}

}  // namespace tracker
