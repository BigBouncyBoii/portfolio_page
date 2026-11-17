#pragma once

#include <windows.h>

#include <string>
#include <thread>

#include "tracker/key_counter.hpp"

namespace tracker {

// Installs a WH_KEYBOARD_LL hook on a dedicated thread that runs its own
// message loop, forwarding key events to a KeyCounter.
//
// The hook callback does nothing but update the counter and return: Windows
// silently removes low-level hooks that respond slowly.
//
// Only one KeyboardHook may be running per process (the hook callback has no
// user-data pointer, so the counter is reached through a global).
class KeyboardHook {
public:
    explicit KeyboardHook(KeyCounter& counter) : counter_(counter) {}
    ~KeyboardHook() { stop(); }

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    // Starts the hook thread and waits until the hook is installed.
    // Returns false (see error()) if it could not be installed.
    bool start();

    // Posts WM_QUIT to the hook thread, which unhooks and exits. Idempotent.
    void stop();

    bool running() const { return thread_.joinable(); }
    const std::string& error() const { return error_; }

private:
    KeyCounter& counter_;
    std::thread thread_;
    DWORD thread_id_ = 0;
    std::string error_;
};

}  // namespace tracker
