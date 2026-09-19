#include "DoubleResetTrigger.h"

DoubleResetTrigger::DoubleResetTrigger(RtcReadFn read, RtcWriteFn write, uint32_t timeoutMs)
    : read_(read), write_(write), timeoutMs_(timeoutMs) {}

DoubleResetStatus DoubleResetTrigger::begin(uint32_t now) {
  uint32_t value = 0U;
  if (read_ == nullptr || write_ == nullptr ||
      !read_(kRtcBlock, &value, sizeof(value))) {
    return DoubleResetStatus::StorageError;
  }

  if (value == kMarker) {
    const uint32_t cleared = 0U;
    if (!write_(kRtcBlock, &cleared, sizeof(cleared))) {
      return DoubleResetStatus::StorageError;
    }
    armed_ = false;
    return DoubleResetStatus::Detected;
  }

  const uint32_t marker = kMarker;
  if (!write_(kRtcBlock, &marker, sizeof(marker))) {
    return DoubleResetStatus::StorageError;
  }
  armedAt_ = now;
  armed_ = true;
  return DoubleResetStatus::Armed;
}

bool DoubleResetTrigger::process(uint32_t now) {
  if (!armed_ || static_cast<uint32_t>(now - armedAt_) < timeoutMs_) return true;
  return disarm();
}

bool DoubleResetTrigger::disarm() {
  const uint32_t cleared = 0U;
  if (write_ == nullptr || !write_(kRtcBlock, &cleared, sizeof(cleared))) return false;
  armed_ = false;
  return true;
}
