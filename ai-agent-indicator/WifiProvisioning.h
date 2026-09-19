#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

#include "DeviceSettings.h"
#include "DoubleResetTrigger.h"
#include "HoldButton.h"
#include "ProvisioningPolicy.h"

using BeforePortalStartFn = void (*)();

// The pinned library's save callbacks cannot veto a submission. This small
// adapter lets our guarded routes invoke its unchanged protected handlers.
class ProvisioningWiFiManager : public WiFiManager {
 public:
  void saveWifiForm() { handleWifiSave(); }
  void saveParameterForm() { handleParamSave(); }
};

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
  bool validatePortalName();
  void savePortalParameters();
  void markConfigSaved();

  // WiFiManager 2.0.17 exposes its read-only portal query as non-const.
  mutable ProvisioningWiFiManager manager_;
  WiFiManagerParameter deviceNameParameter_{
      "device_name", "Device name", "AI-Agent-Indicator", 32,
      "required pattern='(?=.*[A-Za-z0-9])[A-Za-z0-9 _\\-]{1,32}' "
      "title='Use 1-32 ASCII letters, numbers, spaces, underscores, or hyphens; "
      "include a letter or number'"};
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
  char submittedFriendlyName_[DEVICE_NAME_CAPACITY] = {};
};
