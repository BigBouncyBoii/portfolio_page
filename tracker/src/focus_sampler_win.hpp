#pragma once

#include <windows.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace tracker {

// Once per second, attributes one second to the foreground app's exe name
// (e.g. "Code.exe"). Window titles are never read.
//
// No time is counted when:
//  - there has been no keyboard/touchpad input for `idle_threshold_s`
//  - the session is locked (foreground is LockApp.exe or there is none)
//  - the PC is asleep (the thread isn't running, and we add 1 s per tick
//    rather than wall-clock elapsed time, so a sleep adds nothing)
class FocusSampler {
public:
    explicit FocusSampler(unsigned idle_threshold_s = 120) : idle_threshold_s_(idle_threshold_s) {}
    ~FocusSampler() { stop(); }

    FocusSampler(const FocusSampler&) = delete;
    FocusSampler& operator=(const FocusSampler&) = delete;

    void start();
    void stop();

    // Seconds per app since the last take(); resets the totals.
    std::map<std::string, std::uint64_t> take();

    // Debug info: what the last sample saw ("idle", "locked", or an exe name).
    std::string last_state();

private:
    void run();
    std::string exe_name_for(DWORD pid);

    unsigned idle_threshold_s_;
    HANDLE stop_event_ = nullptr;
    std::thread thread_;

    std::mutex mutex_;  // guards seconds_ and last_state_
    std::map<std::string, std::uint64_t> seconds_;
    std::string last_state_;

    std::unordered_map<DWORD, std::string> name_cache_;  // sampler thread only
};

}  // namespace tracker
