# AI Agent Traffic Light: Quick Build Guide

This guide shows the quickest way to build the Wi-Fi status light using the Arduino firmware and 3D-print files already in this repository.

## What it shows

| Agent state | Light |
|---|---|
| `working` | Red |
| `blocked` or `permission` | Yellow |
| `ready` | Green |

## How the indicator works

1. **The ESP8266 connects to Wi-Fi.** While it is disconnected or reconnecting outside setup, all three LEDs blink together at half-second intervals. The device retries the Wi-Fi connection every 10 seconds. During setup, the LEDs instead cycle red, yellow, then green at half-second intervals.

2. **A steady status light means Wi-Fi is connected.** There is no separate Wi-Fi LED. Once connected, the three-LED blinking stops and one LED stays on to show the current agent state. The numeric device URL is also printed in Serial Monitor.

3. **An AI agent reports its state.** The agent sends a JSON `POST` request to `/api/status` containing `working`, `blocked`, `permission`, or `ready`. The optional `agent` name identifies who sent the update; it is returned by the API but does not change the selected color.

4. **The latest valid update controls the light.** This device uses last-writer-wins behavior, so if several agents report to the same light, the most recent valid report is displayed.

5. **Old states expire automatically.** Every valid report restarts a 120-second timer. If no agent sends another update in that time, the device clears the agent name and returns to green (`ready`). A long-running agent should refresh `working` about once every 60 seconds.

If Wi-Fi drops, blinking all three LEDs temporarily overrides the status color. The status timer continues running while disconnected; after reconnection, the current unexpired state reappears, or green appears if the state expired.

You can read the current state, reporting agent, and remaining time with:

```sh
curl http://DEVICE_IP/api/status
```

## Parts to buy

| Quantity | Part | Notes |
|---:|---|---|
| 1 | Wemos D1 Mini ESP8266 board | A compatible D1 Mini clone is also suitable. |
| 1 each | Red, yellow, and green 8 mm round LEDs | Standard low-current LEDs, preferably diffused. The case holes are 8.2 mm. |
| 3 | 330 ohm, 1/4 W resistors | One resistor is required for each LED. |
| 1 optional | Momentary normally-open push button | Connects D6/GPIO12 to GND for on-demand Wi-Fi setup. |
| About 1 m | Thin insulated hookup wire | 22-26 AWG stranded wire is easy to arrange inside the case. |
| 1 | Data-capable Micro-USB cable | Needed to program and power the board. |
| As needed | Solder | Electronics solder suitable for small circuit boards. |
| As needed | Heat-shrink tubing or electrical tape | Insulates the LED and resistor connections. |
| As needed | PETG or PLA filament | The supplied Ender-3 V3 SE G-code was prepared for 1.75 mm PETG. |

No separate power supply is needed for the basic build; power the Wemos through USB.

## Tools

- 3D printer
- Soldering iron and stand
- Wire cutters and wire strippers
- Helping hands or a small clamp
- Computer with Arduino IDE
- Optional multimeter for checking polarity and connections

> **Soldering safety:** Work in a ventilated area, keep the iron in its stand, and disconnect USB power before soldering. Avoid holding the iron on an LED leg or board pad for more than a few seconds.

## Files already provided

- Arduino firmware: [`ai-agent-indicator/ai-agent-indicator.ino`](ai-agent-indicator/ai-agent-indicator.ino)
- Firmware and API details: [`ai-agent-indicator/README.md`](ai-agent-indicator/README.md)
- Reusable agent reporter: [`ai-agent-indicator/traffic-light.sh`](ai-agent-indicator/traffic-light.sh)
- Agent instruction template: [`ai-agent-indicator/AGENTS.example.md`](ai-agent-indicator/AGENTS.example.md)
- Detailed Codex hook setup: [`ai-agent-indicator/docs/agent-instructions/setup-codex-traffic-light-hooks.md`](ai-agent-indicator/docs/agent-instructions/setup-codex-traffic-light-hooks.md)
- Printable body: [`freecad-case/Case-Body.stl`](freecad-case/Case-Body.stl)
- Printable cover: [`freecad-case/Case-Cover.stl`](freecad-case/Case-Cover.stl)
- Ender-3 V3 SE pre-sliced PETG body: [`freecad-case/Case-Body_PETG_52m30s.gcode`](freecad-case/Case-Body_PETG_52m30s.gcode)
- Ender-3 V3 SE pre-sliced PETG cover: [`freecad-case/Case-Cover_PETG_12m40s.gcode`](freecad-case/Case-Cover_PETG_12m40s.gcode)
- Editable FreeCAD model: [`freecad-case/Case.FCStd`](freecad-case/Case.FCStd)

