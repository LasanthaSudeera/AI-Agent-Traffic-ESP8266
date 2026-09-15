# AI Agent Traffic Light Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ESP8266 blink example with a Wemos D1 Mini traffic light controlled by a last-writer-wins JSON HTTP API.

**Architecture:** A single Arduino sketch owns Wi-Fi connection management, status/expiry state, HTTP request handling, and non-blocking LED rendering. A Python standard-library smoke test exercises the device as a black box after upload, while a README documents hardware, setup, API usage, and bench verification.

**Tech Stack:** Wemos D1 Mini (ESP8266), Arduino C++, ESP8266 Arduino core (`ESP8266WiFi`, `ESP8266WebServer`), ArduinoJson, Python 3 standard library, Arduino CLI or Arduino IDE 2.x.

## Global Constraints

- Red means `working`; yellow means `blocked`, `permission`, stuck, or awaiting user input; green means `ready` or stale/no activity.
- Version 1 is last-writer-wins and stores only one current status. It does not aggregate agents.
- `POST /api/status` accepts JSON bodies no larger than 256 bytes; `state` is required and `agent` is optional with a 32-byte maximum.
- A valid update expires after exactly 120 seconds and falls back to `ready`, an empty agent name, and green.
- All three LEDs blink together at 500 ms intervals whenever Wi-Fi is disconnected.
- Wi-Fi credentials are sketch constants supplied at deployment time. Never commit real credentials.
- Set the DHCP hostname to `AI-Agent-Indicator` before `WiFi.begin()` so compatible routers show that name; do not add mDNS or a `.local` address.
- The HTTP API listens on port 80 without authentication and is intended only for a trusted LAN.
- D1/GPIO5 drives red, D2/GPIO4 drives yellow, and D5/GPIO14 drives green through separate 330 Ω resistors; LEDs are active-high.
- Runtime code remains in `ai-agent-indicator.ino`; ArduinoJson is the only additional firmware library.
- The user approved a strict-TDD exception for this hardware-only, single-sketch prototype. Verify behavior through compilation, black-box requests to the flashed board, and physical LED checks rather than extracting host-only production modules.
- README prose receives a manual requirements review, not source-text grep tests.
- Preserve unrelated worktree changes. The pre-plan change to `ai-agent-indicator.ino` is line-ending-only when viewed with `git diff --ignore-space-at-eol`; recheck before editing.
- The current workspace has Python 3 but no `arduino-cli` on `PATH`. Automated compilation requires installing Arduino CLI and the ESP8266 core, or using Arduino IDE 2.x for the equivalent compile/upload checks.

## File Structure

- Modify `ai-agent-indicator.ino`: all device configuration, state management, Wi-Fi handling, HTTP API, and LED rendering.
- Create `tests/api_smoke.py`: dependency-free black-box HTTP acceptance test for the flashed board.
- Create `README.md`: wiring, prerequisites, credential configuration, upload instructions, API examples, and manual fault tests.

---

### Task 1: Firmware and black-box API behavior

**Files:**
- Modify: `ai-agent-indicator.ino`
- Create: `tests/api_smoke.py`

**Interfaces:**
- Consumes: `POST /api/status` JSON with required `state: string` and optional `agent: string`; Wi-Fi SSID/password and DHCP-hostname constants; `millis()`; ESP8266 Wi-Fi status.
- Produces: `GET /api/status`; `POST /api/status`; JSON fields `state`, `agent`, and `expires_in_seconds`; active-high output on D1/D2/D5; all-LED Wi-Fi fault blink.
- Internal firmware interface: `AgentState`, `parseState()`, `stateName()`, `setCurrentState()`, `expireStatusIfNeeded()`, `expiresInSeconds()`, `renderLeds()`, `maintainWifi()`, `handleStatusPost()`, `handleStatusGet()`, `sendStatusResponse()`, and `sendJsonError()`.

- [ ] **Step 1: Confirm the only pre-existing sketch difference is line endings**

Run:

```bash
git diff --ignore-space-at-eol -- ai-agent-indicator.ino
```

Expected: no output. If substantive user code appears, stop and reconcile it before replacing the blink example.

