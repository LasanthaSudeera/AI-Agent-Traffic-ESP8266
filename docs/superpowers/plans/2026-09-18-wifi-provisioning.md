# Wi-Fi Provisioning Wizard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace firmware-baked credentials with a WiFiManager captive-portal wizard that scans nearby networks, saves a friendly device name, and provides automatic, double-reset, and optional-button recovery.

**Architecture:** Keep the HTTP status API and LED state model in the main sketch. Add small project-owned components for pure timing/input logic, RTC double-reset detection, LittleFS device settings, and the hardware-facing WiFiManager lifecycle; the main sketch coordinates port-80 ownership and LED priority.

**Tech Stack:** Wemos D1 Mini/ESP8266, Arduino C++, ESP8266 Arduino core 3.1.2, WiFiManager 2.0.17, ArduinoJson 6.21.6, LittleFS, Arduino CLI, native C++ compiler, Python 3.

## Global Constraints

- Target `LOLIN(WEMOS) D1 R2 & mini` with FQBN `esp8266:esp8266:d1_mini` and ESP8266 Arduino core 3.1.2.
- Pin WiFiManager 2.0.17; do not fork or vendor it.
- Pin ArduinoJson 6.21.6 to retain the sketch's current ArduinoJson 6 API.
- Do not add an external double-reset library. Reserve RTC user-memory block 64 for the project-owned detector.
- Double reset means two reset-button presses within ten seconds while power remains applied. Detection across full power removal is not guaranteed.
- Clear the RTC reset marker successfully before every firmware-requested restart so a successful setup cannot be mistaken for a second manual reset.
- D6/GPIO12 is an optional active-low setup button using `INPUT_PULLUP`; hold it for three seconds, with 30 ms debounce, to open setup.
- With no saved credentials, open setup immediately. Otherwise, allow five continuous disconnected minutes before opening setup.
- Keep the open setup AP active for at most five minutes and name it `AI-Agent-Light-Setup-XXXX`, using the final four uppercase hex digits of the chip ID.
- Use stock WiFiManager persistence: submitting credentials immediately replaces the saved credentials. Do not add a backup layer.
- Store only schema version 1 and the friendly device name in `/config.json`; never store or return the Wi-Fi password in project JSON or the API.
- Accept friendly names of 1-32 ASCII letters, numbers, spaces, underscores, and hyphens. Default to `AI-Agent-Indicator` for missing or corrupt stored data.
- WiFiManager and the status API both use port 80 and must never run simultaneously.
- Setup LEDs cycle red, yellow, green every 500 ms. Disconnected LEDs blink together every 500 ms. Connected LEDs retain the current agent-state mapping.
- Preserve the existing `/api/status` contract, 120-second expiry, agent hooks, pins D1/D2/D5, and 330-ohm LED wiring.
- Do not modify FreeCAD, STL, G-code, hook, or API smoke-test files.
- The staged sketch edit contains local deployment credentials and is superseded by this feature. Do not print or commit those values; final source must contain no SSID/password constants.
- The workspace currently has neither `arduino-cli` nor a native C++ compiler. At execution time, obtain permission before installing tools, or use equivalent already-installed tools.

## File Structure

- Create `ai-agent-indicator/sketch.yaml`: reproducible build profile.
- Create `DeviceName.{h,cpp}`: pure name validation and hostname derivation.
- Create `HoldButton.{h,cpp}`: pure debounce and hold detection.
- Create `DoubleResetTrigger.{h,cpp}`: RTC-marker algorithm with injected storage.
- Create `ProvisioningPolicy.{h,cpp}`: pure connection/portal timing state machine.
- Create `ai-agent-indicator/tests/provisioning_unit.cpp`: native tests for the pure components.
- Create `DeviceSettings.{h,cpp}`: LittleFS `/config.json` persistence.
- Create `WifiProvisioning.{h,cpp}`: WiFiManager and ESP8266 orchestration.
- Modify `ai-agent-indicator/ai-agent-indicator.ino`: integrate provisioning, server ownership, and LED priority.
- Modify `ai-agent-indicator/README.md` and `QUICK_BUILD_GUIDE.md`: setup and recovery documentation.

