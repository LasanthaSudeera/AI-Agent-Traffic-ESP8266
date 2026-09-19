#include "WifiProvisioning.h"

#include <ESP8266WiFi.h>

#include <cstdio>
#include <cstring>

namespace {

bool readRtc(uint32_t block, uint32_t* data, size_t size) {
  return ESP.rtcUserMemoryRead(block, data, size);
}

bool writeRtc(uint32_t block, const uint32_t* data, size_t size) {
  return ESP.rtcUserMemoryWrite(block, const_cast<uint32_t*>(data), size);
}

}  // namespace

WifiProvisioning::WifiProvisioning(uint8_t buttonPin,
                                   BeforePortalStartFn beforePortalStart)
    : resetTrigger_(readRtc, writeRtc, 10000U),
      buttonPin_(buttonPin),
      beforePortalStart_(beforePortalStart) {}

void WifiProvisioning::begin(uint32_t now) {
  pinMode(buttonPin_, INPUT_PULLUP);
  if (!settings_.begin()) {
    Serial.println(F("Device settings unavailable; using defaults"));
  }
  configureManager();
  deviceNameParameter_.setValue(settings_.friendlyName(), 32);

  const DoubleResetStatus resetStatus = resetTrigger_.begin(now);
  if (resetStatus == DoubleResetStatus::StorageError) {
    Serial.println(F("RTC setup storage error"));
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(settings_.hostname());
  const bool hasSavedCredentials = manager_.getWiFiIsSaved();
  applyAction(policy_.begin(now, hasSavedCredentials,
                           resetStatus == DoubleResetStatus::Detected), now);
}

void WifiProvisioning::process(uint32_t now) {
  if (!resetTrigger_.process(now)) {
    Serial.println(F("RTC marker clear error"));
  }

  const bool held = button_.update(digitalRead(buttonPin_) == LOW, now);
  const bool portalWasActive = isPortalActive();
  if (portalWasActive) {
    // The library timeout is disabled; this is the only portal expiry clock.
    if (ProvisioningPolicy::portalExpired(now, portalStartedAt_, kPortalSeconds * 1000UL)) {
      manager_.stopConfigPortal();
    } else {
      manager_.process();
      if (isPortalActive() &&
          ProvisioningPolicy::portalExpired(millis(), portalStartedAt_, kPortalSeconds * 1000UL)) {
        manager_.stopConfigPortal();
      }
    }
  }

  // Consume a hold during an active portal even if process() just closed it.
  const bool manualSetup = held && !portalWasActive;
  applyAction(policy_.update(now, isConnected(), isPortalActive(), manualSetup), now);

  if (restartPending_ &&
      static_cast<int32_t>(now - restartAt_) >= 0 &&
      resetTrigger_.disarm()) {
    restartPending_ = false;
    restartReady_ = true;
  }
}

bool WifiProvisioning::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiProvisioning::isPortalActive() const {
  return manager_.getConfigPortalActive();
}

bool WifiProvisioning::shouldRestart() const {
  return restartReady_;
}

const char* WifiProvisioning::friendlyName() const {
  return settings_.friendlyName();
}

const char* WifiProvisioning::hostname() const {
  return settings_.hostname();
}

void WifiProvisioning::configureManager() {
  if (managerConfigured_) return;

  manager_.setConfigPortalBlocking(false);
  // WiFiManager 2.0.17 compares absolute deadlines, which can expire early
  // across millis() rollover. process() owns the elapsed-time limit instead.
  manager_.setConfigPortalTimeout(0);
  manager_.setWebPortalClientCheck(false);
  manager_.setAPClientCheck(false);
  manager_.setBreakAfterConfig(false);
  manager_.setSaveConnect(true);
  manager_.setSaveConnectTimeout(15);
  manager_.setShowPassword(false);
  manager_.setShowInfoErase(false);
  manager_.setShowInfoUpdate(false);
  manager_.setDebugOutput(false);
  manager_.setTitle("AI Agent Light Setup");
  manager_.setHostname(settings_.hostname());
  manager_.addParameter(&deviceNameParameter_);
  manager_.setSaveParamsCallback([this] { savePortalParameters(); });
  manager_.setSaveConfigCallback([this] { markConfigSaved(); });
  manager_.setWebServerCallback([this] {
    // 2.0.17 invokes this before stock routes; ESP8266WebServer uses the first
    // matching handler. Hiding info buttons alone leaves these routes live.
    manager_.server->on("/wifisave", HTTP_ANY, [this] {
      if (validatePortalName()) manager_.saveWifiForm();
    });
    manager_.server->on("/paramsave", HTTP_ANY, [this] {
      if (validatePortalName()) manager_.saveParameterForm();
    });
    const auto unavailable = [this] {
      manager_.server->send(404, "text/plain", "Not found");
    };
    for (const char* path : {"/restart", "/r", "/erase", "/update", "/u"}) {
      manager_.server->on(path, HTTP_ANY, unavailable, [] {});
    }
  });
  managerConfigured_ = true;
}

void WifiProvisioning::applyAction(ProvisioningAction action, uint32_t now) {
  switch (action) {
    case ProvisioningAction::StartConnection:
    case ProvisioningAction::RetryConnection:
      startStationConnection(now);
      break;
    case ProvisioningAction::OpenPortal:
      startPortal();
      break;
    case ProvisioningAction::None:
    case ProvisioningAction::Connected:
      break;
  }
}

void WifiProvisioning::startStationConnection(uint32_t now) {
  // Retry and grace timing belong to ProvisioningPolicy.
  (void)now;
  WiFi.begin();
}

void WifiProvisioning::startPortal() {
  if (isPortalActive()) return;
  if (beforePortalStart_ != nullptr) beforePortalStart_();
  std::snprintf(apName_, sizeof(apName_), "AI-Agent-Light-Setup-%04X",
                static_cast<unsigned int>(ESP.getChipId() & 0xFFFF));
  deviceNameParameter_.setValue(settings_.friendlyName(), 32);
  portalStartedAt_ = millis();
  manager_.startConfigPortal(apName_);
}

bool WifiProvisioning::validatePortalName() {
  // Match doParamSave()'s legacy param_0 precedence, but validate the complete
  // request before WiFiManager truncates it to the 32-byte parameter buffer.
  const String raw = manager_.server->hasArg("param_0")
                         ? manager_.server->arg("param_0")
                         : manager_.server->arg("device_name");
  char hostname[DEVICE_NAME_CAPACITY] = {};
  if (raw.length() == std::strlen(raw.c_str()) &&
      normalizeDeviceName(raw.c_str(), submittedFriendlyName_, hostname)) {
    return true;
  }

  manager_.server->sendHeader("Cache-Control", "no-store");
  manager_.server->send(400, "text/html",
      F("<!doctype html><html lang='en'><head><meta name='viewport' "
        "content='width=device-width,initial-scale=1'><title>Correct device name</title>"
        "</head><body><h1>Device name needs correction</h1>"
        "<p>Use 1-32 ASCII letters, numbers, spaces, underscores, or hyphens "
        "after trimming spaces. Include at least one letter or number.</p>"
        "<p>Nothing was saved. Return to setup, correct the device name and "
        "submit your Wi-Fi details again.</p><a href='/wifi'>Return to setup</a>"
        "</body></html>"));
  return false;
}

void WifiProvisioning::savePortalParameters() {
  // Keep the validated, trimmed name even if the raw request had more than
  // 32 bytes due to surrounding spaces; stock doParamSave() truncates first.
  deviceNameParameter_.setValue(submittedFriendlyName_, 32);
  if (!settings_.save(submittedFriendlyName_)) {
    Serial.println(F("Device settings save error"));
  }
}

void WifiProvisioning::markConfigSaved() {
  restartPending_ = true;
  restartAt_ = millis() + kRestartDelayMs;
}
