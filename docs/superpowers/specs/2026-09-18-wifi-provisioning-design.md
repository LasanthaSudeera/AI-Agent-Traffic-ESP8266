# Wi-Fi Provisioning Wizard Design

## Goal

Replace firmware-baked Wi-Fi credentials with an on-device captive-portal wizard. A first-time user should be able to power the ESP8266, join its temporary setup network, select a nearby 2.4 GHz network, enter the password, choose a device name, and then use the existing agent-status API without editing the sketch.

## Scope

This change covers ESP8266 firmware, optional setup-button wiring, dependency instructions, setup documentation, and verification. It preserves the existing status API, agent hooks, state-expiry behavior, and LED-to-agent-state mapping.

FreeCAD, STL, and G-code changes are excluded. The user will update the enclosure for the optional button.

## Selected approach

Use:

- WiFiManager 2.0.17 for network scanning and the captive portal;
- a project-owned `DoubleResetTrigger` using the ESP8266 core's RTC user-memory API;
- a small project-owned `WifiProvisioning` component to coordinate connection attempts, portal lifetime, configuration persistence, button input, and provisioning state.

This was selected over a custom captive portal because it provides the standard network-selection flow with substantially less project-owned networking code. The small project-owned reset detector avoids relying on an archived or inactive library while using the ESP8266 core's supported RTC-memory interface directly.

## Component boundaries

### `DoubleResetTrigger`

This component depends only on the ESP8266 Arduino core. On any boot without an existing marker, it writes a project-specific 32-bit magic value to RTC user-memory block 64 and opens a ten-second reset window. A second boot that sees the marker clears it immediately and reports a double reset. If no second reset occurs, a non-blocking `process()` call clears the marker after ten seconds.

RTC block 64 is reserved exclusively for this component. It is above the first 32 blocks that the ESP8266 core may use during OTA, and all reads and writes use the required four-byte alignment. Read or write failure is logged and treated as no double reset.

The trigger is intended for pressing the board's reset button twice while the board remains powered. RTC memory may be lost on a full power cycle, so rapid unplug-and-replug sequences are not guaranteed to trigger setup. Automatic setup and the D6 button remain available as reliable fallbacks.

### `WifiProvisioning`

This component owns:

- loading and saving project settings in LittleFS;
- detecting whether Wi-Fi credentials already exist;
- attempting station-mode connections;
- starting, processing, and stopping WiFiManager's non-blocking portal;
- using `DoubleResetTrigger` to detect the manual reset sequence;
- debouncing and timing the optional setup button;
- exposing the current connectivity/provisioning state to the main sketch;
- applying the configured DHCP hostname before station connection.

Its public interface must let the sketch initialize provisioning, process it once per loop, query whether Wi-Fi is connected, and query whether the setup portal is active. Networking-library details must not leak into the status API or LED-state code.

### Main sketch

The main sketch continues to own:

- the `/api/status` routes and HTTP responses;
- agent state, agent name, and the 120-second expiry timer;
- the three LEDs and their precedence rules;
- starting and stopping the status server when provisioning changes ownership of port 80.

The main loop must remain non-blocking so provisioning, LED animation, status expiry, button timing, and HTTP handling continue to advance.

## Configuration data

Wi-Fi credentials use the ESP8266/WiFiManager credential mechanism. The project's `/config.json` LittleFS settings file stores only:

- a configuration schema version; and
- the friendly device name.

The project settings file and status API must never contain or return the Wi-Fi password.

The friendly name defaults to `AI-Agent-Indicator`. The wizard accepts 1 to 32 ASCII characters from letters, numbers, spaces, underscores, and hyphens. Leading and trailing whitespace is removed. A DHCP-safe hostname is derived by replacing spaces and underscores with hyphens, collapsing repeated hyphens, and removing leading or trailing hyphens. Invalid wizard input is rejected. Invalid stored data or a derived empty hostname falls back to the default.

A missing, corrupt, or unreadable settings file falls back to the default name and logs a diagnostic over Serial. Failure to save the friendly name must not prevent Wi-Fi provisioning.

## Connection and setup flow

### First boot

If the device has no saved Wi-Fi credentials, it opens the setup portal immediately.

### Normal boot and reconnection

If saved credentials exist, the device attempts to connect for five minutes. This grace period also applies when an established connection drops, allowing a router time to reboot without forcing the user into setup.

While disconnected, the device retries the saved network without blocking the main loop. If it remains disconnected for five continuous minutes, it opens the setup portal for five minutes.

If the portal times out without a successful configuration, it closes and starts another five-minute saved-network connection cycle. Existing credentials and the friendly name remain unchanged.

If no previous credentials exist, a timed-out portal stays closed for a five-minute cooldown while the disconnected LED pattern continues, then opens again. This prevents a brand-new device from broadcasting an open setup network indefinitely.

### Manual setup triggers

Either manual trigger opens the setup portal immediately, without waiting for the connection grace period:

- reset the board twice within ten seconds; or
- hold an optional momentary button connected between D6/GPIO12 and GND for three seconds.

The button uses the ESP8266 internal pull-up and is debounced in software. A short press has no effect. Manual setup does not erase the current credentials. "Double reset" means pressing the board's reset button twice while it remains powered; it does not promise detection across full power removal.

### Captive portal

The device creates an open setup network named `AI-Agent-Light-Setup-XXXX`, where `XXXX` is the final four uppercase hexadecimal characters of the ESP8266 chip ID. The portal is normally available at `192.168.4.1` if a phone or computer does not open it automatically.

