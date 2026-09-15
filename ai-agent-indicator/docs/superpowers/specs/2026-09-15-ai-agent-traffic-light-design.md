# AI Agent Traffic Light — Version 1 Design

## Goal

Build a Wi-Fi-connected traffic light on a Wemos D1 Mini (ESP8266). AI agents or simple scripts set the light through a local HTTP API:

- Red: an agent is working.
- Yellow: an agent is blocked, stuck, or needs user permission.
- Green: the agent is ready or no fresh activity remains.

Version 1 deliberately uses last-writer-wins behavior. The ESP8266 does not retain separate state for multiple agents. Per-agent tracking and aggregation are deferred to a later version.

## Scope

Version 1 includes:

- One Wemos D1 Mini.
- Three discrete LEDs with current-limiting resistors.
- Wi-Fi credentials stored as sketch constants.
- A JSON HTTP API with no authentication.
- A two-minute inactivity timeout.
- Wi-Fi fault indication.
- Example `curl` commands.

Version 1 excludes:

- Per-agent state aggregation.
- Automatic Codex, Claude, or other desktop-agent monitoring.
- A desktop bridge or command-line helper.
- Authentication or encryption.
- A Wi-Fi configuration portal.
- Persistent status across reboot.
- MQTT and cloud services.

## Hardware

| Signal | Wemos pin | ESP8266 GPIO | Wiring |
|---|---:|---:|---|
| Red | D1 | GPIO5 | GPIO → 330 Ω resistor → LED anode |
| Yellow | D2 | GPIO4 | GPIO → 330 Ω resistor → LED anode |
| Green | D5 | GPIO14 | GPIO → 330 Ω resistor → LED anode |

All LED cathodes connect to GND. The LEDs are active-high: `HIGH` turns an LED on and `LOW` turns it off. D1, D2, and D5 avoid ESP8266 boot-strapping pins.

Required parts are one Wemos D1 Mini, one red LED, one yellow LED, one green LED, three 330 Ω resistors, a breadboard, jumper wires, and a USB power cable.

## Runtime Behavior

At boot, the firmware configures all LED pins as outputs and begins connecting to Wi-Fi. All three LEDs blink together while Wi-Fi is unavailable. After connection, the board prints its DHCP-assigned IP address at 115200 baud and starts an HTTP server on port 80.

With no fresh status update, the logical state is `ready` and the green LED is lit. Every valid update replaces the current state and restarts the 120-second inactivity timer. When the timer expires, the state becomes `ready` and the stored agent name is cleared.

Wi-Fi loss overrides the normal display and blinks all three LEDs together. The inactivity timer continues during the outage. After reconnection, the previous state is displayed only if it remains fresh; otherwise green is displayed.

All timing uses non-blocking `millis()` comparisons. The loop must continue servicing HTTP, Wi-Fi reconnection, timeout checks, and LED blinking, and its elapsed-time arithmetic must remain correct across `millis()` rollover.

## HTTP API

### Update status

`POST /api/status` with `Content-Type: application/json` accepts a body of at most 256 bytes:

```json
{
  "state": "working",
  "agent": "codex"
}
```

`state` is required and accepts exactly:

| State | Display |
|---|---|
| `blocked` | Solid yellow |
| `permission` | Solid yellow |
| `working` | Solid red |
| `ready` | Solid green |

`agent` is optional. When present, it must be a string no longer than 32 bytes. It is stored and reported for diagnostics and future compatibility, but it has no effect on version-one state selection.

A valid request returns HTTP 200:

```json
{
  "state": "working",
  "agent": "codex",
  "expires_in_seconds": 120
}
```

An oversized body, malformed JSON, a missing or non-string `state`, an unsupported state, a non-string `agent`, or an oversized `agent` returns HTTP 400. Invalid requests must not change the current state or refresh its timer.

### Read status

`GET /api/status` returns HTTP 200 with the same response shape. `expires_in_seconds` is zero when the state has timed out to `ready`. After timeout, or if no agent name was supplied for the active state, `agent` is an empty string.

All unsupported paths return HTTP 404. Unsupported methods on `/api/status` return HTTP 405.

Example calls:

```sh
curl -X POST http://DEVICE_IP/api/status \
  -H 'Content-Type: application/json' \
  -d '{"state":"working","agent":"codex"}'

curl http://DEVICE_IP/api/status
```

## Firmware Structure

The project remains one Arduino sketch because the prototype is small. Its functions have four clear responsibilities:

1. Wi-Fi connection initializes networking, prints the IP address, and schedules reconnect attempts.
2. API handling validates JSON, updates state, and serializes responses.
3. Status handling stores the current enum, optional agent name, update timestamp, and expiry behavior.
4. LED control renders a solid logical state or the non-blocking Wi-Fi fault pattern.

Configuration constants at the top of the sketch contain the SSID, password, three pins, 120-second timeout, and blink interval. `ESP8266WiFi` and `ESP8266WebServer` come from the ESP8266 Arduino core; `ArduinoJson` is the only additional library.

## Error Handling

- Wi-Fi connection failure or loss: blink all LEDs and retry without blocking.
- Invalid API input: return HTTP 400 and preserve both state and timeout.
- Unknown route: return HTTP 404.
- Wrong method: return HTTP 405.
- Inactivity: return safely to green after 120 seconds.
- Reboot: forget the previous status, reconnect, then default to green.

Because the API is unauthenticated HTTP, it is intended only for a trusted local network. Anyone able to reach the device can change its state. Authentication is a future enhancement.

## Acceptance Tests

1. On startup, all LEDs blink during Wi-Fi connection; after connection, Serial prints the IP and green is solid.
2. Each accepted state activates only its specified LED.
3. Both `blocked` and `permission` activate yellow; `working` activates red.
4. A newer valid update replaces the previous state, regardless of the optional agent name.
5. Malformed JSON and invalid fields return HTTP 400 and leave the display and expiry unchanged.
6. `GET /api/status` reports the displayed logical state, stored agent name, and correct remaining lifetime.
7. After 120 seconds without a valid update, the light returns to green and reports zero seconds remaining.
8. Wi-Fi loss blinks all three LEDs; reconnection restores a still-fresh state or green if it expired.
9. Unknown routes and unsupported methods return HTTP 404 and 405 respectively.
10. The sketch compiles for a Wemos D1 Mini using the ESP8266 Arduino core and its documented ArduinoJson dependency.

## Future Version

A later version can make `agent` required and replace the single status record with a bounded per-agent registry. Each agent will then have its own state and expiry time, and the visible state will prioritize yellow (attention required) over red (working) over green (ready). A local desktop bridge can later observe supported AI-agent tools and send heartbeats automatically without changing the physical wiring.
