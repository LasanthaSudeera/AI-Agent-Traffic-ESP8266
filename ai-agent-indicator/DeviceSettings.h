#pragma once

#include <cstdint>

#include "DeviceName.h"

class DeviceSettings {
 public:
  static constexpr uint8_t kSchemaVersion = 1U;
  static constexpr const char* kDefaultName = "AI-Agent-Indicator";

  bool begin();
  bool save(const char* friendlyName);
  const char* friendlyName() const;
  const char* hostname() const;
  bool mounted() const;

 private:
  void useDefault();
  bool load();

  bool mounted_ = false;
  char friendlyName_[DEVICE_NAME_CAPACITY] = {};
  char hostname_[DEVICE_NAME_CAPACITY] = {};
};
