#include "tracker/publish_json.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <nlohmann/json.hpp>

namespace tracker {

namespace {

using nlohmann::json;

constexpr size_t kMaxApps = 10;

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// [{"name": "VS Code", "seconds": 7800}, ...], sanitised, most used first.
json apps_json(const AppSeconds& raw, const AppAllowlist& allowlist) {
    std::map<std::string, std::uint64_t> merged;
    for (const auto& [exe, secs] : raw) merged[public_app_name(exe, allowlist)] += secs;

    std::vector<std::pair<std::string, std::uint64_t>> sorted(merged.begin(), merged.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (sorted.size() > kMaxApps) sorted.resize(kMaxApps);

    json out = json::array();
    for (const auto& [name, secs] : sorted) out.push_back({{"name", name}, {"seconds", secs}});
    return out;
}

std::uint64_t total_seconds(const AppSeconds& apps) {
    std::uint64_t total = 0;
    for (const auto& entry : apps) total += entry.second;
    return total;
}

// {"keys", "clicks", "active_minutes", "app_seconds", "apps"} for [from, to).
json totals_json(Storage& storage, std::int64_t from, std::int64_t to,
                 const AppAllowlist& allowlist) {
    std::uint64_t keys = 0, clicks = 0, active = 0;
    for (const auto& row : storage.input_between(from, to)) {
        keys += row.keys;
        clicks += row.clicks;
        if (row.keys > 0 || row.clicks > 0) ++active;
    }
    const AppSeconds apps = storage.app_seconds_between(from, to);
    return {{"keys", keys},
            {"clicks", clicks},
            {"active_minutes", active},
            {"app_seconds", total_seconds(apps)},
            {"apps", apps_json(apps, allowlist)}};
}

json day_object(Storage& storage, const DayRange& day, const AppAllowlist& allowlist) {
    json hours = json::array();
    for (int h = 0; h < 24; ++h) hours.push_back({{"hour", h}, {"keys", 0}, {"clicks", 0}});

    for (const auto& row : storage.input_between(day.start, day.end)) {
        // Clamp: a 25-hour DST day folds its extra hour into 23:00.
        const auto h = std::min<std::int64_t>((row.minute_utc - day.start) / 60, 23);
        json& bucket = hours[static_cast<size_t>(h)];
        bucket["keys"] = bucket["keys"].get<std::uint64_t>() + row.keys;
        bucket["clicks"] = bucket["clicks"].get<std::uint64_t>() + row.clicks;
    }

    json out = totals_json(storage, day.start, day.end, allowlist);
    out["date"] = day.date;
    out["hours"] = std::move(hours);
    return out;
}

}  // namespace

std::string public_app_name(const std::string& exe, const AppAllowlist& allowlist) {
    const std::string key = lower(exe);
    for (const auto& [name, label] : allowlist) {
        if (lower(name) == key) return label;
    }
    return "Other";
}

std::string day_json(Storage& storage, const DayRange& day, const AppAllowlist& allowlist) {
    return day_object(storage, day, allowlist).dump(2) + "\n";
}

std::string summary_json(Storage& storage, const std::vector<DayRange>& days,
                         const AppAllowlist& allowlist, const std::string& generated_at) {
    if (days.empty()) return "{}\n";
    const DayRange& today = days.back();
    const DayRange& week_start = days[days.size() >= 7 ? days.size() - 7 : 0];

    json daily = json::array();
    for (const DayRange& d : days) {
        json t = totals_json(storage, d.start, d.end, allowlist);
        t.erase("apps");
        t["date"] = d.date;
        daily.push_back(std::move(t));
    }

    const json out = {
        {"generated_at", generated_at},
        {"today", day_object(storage, today, allowlist)},
        {"last_7_days", totals_json(storage, week_start.start, today.end, allowlist)},
        {"last_30_days", totals_json(storage, days.front().start, today.end, allowlist)},
        {"all_time", totals_json(storage, 0, INT64_MAX, allowlist)},
        {"daily", std::move(daily)},
    };
    return out.dump(2) + "\n";
}

}  // namespace tracker
