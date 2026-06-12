# USB Charging Station

A 3-port USB charging station built around a Waveshare
RP2040-Zero, three SW3518 USB-PD controllers, and a 2.4" ILI9341 TFT
for local visualization of per-port voltage, current, power, protocol,
and a short power history. Powered from 12 V DC; no Wi-Fi, no cloud,
no companion app.

Each SW3518 chip exposes two physical rails — USB-C and USB-A —
sharing a single Vbus. The firmware tracks both rails: per-rail
current, per-rail charging session, and a tile layout that switches
to a stacked two-rail view when both connectors are in use (the chip
clamps Vbus to 5 V in that mode).

This repository is the firmware.

## Quick start

Connect the RP2040-Zero to your computer over USB-C.

Mock firmware (the default) runs without any SW3518 chip wired up,
so you can iterate on the display layout, mock scenarios, and analyzers:

```bash
make upload
make serial
```

`make upload` flashes over USB with picotool; `make serial` opens
`pio device monitor`. As an alternative you can drag `firmware.uf2`
onto the BOOTSEL drive.

SW3518 firmware switches `make_port_reader()` to the real driver.
Requires at least SW3518 #1 wired to GP0=SDA / GP1=SCL:

```bash
make build-hw
```

Host-side unit tests cover `PortHistory`, `ChargeAnalyzer`,
`SessionStats`, mock scenarios, and the Serial command parser.
No board required:

```bash
make test
```

`make clean` wipes every env's build directory (`.pio/build/*`). Use it
when `build_flags` change in `platformio.ini` — PlatformIO's incremental
build does not always pick up flag-only edits:

```bash
make clean
```

`make build` produces `.pio/build/<env>/firmware.uf2`.

## Build envs

| env                          | `USE_MOCK_PORTS` | `make` target |
| ---------------------------- | ---------------- | ------------- |
| `waveshare_rp2040_zero`      | 1 (mock)         | `build`       |
| `waveshare_rp2040_zero_hw`   | 0 (SW3518)       | `build-hw`    |
| `native`                     | 1 (mock, host)   | `test`        |

Override the env explicitly when needed: `make build PIO_ENV=<env>`.

## Mock + Serial command interface

When `USE_MOCK_PORTS=1` (the default), `make_port_reader()` returns
three `MockPortReader` instances driving scripted scenarios. A
line-oriented Serial command parser is wired up so you can drive the
display without real hardware:

```
status                          # one-line snapshot per port
port <idx> detach               # force unplug
port <idx> attach               # force attach (last reading retained)
port <idx> auto                 # drop override, resume scenario
port <idx> <v_mV> <i_mA> <proto>
                                # USB-C-only override, USB-A current = 0
port <idx> <v_mV> <i_c_mA> <i_a_mA> <proto>
                                # explicit per-rail override
                                # <proto> = 5V | QC2.0 | QC3.0 | PD2.0 |
                                #          PD3.0 | PPS
scenario <idx> <id>             # switch scenario for one port
                                # <id> = A (PD3.0 phone, port 0 default)
                                #       | B (dual-rail soak, port 1 default)
                                #       | C (idle/burst, port 2 default)
bl <0-255>                      # ILI9341 backlight PWM duty
                                # (visibility / power-draw trials)
```

Scenario B loops C alone (PD3.0 9 V / 2 A) → both rails (5 V; C = 1.5 A,
A = 0.8 A) → A alone (5 V / 1 A), so leaving the firmware running for
five minutes after boot is enough to exercise the dual-rail tile
layout end-to-end without any real hardware.

`<idx>` is 0..2. The parser is host-test-buildable
(`test/test_serial_cmd/`).

## Hardware

- Waveshare RP2040-Zero (RP2040, dual-core, USB-C, on-board WS2812B)
- 3 × SW3518 USB-PD controllers (I²C address 0x3C is fixed, so each
  chip needs its own bus)
- SW3518 #1 auxiliary 5V output (max 250 mA, independent of the Vbus
  measurement path) feeds the RP2040-Zero and the TFT
- ILI9341 2.4" 320×240 TFT (touch unused, backlight runs at ~10% PWM
  duty for ~40 mA total draw, comfortably within the 250 mA Aux budget)
- 12 V DC input (4.5–24 V tolerated per SW3518 spec)

### Pinout (RP2040-Zero)

| Pin  | Signal       | Peripheral / Destination                     |
| ---- | ------------ | -------------------------------------------- |
| GP0  | I²C0 SDA     | SW3518 #1 SDA                                |
| GP1  | I²C0 SCL     | SW3518 #1 SCL                                |
| GP2  | I²C1 SDA     | SW3518 #2 SDA                                |
| GP3  | I²C1 SCL     | SW3518 #2 SCL                                |
| GP4  | PIO I²C SDA  | SW3518 #3 SDA (clock-stretch tolerant)       |
| GP5  | PIO I²C SCL  | SW3518 #3 SCL                                |
| GP6  | SPI0 SCK     | ILI9341 SCK                                  |
| GP7  | SPI0 MOSI    | ILI9341 MOSI (SDI)                           |
| GP8  | GPIO out     | ILI9341 CS                                   |
| GP14 | GPIO out     | ILI9341 DC                                   |
| GP15 | PWM          | ILI9341 backlight (LED)                      |
| GP16 | WS2812 DIN   | On-board WS2812B (hue-sweep heartbeat, dimmest)|
| 3V3  | Power out    | ILI9341 VCC, ILI9341 RST (tied high)         |
| GND  | Ground       |                                              |

Each SW3518 I²C bus needs an external 4.7 kΩ pull-up on SDA and SCL
to 3V3 (six resistors total). The RP2040's internal pull-ups are too
weak (~50 kΩ) for reliable 100 kHz operation off-board.

ILI9341 RST is tied to 3V3 instead of a GPIO; the driver issues a
soft-reset over SPI at init. MISO is left unwired (no readback path).
The XPT2046 touch controller on the same module is not used.
