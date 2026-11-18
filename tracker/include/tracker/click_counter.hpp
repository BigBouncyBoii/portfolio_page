#pragma once

#include <atomic>
#include <cstdint>

namespace tracker {

// Counts mouse/touchpad button presses (left, right, middle, side buttons;
// touchpad taps arrive as ordinary clicks). Only a running total is kept:
// no positions, no target windows. Movement and scrolling are not counted.
//
// on_click() is called from the input hook thread; count()/take() are safe
// from any thread.
class ClickCounter {
public:
    void on_click() noexcept { count_.fetch_add(1, std::memory_order_relaxed); }

    std::uint64_t count() const noexcept { return count_.load(std::memory_order_relaxed); }
    std::uint64_t take() noexcept { return count_.exchange(0, std::memory_order_relaxed); }

private:
    std::atomic<std::uint64_t> count_{0};
};

}  // namespace tracker
