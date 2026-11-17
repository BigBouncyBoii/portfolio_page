// Integration tests for the real Win32 hook. Real typing can't be simulated
// (SendInput events are flagged as injected and deliberately ignored), so
// counting itself is covered by key_counter_test.cpp and by manual testing.

#include "keyboard_hook_win.hpp"

#include <gtest/gtest.h>

using tracker::KeyboardHook;
using tracker::KeyCounter;

TEST(KeyboardHook, StartsAndStopsCleanly) {
    KeyCounter counter;
    KeyboardHook hook(counter);
    ASSERT_TRUE(hook.start()) << hook.error();
    EXPECT_TRUE(hook.running());
    hook.stop();
    EXPECT_FALSE(hook.running());
    hook.stop();  // idempotent
}

TEST(KeyboardHook, CanRestartAfterStop) {
    KeyCounter counter;
    KeyboardHook hook(counter);
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(hook.start()) << hook.error();
        hook.stop();
    }
}

TEST(KeyboardHook, OnlyOneHookPerProcess) {
    KeyCounter a;
    KeyCounter b;
    KeyboardHook first(a);
    KeyboardHook second(b);
    ASSERT_TRUE(first.start()) << first.error();
    EXPECT_FALSE(second.start());
    EXPECT_FALSE(second.error().empty());
    first.stop();
    EXPECT_TRUE(second.start()) << second.error();
}

TEST(KeyboardHook, InjectedInputIsNotCounted) {
    KeyCounter counter;
    KeyboardHook hook(counter);
    ASSERT_TRUE(hook.start()) << hook.error();

    // VK_F24 press + release: harmless, almost no app reacts to it.
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_F24;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    Sleep(100);  // let the hook thread process the events

    hook.stop();
    EXPECT_EQ(counter.count(), 0u);
}