---

### Task 1: Pin a reproducible firmware build

**Files:**
- Create: `ai-agent-indicator/sketch.yaml`
- Reference: `ai-agent-indicator/ai-agent-indicator.ino`

**Interfaces:**
- Produces: Arduino CLI profile `d1_mini`, consumed by all later firmware compile steps.

- [ ] **Step 1: Check tool availability**

```bash
command -v arduino-cli
command -v c++
```

Expected in this workspace: both are missing. Request permission before installing. For Debian/WSL, install the native compiler with:

```bash
sudo apt-get update
sudo apt-get install -y g++
```

Install Arduino CLI through its official installer and verify `arduino-cli version`. Arduino IDE may be used for equivalent compile/upload checks, but retain the build profile.

- [ ] **Step 2: Create the build profile**

```yaml
default_profile: d1_mini
profiles:
  d1_mini:
    fqbn: esp8266:esp8266:d1_mini
    platforms:
      - platform: esp8266:esp8266 (3.1.2)
        platform_index_url: https://arduino.esp8266.com/stable/package_esp8266com_index.json
    libraries:
      - WiFiManager (2.0.17)
      - ArduinoJson (6.21.6)
```

- [ ] **Step 3: Verify the baseline build**

```bash
arduino-cli compile --profile d1_mini ai-agent-indicator
```

Expected: successful build. Do not display or commit the staged credential diff.

- [ ] **Step 4: Commit only the profile**

```bash
git add ai-agent-indicator/sketch.yaml
git commit --only ai-agent-indicator/sketch.yaml -m "build: pin ESP8266 firmware dependencies"
```

---

### Task 2: Build and test the pure provisioning helpers

**Files:**
- Create: `ai-agent-indicator/DeviceName.h`
- Create: `ai-agent-indicator/DeviceName.cpp`
- Create: `ai-agent-indicator/HoldButton.h`
- Create: `ai-agent-indicator/HoldButton.cpp`
- Create: `ai-agent-indicator/DoubleResetTrigger.h`
- Create: `ai-agent-indicator/DoubleResetTrigger.cpp`
- Create: `ai-agent-indicator/ProvisioningPolicy.h`
- Create: `ai-agent-indicator/ProvisioningPolicy.cpp`
- Create: `ai-agent-indicator/tests/provisioning_unit.cpp`

**Interfaces:**
- Produces: `normalizeDeviceName(const char*, char[33], char[33])`.
- Produces: `HoldButton::update(bool pressed, uint32_t now)`.
- Produces: `DoubleResetTrigger::begin(now)`, `process(now)`, and `disarm()`.
- Produces: `ProvisioningPolicy::begin(...)`, `update(...)`, and `mode()`.
- All files depend only on standard headers so they compile natively and for ESP8266.

- [ ] **Step 1: Write the failing native test runner**

Create `provisioning_unit.cpp` with this harness:

```cpp
#include <cstdint>
#include <cstring>
#include <iostream>
#include "../DeviceName.h"
#include "../DoubleResetTrigger.h"
#include "../HoldButton.h"
#include "../ProvisioningPolicy.h"

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { \
  std::cerr << __LINE__ << ": " #expr "\n"; ++failures; \
} } while (0)

static uint32_t rtcValue = 0;
static bool rtcReadOk = true;
static bool rtcWriteOk = true;
static bool fakeRead(uint32_t block, uint32_t* data, size_t size) {
  CHECK(block == 64U); CHECK(size == sizeof(uint32_t));
  if (!rtcReadOk) return false;
  *data = rtcValue;
  return true;
}
static bool fakeWrite(uint32_t block, const uint32_t* data, size_t size) {
  CHECK(block == 64U); CHECK(size == sizeof(uint32_t));
  if (!rtcWriteOk) return false;
  rtcValue = *data;
  return true;
}
```

Add tests with these exact inputs and outcomes:

