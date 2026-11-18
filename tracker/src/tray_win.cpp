#include "tray_win.hpp"

#include <windows.h>

#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "lifecycle_win.hpp"
#include "session_win.hpp"
#include "util_win.hpp"

namespace tracker {

namespace {

constexpr UINT kTrayMsg = WM_APP + 1;
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTickMs = 5000;
enum : UINT { kCmdPause = 1, kCmdPublish, kCmdQuit };

struct App {
    Session* session = nullptr;
    Logger* log = nullptr;
    NOTIFYICONDATAW icon{};
    UINT taskbar_created = 0;  // broadcast when Explorer (re)starts
};
App* g_app = nullptr;  // the window procedure has no user-data pointer of its own

// "2h 10m", "45m", "<1m"
std::wstring format_duration(std::uint64_t secs) {
    const std::uint64_t mins = secs / 60;
    if (mins == 0) return L"<1m";
    if (mins < 60) return std::to_wstring(mins) + L"m";
    return std::to_wstring(mins / 60) + L"h " + std::to_wstring(mins % 60) + L"m";
}

// Today's apps by foreground time, most used first, ".exe" stripped.
std::vector<std::pair<std::wstring, std::uint64_t>> top_apps_today(size_t limit) {
    const AppSeconds apps = g_app->session->apps_today();
    std::vector<std::pair<std::string, std::uint64_t>> sorted(apps.begin(), apps.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::pair<std::wstring, std::uint64_t>> out;
    for (size_t i = 0; i < sorted.size() && i < limit; ++i) {
        std::wstring name = to_wide(sorted[i].first);
        if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0) {
            name.resize(name.size() - 4);
        }
        out.emplace_back(std::move(name), sorted[i].second);
    }
    return out;
}

// Two lines: today's keys and clicks, then the top apps by foreground time, e.g.
//   Today: 4213 keys, 812 clicks
//   Code 2h 10m · chrome 45m · WindowsTerminal 12m
void update_tray(DWORD action) {
    constexpr size_t kMaxTip = std::size(NOTIFYICONDATAW{}.szTip) - 1;  // 127 chars
    Session& session = *g_app->session;

    std::wstring tip =
        L"Today: " + std::to_wstring(session.stats_today(Counter::Keys).total) + L" keys, " +
        std::to_wstring(session.stats_today(Counter::Clicks).total) + L" clicks";
    if (g_app->session->paused()) tip += L" (paused)";

    const auto top = top_apps_today(3);
    for (size_t i = 0; i < top.size(); ++i) {
        const std::wstring entry = (i == 0 ? L"\n" : L" · ") + top[i].first + L" " +
                                   format_duration(top[i].second);
        if (tip.size() + entry.size() > kMaxTip) break;  // keep whole entries only
        tip += entry;
    }

    wcsncpy_s(g_app->icon.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(action, &g_app->icon);
}

void publish_now() {
    // The publisher arrives in Phase 5.
    g_app->log->write("publish requested (publishing not implemented yet)");
}

void on_command(HWND hwnd, UINT cmd) {
    switch (cmd) {
        case kCmdPause: {
            const bool paused = !g_app->session->paused();
            g_app->session->set_paused(paused);
            g_app->log->write(paused ? "paused" : "resumed");
            update_tray(NIM_MODIFY);
            break;
        }
        case kCmdPublish:
            publish_now();
            break;
        case kCmdQuit:
            DestroyWindow(hwnd);
            break;
        default:
            break;
    }
}

// Read-only (greyed) menu line. A tab right-aligns the text after it.
void add_info(HMENU menu, const std::wstring& text) {
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, text.c_str());
}

// "Keystrokes today  4213" + last hour / busiest minute / avg per active minute.
void add_counter_stats(HMENU menu, Session& session, Counter counter, const wchar_t* title) {
    const CountStats s = session.stats_today(counter);
    const std::uint64_t avg = s.active_minutes ? s.total / s.active_minutes : 0;
    add_info(menu, std::wstring(title) + L"\t" + std::to_wstring(s.total));
    add_info(menu, L"    Last hour\t" + std::to_wstring(session.last_hour(counter)));
    add_info(menu, L"    Busiest minute\t" + std::to_wstring(s.peak_minute));
    add_info(menu, L"    Avg per active minute\t" + std::to_wstring(avg));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
}

// Menu = today's stats (read-only) + actions:
//   Keystrokes today        4213
//     Last hour              512
//     Busiest minute         143
//     Avg per active minute   38
//   ──────────
//   Clicks today             812   (same breakdown)
//   ──────────
//   Top apps today
//     Code                2h 10m
//     ...
//   ──────────
//   Pause / Publish now / Quit
void show_menu(HWND hwnd) {
    constexpr size_t kMenuApps = 8;
    Session& session = *g_app->session;

    HMENU menu = CreatePopupMenu();

    add_counter_stats(menu, session, Counter::Keys, L"Keystrokes today");
    add_counter_stats(menu, session, Counter::Clicks, L"Clicks today");

    const auto apps = top_apps_today(kMenuApps);
    add_info(menu, apps.empty() ? L"No app time yet today" : L"Top apps today");
    for (const auto& [name, secs] : apps) {
        add_info(menu, L"    " + name + L"\t" + format_duration(secs));
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, kCmdPause, g_app->session->paused() ? L"Resume" : L"Pause");
    AppendMenuW(menu, MF_STRING, kCmdPublish, L"Publish now");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdQuit, L"Quit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // otherwise the menu won't close on an outside click
    const UINT cmd = static_cast<UINT>(TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr));
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    on_command(hwnd, cmd);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_app->taskbar_created != 0 && msg == g_app->taskbar_created) {
        update_tray(NIM_ADD);  // Explorer restarted: our icon is gone
        return 0;
    }

