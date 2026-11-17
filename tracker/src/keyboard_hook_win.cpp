#include "keyboard_hook_win.hpp"

#include <atomic>
#include <future>
#include <utility>

namespace tracker {
namespace {

std::atomic<KeyCounter*> g_counter{nullptr};

LRESULT CALLBACK keyboard_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
        KeyCounter* counter = g_counter.load(std::memory_order_relaxed);
        // Skip synthetic input (SendInput, remote tools, macros): count real typing only.
        if (counter != nullptr && (info->flags & LLKHF_INJECTED) == 0) {
            switch (wparam) {
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                    counter->on_key_down(info->vkCode);
                    break;
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    counter->on_key_up(info->vkCode);
                    break;
                default:
                    break;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

}  // namespace

bool KeyboardHook::start() {
    if (running()) return true;

    KeyCounter* expected = nullptr;
    if (!g_counter.compare_exchange_strong(expected, &counter_)) {
        error_ = "another KeyboardHook is already running in this process";
        return false;
    }

    // The thread reports its id once the hook is installed, or 0 on failure.
    std::promise<DWORD> ready;
    std::future<DWORD> ready_future = ready.get_future();

    thread_ = std::thread([this, ready = std::move(ready)]() mutable {
        MSG msg;
        // Force creation of this thread's message queue so that stop()'s
        // PostThreadMessage can never arrive before the queue exists.
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

        HHOOK hook =
            SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, GetModuleHandleW(nullptr), 0);
        if (hook == nullptr) {
            error_ = "SetWindowsHookExW failed (error " + std::to_string(GetLastError()) + ")";
            ready.set_value(0);
            return;
        }
        ready.set_value(GetCurrentThreadId());

        // Low-level hooks are called on this thread while it pumps messages.
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            DispatchMessageW(&msg);
        }

        UnhookWindowsHookEx(hook);
    });

    thread_id_ = ready_future.get();
    if (thread_id_ == 0) {
        thread_.join();
        g_counter.store(nullptr);
        return false;
    }
    return true;
}

void KeyboardHook::stop() {
    if (!running()) return;
    PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
    thread_.join();
    thread_id_ = 0;
    g_counter.store(nullptr);
}

}  // namespace tracker