The supplied G-code was generated in Creality Print for a **Creality Ender-3 V3 SE with a 0.4 mm nozzle** and an eSun PETG profile. It uses 1.75 mm PETG, 0.2 mm layers, a 245 °C nozzle, and an 85 °C bed. **G-code is printer-specific:** before printing, confirm that your machine is an Ender-3 V3 SE with the expected nozzle, bed, start commands, temperatures, and material. If anything differs, slice the two STL files using your own printer profile.

## Wiring

Each LED needs its own 330 ohm resistor.

| LED | Wemos pin | ESP8266 GPIO | Connection |
|---|---|---|---|
| Red | D1 | GPIO5 | D1 -> 330 ohm resistor -> LED anode (+) |
| Yellow | D2 | GPIO4 | D2 -> 330 ohm resistor -> LED anode (+) |
| Green | D5 | GPIO14 | D5 -> 330 ohm resistor -> LED anode (+) |
| Optional setup button | D6 | GPIO12 | Momentary normally-open button from D6 to GND |

Connect all three LED cathodes (-) to a `GND` pin on the Wemos.

The longer LED leg is normally the anode. The shorter leg, next to the flat side of the LED body, is normally the cathode. Check the LED datasheet or use a multimeter if the legs have already been cut.

```text
D1 ---- 330 ohm ---- red LED anode
D2 ---- 330 ohm ---- yellow LED anode
D5 ---- 330 ohm ---- green LED anode
GND ---------------- all three LED cathodes
D6 ----------------- optional momentary button ---------------- GND
```

The resistor may be placed on either side of its LED as long as it remains in series. Keep bare connections separated and insulated.

Hold the optional setup button for three seconds to open the Wi-Fi wizard; a
short press does nothing. The current enclosure has no guaranteed button
opening. The user will update the enclosure design if this option is fitted.

## Quick assembly

1. **Print the enclosure.** For an Ender-3 V3 SE with a 0.4 mm nozzle and compatible PETG, you can use the supplied G-code after checking its settings. It takes approximately 53 minutes for the body and 13 minutes for the cover. For any other setup, slice `Case-Body.stl` and `Case-Cover.stl` with the correct printer and filament profile.

2. **Dry-fit everything before soldering.** Check that the Wemos, USB opening, cover, and three 8 mm LEDs fit. Place the LEDs in traffic-light order: red, yellow, then green. Do not force an LED into an undersized or rough opening.

3. **Prepare the LEDs.** Cut short lengths of hookup wire. Mark the common ground wires or use black wire so they cannot be confused with the three signal wires.

4. **Solder the circuit.** Connect each signal pin through its own 330 ohm resistor to the matching LED anode. Join all three cathodes to `GND`. Use heat-shrink tubing or electrical tape so exposed leads cannot touch.

5. **Install Arduino support.** In Arduino IDE, install **ESP8266 Arduino core 3.1.2**, **WiFiManager 2.0.17**, and **ArduinoJson 6.21.6**. Select **LOLIN(WEMOS) D1 R2 & mini** and the correct USB port.

