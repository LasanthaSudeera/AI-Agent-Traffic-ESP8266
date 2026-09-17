# Quick Build Guide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a root-level guide that enables a beginner to print, wire, program, assemble, and test the AI Agent Traffic Light quickly.

**Architecture:** Add one self-contained Markdown document and link to the repository's existing source assets rather than copying them. Verify its technical details against the current firmware, FreeCAD model, print files, and README.

**Tech Stack:** Markdown, Wemos D1 Mini/ESP8266, Arduino IDE, ArduinoJson, FreeCAD/STL/G-code, PETG or PLA, shell `curl`

## Global Constraints

- Create `QUICK_BUILD_GUIDE.md` at the repository root.
- Specify three separate 8 mm round LEDs; the FreeCAD openings are 8.2 mm in diameter.
- Use one 330 ohm series resistor for each LED.
- Wire red to D1/GPIO5, yellow to D2/GPIO4, green to D5/GPIO14, and all cathodes to GND.
- Power the basic build through the Wemos D1 Mini's USB connector.
- Link the existing firmware, STL, G-code, FreeCAD, and README files with relative paths.
- Warn readers to verify that the supplied G-code matches their printer before using it.
- Do not modify the firmware or any enclosure asset.

---

### Task 1: Create and verify the quick-build guide

**Files:**
- Create: `QUICK_BUILD_GUIDE.md`
- Reference: `ai-agent-indicator/ai-agent-indicator.ino`
- Reference: `ai-agent-indicator/README.md`
- Reference: `freecad-case/Case-Body.stl`
- Reference: `freecad-case/Case-Cover.stl`
- Reference: `freecad-case/Case-Body_PETG_52m30s.gcode`
- Reference: `freecad-case/Case-Cover_PETG_12m40s.gcode`
- Reference: `freecad-case/Case.FCStd`

**Interfaces:**
- Consumes: The firmware pin assignments and Wi-Fi/API behavior, plus the enclosure's 8.2 mm LED openings.
- Produces: A standalone Markdown build guide with working relative repository links.

- [ ] **Step 1: Create the parts, tools, and asset sections**

Write a concise introduction and status table, followed by a shopping list containing one Wemos D1 Mini ESP8266, one red/one yellow/one green 8 mm LED, three 330 ohm resistors, thin insulated hookup wire, solder, heat-shrink tubing or electrical tape, a data-capable Micro-USB cable, and PETG or PLA filament. List a soldering iron, wire cutters/strippers, helping hands, computer with Arduino IDE, and a 3D printer as tools.

Add links to every file listed under **Files**. Describe the G-code as pre-sliced for 1.75 mm PETG at 0.2 mm layers, while warning that machine-specific G-code must not be run without confirming printer compatibility.

- [ ] **Step 2: Add the wiring and soldering procedure**

Include this exact wiring map:

| LED | Wemos pin | ESP8266 GPIO | Connection |
|---|---|---|---|
| Red | D1 | GPIO5 | D1 -> 330 ohm resistor -> LED anode |
| Yellow | D2 | GPIO4 | D2 -> 330 ohm resistor -> LED anode |
| Green | D5 | GPIO14 | D5 -> 330 ohm resistor -> LED anode |

State that the longer LED leg is normally the anode, the shorter leg/flat side is the cathode, and all three cathodes connect to a Wemos GND pin. Tell readers to disconnect USB power before soldering, dry-fit components first, keep exposed conductors insulated, and avoid prolonged heat on LED legs or board pads.

- [ ] **Step 3: Add printing, programming, assembly, and testing steps**

Order the procedure as follows:

1. Print the body and cover, preferably by slicing the STL files for the user's printer.
2. Dry-fit the three 8 mm LEDs in traffic-light order: red, yellow, green.
3. Solder each LED through its own 330 ohm resistor to the correct signal pin and join the cathodes to GND.
4. Install the ESP8266 Arduino board package and ArduinoJson, select `LOLIN(WEMOS) D1 R2 & mini`, enter local Wi-Fi credentials in the sketch, and upload it.
5. Open Serial Monitor at 115200 baud and record the numeric device URL.
6. Test every state before closing the case.
7. Insulate the joints, arrange wires without pinching them, and install the cover.

Use a `curl` example with `DEVICE_IP` and `{"state":"working","agent":"codex"}`. Explain the expected red/yellow/green states and that all LEDs blinking together means Wi-Fi is disconnected.

- [ ] **Step 4: Add concise troubleshooting**

Cover these concrete cases:

- LED does not light: reverse-check its polarity, solder joints, resistor, and assigned pin.
- All LEDs blink: verify SSID/password and 2.4 GHz Wi-Fi availability.
- Upload fails: verify a data-capable USB cable, correct board, port, and required libraries.
- LED does not fit: lightly clean the print opening; do not force the LED, and resize/reprint from `Case.FCStd` if the purchased body differs from 8 mm.
- API call fails: use the numeric IP shown in Serial Monitor and keep the computer on the same LAN.

- [ ] **Step 5: Verify the document**

Run:

```bash
test -f QUICK_BUILD_GUIDE.md
rg -n '8 mm|8\.2 mm|330|D1|GPIO5|D2|GPIO4|D5|GPIO14|GND|115200|DEVICE_IP' QUICK_BUILD_GUIDE.md
rg -o '\([^)]*\)' QUICK_BUILD_GUIDE.md
git diff --check -- QUICK_BUILD_GUIDE.md
```

Expected: the file exists; the required dimensions, resistor value, pins, baud rate, and test placeholder appear; every local link resolves to an existing repository file when its parentheses are removed; and `git diff --check` prints no errors.

- [ ] **Step 6: Commit only the guide**

```bash
git add QUICK_BUILD_GUIDE.md
git commit -m "docs: add quick build guide"
```

Do not stage the pre-existing modification to `ai-agent-indicator/ai-agent-indicator.ino`.
