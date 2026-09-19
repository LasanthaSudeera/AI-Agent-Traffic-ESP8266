#include "DeviceSettings.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <cstring>

namespace {

constexpr const char* kConfigPath = "/config.json";
constexpr const char* kTemporaryConfigPath = "/config.tmp";

}  // namespace

bool DeviceSettings::begin() {
  useDefault();
  mounted_ = LittleFS.begin();
  if (!mounted_) return false;
  return load();
}

bool DeviceSettings::save(const char* friendlyName) {
  char newFriendlyName[DEVICE_NAME_CAPACITY] = {};
  char newHostname[DEVICE_NAME_CAPACITY] = {};
  if (!normalizeDeviceName(friendlyName, newFriendlyName, newHostname) || !mounted_) return false;

  LittleFS.remove(kTemporaryConfigPath);
  File file = LittleFS.open(kTemporaryConfigPath, "w");
  if (!file) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }

  StaticJsonDocument<128> document;
  document["schema"] = kSchemaVersion;
  document["name"] = newFriendlyName;
  size_t written = serializeJson(document, file);
  file.close();
  if (written == 0U) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }

  if (LittleFS.exists(kConfigPath) && !LittleFS.remove(kConfigPath)) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }
  if (!LittleFS.rename(kTemporaryConfigPath, kConfigPath)) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }

  std::memcpy(friendlyName_, newFriendlyName, sizeof(friendlyName_));
  std::memcpy(hostname_, newHostname, sizeof(hostname_));
  return true;
}

const char* DeviceSettings::friendlyName() const {
  return friendlyName_;
}

const char* DeviceSettings::hostname() const {
  return hostname_;
}

bool DeviceSettings::mounted() const {
  return mounted_;
}

void DeviceSettings::useDefault() {
  normalizeDeviceName(kDefaultName, friendlyName_, hostname_);
}

bool DeviceSettings::load() {
  if (!LittleFS.exists(kConfigPath)) return true;

  File file = LittleFS.open(kConfigPath, "r");
  if (!file) return false;

  StaticJsonDocument<128> document;
  DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error || !document["schema"].is<uint8_t>() ||
      document["schema"].as<uint8_t>() != kSchemaVersion ||
      !document["name"].is<const char*>()) {
    return false;
  }

  char loadedFriendlyName[DEVICE_NAME_CAPACITY] = {};
  char loadedHostname[DEVICE_NAME_CAPACITY] = {};
  if (!normalizeDeviceName(document["name"].as<const char*>(), loadedFriendlyName,
                           loadedHostname)) {
    return false;
  }

  std::memcpy(friendlyName_, loadedFriendlyName, sizeof(friendlyName_));
  std::memcpy(hostname_, loadedHostname, sizeof(hostname_));
  return true;
}
