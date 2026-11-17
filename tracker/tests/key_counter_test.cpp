#include "tracker/key_counter.hpp"

#include <gtest/gtest.h>

#include <thread>

using tracker::KeyCounter;

namespace {
constexpr unsigned kA = 0x41;  // VK 'A'
constexpr unsigned kB = 0x42;  // VK 'B'
}  // namespace

TEST(KeyCounter, StartsAtZero) {
    KeyCounter c;
    EXPECT_EQ(c.count(), 0u);
}

TEST(KeyCounter, CountsPressAndRelease) {
    KeyCounter c;
    c.on_key_down(kA);
    c.on_key_up(kA);
    c.on_key_down(kA);
    c.on_key_up(kA);
    EXPECT_EQ(c.count(), 2u);
}

TEST(KeyCounter, IgnoresAutoRepeat) {
    KeyCounter c;
    for (int i = 0; i < 30; ++i) c.on_key_down(kA);  // key held: repeats
    c.on_key_up(kA);
    EXPECT_EQ(c.count(), 1u);
}

TEST(KeyCounter, KeysAreTrackedIndependently) {
    KeyCounter c;
    c.on_key_down(kA);  // e.g. Shift held...
    c.on_key_down(kB);  // ...while another key is pressed
    c.on_key_down(kB);  // repeat of B
    c.on_key_up(kB);
    c.on_key_down(kB);
    EXPECT_EQ(c.count(), 3u);
}

TEST(KeyCounter, KeyUpWithoutKeyDownIsHarmless) {
    KeyCounter c;
    c.on_key_up(kA);  // e.g. key was already held when tracking started
    c.on_key_down(kA);
    EXPECT_EQ(c.count(), 1u);
}

TEST(KeyCounter, OutOfRangeKeysAreIgnored) {
    KeyCounter c;
    c.on_key_down(KeyCounter::kKeyCodes);
    c.on_key_down(0xFFFFFFFFu);
    c.on_key_up(0xFFFFFFFFu);
    EXPECT_EQ(c.count(), 0u);
}

TEST(KeyCounter, TakeReturnsCountAndResets) {
    KeyCounter c;
    c.on_key_down(kA);
    c.on_key_down(kB);
    EXPECT_EQ(c.take(), 2u);
    EXPECT_EQ(c.count(), 0u);
    EXPECT_EQ(c.take(), 0u);
}

TEST(KeyCounter, TakeDoesNotForgetHeldKeys) {
    KeyCounter c;
    c.on_key_down(kA);
    EXPECT_EQ(c.take(), 1u);
    c.on_key_down(kA);  // still a repeat of the same press
    EXPECT_EQ(c.count(), 0u);
}

TEST(KeyCounter, ConcurrentTakeLosesNothing) {
    KeyCounter c;
    constexpr unsigned kPresses = 200000;
    std::uint64_t taken = 0;

    std::thread input([&] {
        for (unsigned i = 0; i < kPresses; ++i) {
            c.on_key_down(kA);
            c.on_key_up(kA);
        }
    });
    for (int i = 0; i < 1000; ++i) taken += c.take();
    input.join();
    taken += c.take();

    EXPECT_EQ(taken, kPresses);
}
