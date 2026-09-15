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