6. **Upload and configure the firmware.** Open `ai-agent-indicator/ai-agent-indicator.ino` and upload the sketch. On first boot, join the open `AI-Agent-Light-Setup-XXXX` Wi-Fi network. Wait for the captive page, or browse to `http://192.168.4.1`. Select a scanned 2.4 GHz network or type a hidden SSID, enter its password and a friendly device name, then submit. The device saves the settings and restarts once; reconnect the computer or phone to the normal LAN.

7. **Find the device address.** Open Serial Monitor at **115200 baud**. After connection, note the numeric URL printed as `http://DEVICE_IP`.

8. **Test before closing the case.** Replace `DEVICE_IP` below with the numeric IP address:

   ```sh
   curl -X POST http://DEVICE_IP/api/status \
     -H 'Content-Type: application/json' \
     -d '{"state":"working","agent":"codex"}'
   ```

   The red LED should turn on. Repeat with `blocked` or `permission` for yellow and `ready` for green.

9. **Finish the assembly.** Disconnect USB power, arrange the wires so the cover cannot pinch them, secure loose parts if necessary, and install the cover. Reconnect USB power and run the test once more.

## Wi-Fi setup and recovery behavior

Without saved credentials, setup opens immediately. With saved credentials,
the device first tries to reconnect for five minutes while all LEDs blink
together at 500 ms intervals. If it cannot connect, the open setup AP appears
for five minutes and the LEDs cycle red → yellow → green at 500 ms per
color. If setup expires without a working submission, the device returns to
connection attempts and repeats this automatic cycle. Connected agent colors
remain unchanged.

Stock WiFiManager saves each submitted network immediately. A wrong password
therefore replaces the previously saved credentials. Correct it before the
active portal closes, wait five minutes for setup to return, press reset twice
within ten seconds, or hold D6/GPIO12 to GND for three seconds. Waiting longer
than ten seconds between resets does not count. Double-reset detection uses RTC
memory and is not guaranteed across a power loss.

The setup AP is intentionally open and unauthenticated, so nearby users can
reach normal setup during its five-minute window. Hidden reboot,
credential-erase, and OTA update routes are disabled, but normal setup remains
available without authentication.

## Connect AI agents

The included [`traffic-light.sh`](ai-agent-indicator/traffic-light.sh) reporter accepts these commands:

```sh
bash ai-agent-indicator/traffic-light.sh working
bash ai-agent-indicator/traffic-light.sh blocked
bash ai-agent-indicator/traffic-light.sh permission
bash ai-agent-indicator/traffic-light.sh ready
```

`working` posts immediately and starts a background refresh every 60 seconds. Any other state stops that refresher before posting, so an old `working` process cannot overwrite the newer state. The included copy reports as agent `claude`.

Before using the script, open it and check these values:

```sh
ENDPOINT='http://ai-agent-status.local/api/status'
AGENT='claude'
```

Change `AGENT` for each copy, such as `codex`, `claude`, or another short name. The firmware advertises a DHCP hostname but does not provide mDNS. If your router does not resolve `ai-agent-status.local`, change `ENDPOINT` to the numeric address shown in Serial Monitor, for example `http://192.168.1.42/api/status`.

The reporter uses Bash, `curl`, `setsid`, and `pgrep`, so it is intended for Linux or WSL. Requests are best-effort: a temporary network failure does not fail the agent's task.

### Automatic Codex hooks

The checked-in setup was verified with Codex CLI 0.154.0. Hook support and event names can change, so inspect the installed Codex hook screen before trusting the configuration. The [detailed Codex setup note](ai-agent-indicator/docs/agent-instructions/setup-codex-traffic-light-hooks.md) includes the design constraints and verification procedure.

1. Copy the reporter into the Codex hooks directory and make it executable:

   ```sh
   mkdir -p ~/.codex/hooks
   cp ai-agent-indicator/traffic-light.sh ~/.codex/hooks/traffic-light.sh
   chmod +x ~/.codex/hooks/traffic-light.sh
   ```

2. Edit `~/.codex/hooks/traffic-light.sh`: change `AGENT='claude'` to `AGENT='codex'`, update `ENDPOINT` if needed, and change the invalid-input branch from `exit 64` to `exit 0` so a bad hook call can never interrupt Codex.