- [ ] **Step 2: Write the black-box acceptance test before the firmware implementation**

Create `tests/api_smoke.py` with this behavior and concrete request helpers:

```python
#!/usr/bin/env python3
import argparse
import json
import time
import urllib.error
import urllib.request


def request(base_url, method, path, payload=None, raw_body=None):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    elif raw_body is not None:
        data = raw_body
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(
        f"{base_url.rstrip('/')}{path}", data=data, headers=headers, method=method
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            body = response.read().decode("utf-8")
            return response.status, json.loads(body) if body else {}
    except urllib.error.HTTPError as error:
        body = error.read().decode("utf-8")
        return error.code, json.loads(body) if body else {}


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def expect_status(base_url, method, path, expected, payload=None, raw_body=None):
    status, body = request(base_url, method, path, payload, raw_body)
    expect(status == expected, f"{method} {path}: expected {expected}, got {status}: {body}")
    return body


def run(base_url, quick):
    body = expect_status(base_url, "POST", "/api/status", 200,
                         {"state": "working", "agent": "codex"})
    expect(body["state"] == "working", body)
    expect(body["agent"] == "codex", body)
    expect(1 <= body["expires_in_seconds"] <= 120, body)

    body = expect_status(base_url, "GET", "/api/status", 200)
    expect(body["state"] == "working" and body["agent"] == "codex", body)

    expect_status(base_url, "POST", "/api/status", 200,
                  {"state": "blocked", "agent": "agent-a"})
    expect_status(base_url, "POST", "/api/status", 200,
                  {"state": "permission", "agent": "agent-b"})
    body = expect_status(base_url, "POST", "/api/status", 200,
                         {"state": "ready", "agent": "agent-c"})
    expect(body["state"] == "ready" and body["agent"] == "agent-c", body)

    expect_status(base_url, "POST", "/api/status", 200,
                  {"state": "working", "agent": "preserved"})
    invalid_requests = [
        ({}, None),
        ({"state": 7}, None),
        ({"state": "unknown"}, None),
        ({"state": "ready", "agent": 7}, None),
        ({"state": "ready", "agent": "x" * 33}, None),
        (None, b"{not-json"),
        (None, json.dumps({"state": "ready", "padding": "x" * 300}).encode("utf-8")),
    ]
    for payload, raw_body in invalid_requests:
        expect_status(base_url, "POST", "/api/status", 400, payload, raw_body)

    body = expect_status(base_url, "GET", "/api/status", 200)
    expect(body["state"] == "working" and body["agent"] == "preserved", body)
    expect_status(base_url, "GET", "/missing", 404)
    expect_status(base_url, "PUT", "/api/status", 405, {"state": "ready"})

    if not quick:
        expect_status(base_url, "POST", "/api/status", 200,
                      {"state": "working", "agent": "timeout-test"})
        time.sleep(121)
        body = expect_status(base_url, "GET", "/api/status", 200)
        expect(body == {"state": "ready", "agent": "", "expires_in_seconds": 0}, body)

    print("API smoke test passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", help="Device URL, for example http://192.168.1.42")
    parser.add_argument("--quick", action="store_true", help="Skip the 121-second expiry test")
    args = parser.parse_args()
    base_url = args.base_url or input("Device URL printed in Serial Monitor: ").strip()
    run(base_url, args.quick)


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Verify the test file is valid Python**

Run:

```bash
python3 -m py_compile tests/api_smoke.py
```

Expected: exit 0 with no output.

- [ ] **Step 4: Replace the blink example with the firmware configuration and state model**

At the top of `ai-agent-indicator.ino`, define the exact types, pins, limits, and state storage. Leave credentials empty in committed code; insert real values only in the local copy used for upload.

```cpp
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* DEVICE_HOSTNAME = "AI-Agent-Indicator";

constexpr uint8_t RED_LED_PIN = D1;
constexpr uint8_t YELLOW_LED_PIN = D2;
constexpr uint8_t GREEN_LED_PIN = D5;
constexpr uint32_t STATUS_TIMEOUT_MS = 120000UL;
constexpr uint32_t WIFI_BLINK_INTERVAL_MS = 500UL;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000UL;
constexpr size_t MAX_REQUEST_BYTES = 256;
constexpr size_t MAX_AGENT_BYTES = 32;

