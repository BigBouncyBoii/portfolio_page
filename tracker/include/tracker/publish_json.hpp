#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "tracker/storage.hpp"

namespace tracker {

using AppAllowlist = std::map<std::string, std::string>;  // exe name -> public label

struct DayRange {
    std::string date;        // "YYYY-MM-DD", local time
    std::int64_t start = 0;  // local midnight, unix minutes
    std::int64_t end = 0;    // next local midnight
};

// Sanitiser: allowlisted exe (case-insensitive) -> its label, anything else -> "Other".
std::string public_app_name(const std::string& exe, const AppAllowlist& allowlist);

// stats/YYYY-MM-DD.json: day totals, 24 hourly buckets and apps.
std::string day_json(Storage& storage, const DayRange& day, const AppAllowlist& allowlist);

// stats/summary.json: today, last 7 / 30 days, all time and a 30-day series.
// `days` = the last 30 days, oldest first (today last). `generated_at` = ISO-8601 UTC.
std::string summary_json(Storage& storage, const std::vector<DayRange>& days,
                         const AppAllowlist& allowlist, const std::string& generated_at);

}  // namespace tracker
