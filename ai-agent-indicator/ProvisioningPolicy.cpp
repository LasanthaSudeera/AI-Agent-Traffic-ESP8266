#include "ProvisioningPolicy.h"

ProvisioningPolicy::ProvisioningPolicy(uint32_t connectionGraceMs, uint32_t retryMs)
    : connectionGraceMs_(connectionGraceMs), retryMs_(retryMs) {}

ProvisioningAction ProvisioningPolicy::begin(uint32_t now, bool hasSavedCredentials,
                                              bool manualSetup) {
  wifiConnected_ = false;
  if (manualSetup || !hasSavedCredentials) {
    mode_ = ProvisioningMode::Portal;
    return ProvisioningAction::OpenPortal;
  }

  mode_ = ProvisioningMode::Connecting;
  connectionStartedAt_ = now;
  lastRetryAt_ = now;
  return ProvisioningAction::StartConnection;
}

ProvisioningAction ProvisioningPolicy::update(uint32_t now, bool wifiConnected,
                                               bool portalActive, bool manualSetup) {
  if (mode_ == ProvisioningMode::Portal) {
    if (portalActive) return ProvisioningAction::None;
    wifiConnected_ = wifiConnected;
    if (wifiConnected) {
      mode_ = ProvisioningMode::Connected;
      return ProvisioningAction::Connected;
    }
    mode_ = ProvisioningMode::Connecting;
    connectionStartedAt_ = now;
    lastRetryAt_ = now;
    return ProvisioningAction::StartConnection;
  }

  if (manualSetup) {
    mode_ = ProvisioningMode::Portal;
    return ProvisioningAction::OpenPortal;
  }

  if (wifiConnected) {
    bool newlyConnected = mode_ != ProvisioningMode::Connected || !wifiConnected_;
    wifiConnected_ = true;
    mode_ = ProvisioningMode::Connected;
    return newlyConnected ? ProvisioningAction::Connected : ProvisioningAction::None;
  }

  if (mode_ == ProvisioningMode::Connected || wifiConnected_) {
    wifiConnected_ = false;
    mode_ = ProvisioningMode::Connecting;
    connectionStartedAt_ = now;
    lastRetryAt_ = now;
    return ProvisioningAction::StartConnection;
  }

  wifiConnected_ = false;
  if (static_cast<uint32_t>(now - connectionStartedAt_) >= connectionGraceMs_) {
    mode_ = ProvisioningMode::Portal;
    return ProvisioningAction::OpenPortal;
  }
  if (static_cast<uint32_t>(now - lastRetryAt_) >= retryMs_) {
    lastRetryAt_ = now;
    return ProvisioningAction::RetryConnection;
  }
  return ProvisioningAction::None;
}

ProvisioningMode ProvisioningPolicy::mode() const {
  return mode_;
}