| Unit | Input | Expected |
|---|---|---|
| Device name | `"  Desk_light--1  "` | friendly `Desk_light--1`, hostname `Desk-light-1` |
| Device name | empty, `___`, `bad/name`, 33 characters | reject |
| Hold button | press; 29 ms; 30 ms; 3029 ms; 3030 ms | only 3030 ms fires |
| Hold button | keep held after firing | no second event |
| Hold button | debounced release then another 3-second hold | fires again |
| Double reset | empty RTC, then second object before clear | `Armed`, then `Detected`, marker cleared |
| Double reset | process at 9999/10000 ms | marker present, then cleared |
| Double reset | start near `UINT32_MAX` | clears correctly across rollover |
| Double reset | read failure and write failure | `StorageError` |
| Double reset | call `disarm()` while armed | marker cleared; later boot is not detected |
| Policy | first boot/no credentials | `OpenPortal` immediately |
| Policy | saved credentials | `StartConnection` |
| Policy | disconnected 299999/300000 ms | no portal, then `OpenPortal` |
| Policy | connected then disconnected | fresh five-minute timer |
| Policy | manual trigger in connecting/connected | `OpenPortal` |
| Policy | manual trigger while portal is active | remain in `Portal`; no second action |
| Policy | portal closes connected/disconnected | `Connected` / `StartConnection` |
| Policy | timers near `UINT32_MAX` | correct rollover behavior |

End with:

```cpp
int main() {
  testDeviceNames();
  testHoldButton();
  testDoubleReset();
  testProvisioningPolicy();
  if (failures) return 1;
  std::cout << "provisioning unit tests passed\n";
  return 0;
}
```

- [ ] **Step 2: Verify the test initially fails**

```bash
c++ -std=c++17 -Wall -Wextra -Werror \
  ai-agent-indicator/tests/provisioning_unit.cpp \
  ai-agent-indicator/DeviceName.cpp ai-agent-indicator/HoldButton.cpp \
  ai-agent-indicator/DoubleResetTrigger.cpp ai-agent-indicator/ProvisioningPolicy.cpp \
  -o /tmp/ai-agent-provisioning-unit
```

Expected: missing production headers/sources.

- [ ] **Step 3: Implement name normalization**

```cpp
constexpr size_t DEVICE_NAME_CAPACITY = 33U;
bool normalizeDeviceName(const char* input,
                         char friendly[DEVICE_NAME_CAPACITY],
                         char hostname[DEVICE_NAME_CAPACITY]);
```

Set both outputs empty first. Reject null, empty after trimming ASCII spaces, more than 32 bytes, non-ASCII, or characters outside `[A-Za-z0-9 _-]`. Copy the trimmed friendly name. For the hostname, replace spaces/underscores/hyphens with one collapsed hyphen, remove leading/trailing hyphens, and reject an empty result.

- [ ] **Step 4: Implement hold detection**

```cpp
class HoldButton {
 public:
  HoldButton(uint32_t debounceMs, uint32_t holdMs);
  bool update(bool pressed, uint32_t now);
 private:
  uint32_t debounceMs_, holdMs_, rawChangedAt_ = 0, pressedAt_ = 0;
  bool rawPressed_ = false, stablePressed_ = false, fired_ = false;
};
```

Use unsigned subtraction for all elapsed checks. Start the hold timer after a debounced press, return `true` once at three seconds, and re-arm only after debounced release.

- [ ] **Step 5: Implement RTC double-reset detection**

```cpp
using RtcReadFn = bool (*)(uint32_t, uint32_t*, size_t);
using RtcWriteFn = bool (*)(uint32_t, const uint32_t*, size_t);
enum class DoubleResetStatus : uint8_t { Armed, Detected, StorageError };

class DoubleResetTrigger {
 public:
  static constexpr uint32_t kRtcBlock = 64U;
  static constexpr uint32_t kMarker = 0xA17A11E5UL;
  DoubleResetTrigger(RtcReadFn, RtcWriteFn, uint32_t timeoutMs);
  DoubleResetStatus begin(uint32_t now);
  bool process(uint32_t now);
  bool disarm();
 private:
  RtcReadFn read_;
  RtcWriteFn write_;
  uint32_t timeoutMs_, armedAt_ = 0;
  bool armed_ = false;
};
```

