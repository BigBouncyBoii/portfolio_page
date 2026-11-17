// Windows entry point. Built with the GUI subsystem so no console window
// appears at login; output is only shown when launched from a terminal.

#include <windows.h>

#include <shellapi.h>

#include <string>
#include <vector>

#include "tracker/cli.hpp"
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

    // Phase 0: nothing to track yet. Later phases start hooks/sampler here.
    if (opts.console) {
        write_out(std::string(tracker::kName) + " " + tracker::kVersion +
                  ": started (no collectors yet), exiting.\n");
    }
    return 0;
}
