#include "lifecycle_win.hpp"

#include "util_win.hpp"

namespace tracker {

namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"PortfolioTracker";

std::wstring installed_exe() { return data_dir() + L"\\tracker.exe"; }

std::string last_error_text(const std::string& what, DWORD code) {
    return what + " (error " + std::to_string(code) + ")\n";
}

// Closes a running background tracker (it saves on the way out) and waits for
// it to exit so its exe can be overwritten. True if none is left running.
bool stop_running() {
    HWND hwnd = FindWindowW(kWindowClass, nullptr);
    if (hwnd == nullptr) return true;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    if (proc == nullptr) return false;
    const bool exited = WaitForSingleObject(proc, 10000) == WAIT_OBJECT_0;
    CloseHandle(proc);
    return exited;
}

}  // namespace

bool acquire_single_instance() {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\PortfolioTracker.Instance");
    return mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

bool request_publish() {
    HWND hwnd = FindWindowW(kWindowClass, nullptr);
    return hwnd != nullptr && PostMessageW(hwnd, kMsgPublishNow, 0, 0) != 0;
}

bool install(std::string& message) {
    const std::wstring dir = data_dir();
    const std::wstring target = installed_exe();
    const std::wstring self = exe_path();

    if (!stop_running()) {
        message = "The running tracker did not exit. Quit it from the tray icon and retry.\n";
        return false;
    }

    if (lstrcmpiW(self.c_str(), target.c_str()) != 0 &&
        !CopyFileW(self.c_str(), target.c_str(), FALSE)) {
        message = last_error_text("Could not copy to " + to_utf8(target.c_str()), GetLastError());
        return false;
    }

    const std::wstring command = L"\"" + target + L"\"";
    const LSTATUS rc =
        RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, REG_SZ, command.c_str(),
                        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    if (rc != ERROR_SUCCESS) {
        message = last_error_text("Could not write the auto-start registry entry",
                                  static_cast<DWORD>(rc));
        return false;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmdline = command;  // CreateProcessW may modify this buffer
    if (!CreateProcessW(target.c_str(), cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        dir.c_str(), &si, &pi)) {
        message = last_error_text("Installed, but could not launch " + to_utf8(target.c_str()),
                                  GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    message = "Installed to " + to_utf8(target.c_str()) +
              "\nIt starts automatically at login and is running now (see the tray icon).\n";
    return true;
}

bool uninstall(std::string& message) {
    const bool stopped = stop_running();

    const LSTATUS rc = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue);
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
        message = last_error_text("Could not remove the auto-start registry entry",
                                  static_cast<DWORD>(rc));
        return false;
    }

    message = "Auto-start removed.";
    if (!stopped) message += " The running tracker did not exit; quit it from the tray icon.";

    const std::wstring target = installed_exe();
    if (lstrcmpiW(exe_path().c_str(), target.c_str()) == 0) {
        message += "\nDelete " + to_utf8(target.c_str()) + " manually (it is this exe).";
    } else {
        DeleteFileW(target.c_str());
    }
    message += "\nYour data (tracker.db, config.json, tracker.log) is kept in " +
               to_utf8(data_dir().c_str()) + "\n";
    return true;
}

}  // namespace tracker