`begin()` reads block 64. If it sees `kMarker`, clear it successfully before returning `Detected`; otherwise return `StorageError`. Without a marker, write it, record `now`, and return `Armed`. After ten seconds, `process()` retries `disarm()` until successful. `disarm()` writes zero and changes `armed_` only after the write succeeds.

- [ ] **Step 6: Implement the connection policy**

```cpp
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
};
```

Use the test table as the complete transition contract. Manual setup overrides connecting or connected state but is consumed without action when already in `Portal`. A newly observed connection returns `Connected`. A newly observed disconnection starts a fresh grace window and returns `StartConnection`. While connecting, return `RetryConnection` every ten seconds; once the continuous disconnected interval reaches five minutes, return `OpenPortal` instead. A portal closing while disconnected starts a fresh five-minute connection/cooldown cycle and returns `StartConnection`, including when no credentials exist. Track connection-start and last-retry timestamps with unsigned subtraction.

- [ ] **Step 7: Run the tests and firmware compile**

Repeat the native compile command, then:

```bash
/tmp/ai-agent-provisioning-unit
arduino-cli compile --profile d1_mini ai-agent-indicator
```

Expected: `provisioning unit tests passed`; firmware build succeeds.

- [ ] **Step 8: Commit the helpers and tests**

```bash
git add ai-agent-indicator/DeviceName.h ai-agent-indicator/DeviceName.cpp \
  ai-agent-indicator/HoldButton.h ai-agent-indicator/HoldButton.cpp \
  ai-agent-indicator/DoubleResetTrigger.h ai-agent-indicator/DoubleResetTrigger.cpp \
  ai-agent-indicator/ProvisioningPolicy.h ai-agent-indicator/ProvisioningPolicy.cpp \
  ai-agent-indicator/tests/provisioning_unit.cpp
git commit --only ai-agent-indicator/DeviceName.h ai-agent-indicator/DeviceName.cpp \
  ai-agent-indicator/HoldButton.h ai-agent-indicator/HoldButton.cpp \
  ai-agent-indicator/DoubleResetTrigger.h ai-agent-indicator/DoubleResetTrigger.cpp \
  ai-agent-indicator/ProvisioningPolicy.h ai-agent-indicator/ProvisioningPolicy.cpp \
  ai-agent-indicator/tests/provisioning_unit.cpp \
  -m "test: add provisioning state helpers"
```

---

### Task 3: Persist the friendly device name in LittleFS

**Files:**
- Create: `ai-agent-indicator/DeviceSettings.h`
- Create: `ai-agent-indicator/DeviceSettings.cpp`

**Interfaces:**
- Consumes: `normalizeDeviceName()` and ESP8266 `LittleFS`.
- Produces: `begin()`, `save(const char*)`, `friendlyName()`, `hostname()`, and `mounted()`.
- Verification: validation remains covered by the native tests from Task 2; LittleFS mount, read, and write behavior is covered by the firmware compile here and the physical fallback test in Task 6.

- [ ] **Step 1: Declare the settings interface**

```cpp
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
```

- [ ] **Step 2: Implement load and fallback behavior**

`begin()` sets defaults, calls `LittleFS.begin()`, and never formats flash. A mount failure returns `false` with defaults active. A missing `/config.json` is a valid default configuration. For an existing file, deserialize with `StaticJsonDocument<128>`, require integer `schema == 1`, require string `name`, and pass it through `normalizeDeviceName()`. Open, parse, schema, or validation failure retains defaults and returns `false` from `load()`.

- [ ] **Step 3: Implement bounded writes**

`save()` validates into temporary 33-byte buffers before writing. Serialize exactly this shape to `/config.tmp`:

```json
{"schema":1,"name":"Desk Light"}
```

