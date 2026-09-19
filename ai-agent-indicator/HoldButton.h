#pragma once

#include <cstdint>

class HoldButton {
 public:
  HoldButton(uint32_t debounceMs, uint32_t holdMs);
  bool update(bool pressed, uint32_t now);

 private:
  uint32_t debounceMs_;
  uint32_t holdMs_;
  uint32_t rawChangedAt_ = 0U;
  uint32_t pressedAt_ = 0U;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool fired_ = false;
};
