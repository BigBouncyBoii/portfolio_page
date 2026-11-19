// Windows entry point. Built with the GUI subsystem so no console window
// appears at login; output is only shown when launched from a terminal.

#include <windows.h>

#include <shellapi.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "lifecycle_win.hpp"
#include "publisher_win.hpp"
#include "session_win.hpp"
#include "tracker/cli.hpp"
#include "tracker/config.hpp"
#include "tracker/logger.hpp"
#include "tracker/version.hpp"
#include "tray_win.hpp"
#include "util_win.hpp"

namespace {

using tracker::to_utf8;
using tracker::to_wide;

std::vector<std::string> get_args() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    if (argv == nullptr) return args;
    for (int i = 1; i < argc; ++i) {  // skip program name
        args.push_back(to_utf8(argv[i]));
    }
    LocalFree(argv);
    return args;
}

// Writes text to stdout if it was redirected (pipe/file), otherwise to the
// terminal that launched us. Returns false if there is nowhere to write.
bool write_out(const std::string& text) {
    HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (std_out != nullptr && std_out != INVALID_HANDLE_VALUE &&
        GetFileType(std_out) != FILE_TYPE_UNKNOWN) {
        DWORD written = 0;
        return WriteFile(std_out, text.data(), static_cast<DWORD>(text.size()), &written,
                         nullptr) != 0;
    }

    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return false;

    HANDLE con = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, 0, nullptr);
    if (con == INVALID_HANDLE_VALUE) return false;

    const std::wstring wide = to_wide(text);
    DWORD written = 0;
    const BOOL ok =
        WriteConsoleW(con, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
    CloseHandle(con);
    return ok != 0;
}

// Like write_out, but falls back to a message box (e.g. when double-clicked).
void show(const std::string& text, UINT icon) {
    if (!write_out(text)) {
        MessageBoxW(nullptr, to_wide(text).c_str(), L"tracker", MB_OK | icon);
    }
}

// ---------------------------------------------------------------------------
// --console: debug mode with its own console window showing live counts.
// ---------------------------------------------------------------------------

HANDLE g_stop_requested = nullptr;  // set by Ctrl+C / closing the window
HANDLE g_stopped = nullptr;         // set once everything is saved

BOOL WINAPI on_console_ctrl(DWORD type) {
    SetEvent(g_stop_requested);
    // For close/logoff/shutdown, Windows kills the process as soon as this
    // handler returns, so give the main thread time to save first.
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        WaitForSingleObject(g_stopped, 4000);
    }
    return TRUE;
}

class DebugConsole {
public:
    DebugConsole() {
        // We're a GUI-subsystem app, so we have no console of our own; open a
        // fresh window rather than sharing the launching terminal's input.
        AllocConsole();
        SetConsoleTitleW(L"tracker --console");
        out_ = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    }
    ~DebugConsole() {
        if (out_ != INVALID_HANDLE_VALUE) CloseHandle(out_);
    }
    DebugConsole(const DebugConsole&) = delete;
    DebugConsole& operator=(const DebugConsole&) = delete;

    void print(const std::string& text) const {
        if (out_ == INVALID_HANDLE_VALUE) return;
        const std::wstring wide = to_wide(text);
        DWORD written = 0;
        WriteConsoleW(out_, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
    }

private:
    HANDLE out_ = INVALID_HANDLE_VALUE;
};

std::string timestamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u", static_cast<unsigned>(t.wHour),
                  static_cast<unsigned>(t.wMinute), static_cast<unsigned>(t.wSecond));
    return buf;
}

// "Code.exe 4s, chrome.exe 1s"
std::string format_apps(const std::map<std::string, std::uint64_t>& apps) {
    std::string out;
    for (const auto& [app, secs] : apps) {
        if (!out.empty()) out += ", ";
        out += app + " " + std::to_string(secs) + "s";
    }
    return out.empty() ? "-" : out;
}

void print_today(const DebugConsole& console, tracker::Session& session) {
    console.print("Today: " + std::to_string(session.stats_today(tracker::Counter::Keys).total) +
                  " keys, " +
                  std::to_string(session.stats_today(tracker::Counter::Clicks).total) +
                  " clicks; apps: " + format_apps(session.apps_today()) + "\n\n");
}

