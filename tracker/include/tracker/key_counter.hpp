#pragma once

#include <atomic>
#include <bitset>
#include <cstdint>

namespace tracker {

// Counts key presses (not key identities: only a running total is kept).
//
// Auto-repeat is ignored: holding a key down counts once. This is done by
// remembering which keys are currently down, because the low-level keyboard
// hook delivers repeats as ordinary key-down events.
//
// Threading: on_key_down / on_key_up must be called from a single thread (the
// input hook thread). count() and take() are safe to call from any thread.
class KeyCounter {
public:
    static constexpr unsigned kKeyCodes = 256;  // Windows virtual-key codes

    void on_key_down(unsigned key) noexcept;
    void on_key_up(unsigned key) noexcept;

    // Presses counted since construction or the last take().
    std::uint64_t count() const noexcept { return count_.load(std::memory_order_relaxed); }

    // Returns the current count and atomically resets it to zero.
    std::uint64_t take() noexcept { return count_.exchange(0, std::memory_order_relaxed); }

private:
    std::bitset<kKeyCodes> down_;  // touched only by the input thread
    std::atomic<std::uint64_t> count_{0};
};

}  // namespace tracker
