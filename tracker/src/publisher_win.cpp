#include "publisher_win.hpp"

#include <windows.h>

#include <wincred.h>
#include <winhttp.h>

#include <algorithm>
#include <ctime>
#include <fstream>

#include <nlohmann/json.hpp>

#include "tracker/publish_json.hpp"
#include "util_win.hpp"

namespace tracker {

namespace {

using nlohmann::json;

constexpr wchar_t kCredTarget[] = L"PortfolioTracker/GitHubToken";
constexpr int kSummaryDays = 30;

std::string load_token() {
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(kCredTarget, CRED_TYPE_GENERIC, 0, &cred)) return {};
    std::string token(reinterpret_cast<const char*>(cred->CredentialBlob),
                      cred->CredentialBlobSize);
    CredFree(cred);
    return token;
}

std::string base64(const std::string& in) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto byte = [&](size_t i) { return i < in.size() ? static_cast<unsigned char>(in[i]) : 0u; };
    std::string out;
    for (size_t i = 0; i < in.size(); i += 3) {
        const unsigned n = (byte(i) << 16) | (byte(i + 1) << 8) | byte(i + 2);
        out += kTable[(n >> 18) & 63];
        out += kTable[(n >> 12) & 63];
        out += i + 1 < in.size() ? kTable[(n >> 6) & 63] : '=';
        out += i + 2 < in.size() ? kTable[n & 63] : '=';
    }
    return out;
}

// The last `n` local days, oldest first (today last).
std::vector<DayRange> last_days(int n) {
    const std::time_t now = std::time(nullptr);
    std::tm today{};
    localtime_s(&today, &now);
    today.tm_hour = today.tm_min = today.tm_sec = 0;

    std::vector<DayRange> days;
    for (int i = n - 1; i >= 0; --i) {
        std::tm d = today;
        d.tm_mday -= i;
        d.tm_isdst = -1;
        const std::time_t start = std::mktime(&d);  // also normalises d
        char date[16];
        std::strftime(date, sizeof(date), "%Y-%m-%d", &d);
        std::tm next = d;
        next.tm_mday += 1;
        next.tm_isdst = -1;
        days.push_back({date, static_cast<std::int64_t>(start) / 60,
                        static_cast<std::int64_t>(std::mktime(&next)) / 60});
    }
    return days;
}

std::string utc_now_iso() {
    const std::time_t now = std::time(nullptr);
    std::tm t{};
    gmtime_s(&t, &now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return buf;
}

void write_local_copy(const std::string& repo_path, const std::string& content) {
    const std::wstring dir = data_dir() + L"\\publish";
    CreateDirectoryW(dir.c_str(), nullptr);
    CreateDirectoryW((dir + L"\\stats").c_str(), nullptr);
    std::wstring file = dir + L"\\" + to_wide(repo_path);
    std::replace(file.begin(), file.end(), L'/', L'\\');
    std::ofstream(file, std::ios::binary) << content;
}

// ---------------------------------------------------------------------------
// GitHub REST API over WinHTTP.
// ---------------------------------------------------------------------------

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h) WinHttpCloseHandle(h);
    }
};

struct Response {
    DWORD status = 0;
    std::string body;
};

// False on a network error (no HTTP response); `error` says why.
bool github_request(const wchar_t* method, const std::wstring& path, const std::string& token,
                    const std::string& body, Response& out, std::string& error) {
    Handle session{WinHttpOpen(L"portfolio-tracker", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.h) {
        error = "WinHttpOpen failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    WinHttpSetTimeouts(session.h, 10000, 10000, 15000, 15000);

    Handle connect{WinHttpConnect(session.h, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0)};
    Handle request{connect.h ? WinHttpOpenRequest(connect.h, method, path.c_str(), nullptr,
                                                  WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                             : nullptr};
    const std::wstring headers =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n"
        L"Content-Type: application/json\r\n"
        L"Authorization: Bearer " + to_wide(token) + L"\r\n";
    const DWORD len = static_cast<DWORD>(body.size());

    if (!request.h ||
        !WinHttpSendRequest(request.h, headers.c_str(), static_cast<DWORD>(-1),
                            len ? const_cast<char*>(body.data()) : WINHTTP_NO_REQUEST_DATA, len,
                            len, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr)) {
        error = "network error " + std::to_string(GetLastError());
        return false;
    }

    DWORD size = sizeof(out.status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &out.status, &size,
                        WINHTTP_NO_HEADER_INDEX);

    out.body.clear();
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(request.h, &avail) && avail > 0) {
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.h, chunk.data(), avail, &read) || read == 0) break;
        out.body.append(chunk.data(), read);
    }
    return true;
}

std::string api_error(const Response& r) {
    const json j = json::parse(r.body, nullptr, false);
    const std::string msg =
        j.is_object() && j.contains("message") && j["message"].is_string() ? j["message"].get<std::string>() : "";
    return "HTTP " + std::to_string(r.status) + (msg.empty() ? "" : ": " + msg);
}

