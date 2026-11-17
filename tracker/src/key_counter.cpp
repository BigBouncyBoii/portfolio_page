#include "tracker/key_counter.hpp"

namespace tracker {

void KeyCounter::on_key_down(unsigned key) noexcept {
    if (key >= kKeyCodes || down_.test(key)) return;  // out of range, or auto-repeat
    down_.set(key);
    count_.fetch_add(1, std::memory_order_relaxed);
}

void KeyCounter::on_key_up(unsigned key) noexcept {
    if (key >= kKeyCodes) return;
    down_.reset(key);
}

}  // namespace tracker