enum class AgentState : uint8_t { READY, WORKING, BLOCKED, PERMISSION };

ESP8266WebServer server(80);
AgentState currentState = AgentState::READY;
String currentAgent;
uint32_t lastValidUpdateMs = 0;
uint32_t lastWifiAttemptMs = 0;
bool hasFreshStatus = false;
bool wasWifiConnected = false;
```

Implement state conversion and mutation with exact matching; do not accept aliases or change state until the entire request has validated:

```cpp
bool parseState(const char* value, AgentState& parsed) {
  if (strcmp(value, "ready") == 0) parsed = AgentState::READY;
  else if (strcmp(value, "working") == 0) parsed = AgentState::WORKING;
  else if (strcmp(value, "blocked") == 0) parsed = AgentState::BLOCKED;
  else if (strcmp(value, "permission") == 0) parsed = AgentState::PERMISSION;
  else return false;
  return true;
}

const char* stateName(AgentState state) {
  switch (state) {
    case AgentState::WORKING: return "working";
    case AgentState::BLOCKED: return "blocked";
    case AgentState::PERMISSION: return "permission";
    default: return "ready";
  }
}

void setCurrentState(AgentState state, const char* agent, uint32_t now) {
  currentState = state;
  currentAgent = agent;
  lastValidUpdateMs = now;
  hasFreshStatus = true;
}

void expireStatusIfNeeded(uint32_t now) {
  if (hasFreshStatus && static_cast<uint32_t>(now - lastValidUpdateMs) >= STATUS_TIMEOUT_MS) {
    currentState = AgentState::READY;
    currentAgent = "";
    hasFreshStatus = false;
  }
}

uint32_t expiresInSeconds(uint32_t now) {
  if (!hasFreshStatus) return 0;
  uint32_t elapsed = static_cast<uint32_t>(now - lastValidUpdateMs);
  if (elapsed >= STATUS_TIMEOUT_MS) return 0;
  return (STATUS_TIMEOUT_MS - elapsed + 999UL) / 1000UL;
}
```

- [ ] **Step 5: Implement deterministic LED rendering and non-blocking Wi-Fi recovery**

Use one helper to prevent mixed solid states. `working` maps to red; both attention states map to yellow; `ready` maps to green. Wi-Fi disconnection overrides the logical state.

```cpp
void writeLeds(bool red, bool yellow, bool green) {
  digitalWrite(RED_LED_PIN, red ? HIGH : LOW);
  digitalWrite(YELLOW_LED_PIN, yellow ? HIGH : LOW);
  digitalWrite(GREEN_LED_PIN, green ? HIGH : LOW);
}

void renderLeds(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    bool on = ((now / WIFI_BLINK_INTERVAL_MS) % 2U) == 0U;
    writeLeds(on, on, on);
    return;
  }

  switch (currentState) {
    case AgentState::WORKING: writeLeds(true, false, false); break;
    case AgentState::BLOCKED:
    case AgentState::PERMISSION: writeLeds(false, true, false); break;
    default: writeLeds(false, false, true); break;
  }
}

void maintainWifi(uint32_t now) {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasWifiConnected) {
    Serial.print("Connected. Device URL: http://");
    Serial.println(WiFi.localIP());
  } else if (!connected &&
             static_cast<uint32_t>(now - lastWifiAttemptMs) >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiAttemptMs = now;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  wasWifiConnected = connected;
}
```

- [ ] **Step 6: Implement JSON responses, validation, and routes**

Use `StaticJsonDocument` so memory use is bounded. Call `expireStatusIfNeeded(millis())` immediately before every status response.

```cpp
void sendJsonError(int code, const char* message) {
  StaticJsonDocument<96> doc;
  doc["error"] = message;
  String output;
  serializeJson(doc, output);
  server.send(code, "application/json", output);
}