The portal:

- remains open for no more than five minutes;
- lists nearby 2.4 GHz networks;
- permits manual SSID entry for hidden networks;
- asks for the Wi-Fi password and friendly device name;
- shows a save confirmation before restarting after success.

Candidate credentials and the candidate friendly name are staged in RAM. The last working Wi-Fi configuration is also held in RAM for the duration of the portal and must not be logged or exposed. Candidate values replace the saved values only after the candidate network connects successfully. A failed candidate keeps the portal available for correction. A portal timeout restores the prior configuration and resumes the saved-network connection cycle.

The hotspot is intentionally open. Documentation must warn that anyone nearby can access setup during its five-minute window. No setup password or additional web authentication is included in this version.

## HTTP server ownership

WiFiManager's captive portal and the existing status API both require port 80. They must never run at the same time.

Before opening setup, the firmware stops the status server. After the portal closes and station Wi-Fi connects, the firmware starts the status server and makes the existing API available. During an ordinary connection loss, the status server may remain initialized until setup begins, although it is unreachable without a network connection.

The `/api/status` request and response formats remain unchanged.

## LED behavior

LED display priority is:

1. setup portal;
2. disconnected or reconnecting;
3. connected agent state.

While the setup portal is active, one LED at a time cycles red, yellow, then green at a fixed 500 ms step. While disconnected and attempting reconnection, all three LEDs blink together at the existing 500 ms interval. Once connected, a steady LED displays the existing mapping:

| Agent state | LED |
|---|---|
| `working` | Red |
| `blocked` or `permission` | Yellow |
| `ready` or expired | Green |

The 120-second agent-status expiry timer continues during setup and disconnection. After reconnection, the device displays the current unexpired agent state or green if it expired.

## Error handling and recovery

- An invalid friendly name is rejected in the wizard with a correction message.
- Failed Wi-Fi credentials keep the portal active until corrected or timed out.
- A portal timeout preserves the prior working configuration.
- A LittleFS mount or read failure uses the default name, logs the error, and still permits every provisioning trigger; the RTC-based double-reset detector is independent of LittleFS.
- A LittleFS write failure reports the problem over Serial; successfully connected Wi-Fi remains usable with the default name.
- An RTC-memory read or write failure is logged and disables only that double-reset attempt; automatic setup and the D6 button continue to work.
- The setup portal is stopped before the status API is started, preventing port conflicts.
- Timing uses rollover-safe `millis()` arithmetic and contains no blocking five-minute delays.

## Dependencies and compatibility

The pinned build baseline is:

- ESP8266 Arduino core 3.1.2;
- WiFiManager 2.0.17;
- the existing ArduinoJson dependency;
- LittleFS supplied by the ESP8266 core.

No external double-reset library is required. Changing the pinned ESP8266 core requires rerunning the compile, RTC-memory, and physical trigger checks before updating the documented baseline.

## Verification

### Build checks

- Compile the firmware for `LOLIN(WEMOS) D1 R2 & mini` using the pinned library versions.
- Run the existing API smoke test after the device is connected.
- Confirm no real SSID or password is required in, or committed to, the sketch.
- Confirm the build has no external reset-detector dependency.

### Physical acceptance checks

1. Erase saved Wi-Fi data and confirm first boot opens setup immediately.
2. Confirm the portal lists nearby networks and accepts a manually entered hidden SSID.
3. Save a valid network and friendly name, then confirm restart, DHCP hostname, API availability, and persistence over another power cycle.
4. Restart the router and confirm the device retries for five minutes without losing settings or opening setup early.
5. Leave Wi-Fi unavailable for five minutes and confirm the portal opens.
6. Let the portal time out for five minutes and confirm it returns to the prior saved-network retry cycle.
7. Enter invalid credentials and confirm they can be corrected without losing the prior working configuration.
8. Confirm two reset-button presses within ten seconds open setup, while a second press after ten seconds does not.
9. Confirm a three-second D6/GPIO12-to-GND button hold opens setup and a short press does not.
10. Confirm setup, reconnection, and each agent-state LED pattern.
11. Confirm the 120-second status expiry continues while setup or disconnection is active.
12. Corrupt or remove the settings file and confirm safe fallback to `AI-Agent-Indicator`.

## Documentation changes

Update `QUICK_BUILD_GUIDE.md` and `ai-agent-indicator/README.md` to cover:

- WiFiManager installation and the pinned ESP8266 core version;
- first-time setup, network selection, hidden SSIDs, and friendly naming;
- the two five-minute phases: connection grace and portal lifetime;
- reset-button double-reset recovery and its power-cycle limitation;
- optional momentary-button wiring from D6/GPIO12 to GND;
- setup, disconnected, and agent-state LED meanings;
- the open-hotspot security warning;
- recovery from a wrong password or unavailable router;
- removal of instructions to bake credentials into the sketch.

The hook scripts and hook instructions retain their existing API contract and require no behavioral changes.

## Success criteria

- A new user can configure Wi-Fi from a phone or computer without editing firmware.
- Available 2.4 GHz networks are visible in the wizard, with a manual path for hidden networks.
- Router outages up to five minutes do not trigger setup.
- Automatic and manual setup entry both work.
- Previous working credentials survive failed or abandoned setup.
- The status API, agent hooks, expiry behavior, and LED mapping work unchanged after connection.
- No Wi-Fi password appears in project settings, API responses, documentation examples, or committed firmware.