Require a nonzero `serializeJson()` result, close the file, remove the old `/config.json`, and rename the temporary file. Only then update the in-memory values. On failure, remove `/config.tmp`, retain current in-memory settings, and return `false`.

- [ ] **Step 4: Compile and commit**

```bash
arduino-cli compile --profile d1_mini ai-agent-indicator
git add ai-agent-indicator/DeviceSettings.h ai-agent-indicator/DeviceSettings.cpp
git commit --only ai-agent-indicator/DeviceSettings.h \
  ai-agent-indicator/DeviceSettings.cpp \
  -m "feat: persist provisioning device name"
```

Expected: Arduino compile succeeds; only the settings files enter this commit.

---

### Task 4: Add the WiFiManager provisioning coordinator

**Files:**
- Create: `ai-agent-indicator/WifiProvisioning.h`
- Create: `ai-agent-indicator/WifiProvisioning.cpp`
- Consume: all components from Tasks 2 and 3.

**Interfaces:**
- Consumes: D6 input, saved ESP8266 station settings, LittleFS settings, WiFiManager callbacks, and `millis()`.
- Produces: `begin(now)`, `process(now)`, `isConnected()`, `isPortalActive()`, `friendlyName()`, `hostname()`, and `shouldRestart()`.
- Calls: `BeforePortalStartFn` immediately before WiFiManager takes port 80.

- [ ] **Step 1: Declare the coordinator**

```cpp
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
};
```

Members include `WiFiManager`, one `WiFiManagerParameter`, `DeviceSettings`, `HoldButton(30, 3000)`, `DoubleResetTrigger`, `ProvisioningPolicy(300000, 10000)`, D6 pin, pre-portal callback, restart-pending/restart-ready flags and deadline, and a 32-byte AP-name buffer.

- [ ] **Step 2: Add RTC adapters**

```cpp
static bool readRtc(uint32_t block, uint32_t* data, size_t size) {
  return ESP.rtcUserMemoryRead(block, data, size);
}
static bool writeRtc(uint32_t block, const uint32_t* data, size_t size) {
  return ESP.rtcUserMemoryWrite(block, const_cast<uint32_t*>(data), size);
}
```

Construct `DoubleResetTrigger(readRtc, writeRtc, 10000U)`. Log only generic success/error labels, never RTC contents, SSIDs, or passwords.

- [ ] **Step 3: Configure WiFiManager once**

```cpp
manager_.setConfigPortalBlocking(false);
manager_.setConfigPortalTimeout(kPortalSeconds);
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
```

Create and add this parameter exactly once:

```cpp
WiFiManagerParameter deviceNameParameter_{
    "device_name", "Device name", "AI-Agent-Indicator", 32,
    "required maxlength='32' pattern='[A-Za-z0-9 _-]{1,32}' "
    "title='Use letters, numbers, spaces, underscores, or hyphens'"};
```

Register `setSaveParamsCallback([this] { savePortalParameters(); })` and `setSaveConfigCallback([this] { markConfigSaved(); })`. Parameter save validates and writes only the friendly name; invalid bypassed input is ignored with a generic Serial warning. Config save sets restart pending with a deadline of `millis() + 1000` after WiFiManager reports successful Wi-Fi configuration.

- [ ] **Step 4: Implement boot decisions**

`begin(now)` performs this order:

1. Configure D6 as `INPUT_PULLUP`.
2. Load `DeviceSettings`; log fallback without printing stored values.
3. Configure WiFiManager and set the parameter value to the loaded name.
4. Call `DoubleResetTrigger::begin(now)` and log a storage error if returned.
5. Set `WiFi.mode(WIFI_STA)`, enable auto-reconnect, and apply the configured hostname before connecting.
6. Read `manager_.getWiFiIsSaved()`.
7. Call `policy_.begin(now, hasSavedCredentials, resetStatus == Detected)` and apply its action.

`StartConnection` and `RetryConnection` call parameterless `WiFi.begin()` so the ESP8266 uses persisted settings. `OpenPortal` first calls `beforePortalStart_()`, formats `AI-Agent-Light-Setup-%04X` using `ESP.getChipId() & 0xFFFF`, refreshes the custom parameter, and calls `manager_.startConfigPortal(apName_)` without a password.

