# AI Agent Traffic Light: Quick Build Guide

This guide shows the quickest way to build the Wi-Fi status light using the Arduino firmware and 3D-print files already in this repository.

## What it shows

| Agent state | Light |
|---|---|
| `working` | Red |
| `blocked` or `permission` | Yellow |
| `ready` | Green |

The light returns to green after 120 seconds without an update. If all three LEDs blink together, the ESP8266 is trying to connect to Wi-Fi.

## Parts to buy

| Quantity | Part | Notes |
|---:|---|---|
| 1 | Wemos D1 Mini ESP8266 board | A compatible D1 Mini clone is also suitable. |
| 1 each | Red, yellow, and green 8 mm round LEDs | Standard low-current LEDs, preferably diffused. The case holes are 8.2 mm. |
| 3 | 330 ohm, 1/4 W resistors | One resistor is required for each LED. |
| About 1 m | Thin insulated hookup wire | 22-26 AWG stranded wire is easy to arrange inside the case. |
| 1 | Data-capable Micro-USB cable | Needed to program and power the board. |
| As needed | Solder | Electronics solder suitable for small circuit boards. |
| As needed | Heat-shrink tubing or electrical tape | Insulates the LED and resistor connections. |
| As needed | PETG or PLA filament | The supplied G-code was prepared for 1.75 mm PETG. |

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
- Printable body: [`freecad-case/Case-Body.stl`](freecad-case/Case-Body.stl)
- Printable cover: [`freecad-case/Case-Cover.stl`](freecad-case/Case-Cover.stl)
- Pre-sliced PETG body: [`freecad-case/Case-Body_PETG_52m30s.gcode`](freecad-case/Case-Body_PETG_52m30s.gcode)
- Pre-sliced PETG cover: [`freecad-case/Case-Cover_PETG_12m40s.gcode`](freecad-case/Case-Cover_PETG_12m40s.gcode)
- Editable FreeCAD model: [`freecad-case/Case.FCStd`](freecad-case/Case.FCStd)

The supplied G-code uses 1.75 mm PETG, 0.2 mm layers, a 245 °C nozzle, and an 85 °C bed. **G-code is printer-specific:** only use these files after confirming that the printer model, bed size, start commands, temperatures, and material are compatible. The safest option is to slice the two STL files for your own printer.

## Wiring

Each LED needs its own 330 ohm resistor.

| LED | Wemos pin | ESP8266 GPIO | Connection |
|---|---|---|---|
| Red | D1 | GPIO5 | D1 -> 330 ohm resistor -> LED anode (+) |
| Yellow | D2 | GPIO4 | D2 -> 330 ohm resistor -> LED anode (+) |
| Green | D5 | GPIO14 | D5 -> 330 ohm resistor -> LED anode (+) |

Connect all three LED cathodes (-) to a `GND` pin on the Wemos.

The longer LED leg is normally the anode. The shorter leg, next to the flat side of the LED body, is normally the cathode. Check the LED datasheet or use a multimeter if the legs have already been cut.

```text
D1 ---- 330 ohm ---- red LED anode
D2 ---- 330 ohm ---- yellow LED anode
D5 ---- 330 ohm ---- green LED anode
GND ---------------- all three LED cathodes
```

The resistor may be placed on either side of its LED as long as it remains in series. Keep bare connections separated and insulated.

## Quick assembly

1. **Print the enclosure.** Slice `Case-Body.stl` and `Case-Cover.stl` for your printer. The included PETG G-code takes approximately 53 minutes for the body and 13 minutes for the cover, but only use it with a compatible printer.

2. **Dry-fit everything before soldering.** Check that the Wemos, USB opening, cover, and three 8 mm LEDs fit. Place the LEDs in traffic-light order: red, yellow, then green. Do not force an LED into an undersized or rough opening.

3. **Prepare the LEDs.** Cut short lengths of hookup wire. Mark the common ground wires or use black wire so they cannot be confused with the three signal wires.

4. **Solder the circuit.** Connect each signal pin through its own 330 ohm resistor to the matching LED anode. Join all three cathodes to `GND`. Use heat-shrink tubing or electrical tape so exposed leads cannot touch.

5. **Install Arduino support.** In Arduino IDE, install the ESP8266 board package and the `ArduinoJson` library. Select **LOLIN(WEMOS) D1 R2 & mini** and the correct USB port.

6. **Configure and upload the firmware.** Open `ai-agent-indicator/ai-agent-indicator.ino`, set `WIFI_SSID` and `WIFI_PASSWORD` for the local 2.4 GHz Wi-Fi network, and upload the sketch. Do not commit real Wi-Fi credentials to Git.

7. **Find the device address.** Open Serial Monitor at **115200 baud**. After connection, note the numeric URL printed as `http://DEVICE_IP`.

8. **Test before closing the case.** Replace `DEVICE_IP` below with the numeric IP address:

   ```sh
   curl -X POST http://DEVICE_IP/api/status \
     -H 'Content-Type: application/json' \
     -d '{"state":"working","agent":"codex"}'
   ```

   The red LED should turn on. Repeat with `blocked` or `permission` for yellow and `ready` for green.

9. **Finish the assembly.** Disconnect USB power, arrange the wires so the cover cannot pinch them, secure loose parts if necessary, and install the cover. Reconnect USB power and run the test once more.

## Quick troubleshooting

| Problem | What to check |
|---|---|
| One LED does not light | Check its polarity, solder joints, 330 ohm resistor, and assigned Wemos pin. |
| All LEDs blink together | Check the Wi-Fi name and password and confirm that a 2.4 GHz network is available. |
| Firmware will not upload | Use a data-capable USB cable and verify the selected board, USB port, ESP8266 board package, and ArduinoJson library. |
| An LED does not fit | Remove print debris or lightly clean the opening. Do not force it. The holes are 8.2 mm for 8 mm LED bodies; resize and reprint from `Case.FCStd` if your LEDs differ. |
| The API call fails | Use the numeric IP from Serial Monitor and make sure the computer and ESP8266 are on the same local network. |

For all API commands and the full device behavior, see the [firmware README](ai-agent-indicator/README.md).