// Runs on a worker thread. GET each file's current SHA, then PUT the new content.
bool upload_files(const std::string& repo, const std::string& branch, const std::string& token,
                  const std::vector<std::pair<std::string, std::string>>& files, Logger& log) {
    for (const auto& [path, content] : files) {
        const std::wstring api = to_wide("/repos/" + repo + "/contents/" + path);
        Response r;
        std::string error;
        if (!github_request(L"GET", api + to_wide("?ref=" + branch), token, "", r, error)) {
            log.write("publish: GET " + path + ": " + error);
            return false;
        }

        std::string sha;
        if (r.status == 200) {
            const json j = json::parse(r.body, nullptr, false);
            if (j.is_object() && j.contains("sha") && j["sha"].is_string()) sha = j["sha"].get<std::string>();
        } else if (r.status != 404) {
            log.write("publish: GET " + path + ": " + api_error(r));
            return false;
        }

        json put = {{"message", "stats: update " + path},
                    {"content", base64(content)},
                    {"branch", branch}};
        if (!sha.empty()) put["sha"] = sha;
        if (!github_request(L"PUT", api, token, put.dump(), r, error)) {
            log.write("publish: PUT " + path + ": " + error);
            return false;
        }
        if (r.status != 200 && r.status != 201) {
            log.write("publish: PUT " + path + ": " + api_error(r));
            return false;
        }
    }
    return true;
}

}  // namespace

bool save_token(const std::string& raw, std::string& message) {
    const auto first = raw.find_first_not_of(" \t\r\n");
    const std::string token =
        first == std::string::npos ? "" : raw.substr(first, raw.find_last_not_of(" \t\r\n") - first + 1);

    if (token.empty()) {
        if (!CredDeleteW(kCredTarget, CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) {
            message = "Could not remove the token (error " + std::to_string(GetLastError()) + ")\n";
            return false;
        }
        message = "GitHub token removed from Credential Manager.\n";
        return true;
    }

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(kCredTarget);
    cred.UserName = const_cast<LPWSTR>(L"github");
    cred.CredentialBlobSize = static_cast<DWORD>(token.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&cred, 0)) {
        message = "Could not save the token (error " + std::to_string(GetLastError()) + ")\n";
        return false;
    }
    message = "GitHub token saved to Windows Credential Manager (PortfolioTracker/GitHubToken).\n";
    return true;
}

Publisher::~Publisher() {
    if (upload_.valid()) upload_.wait();
}

void Publisher::set_status(const std::string& status) {
    status_ = status;
    log_.write("publish: " + status);
}

std::string Publisher::take_status() {
    std::string s;
    s.swap(status_);
    return s;
}

void Publisher::collect_result() {
    using namespace std::chrono_literals;
    if (!upload_.valid() || upload_.wait_for(0s) != std::future_status::ready) return;

    if (upload_.get()) {
        for (const auto& [path, content] : in_flight_) published_[path] = content;
        failures_ = 0;
        set_status("uploaded " + std::to_string(in_flight_.size()) + " files to " +
                   config_.publish_repo + "@" + config_.publish_branch);
    } else {
        // Exponential backoff: 1, 2, 4, ... minutes, capped at the publish interval.
        ++failures_;
        const unsigned minutes =
            std::min(config_.publish_interval_min, 1u << std::min(failures_ - 1, 10u));
        next_ = Clock::now() + std::chrono::minutes(minutes);
        set_status("upload failed (see log); retrying in " + std::to_string(minutes) + " min");
    }
    in_flight_.clear();
}

void Publisher::tick(Session& session) {
    collect_result();
    if (!upload_.valid() && Clock::now() >= next_) publish_now(session);
}

void Publisher::publish_now(Session& session) {
    collect_result();
    if (upload_.valid()) {
        set_status("an upload is already in progress");
        return;
    }
    next_ = Clock::now() + std::chrono::minutes(config_.publish_interval_min);

    session.save();  // include the current, partial minute
    Storage& storage = session.storage();
    const std::vector<DayRange> days = last_days(kSummaryDays);

    // Day files for yesterday (so its last minutes get published) and today.
    Files changed;
    for (size_t i = days.size() - 2; i < days.size(); ++i) {
        const std::string path = "stats/" + days[i].date + ".json";
        const std::string content = day_json(storage, days[i], config_.app_allowlist);
        write_local_copy(path, content);
        if (published_[path] != content) changed.emplace_back(path, content);
    }
    if (changed.empty()) {
        set_status("nothing changed since the last upload");
        return;
    }

    const std::string summary =
        summary_json(storage, days, config_.app_allowlist, utc_now_iso());
    write_local_copy("stats/summary.json", summary);
    changed.emplace_back("stats/summary.json", summary);

    const std::string token = load_token();
    if (token.empty() || config_.publish_repo.empty()) {
        set_status("wrote local copies only (" +
                   std::string(token.empty() ? "no token: run tracker.exe --set-token <token>"
                                             : "publish_repo is empty in config.json") +
                   ")");
        return;
    }

    in_flight_ = changed;
    upload_ = std::async(std::launch::async, upload_files, config_.publish_repo,
                         config_.publish_branch, token, in_flight_, std::ref(log_));
    set_status("uploading " + std::to_string(changed.size()) + " files...");
}

}  // namespace tracker