- [ ] **Step 5: Implement loop processing**

`process(now)` performs this order:

1. Process RTC-marker expiry and log only a generic clear error.
2. Feed `digitalRead(buttonPin_) == LOW` into `HoldButton`.
3. Call `manager_.process()` only while the portal is active.
4. Pass Wi-Fi state, actual portal state, and the one-shot manual trigger to `policy_.update()`.
5. Apply the returned action.
6. Once a pending restart reaches its rollover-safe deadline, call `resetTrigger_.disarm()`. Retry on later loops if the write fails; set restart ready only after it succeeds.

Implement the deadline and query exactly as rollover-safe state, not as a direct absolute-time comparison:

```cpp
if (restartPending_ &&
    static_cast<int32_t>(now - restartAt_) >= 0 &&
    resetTrigger_.disarm()) {
  restartPending_ = false;
  restartReady_ = true;
}

bool WifiProvisioning::shouldRestart() const {
  return restartReady_;
}
```

`isPortalActive()` returns `manager_.getConfigPortalActive()`. `isConnected()` returns `WiFi.status() == WL_CONNECTED`. Neither method caches a second copy of library state.

All five-minute timing remains in `ProvisioningPolicy`; add no long delays. WiFiManager may block for at most 15 seconds while validating a submitted network. Failed credentials leave the portal active. Timeout/restart uses the most recently persisted credentials, with no backup.

If the D6 hold event occurs while the portal is already active, consume and ignore it; never call `startConfigPortal()` a second time for the same active portal.

- [ ] **Step 6: Compile and commit**

```bash
arduino-cli compile --profile d1_mini ai-agent-indicator
git add ai-agent-indicator/WifiProvisioning.h ai-agent-indicator/WifiProvisioning.cpp
git commit --only ai-agent-indicator/WifiProvisioning.h \
  ai-agent-indicator/WifiProvisioning.cpp \
  -m "feat: add Wi-Fi provisioning coordinator"
```

Expected: firmware compiles with WiFiManager 2.0.17 and no external double-reset library.

---

### Task 5: Integrate provisioning with the API and LEDs

**Files:**
- Modify: `ai-agent-indicator/ai-agent-indicator.ino`
- Test: `ai-agent-indicator/tests/provisioning_unit.cpp`
- Test: `ai-agent-indicator/tests/api_smoke.py`

**Interfaces:**
- Consumes: `WifiProvisioning` state and its pre-portal callback.
- Preserves: existing API handlers, JSON validation, state mapping, and expiry functions.
- Produces: mutually exclusive port-80 servers and setup/disconnected/agent LED priority.

- [ ] **Step 1: Remove compile-time credential ownership**

Delete `WIFI_SSID`, `WIFI_PASSWORD`, `DEVICE_HOSTNAME`, `WIFI_RETRY_INTERVAL_MS`, `lastWifiAttemptMs`, `wasWifiConnected`, and `maintainWifi()`. Add:

```cpp
#include "WifiProvisioning.h"

constexpr uint8_t SETUP_BUTTON_PIN = D6;
bool apiServerRunning = false;

void stopStatusServer();
WifiProvisioning provisioning(SETUP_BUTTON_PIN, stopStatusServer);
```

Do not copy staged credential values anywhere.

- [ ] **Step 2: Make port-80 ownership explicit**

```cpp
void stopStatusServer() {
  if (!apiServerRunning) return;
  server.stop();
  apiServerRunning = false;
}

void syncStatusServer() {
  if (provisioning.isPortalActive()) {
    stopStatusServer();
    return;
  }
  if (provisioning.isConnected() && !apiServerRunning) {
    server.begin();
    apiServerRunning = true;
    Serial.print("Connected. Device URL: http://");
    Serial.println(WiFi.localIP());
  }
}
```

Register routes once in `setup()`, but start the status server only when connected and the portal is inactive. `WifiProvisioning::startPortal()` invokes `stopStatusServer()` before WiFiManager starts its server.

