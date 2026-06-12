# Vendored: pico-examples PIO I2C

Upstream:  https://github.com/raspberrypi/pico-examples (pio/i2c)
Commit:    996867c7198703d5affae48f447c0fc7d6922a40
Fetched:   2026-06-12
License:   BSD-3-Clause (see LICENSE)

## Why vendored

The RP2040 has only two hardware I2C controllers and both are taken by
SW3518 #1 and #2. The third SW3518 cannot share a bus (the chip address
is fixed at 0x3C), so it runs on a PIO state machine instead. This is
the reference PIO I2C implementation from pico-examples; it supports
clock stretching (`wait 1 pin` after each SCL rising edge) and reports
NAK via a PIO IRQ flag. pico-examples is not consumable as a PlatformIO
`lib_deps` package, so the four files are copied here.

## Files included

- `src/i2c.pio`    — PIO program source, kept for reference/regeneration
- `src/i2c.pio.h`  — generated from `i2c.pio`; regenerate with
                     `pioasm -o c-sdk i2c.pio i2c.pio.h`
- `src/pio_i2c.c`  — transaction-level helpers (start/stop/read/write)
- `src/pio_i2c.h`  — public API
- `LICENSE`        — BSD-3-Clause license text

## Local modifications

None. The Wire-style adapter lives outside this directory in
`src/pio_i2c_wrapper.h` so the vendored files stay diffable against
upstream.

## Constraints inherited from the PIO program

- SCL must be the GPIO directly after SDA (wait-instruction pin
  mapping); the board uses GP4=SDA / GP5=SCL.
- Bus clock is fixed at 100 kHz by `i2c_program_init`.
