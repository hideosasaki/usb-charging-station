# USB Charging Station

A 3-port USB charging station built around a Waveshare
RP2040-Zero, three SW3518 USB-PD controllers, and a 2.4" ILI9341 TFT
for local visualization of per-port voltage, current, power, protocol,
and a short power history. Powered from 12 V DC; no Wi-Fi, no cloud,
no companion app.

This repository is the firmware.

## Quick start

Connect the RP2040-Zero to your computer over USB-C.

```bash
# Mock firmware: the default. Runs without any SW3518 chip wired up so
# you can iterate on the display layout, mock scenarios, and analyzers.
make build
pio run -t upload                # or drag firmware.uf2 onto the BOOTSEL drive
pio device monitor

# SW3518 firmware: switches make_port_reader() to the real driver.
# Requires at least SW3518 #1 wired to GP0=SDA / GP1=SCL.
make build-hw

# Host-side unit tests (PortHistory, ChargeAnalyzer, SessionStats,
# mock scenarios, Serial command parser). No board required.
make test
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
                                # pin a fixed reading until cleared
                                # <proto> = 5V | QC2.0 | QC3.0 | PD2.0 |
                                #          PD3.0 | PPS
scenario <idx> <id>             # switch scenario for one port
                                # <id> = 0 (PD3.0 phone) | 1 (5V steady)
                                #       | 2 (idle/burst)
```

`<idx>` is 0..2. The parser is host-test-buildable
(`test/test_serial_cmd/`).

## Hardware

- Waveshare RP2040-Zero (RP2040, dual-core, USB-C, on-board WS2812B)
- 3 × SW3518 USB-PD controllers (I²C address 0x3C is fixed, so each
  chip needs its own bus)
- BP5293-50 (12V→5V) dedicated MCU rail
- ILI9341 2.4" 320×240 TFT (touch unused)
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
| GP16 | WS2812 DIN   | On-board WS2812B status LED                  |
| 3V3  | Power out    | ILI9341 VCC, ILI9341 RST (tied high)         |
| GND  | Ground       |                                              |

Each SW3518 I²C bus needs an external 4.7 kΩ pull-up on SDA and SCL
to 3V3 (six resistors total). The RP2040's internal pull-ups are too
weak (~50 kΩ) for reliable 100 kHz operation off-board.

ILI9341 RST is tied to 3V3 instead of a GPIO; the driver issues a
soft-reset over SPI at init. MISO is left unwired (no readback path).
The XPT2046 touch controller on the same module is not used.