- [ ] **Step 3: Add LED display priority**

Replace only the disconnected branch at the start of `renderLeds(now)`:

```cpp
if (provisioning.isPortalActive()) {
  uint8_t phase = (now / WIFI_BLINK_INTERVAL_MS) % 3U;
  writeLeds(phase == 0U, phase == 1U, phase == 2U);
  return;
}
if (!provisioning.isConnected()) {
  bool on = ((now / WIFI_BLINK_INTERVAL_MS) % 2U) == 0U;
  writeLeds(on, on, on);
  return;
}
```

Leave the connected agent-state switch unchanged.

- [ ] **Step 4: Replace setup and loop lifecycle**

```cpp
void setup() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  writeLeds(false, false, false);
  Serial.begin(115200);
  setupRoutes();
  provisioning.begin(millis());
  syncStatusServer();
}

void loop() {
  uint32_t now = millis();
  provisioning.process(now);
  syncStatusServer();
  expireStatusIfNeeded(now);
  if (apiServerRunning) server.handleClient();
  renderLeds(now);
  if (provisioning.shouldRestart()) ESP.restart();
  yield();
}
```

- [ ] **Step 5: Run automated checks**

```bash
c++ -std=c++17 -Wall -Wextra -Werror \
  ai-agent-indicator/tests/provisioning_unit.cpp \
  ai-agent-indicator/DeviceName.cpp ai-agent-indicator/HoldButton.cpp \
  ai-agent-indicator/DoubleResetTrigger.cpp ai-agent-indicator/ProvisioningPolicy.cpp \
  -o /tmp/ai-agent-provisioning-unit
/tmp/ai-agent-provisioning-unit
python3 -m py_compile ai-agent-indicator/tests/api_smoke.py
arduino-cli compile --profile d1_mini ai-agent-indicator
git diff --check
```

Expected: unit tests pass, Python compiles, firmware builds, and whitespace checks pass.

- [ ] **Step 6: Check credential/dependency removal without printing secrets**

```bash
rg -n 'WIFI_SSID|WIFI_PASSWORD|ESP_DoubleResetDetector|DoubleResetDetector_Generic' \
  ai-agent-indicator --glob '!docs/**'
git status --short
git diff --cached --name-only
```

Expected: `rg` has no matches. Inspect only file names in the index; do not print the old staged credential diff.

- [ ] **Step 7: Commit the integrated sketch**

```bash
git add ai-agent-indicator/ai-agent-indicator.ino
git commit --only ai-agent-indicator/ai-agent-indicator.ino \
  -m "feat: add captive Wi-Fi setup wizard"
```

Expected: committed sketch contains no credential literals; unrelated files remain untouched.

---

### Task 6: Document and physically verify provisioning

**Files:**
- Modify: `ai-agent-indicator/README.md`
- Modify: `QUICK_BUILD_GUIDE.md`
- Test: `ai-agent-indicator/tests/api_smoke.py`

**Interfaces:**
- Documents: wizard, device name, both five-minute phases, triggers, optional wiring, LED patterns, credential replacement, and security.
- Preserves: existing API and agent-hook instructions.

- [ ] **Step 1: Replace baked-credential setup in the firmware README**

Document these exact requirements:

```text
ESP8266 Arduino core 3.1.2
WiFiManager 2.0.17
ArduinoJson 6.21.6
Board: LOLIN(WEMOS) D1 R2 & mini
```

Explain: join open `AI-Agent-Light-Setup-XXXX`; wait for the captive page or browse to `192.168.4.1`; select a 2.4 GHz network or type a hidden SSID; enter password and device name; reconnect to the normal LAN after restart.

- [ ] **Step 2: Document recovery and security**

State that saved Wi-Fi gets a five-minute reconnect grace period followed by a five-minute portal. Submitting wrong credentials replaces the previous saved credentials because stock WiFiManager is used. Recovery is correction while the portal remains active, waiting five minutes for automatic setup, pressing reset twice within ten seconds, or holding D6-to-GND for three seconds. The setup AP is open to nearby users during its five-minute window.