int run_console(const tracker::Config& config, tracker::Logger& log) {
    constexpr DWORD kReportIntervalMs = 5000;

    g_stop_requested = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_stopped = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    DebugConsole console;
    SetConsoleCtrlHandler(on_console_ctrl, TRUE);

    const std::string db_path = tracker::data_path(L"tracker.db");
    tracker::Session session(config.idle_threshold_s);
    if (!session.start(db_path)) {
        log.write("start failed: " + session.error());
        MessageBoxW(nullptr, to_wide(session.error()).c_str(), L"tracker", MB_OK | MB_ICONERROR);
        return 1;
    }
    log.write("running in console mode");
    tracker::Publisher publisher(config, log);

    console.print(std::string(tracker::kName) + " " + tracker::kVersion +
                  ": counting keystrokes, clicks and foreground app time.\n"
                  "Data folder: " + to_utf8(tracker::data_dir().c_str()) +
                  " (db saved once per minute; config.json, tracker.log)\n"
                  "Idle threshold: " + std::to_string(config.idle_threshold_s) + " s\n"
                  "Press Ctrl+C or close this window to stop.\n\n");
    print_today(console, session);

    while (WaitForSingleObject(g_stop_requested, kReportIntervalMs) == WAIT_TIMEOUT) {
        const tracker::Session::Tick t = session.tick();
        if (t.saved) {
            console.print(timestamp() + (session.error().empty()
                                             ? "  [saved to database]\n"
                                             : "  !! " + session.error() + "\n"));
        }
        console.print(timestamp() + "  keys +" + std::to_string(t.keys) +
                      "  clicks +" + std::to_string(t.clicks) + "  now: " + session.state() + "  apps: " + format_apps(t.apps) + "\n");
        publisher.tick(session);
        const std::string status = publisher.take_status();
        if (!status.empty()) console.print(timestamp() + "  [publish] " + status + "\n");
    }

    session.stop();
    if (!session.error().empty()) console.print("!! " + session.error() + "\n");
    log.write("stopped (console mode)");

    console.print("\nStopped and saved.\n");
    print_today(console, session);

    SetEvent(g_stopped);
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const tracker::ParseResult parsed = tracker::parse_args(get_args());
    if (!parsed.options) {
        show("error: " + parsed.error + "\n\n" + tracker::usage(), MB_ICONERROR);
        return 2;
    }

    const tracker::Options& opts = *parsed.options;
    std::string message;

    switch (opts.command) {
        case tracker::Command::Help:
            show(tracker::usage(), MB_ICONINFORMATION);
            return 0;
        case tracker::Command::Version:
            show(std::string(tracker::kName) + " " + tracker::kVersion + "\n",
                 MB_ICONINFORMATION);
            return 0;
        case tracker::Command::Install: {
            const bool ok = tracker::install(message);
            show(message, ok ? MB_ICONINFORMATION : MB_ICONERROR);
            return ok ? 0 : 1;
        }
        case tracker::Command::Uninstall: {
            const bool ok = tracker::uninstall(message);
            show(message, ok ? MB_ICONINFORMATION : MB_ICONERROR);
            return ok ? 0 : 1;
        }
        case tracker::Command::PublishNow:
            if (tracker::request_publish()) {
                show("Publish requested.\n", MB_ICONINFORMATION);
                return 0;
            }
            show("tracker is not running in the background.\n", MB_ICONERROR);
            return 1;
        case tracker::Command::SetToken: {
            const bool ok = tracker::save_token(opts.token, message);
            show(message, ok ? MB_ICONINFORMATION : MB_ICONERROR);
            return ok ? 0 : 1;
        }
        case tracker::Command::Run:
            break;
    }

    if (!tracker::acquire_single_instance()) {
        show("tracker is already running (check the tray icon or an open --console window).\n",
             MB_ICONINFORMATION);
        return 1;
    }

    tracker::Logger log;
    log.open(tracker::data_path(L"tracker.log"));
    log.write(std::string("---- ") + tracker::kName + " " + tracker::kVersion + " starting");

    std::string config_error;
    const tracker::Config config =
        tracker::load_config(tracker::data_path(L"config.json"), config_error);
    if (!config_error.empty()) log.write(config_error + " (using defaults)");

    return opts.console ? run_console(config, log) : tracker::run_background(config, log);
}