    switch (msg) {
        case WM_TIMER: {
            const Session::Tick t = g_app->session->tick();
            if (t.saved && !g_app->session->error().empty()) {
                g_app->log->write(g_app->session->error());
            }
            update_tray(NIM_MODIFY);
            return 0;
        }
        case kTrayMsg:
            if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP) show_menu(hwnd);
            return 0;
        case kMsgPublishNow:
            publish_now();
            return 0;
        case WM_QUERYENDSESSION:
            // Logoff/shutdown may still be cancelled, so save but keep running.
            g_app->session->tick();
            if (!g_app->session->save()) g_app->log->write(g_app->session->error());
            return TRUE;
        case WM_ENDSESSION:
            if (wp) {  // session really is ending; we may be killed after returning
                g_app->session->stop();
                g_app->log->write("session ending: saved and stopped");
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kTimerId);
            Shell_NotifyIconW(NIM_DELETE, &g_app->icon);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

int run_background(const Config& config, Logger& log) {
    Session session(config.idle_threshold_s);
    if (!session.start(data_path(L"tracker.db"))) {
        log.write("start failed: " + session.error());
        MessageBoxW(nullptr, to_wide(session.error()).c_str(), L"tracker", MB_OK | MB_ICONERROR);
        return 1;
    }

    App app;
    app.session = &session;
    app.log = &log;
    app.taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    g_app = &app;

    // A hidden top-level window rather than a message-only (HWND_MESSAGE) one:
    // message-only windows don't receive broadcasts such as
    // WM_QUERYENDSESSION / WM_ENDSESSION or TaskbarCreated.
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, kWindowClass, L"tracker", WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                                nullptr, inst, nullptr);
    if (hwnd == nullptr) {
        log.write("could not create window (error " + std::to_string(GetLastError()) + ")");
        g_app = nullptr;
        return 1;
    }

    app.icon.cbSize = sizeof(app.icon);
    app.icon.hWnd = hwnd;
    app.icon.uID = 1;
    app.icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    app.icon.uCallbackMessage = kTrayMsg;
    app.icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    update_tray(NIM_ADD);
    SetTimer(hwnd, kTimerId, kTickMs, nullptr);
    log.write("running in background (idle threshold " + std::to_string(config.idle_threshold_s) +
              " s)");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    session.stop();
    if (!session.error().empty()) log.write(session.error());
    log.write("stopped");
    g_app = nullptr;
    return 0;
}

}  // namespace tracker
