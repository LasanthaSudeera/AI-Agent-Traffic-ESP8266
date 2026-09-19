#include "HoldButton.h"

HoldButton::HoldButton(uint32_t debounceMs, uint32_t holdMs)
    : debounceMs_(debounceMs), holdMs_(holdMs) {}

bool HoldButton::update(bool pressed, uint32_t now) {
  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    rawChangedAt_ = now;
  }

  if (rawPressed_ != stablePressed_ &&
      static_cast<uint32_t>(now - rawChangedAt_) >= debounceMs_) {
    stablePressed_ = rawPressed_;
    fired_ = false;
    if (stablePressed_) pressedAt_ = now;
  }

  if (stablePressed_ && !fired_ &&
      static_cast<uint32_t>(now - pressedAt_) >= holdMs_) {
    fired_ = true;
    return true;
  }
  return false;
}
