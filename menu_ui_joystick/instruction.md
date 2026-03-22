# ESP8266 Menu UI — Joystick + Buttons OLED Interface

## Hardware

- **Board:** NodeMCU ESP8266 (v2)
- **Display:** SH1106 1.3" I2C OLED (128x64)
- **Input:** Analog joystick (VRY + SW) + Select button + Back button

## Wiring Diagram

```
                    NodeMCU ESP8266
                  ┌─────────────────┐
                  │      USB        │
                  │    ┌─────┐      │
                  │    └─────┘      │
                  │                 │
          3V3  ●──┤ 3V3        D0  ├──○ (free)
               ●──┤ GND        D1  ├──● OLED SCL
                  │ A0 ←──────VRY  │
                  │            D2  ├──● OLED SDA
                  │            D3  ├──● BTN_BACK ──┐
                  │            D4  ├──● BTN_SEL ───┤
                  │            D5  ├──● JOY_SW ────┤
                  │            D6  ├──○ (free)     │
                  │            D7  ├──○ (free)     │
                  │            D8  ├──○ (avoid)    │
                  │                 │               │
                  └─────────────────┘               │
                                                   GND

    Joystick module:
    ┌──────────┐
    │ GND ─────┼──── GND
    │ 5V  ─────┼──── 3V3  (use 3.3V, NOT 5V!)
    │ VRX ─────┼──── (not connected)
    │ VRY ─────┼──── A0
    │ SW  ─────┼──── D5
    └──────────┘
```

### OLED Display (I2C)

| OLED Pin | NodeMCU Pin |
|----------|-------------|
| VDD      | 3V3         |
| GND      | GND         |
| SCK      | D1 (GPIO5)  |
| SDA      | D2 (GPIO4)  |

### Joystick

| Joystick Pin | NodeMCU Pin | Notes |
|--------------|-------------|-------|
| GND          | GND         |       |
| 5V           | 3V3         | Must use 3.3V so A0 reads correctly |
| VRY          | A0          | Up/down navigation |
| SW           | D5 (GPIO14) | Click = select (internal pull-up) |
| VRX          | not connected | ESP8266 has only 1 analog input |

### Buttons (wired to GND)

| Button   | NodeMCU Pin | GPIO   | Function |
|----------|-------------|--------|----------|
| Select   | D4          | GPIO2  | Select / enter / toggle (also onboard LED) |
| Back     | D3          | GPIO0  | Short press = back, long hold = ctrl |

> **D4 note:** Shares the onboard blue LED — LED blinks when button is pressed.
> Must be HIGH at boot (pull-up is fine, just don't hold it during reset).

### Pin Map

| Pin | Status | Used for |
|-----|--------|----------|
| A0  | Taken  | Joystick VRY |
| D0  | Free   | — |
| D1  | Taken  | OLED SCL |
| D2  | Taken  | OLED SDA |
| D3  | Taken  | Back button |
| D4  | Taken  | Select button |
| D5  | Taken  | Joystick SW |
| D6  | Free   | — |
| D7  | Free   | — |
| D8  | Avoid  | Pull-down, boot issues |

## Button Schematic

```
  GPIO ●────┤ ├──── GND
          button

  D3, D4, D5 — internal pull-up, no external resistor needed
```

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

| Input | Action |
|-------|--------|
| Joystick up/down | Navigate menu items, adjust brightness |
| Joystick click (SW) | Select / enter / toggle |
| Select button (D4) | Select / enter / toggle (same as joystick click) |
| Back button short press | Go back one level |
| Back button long hold (>500ms) | Ctrl: quick toggle Light/Fan, reset brightness |

Auto-repeat: holding the joystick up/down starts repeating after 400ms at 150ms intervals.

| Context          | Joystick U/D   | Click / Select  | Back (short) | Back (hold)       |
|------------------|----------------|-----------------|--------------|-------------------|
| Main Menu        | Navigate items | Enter submenu   | —            | Quick toggle Light/Fan |
| Home             | —              | —               | Back         | —                 |
| Light / Fan      | —              | Toggle ON/OFF   | Back         | Toggle ON/OFF     |
| Brightness       | Adjust value   | Back            | Back         | Reset to 100%     |
| About            | —              | Back            | Back         | —                 |

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
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 menu_ui_joystick/
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
  menu_ui_joystick/
```

Replace `XXXXX` with the actual port number shown by `ls`.

> **Tip:** The port number can change when you reconnect the USB cable.
> Always check with `ls /dev/cu.usbserial*` before uploading.
