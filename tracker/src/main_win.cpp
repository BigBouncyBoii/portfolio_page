// Windows entry point. Built with the GUI subsystem so no console window
// appears at login; output is only shown when launched from a terminal.

#include <windows.h>

#include <shellapi.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "focus_sampler_win.hpp"
#include "keyboard_hook_win.hpp"
#include "tracker/cli.hpp"
#include "tracker/key_counter.hpp"
#include "tracker/version.hpp"

namespace {

std::string to_utf8(const wchar_t* wide) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int src_len = static_cast<int>(utf8.size());
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), src_len, nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), src_len, out.data(), len);
    return out;
}

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
HANDLE g_stopped = nullptr;         // set once the hook is removed

BOOL WINAPI on_console_ctrl(DWORD type) {
    SetEvent(g_stop_requested);
    // For close/logoff/shutdown, Windows kills the process as soon as this
    // handler returns, so give the main thread time to unhook cleanly first.
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

int run_console() {
    constexpr DWORD kReportIntervalMs = 5000;

    g_stop_requested = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_stopped = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    DebugConsole console;
    SetConsoleCtrlHandler(on_console_ctrl, TRUE);

    tracker::KeyCounter counter;
    tracker::KeyboardHook hook(counter);
    if (!hook.start()) {
        MessageBoxW(nullptr, to_wide("Could not install keyboard hook: " + hook.error()).c_str(),
                    L"tracker", MB_OK | MB_ICONERROR);
        return 1;
    }

    tracker::FocusSampler sampler;
    sampler.start();

    console.print(std::string(tracker::kName) + " " + tracker::kVersion +
                  ": counting keystrokes and foreground app time.\n"
                  "Press Ctrl+C or close this window to stop.\n\n");

    std::uint64_t session_total = 0;
    std::map<std::string, std::uint64_t> session_apps;
    while (WaitForSingleObject(g_stop_requested, kReportIntervalMs) == WAIT_TIMEOUT) {
        const std::uint64_t delta = counter.take();
        session_total += delta;
        const auto apps = sampler.take();
        for (const auto& [app, secs] : apps) session_apps[app] += secs;
        console.print(timestamp() + "  keys +" + std::to_string(delta) + " (total " +
                      std::to_string(session_total) + ")  now: " + sampler.last_state() +
                      "  apps: " + format_apps(apps) + "\n");
    }

    hook.stop();
    sampler.stop();
    session_total += counter.take();
    for (const auto& [app, secs] : sampler.take()) session_apps[app] += secs;
    console.print("\nStopped. Session: " + std::to_string(session_total) +
                  " keystrokes. App time: " + format_apps(session_apps) + "\n");

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

    switch (opts.command) {
        case tracker::Command::Help:
            show(tracker::usage(), MB_ICONINFORMATION);
            return 0;
        case tracker::Command::Version:
            show(std::string(tracker::kName) + " " + tracker::kVersion + "\n",
                 MB_ICONINFORMATION);
            return 0;
        case tracker::Command::Run:
            break;
    }

    if (opts.console) {
        return run_console();
    }

    // Background (tray) mode arrives in Phase 4; until then there would be no
    // way to see or stop a hidden tracker, so point at debug mode instead.
    show("Background mode is not implemented yet. Run: tracker.exe --console\n",
         MB_ICONINFORMATION);
    return 0;
}
