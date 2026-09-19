#pragma once

#include <cstdint>

enum class ProvisioningMode : uint8_t { Connecting, Portal, Connected };
enum class ProvisioningAction : uint8_t {
  None, StartConnection, RetryConnection, OpenPortal, Connected
};

class ProvisioningPolicy {
 public:
  ProvisioningPolicy(uint32_t connectionGraceMs, uint32_t retryMs);
  ProvisioningAction begin(uint32_t now, bool hasSavedCredentials,
                           bool manualSetup);
  ProvisioningAction update(uint32_t now, bool wifiConnected,
                            bool portalActive, bool manualSetup);
  ProvisioningMode mode() const;

 private:
  uint32_t connectionGraceMs_;
  uint32_t retryMs_;
  uint32_t connectionStartedAt_ = 0U;
  uint32_t lastRetryAt_ = 0U;
  ProvisioningMode mode_ = ProvisioningMode::Connecting;
  bool wifiConnected_ = false;
};
