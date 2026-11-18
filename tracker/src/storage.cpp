#include "tracker/storage.hpp"

#include <sqlite3.h>

namespace tracker {
namespace {

constexpr const char* kSchema = R"sql(
    PRAGMA journal_mode = WAL;
    PRAGMA synchronous = NORMAL;
    CREATE TABLE IF NOT EXISTS input_minute (
        minute_utc INTEGER PRIMARY KEY,
        keys       INTEGER NOT NULL DEFAULT 0,
        clicks     INTEGER NOT NULL DEFAULT 0
    );
    CREATE TABLE IF NOT EXISTS app_minute (
        minute_utc INTEGER NOT NULL,
        app        TEXT    NOT NULL,
        seconds    INTEGER NOT NULL DEFAULT 0,
        PRIMARY KEY (minute_utc, app)
    );
)sql";

// Minimal RAII wrapper so early returns always finalize the statement.
struct Stmt {
    sqlite3_stmt* s = nullptr;
    ~Stmt() { sqlite3_finalize(s); }
};

}  // namespace

Storage::~Storage() {
    sqlite3_close(db_);
}

bool Storage::fail(const char* what) {
    error_ = std::string(what) + ": " + (db_ ? sqlite3_errmsg(db_) : "no database");
    return false;
}

bool Storage::exec(const char* sql) {
    return sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK || fail("exec");
}

bool Storage::open(const std::string& path_utf8) {
    if (sqlite3_open(path_utf8.c_str(), &db_) != SQLITE_OK) return fail("open");
    sqlite3_busy_timeout(db_, 2000);
    if (!exec(kSchema)) return false;

    // Databases created before click counting have no `clicks` column.
    Stmt probe;
    if (sqlite3_prepare_v2(db_, "SELECT clicks FROM input_minute LIMIT 0", -1, &probe.s,
                           nullptr) != SQLITE_OK) {
        return exec("ALTER TABLE input_minute ADD COLUMN clicks INTEGER NOT NULL DEFAULT 0");
    }
    return true;
}

bool Storage::flush(std::int64_t minute_utc, std::uint64_t keys, std::uint64_t clicks,
                    const AppSeconds& apps) {
    if (keys == 0 && clicks == 0 && apps.empty()) return true;
    if (!exec("BEGIN")) return false;

    bool ok = true;
    if (keys > 0 || clicks > 0) {
        Stmt st;
        ok = sqlite3_prepare_v2(db_,
                                "INSERT INTO input_minute (minute_utc, keys, clicks) VALUES (?, ?, ?) "
                                "ON CONFLICT(minute_utc) DO UPDATE SET keys = keys + excluded.keys, "
                                "clicks = clicks + excluded.clicks",
                                -1, &st.s, nullptr) == SQLITE_OK;
        ok = ok && sqlite3_bind_int64(st.s, 1, minute_utc) == SQLITE_OK &&
             sqlite3_bind_int64(st.s, 2, static_cast<sqlite3_int64>(keys)) == SQLITE_OK &&
             sqlite3_bind_int64(st.s, 3, static_cast<sqlite3_int64>(clicks)) == SQLITE_OK &&
             sqlite3_step(st.s) == SQLITE_DONE;
    }

    if (ok && !apps.empty()) {
        Stmt st;
        ok = sqlite3_prepare_v2(
                 db_,
                 "INSERT INTO app_minute (minute_utc, app, seconds) VALUES (?, ?, ?) "
                 "ON CONFLICT(minute_utc, app) DO UPDATE SET seconds = seconds + excluded.seconds",
                 -1, &st.s, nullptr) == SQLITE_OK;
        for (auto it = apps.begin(); ok && it != apps.end(); ++it) {
            ok = sqlite3_bind_int64(st.s, 1, minute_utc) == SQLITE_OK &&
                 sqlite3_bind_text(st.s, 2, it->first.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
                 sqlite3_bind_int64(st.s, 3, static_cast<sqlite3_int64>(it->second)) == SQLITE_OK &&
                 sqlite3_step(st.s) == SQLITE_DONE;
            sqlite3_reset(st.s);
        }
    }

    if (!ok) {
        fail("flush");
        exec("ROLLBACK");
        return false;
    }
    return exec("COMMIT");
}

CountStats Storage::stats_since(Counter counter, std::int64_t minute_utc) {
    // Column names are fixed strings, never user input.
    const char* sql = counter == Counter::Keys
        ? "SELECT COALESCE(SUM(keys), 0), COUNT(*), COALESCE(MAX(keys), 0) "
          "FROM input_minute WHERE minute_utc >= ? AND keys > 0"
        : "SELECT COALESCE(SUM(clicks), 0), COUNT(*), COALESCE(MAX(clicks), 0) "
          "FROM input_minute WHERE minute_utc >= ? AND clicks > 0";
    CountStats out;
    Stmt st;
    if (sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr) != SQLITE_OK) {
        fail("query");
        return out;
    }
    sqlite3_bind_int64(st.s, 1, minute_utc);
    if (sqlite3_step(st.s) != SQLITE_ROW) return out;
    out.total = static_cast<std::uint64_t>(sqlite3_column_int64(st.s, 0));
    out.active_minutes = static_cast<std::uint64_t>(sqlite3_column_int64(st.s, 1));
    out.peak_minute = static_cast<std::uint64_t>(sqlite3_column_int64(st.s, 2));
    return out;
}

AppSeconds Storage::app_seconds_since(std::int64_t minute_utc) {
    AppSeconds out;
    Stmt st;
    if (sqlite3_prepare_v2(db_,
                           "SELECT app, SUM(seconds) FROM app_minute WHERE minute_utc >= ? GROUP BY app",
                           -1, &st.s, nullptr) != SQLITE_OK) {
        fail("query");
        return out;
    }
    sqlite3_bind_int64(st.s, 1, minute_utc);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        const auto* app = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 0));
        out[app ? app : ""] = static_cast<std::uint64_t>(sqlite3_column_int64(st.s, 1));
    }
    return out;
}

}  // namespace tracker
