#pragma once

#include <cstdint>
#include <string>

#include "focus_sampler_win.hpp"
#include "keyboard_hook_win.hpp"
#include "tracker/click_counter.hpp"
#include "tracker/key_counter.hpp"
#include "tracker/storage.hpp"

namespace tracker {

// Collectors + aggregator: the input hook (keys + clicks) and focus sampler
// feed the current minute's bucket, which is written to SQLite when the minute
// changes (so a crash loses at most ~1 minute). Used by console and tray mode.
//
// Call everything from one thread (the hook and sampler run their own).
class Session {
public:
    explicit Session(unsigned idle_threshold_s)
        : hook_(keys_, &clicks_), sampler_(idle_threshold_s) {}
    ~Session() { stop(); }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool start(const std::string& db_path_utf8);  // false -> see error()

    struct Tick {
        std::uint64_t keys = 0;  // counted since the last tick
        std::uint64_t clicks = 0;
        AppSeconds apps;
        bool saved = false;  // the previous minute was written this tick
    };
    // Call every few seconds.
    Tick tick();

    bool save();  // write the current bucket now (e.g. before shutdown)
    void stop();  // stop collectors and save; idempotent

    // While paused, counts are discarded (nothing is recorded).
    void set_paused(bool paused) { paused_ = paused; }
    bool paused() const { return paused_; }

    // All include the current, not-yet-saved minute.
    CountStats stats_today(Counter counter);
    std::uint64_t last_hour(Counter counter);
    AppSeconds apps_today();

    std::string state() { return sampler_.last_state(); }
    const std::string& error() const { return error_; }  // last start/save error

private:
    Tick collect();
    std::uint64_t unsaved(Counter counter) const {
        return counter == Counter::Keys ? bucket_keys_ : bucket_clicks_;
    }

    KeyCounter keys_;
    ClickCounter clicks_;
    KeyboardHook hook_;
    FocusSampler sampler_;
    Storage storage_;
    bool running_ = false;
    bool paused_ = false;

    std::int64_t bucket_minute_ = 0;
    std::uint64_t bucket_keys_ = 0;
    std::uint64_t bucket_clicks_ = 0;
    AppSeconds bucket_apps_;
    std::string error_;
};

}  // namespace tracker
