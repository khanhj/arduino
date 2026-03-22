# ESP8266 Menu UI — 5-Button OLED Interface

## Hardware

- **Board:** NodeMCU ESP8266 (v2)
- **Display:** SH1106 1.3" I2C OLED (128x64)
- **Buttons:** 5x momentary push buttons

## Wiring Diagram

```
                    NodeMCU ESP8266
                  ┌─────────────────┐
                  │      USB        │
                  │    ┌─────┐      │
                  │    └─────┘      │
                  │                 │
          3V3  ●──┤ 3V3        D0  ├──● BTN_CTRL ──┐
               ●──┤ GND        D1  ├──● OLED SCL   │
                  │ D1(SCL)    D2  ├──● OLED SDA   │
                  │ D2(SDA)    D3  ├──● BTN_BACK ──┤
                  │ D3         D4  ├──○ (LED)      │
                  │ D4         D5  ├──● BTN_UP ────┤
                  │ D5         D6  ├──● BTN_DOWN ──┤
                  │ D6         D7  ├──● BTN_SEL ───┤
                  │ D7         D8  ├──○ (avoid)    │
                  │                 │               │
                  └─────────────────┘               │
                                                   GND

    ┌──────────────────────────────────────────────────┐
    │  All buttons: one leg to GPIO pin, other to GND  │
    └──────────────────────────────────────────────────┘
```

### OLED Display (I2C)

| OLED Pin | NodeMCU Pin |
|----------|-------------|
| VDD      | 3V3         |
| GND      | GND         |
| SCK      | D1 (GPIO5)  |
| SDA      | D2 (GPIO4)  |

### Buttons (all wired to GND)

| Button   | NodeMCU Pin | GPIO   | Pull-up          |
|----------|-------------|--------|------------------|
| Up       | D5          | GPIO14 | Internal         |
| Down     | D6          | GPIO12 | Internal         |
| Select   | D7          | GPIO13 | Internal         |
| Back     | D3          | GPIO0  | Internal         |
| Ctrl     | D0          | GPIO16 | External 10K to 3V3 |

> **D0 note:** GPIO16 does not support internal pull-up.
> Solder a 10K resistor between D0 and 3V3.

### Pins NOT available

| Pin     | Reason                          |
|---------|---------------------------------|
| D1, D2  | Used by OLED (I2C)              |
| D4      | Onboard LED, must be HIGH at boot |
| D8      | External pull-down, must be LOW at boot |
| S0–S3, SC | Connected to onboard SPI flash  |

## Button Schematic

```
    3V3
     │
    [10K]  ← only needed for D0
     │
  GPIO ●────┤ ├──── GND
          button
```

For D3, D5, D6, D7 — no external resistor needed (internal pull-up used).

## Menu Structure

```
MENU
├── Home        → shows Light/Fan status + uptime
├── Light       → toggle ON/OFF
├── Fan         → toggle ON/OFF
├── Settings
│   └── Brightness  → adjust display brightness
└── About       → device info
```

## Controls

| Context          | Up/Down        | Select          | Back       | Ctrl              |
|------------------|----------------|-----------------|------------|-------------------|
| Main Menu        | Navigate items | Enter submenu   | —          | Quick toggle Light/Fan |
| Home             | —              | —               | Back       | —                 |
| Light / Fan      | —              | Toggle ON/OFF   | Back       | Toggle ON/OFF     |
| Brightness       | Adjust value   | Back            | Back       | Reset to 100%     |
| About            | —              | Back            | Back       | —                 |

## How to Flash

### Prerequisites

Install arduino-cli, ESP8266 board support, and U8g2 library:

```bash
# Install ESP8266 board core
arduino-cli config add board_manager.additional_urls \
  https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

# Install U8g2 display library
arduino-cli lib install "U8g2"
```

### Compile

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 menu_ui/
```

### Find USB Port

```bash
ls /dev/cu.usbserial*
```

### Upload

```bash
arduino-cli upload \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --port /dev/cu.usbserial-XXXXX \
  menu_ui/
```

Replace `XXXXX` with the actual port number shown by `ls`.

> **Tip:** The port number can change when you reconnect the USB cable.
> Always check with `ls /dev/cu.usbserial*` before uploading.