3. Create or merge `~/.codex/hooks.json` with this lifecycle mapping. If that file already exists, preserve its other hooks instead of replacing them.

   ```json
   {
     "description": "Global AI Agent Traffic Light lifecycle reporting for Codex.",
     "hooks": {
       "UserPromptSubmit": [{
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh working",
           "timeout": 6
         }]
       }],
       "PermissionRequest": [{
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh permission",
           "timeout": 6
         }]
       }],
       "PreToolUse": [{
         "matcher": "^request_user_input$",
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh blocked",
           "timeout": 6
         }]
       }],
       "PostToolUse": [{
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh working",
           "timeout": 6
         }]
       }],
       "Stop": [{
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh ready",
           "timeout": 6
         }]
       }],
       "Interrupt": [{
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh ready",
           "timeout": 3
         }]
       }],
       "SessionEnd": [{
         "hooks": [{
           "type": "command",
           "command": "/home/YOUR_USER/.codex/hooks/traffic-light.sh ready",
           "timeout": 3
         }]
       }]
     }
   }
   ```

4. Replace every `/home/YOUR_USER` with the absolute home path returned by `printf '%s\n' "$HOME"`. Keep `PostToolUse` synchronous so it cannot finish after `Stop` and incorrectly return the light to red.

5. Restart Codex, open `/hooks`, review every command and hash, and trust the configuration. Do not bypass hook trust.

6. Start a disposable task and confirm the sequence: submitting work shows red, an approval request shows yellow, a structured question shows yellow, and finishing or cancelling shows green.

### Other hook-capable agents

Copy the reporter for the agent, change its `AGENT` value, and connect lifecycle events to the same four arguments:

| Agent event | Reporter argument | Light |
|---|---|---|
| A task or tool starts/resumes | `working` | Red |
| Work cannot continue without guidance | `blocked` | Yellow |
| User approval is required | `permission` | Yellow |
| The turn finishes or is cancelled | `ready` | Green |

Use an absolute script path in hook configuration. Run the non-`working` hook before ending a turn so it revokes the background refresher. If an agent has no hook system, place the commands from [`AGENTS.example.md`](ai-agent-indicator/AGENTS.example.md) in that project's agent instructions and change the agent name as needed.

## Quick troubleshooting

| Problem | What to check |
|---|---|
| One LED does not light | Check its polarity, solder joints, 330 ohm resistor, and assigned Wemos pin. |
| All LEDs blink together | The device is in its five-minute router reconnect grace period. Check that the router and saved 2.4 GHz network are available; setup opens only after the grace period. |
| Wrong Wi-Fi password was saved | While the portal is active, submit the corrected password. Otherwise wait for the automatic five-minute setup window, double reset within ten seconds, or hold D6/GPIO12 to GND for three seconds. |
| Hidden Wi-Fi network is missing | Open the portal's Wi-Fi form and type the hidden 2.4 GHz SSID manually instead of selecting a scan result. |
| Setup portal does not open automatically | While joined to `AI-Agent-Light-Setup-XXXX`, browse directly to `http://192.168.4.1`. |
| Double reset does not open setup | Make sure both reset presses occur within ten seconds. Detection is not guaranteed if power is removed between resets; wait for automatic setup or use the D6 button. |
| Firmware will not upload | Use a data-capable USB cable and verify the selected board, USB port, ESP8266 Arduino core 3.1.2, WiFiManager 2.0.17, and ArduinoJson 6.21.6. |
| An LED does not fit | Remove print debris or lightly clean the opening. Do not force it. The holes are 8.2 mm for 8 mm LED bodies; resize and reprint from `Case.FCStd` if your LEDs differ. |
| The API call fails | Use the numeric IP from Serial Monitor and make sure the computer and ESP8266 are on the same local network. |

For all API commands and the full device behavior, see the [firmware README](ai-agent-indicator/README.md).
