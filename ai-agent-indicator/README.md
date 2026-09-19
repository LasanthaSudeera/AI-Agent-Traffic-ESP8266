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

An optional setup button provides a direct way to reopen the Wi-Fi wizard:

| Purpose | Wemos pin | ESP8266 GPIO | Connection |
|---|---|---|---|
| Optional setup button | D6 | GPIO12 | Momentary normally-open button from D6 to GND |

Hold the button for three seconds to open setup. A short press does nothing.
The current enclosure has no guaranteed opening for this button; the user will
update the enclosure design if the optional button is fitted.

## Software setup

Install these pinned dependencies:

- ESP8266 Arduino core 3.1.2
- WiFiManager 2.0.17
- ArduinoJson 6.21.6
- Board: LOLIN(WEMOS) D1 R2 & mini

Upload the sketch, then configure Wi-Fi with the setup wizard:

1. Join the open Wi-Fi network `AI-Agent-Light-Setup-XXXX`, where `XXXX` is
   unique to the board. The setup AP is available for five minutes.
2. Wait for the captive page to open, or browse to `http://192.168.4.1`.
3. Select a scanned 2.4 GHz network, or type a hidden SSID, then enter its
   password and a friendly device name.
4. Submit the form. The board saves the settings and restarts once. Reconnect
   your computer or phone to the normal LAN.
5. Open Serial Monitor at 115200 baud to read the numeric API URL.

The default friendly name and DHCP hostname are `AI-Agent-Indicator`. A custom
friendly name is saved across restarts and converted into its DHCP hostname so
compatible routers show a recognizable client name. The firmware does not
provide mDNS or a `.local` URL; use the numeric IP address for API requests.

## Wi-Fi recovery

On first boot without saved credentials, the setup portal opens immediately.
On later boots, saved Wi-Fi receives a five-minute reconnect grace period. If
the connection remains unavailable, the setup portal opens for five minutes.
If no working settings are submitted, it closes and the device automatically
returns to connection attempts; the five-minute reconnect/setup cycle repeats.

Stock WiFiManager behavior saves submitted Wi-Fi credentials immediately, so
submitting a wrong password replaces the previous saved credentials. Correct
the details while the portal is still active, or recover by any of these paths:

- wait through the five-minute reconnect grace period for setup to open again;
- press reset twice within ten seconds to open setup immediately; or
- hold the optional D6/GPIO12 button to GND for three seconds.

Waiting longer than ten seconds between resets does not trigger setup. The
double-reset marker uses RTC memory, so detection is not guaranteed across a
power loss; use the timed retry cycle or D6 button instead.

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

Before accepting provisioning on hardware, flash a board and record the result
of every item in this checklist:

1. In Arduino IDE, select **Tools → Erase Flash → Sketch + WiFi Settings** and
   upload the main firmware. Confirm first boot opens setup immediately, then
   restore **Tools → Erase Flash → Only Sketch** for later uploads so they do
   not erase saved Wi-Fi or LittleFS.
2. Confirm nearby networks appear and a hidden SSID can be entered.
3. Save valid Wi-Fi and a custom name; confirm one clean restart, no accidental
   return to setup, the DHCP hostname, API URL, and persistence across a power
   cycle.
4. Turn off the router; confirm all LEDs blink for five minutes without setup
   opening early.
5. Keep Wi-Fi unavailable; confirm the setup AP opens after five minutes.
6. Open setup without submitting; confirm it closes after five minutes and
   returns to connection attempts.
7. Submit a wrong password; confirm the portal stays available and accepts a
   correction.
8. Save unusable credentials and restart; press reset twice within ten seconds
   and confirm setup opens. Wait longer than ten seconds and confirm it does
   not count.
9. Hold D6 to GND for three seconds; confirm setup opens. Confirm a short press
   does nothing.
10. Confirm setup cycles red/yellow/green, disconnection blinks all three, and
    connected API states show the existing colors.
11. Stop every automatic reporter/background refresher first (for the included
    reporter, send `ready` and confirm its refresher has exited). Then post one
    `working` update with a direct `curl`, not `traffic-light.sh`; remain in
    setup or disconnected for over 120 seconds, reconnect, and confirm green.
12. Corrupt `/config.json` with a disposable sketch, reflash the main firmware,
    and confirm fallback to `AI-Agent-Indicator` without blocking setup. Keep
    the same **Flash Size** filesystem layout for both uploads and keep **Erase
    Flash → Only Sketch** selected so LittleFS survives. Use this test-only
    sketch, confirm Serial Monitor prints `corrupt config written`, and do not
    commit it:

    ```cpp
    #include <LittleFS.h>

    void setup() {
      Serial.begin(115200);
      LittleFS.setConfig(LittleFSConfig(false));
      if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
      }
      File file = LittleFS.open("/config.json", "w");
      if (!file || file.print("{\"schema\":999}") == 0) {
        Serial.println("corrupt config write failed");
        return;
      }
      file.close();
      File check = LittleFS.open("/config.json", "r");
      if (check && check.readString() == "{\"schema\":999}") {
        Serial.println("corrupt config written");
      } else {
        Serial.println("corrupt config verification failed");
      }
      check.close();
    }

    void loop() {}
    ```

curl -fsS -X POST -H 'Content-Type: application/json' -d '{"state":"working","agent":"claude"}'    http://ai-agent-status.local/api/status
curl -fsS -X POST -H 'Content-Type: application/json' -d '{"state":"blocked","agent":"claude"}'    http://ai-agent-status.local/api/status
curl -fsS -X POST -H 'Content-Type: application/json' -d '{"state":"permission","agent":"claude"}' http://ai-agent-status.local/api/status
curl -fsS -X POST -H 'Content-Type: application/json' -d '{"state":"ready","agent":"claude"}'      http://ai-agent-status.local/api/status

## Fault behavior and security

During setup, the LEDs cycle red → yellow → green, with each color shown
for 500 ms. While disconnected or reconnecting, all three LEDs blink together
at 500 ms intervals. Once connected, the steady agent colors described above
are unchanged.

The setup AP is open, so nearby users can access normal setup during its
five-minute window. The portal is intentionally unauthenticated. Hidden
reboot, credential-erase, and OTA update routes have been disabled, but this
does not authenticate the normal setup form. The status API also has no
authentication; use the device only on a trusted LAN.