void sendStatusResponse() {
  uint32_t now = millis();
  expireStatusIfNeeded(now);
  StaticJsonDocument<192> doc;
  doc["state"] = stateName(currentState);
  doc["agent"] = currentAgent;
  doc["expires_in_seconds"] = expiresInSeconds(now);
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleStatusGet() {
  sendStatusResponse();
}

void handleStatusPost() {
  String body = server.arg("plain");
  if (body.length() > MAX_REQUEST_BYTES) {
    sendJsonError(400, "body exceeds 256 bytes");
    return;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    sendJsonError(400, "malformed JSON");
    return;
  }
  if (!doc["state"].is<const char*>()) {
    sendJsonError(400, "state must be a string");
    return;
  }

  AgentState parsed;
  if (!parseState(doc["state"].as<const char*>(), parsed)) {
    sendJsonError(400, "unsupported state");
    return;
  }

  const char* agent = "";
  if (!doc["agent"].isNull()) {
    if (!doc["agent"].is<const char*>()) {
      sendJsonError(400, "agent must be a string");
      return;
    }
    agent = doc["agent"].as<const char*>();
    if (strlen(agent) > MAX_AGENT_BYTES) {
      sendJsonError(400, "agent exceeds 32 bytes");
      return;
    }
  }

  setCurrentState(parsed, agent, millis());
  sendStatusResponse();
}
```

Register both valid methods. Make `/api/status` with any other method return 405 and every other path return 404:

```cpp
void setupRoutes() {
  server.on("/api/status", HTTP_GET, handleStatusGet);
  server.on("/api/status", HTTP_POST, handleStatusPost);
  server.onNotFound([]() {
    if (server.uri() == "/api/status") sendJsonError(405, "method not allowed");
    else sendJsonError(404, "not found");
  });
}
```

- [ ] **Step 7: Assemble non-blocking setup and loop functions**

Initialize LEDs LOW before networking. Do not add `delay()` calls.

```cpp
void setup() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  writeLeds(false, false, false);

  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (!WiFi.hostname(DEVICE_HOSTNAME)) {
    Serial.println("Failed to set DHCP hostname");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();

  setupRoutes();
  server.begin();
}

void loop() {
  uint32_t now = millis();
  maintainWifi(now);
  expireStatusIfNeeded(now);
  server.handleClient();
  renderLeds(now);
  yield();
}
```

- [ ] **Step 8: Configure the build toolchain and compile**

If Arduino CLI is available, install the board core and sole additional library, then compile:

```bash
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli lib install ArduinoJson
arduino-cli compile --fqbn esp8266:esp8266:d1_mini .
```

Expected: all commands exit 0 and compilation reports program and global-variable memory usage. If Arduino CLI cannot be installed, perform the equivalent compile in Arduino IDE 2.x using board “LOLIN(WEMOS) D1 R2 & mini,” the ESP8266 board package, and ArduinoJson.

- [ ] **Step 9: Upload with local credentials and run the quick API test**

Put the actual SSID and password into the two local constants and do not stage them. Run `arduino-cli board list`, copy the attached board's exact port, and pass it to `arduino-cli upload --fqbn esp8266:esp8266:d1_mini -p <port> .`; alternatively, upload through Arduino IDE. Read the device URL at 115200 baud. Here `<port>` is runtime hardware input, not a value to commit.

Before sending any request, confirm the connected board shows solid green. Then run:

```bash
python3 tests/api_smoke.py --quick
```

Expected: `API smoke test passed`. Confirm the router's connected-client list identifies the board as `AI-Agent-Indicator`. Afterward, use the four POST examples from the test one at a time, pausing after each request to confirm working is red, blocked and permission are yellow, and ready is green.

- [ ] **Step 10: Run the full timeout acceptance test**

Run:

```bash
python3 tests/api_smoke.py
```

Expected after roughly 121 seconds: `API smoke test passed`, with the LED returning from red to green during the wait.

- [ ] **Step 11: Remove credentials, recompile, and commit Task 1**

Restore both credential constants to empty strings, then run:

```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini .
python3 -m py_compile tests/api_smoke.py
git diff --check -- ai-agent-indicator.ino tests/api_smoke.py
git add ai-agent-indicator.ino tests/api_smoke.py
git commit -m "feat: add Wi-Fi agent traffic light firmware"
```

Expected: compile and Python syntax checks exit 0, `git diff --check` prints nothing, and the commit contains only the sketch and smoke test.

---

### Task 2: Setup documentation and final bench verification

**Files:**
- Create: `README.md`
- Verify: `ai-agent-indicator.ino`
- Verify: `tests/api_smoke.py`

**Interfaces:**
- Consumes: Task 1 pin constants, accepted state names, `/api/status` contract, build FQBN, Serial baud rate, and smoke-test CLI.
- Produces: exact wiring and setup procedure, copyable `curl` examples, trusted-LAN warning, and a complete operator verification checklist.

- [ ] **Step 1: Create the project README**

Write `README.md` with these exact sections and facts:

````markdown
# AI Agent Traffic Light

A Wemos D1 Mini status light controlled through a local JSON HTTP API.

## State colors

| Agent state | LED |
|---|---|
| `working` | Red |
| `blocked` or `permission` | Yellow |
| `ready` or 120 seconds without an update | Green |

## Wiring

Connect D1/GPIO5 to the red LED, D2/GPIO4 to the yellow LED, and
D5/GPIO14 to the green LED. Each GPIO connects through its own 330 Ω
resistor to the LED anode; connect all cathodes to GND.

## Software setup

Install the ESP8266 Arduino board package and ArduinoJson. Select
“LOLIN(WEMOS) D1 R2 & mini.” Set `WIFI_SSID` and `WIFI_PASSWORD` locally,
upload the sketch, and open Serial Monitor at 115200 baud to read the URL.
Do not commit real Wi-Fi credentials.

The firmware advertises the DHCP hostname `AI-Agent-Indicator` so compatible
routers show a recognizable client name. It does not provide mDNS or a
`.local` URL; use the numeric IP address for API requests.

## API

Send a status:

```sh
curl -X POST http://DEVICE_IP/api/status \
  -H 'Content-Type: application/json' \
  -d '{"state":"working","agent":"codex"}'
```

Read the current status:

```sh
curl http://DEVICE_IP/api/status
```

Valid states are `working`, `blocked`, `permission`, and `ready`. The
optional agent name is limited to 32 bytes. The latest valid update wins.

## Test

Run `python3 tests/api_smoke.py` and enter the URL printed in Serial Monitor.
Use `--quick` to skip the 121-second expiry check.

## Fault behavior and security

All three LEDs blink together while Wi-Fi is disconnected. The device retries
until connectivity returns. This version has no authentication; use it only on
a trusted LAN.
````

Replace line wrapping as needed, but retain every fact and command.

- [ ] **Step 2: Review the README against the approved design**

Read the rendered document and manually confirm it states all three pin/color mappings, 330 Ω resistors, all four API states, the 120-second fallback, last-writer-wins behavior, Wi-Fi fault blinking, local credential handling, DHCP hostname `AI-Agent-Indicator`, the absence of mDNS/`.local` resolution, the smoke-test command, and the trusted-LAN warning. Fix omissions or contradictions before continuing.

- [ ] **Step 3: Perform the Wi-Fi fault bench test**

With the board showing a fresh `working` state, disable its access point or move it out of range. Confirm all three LEDs blink together rather than showing solid red. Restore Wi-Fi within 120 seconds and confirm red returns; repeat with an outage longer than 120 seconds and confirm green appears after reconnection. Finally, set `working`, reset the board, and confirm it forgets the previous state, shows green after reconnecting, and reappears as `AI-Agent-Indicator` in the router client list.

- [ ] **Step 4: Run final automated verification**

With credentials removed from the committed sketch, run:

```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini .
python3 -m py_compile tests/api_smoke.py
git diff --check -- ai-agent-indicator.ino tests/api_smoke.py README.md
```

Expected: every command exits 0 and the diff check prints nothing. Then temporarily restore local credentials, upload, run `python3 tests/api_smoke.py`, confirm `API smoke test passed`, and remove the credentials again.

- [ ] **Step 5: Commit Task 2**

Run:

```bash
git add README.md
git commit -m "docs: add traffic light setup guide"
```

Expected: the commit contains only `README.md`; `git status --short` shows no new task changes and does not include real credentials.
