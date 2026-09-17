# Quick Build Guide Design

## Goal

Add a root-level `QUICK_BUILD_GUIDE.md` that lets a beginner assemble the AI Agent Traffic Light quickly using the firmware and enclosure files already in this repository.

## Audience and scope

The guide is for someone comfortable with basic soldering but unfamiliar with this project. It will cover the complete physical build, firmware upload, enclosure assembly, and a simple API test without duplicating the full firmware documentation.

## Structure

The guide will contain:

1. A short description of the finished device and its status colors.
2. A shopping list with quantities and useful specifications.
3. A tools list and brief soldering safety note.
4. Direct links to the existing Arduino sketch, STL files, ready-to-print G-code, and editable FreeCAD source.
5. A wiring table for the three LEDs, their 330 ohm resistors, and the Wemos D1 Mini pins.
6. Numbered instructions for printing, dry-fitting, soldering, uploading, assembling, and testing.
7. A compact troubleshooting section for reversed LEDs, Wi-Fi failure, and a poor enclosure fit.

## Hardware assumptions

- One ESP8266-based Wemos D1 Mini board.
- Three separate 8 mm round LEDs: red, yellow, and green.
- The FreeCAD model's LED openings are 8.2 mm in diameter.
- One 330 ohm resistor is used in series with each LED.
- The LED anodes connect through their resistors to D1/GPIO5, D2/GPIO4, and D5/GPIO14. All LED cathodes connect to GND.
- The board is powered through USB; no separate power supply is required for the basic build.

## Repository assets

- Firmware: `ai-agent-indicator/ai-agent-indicator.ino`
- Body STL: `freecad-case/Case-Body.stl`
- Cover STL: `freecad-case/Case-Cover.stl`
- Ready-to-print PETG G-code: `freecad-case/Case-Body_PETG_52m30s.gcode` and `freecad-case/Case-Cover_PETG_12m40s.gcode`
- Editable model: `freecad-case/Case.FCStd`

The guide will warn that supplied G-code is printer-specific and should only be used after checking printer compatibility; otherwise, the user should slice the STL files for their own printer.

## Success criteria

- A reader can identify and buy every required component.
- The LED size and pin connections agree with the current FreeCAD model and firmware.
- Every referenced repository path exists.
- The main build procedure is short, ordered, and usable without searching other files.
- The guide includes a working `curl` test and points readers to the existing README for API details.