- [ ] **Step 3: Document button wiring and LED meanings**

Add:

| Purpose | Wemos pin | ESP8266 GPIO | Connection |
|---|---|---|---|
| Optional setup button | D6 | GPIO12 | Momentary normally-open button from D6 to GND |

Explain setup's 500 ms red → yellow → green cycle, the 500 ms all-LED reconnect blink, and steady connected agent colors. State that the current enclosure has no guaranteed button opening and the user will update its design.

- [ ] **Step 4: Update the quick-build guide**

Replace firmware credential editing with wizard instructions. Add WiFiManager to prerequisites, the optional button to parts/wiring, and troubleshooting for wrong credentials, hidden SSIDs, a portal that does not auto-open, double-reset timing, and the router grace period. Keep Ender-3 V3 SE and 8 mm LED information unchanged.

- [ ] **Step 5: Run documentation checks**

```bash
rg -n 'WiFiManager 2\.0\.17|3\.1\.2|AI-Agent-Light-Setup|192\.168\.4\.1|D6|GPIO12|five minutes|double reset|open' \
  ai-agent-indicator/README.md QUICK_BUILD_GUIDE.md
rg -n 'set `WIFI_SSID`|set `WIFI_PASSWORD`' \
  ai-agent-indicator/README.md QUICK_BUILD_GUIDE.md
git diff --check
```

Expected: the first search finds the new instructions; the obsolete-instruction search has no matches; the diff check passes.

- [ ] **Step 6: Run the physical acceptance checklist**

Flash the board and record each result:

1. Erase saved Wi-Fi and confirm first boot opens setup immediately.
2. Confirm nearby networks appear and a hidden SSID can be entered.
3. Save valid Wi-Fi and a custom name; confirm one clean restart, no accidental return to setup, the DHCP hostname, API URL, and persistence across power cycle.
4. Turn off the router; confirm all LEDs blink for five minutes without setup opening early.
5. Keep Wi-Fi unavailable; confirm the setup AP opens after five minutes.
6. Open setup without submitting; confirm it closes after five minutes and returns to connection attempts.
7. Submit a wrong password; confirm the portal stays available and accepts a correction.
8. Save unusable credentials and restart; press reset twice within ten seconds and confirm setup opens. Wait longer than ten seconds and confirm it does not count.
9. Hold D6 to GND for three seconds; confirm setup opens. Confirm a short press does nothing.
10. Confirm setup cycles red/yellow/green, disconnection blinks all three, and connected API states show existing colors.
11. Post `working`, remain in setup/disconnected for over 120 seconds, reconnect, and confirm green.
12. Corrupt `/config.json` with a disposable sketch, reflash the main firmware, and confirm fallback to `AI-Agent-Indicator` without blocking setup. The disposable sketch is test-only and must not be committed:

    ```cpp
    #include <LittleFS.h>

    void setup() {
      LittleFS.begin();
      File file = LittleFS.open("/config.json", "w");
      file.print("{\"schema\":999}");
      file.close();
    }

    void loop() {}
    ```

- [ ] **Step 7: Run API regression and final automated verification**

```bash
python3 ai-agent-indicator/tests/api_smoke.py --base-url http://DEVICE_IP --quick
python3 ai-agent-indicator/tests/api_smoke.py --base-url http://DEVICE_IP
/tmp/ai-agent-provisioning-unit
python3 -m py_compile ai-agent-indicator/tests/api_smoke.py
arduino-cli compile --profile d1_mini ai-agent-indicator
git diff --check
git status --short
```

Expected: both API runs pass, including 121-second expiry; unit tests and firmware build pass; no enclosure or hook files changed.

- [ ] **Step 8: Commit only documentation**

```bash
git add ai-agent-indicator/README.md QUICK_BUILD_GUIDE.md
git commit --only ai-agent-indicator/README.md QUICK_BUILD_GUIDE.md \
  -m "docs: explain captive Wi-Fi setup"
```
