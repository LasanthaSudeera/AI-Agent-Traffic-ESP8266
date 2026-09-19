#include "DeviceSettings.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <flash_hal.h>

#include <cstring>

namespace {

constexpr const char* kConfigPath = "/config.json";
constexpr const char* kTemporaryConfigPath = "/config.tmp";
constexpr const char* kBackupConfigPath = "/config.bak";

bool filesystemIsErased() {
  // Use the same selected flash layout as LittleFS in ESP8266 core 3.1.2.
  // A failed mount alone must never authorize erasing an existing filesystem.
  const uint32_t start = FS_PHYS_ADDR;
  const uint32_t size = FS_PHYS_SIZE;
  const uint32_t flashSize = ESP.getFlashChipRealSize();
  uint32_t words[64];
  if (size == 0U || size % sizeof(words) != 0U ||
      start > flashSize || size > flashSize - start) {
    return false;
  }
  for (uint32_t offset = 0U; offset < size; offset += sizeof(words)) {
    if (!ESP.flashRead(start + offset, words, sizeof(words))) return false;
    for (uint32_t word : words) {
      if (word != UINT32_MAX) return false;
    }
    yield();
  }
  return true;
}

bool verifyTemporaryConfig(const JsonDocument& document) {
  // Compare the entire reopened file with the canonical JSON we intended to
  // write. This verifies both schema and exact name, including length/tail;
  // a parser alone could accept a valid prefix followed by corrupt data.
  char expectedJson[96] = {};
  const size_t expectedSize = measureJson(document);
  if (expectedSize >= sizeof(expectedJson) ||
      serializeJson(document, expectedJson, sizeof(expectedJson)) != expectedSize) {
    return false;
  }
  File file = LittleFS.open(kTemporaryConfigPath, "r");
  if (!file) return false;
  bool matches = file.size() == expectedSize;
  for (size_t index = 0U; matches && index < expectedSize; ++index) {
    matches = file.read() == static_cast<unsigned char>(expectedJson[index]);
  }
  file.close();
  return matches;
}

}  // namespace

bool DeviceSettings::begin() {
  useDefault();
  mounted_ = false;
  if (!LittleFS.setConfig(LittleFSConfig(false))) return false;
  mounted_ = LittleFS.begin();
  if (!mounted_ && filesystemIsErased()) {
    // A new board has no filesystem yet. Initialize only a fully erased region.
    mounted_ = LittleFS.format() && LittleFS.begin();
  }
  if (!mounted_) return false;
  return load();
}

bool DeviceSettings::save(const char* friendlyName) {
  char newFriendlyName[DEVICE_NAME_CAPACITY] = {};
  char newHostname[DEVICE_NAME_CAPACITY] = {};
  if (!normalizeDeviceName(friendlyName, newFriendlyName, newHostname) || !mounted_) return false;

  if (LittleFS.exists(kTemporaryConfigPath) && !LittleFS.remove(kTemporaryConfigPath)) {
    return false;
  }
  File file = LittleFS.open(kTemporaryConfigPath, "w");
  if (!file) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }

  StaticJsonDocument<128> document;
  document["schema"] = kSchemaVersion;
  document["name"] = newFriendlyName;
  size_t expected = measureJson(document);
  file.clearWriteError();
  size_t written = serializeJson(document, file);
  bool writeFailed = file.getWriteError() != 0;
  file.close();
  // In core 3.1.2 close()/flush() discard LittleFS sync errors. Reopening is
  // mandatory before touching either copy of the previously saved settings.
  if (written != expected || writeFailed || !verifyTemporaryConfig(document)) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }

  if (LittleFS.exists(kBackupConfigPath)) {
    if (!LittleFS.exists(kConfigPath) || !LittleFS.remove(kBackupConfigPath)) {
      LittleFS.remove(kTemporaryConfigPath);
      return false;
    }
  }

  bool hadConfig = LittleFS.exists(kConfigPath);
  if (hadConfig && !LittleFS.rename(kConfigPath, kBackupConfigPath)) {
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }
  if (!LittleFS.rename(kTemporaryConfigPath, kConfigPath)) {
    if (hadConfig) LittleFS.rename(kBackupConfigPath, kConfigPath);
    LittleFS.remove(kTemporaryConfigPath);
    return false;
  }
  if (hadConfig) LittleFS.remove(kBackupConfigPath);

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
  if (!LittleFS.exists(kConfigPath) && LittleFS.exists(kBackupConfigPath) &&
      !LittleFS.rename(kBackupConfigPath, kConfigPath)) {
    return false;
  }
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
