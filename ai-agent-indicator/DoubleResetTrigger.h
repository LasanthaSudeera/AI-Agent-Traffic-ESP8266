#pragma once

#include <cstddef>
#include <cstdint>

using RtcReadFn = bool (*)(uint32_t, uint32_t*, size_t);
using RtcWriteFn = bool (*)(uint32_t, const uint32_t*, size_t);

enum class DoubleResetStatus : uint8_t { Armed, Detected, StorageError };

class DoubleResetTrigger {
 public:
  static constexpr uint32_t kRtcBlock = 64U;
  static constexpr uint32_t kMarker = 0xA17A11E5UL;

  DoubleResetTrigger(RtcReadFn read, RtcWriteFn write, uint32_t timeoutMs);
  DoubleResetStatus begin(uint32_t now);
  bool process(uint32_t now);
  bool disarm();

 private:
  RtcReadFn read_;
  RtcWriteFn write_;
  uint32_t timeoutMs_;
  uint32_t armedAt_ = 0U;
  bool armed_ = false;
};
