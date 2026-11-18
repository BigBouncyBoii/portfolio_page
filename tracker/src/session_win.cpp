#include "session_win.hpp"

#include <algorithm>

#include "util_win.hpp"

namespace tracker {

bool Session::start(const std::string& db_path_utf8) {
    if (!storage_.open(db_path_utf8)) {
        error_ = "could not open database: " + storage_.error();
        return false;
    }
    if (!hook_.start()) {
        error_ = "could not install keyboard hook: " + hook_.error();
        return false;
    }
    sampler_.start();
    bucket_minute_ = now_minute_utc();
    running_ = true;
    return true;
}

Session::Tick Session::collect() {
    Tick t;
    t.keys = counter_.take();
    t.apps = sampler_.take();
    if (paused_) return Tick{};

    bucket_keys_ += t.keys;
    for (const auto& [app, secs] : t.apps) bucket_apps_[app] += secs;
    return t;
}

Session::Tick Session::tick() {
    bool saved = false;
    const std::int64_t minute = now_minute_utc();
    if (minute != bucket_minute_) {
        save();
        bucket_minute_ = minute;
        saved = true;
    }
    Tick t = collect();
    t.saved = saved;
    return t;
}

bool Session::save() {
    error_.clear();
    const bool ok = storage_.flush(bucket_minute_, bucket_keys_, bucket_apps_);
    if (!ok) error_ = "save failed: " + storage_.error();
    bucket_keys_ = 0;
    bucket_apps_.clear();
    return ok;
}

void Session::stop() {
    if (!running_) return;
    running_ = false;
    hook_.stop();
    sampler_.stop();
    collect();
    save();
}

std::uint64_t Session::keys_today() {
    return storage_.keys_since(local_midnight_minute_utc()) + bucket_keys_;
}

std::uint64_t Session::keys_last_hour() {
    return storage_.keys_since(now_minute_utc() - 60) + bucket_keys_;
}

KeyStats Session::key_stats_today() {
    KeyStats s = storage_.key_stats_since(local_midnight_minute_utc());
    if (bucket_keys_ > 0) {
        s.total += bucket_keys_;
        s.active_minutes += 1;
        s.peak_minute = std::max(s.peak_minute, bucket_keys_);
    }
    return s;
}

AppSeconds Session::apps_today() {
    AppSeconds apps = storage_.app_seconds_since(local_midnight_minute_utc());
    for (const auto& [app, secs] : bucket_apps_) apps[app] += secs;
    return apps;
}

}  // namespace tracker
