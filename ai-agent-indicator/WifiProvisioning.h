#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

#include "DeviceSettings.h"
#include "DoubleResetTrigger.h"
#include "HoldButton.h"
#include "ProvisioningPolicy.h"

using BeforePortalStartFn = void (*)();

class WifiProvisioning {
 public:
  static constexpr uint32_t kConnectionGraceMs = 300000UL;
  static constexpr uint32_t kRetryMs = 10000UL;
  static constexpr uint32_t kPortalSeconds = 300UL;
  static constexpr uint32_t kRestartDelayMs = 1000UL;

  WifiProvisioning(uint8_t buttonPin, BeforePortalStartFn beforePortalStart);
  void begin(uint32_t now);
  void process(uint32_t now);
  bool isConnected() const;
  bool isPortalActive() const;
  bool shouldRestart() const;
  const char* friendlyName() const;
  const char* hostname() const;

 private:
  void configureManager();
  void applyAction(ProvisioningAction action, uint32_t now);
  void startStationConnection(uint32_t now);
  void startPortal();
  void savePortalParameters();
  void markConfigSaved();

  // WiFiManager 2.0.17 exposes its read-only portal query as non-const.
  mutable WiFiManager manager_;
  WiFiManagerParameter deviceNameParameter_{
      "device_name", "Device name", "AI-Agent-Indicator", 32,
      "required maxlength='32' pattern='[A-Za-z0-9 _-]{1,32}' "
      "title='Use letters, numbers, spaces, underscores, or hyphens'"};
  DeviceSettings settings_;
  HoldButton button_{30U, 3000U};
  DoubleResetTrigger resetTrigger_;
  ProvisioningPolicy policy_{kConnectionGraceMs, kRetryMs};
  uint8_t buttonPin_;
  BeforePortalStartFn beforePortalStart_;
  bool managerConfigured_ = false;
  bool restartPending_ = false;
  bool restartReady_ = false;
  uint32_t restartAt_ = 0U;
  uint32_t portalStartedAt_ = 0U;
  char apName_[32] = {};
};
