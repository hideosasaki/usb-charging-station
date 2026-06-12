# USB Charging Station - build & flash wrapper
#
# PIO_ENV selects which PlatformIO env to build/flash. The default env
# ships the mock PortReader for display work without SW3518 hardware.
# Use the *-hw targets (or PIO_ENV=...) to build the SW3518 firmware.

.PHONY: build upload serial flash clean help \
        build-hw upload-hw flash-hw test

PIO     ?= $(firstword $(wildcard $(HOME)/.platformio/penv/bin/pio) pio)
PIO_ENV ?= waveshare_rp2040_zero
HW_ENV  := waveshare_rp2040_zero_hw

help:
	@echo "Targets (mock firmware, default):"
	@echo "  make build       - PlatformIO build for $(PIO_ENV)"
	@echo "  make upload      - build + flash over USB (picotool)"
	@echo "  make serial      - pio device monitor"
	@echo "  make flash       - upload then serial"
	@echo ""
	@echo "Targets (SW3518 firmware):"
	@echo "  make build-hw    - PlatformIO build for $(HW_ENV)"
	@echo "  make upload-hw   - build + flash $(HW_ENV) over USB"
	@echo "  make flash-hw    - upload-hw then serial"
	@echo ""
	@echo "Other:"
	@echo "  make test        - pio test -e native (host-side unit tests)"
	@echo "  make clean       - PlatformIO clean"
	@echo ""
	@echo "Override env explicitly: make build PIO_ENV=<env-name>"

build:
	$(PIO) run -e $(PIO_ENV)

upload:
	$(PIO) run -e $(PIO_ENV) -t upload

serial:
	$(PIO) device monitor

flash: upload serial

build-hw:
	$(PIO) run -e $(HW_ENV)

upload-hw:
	$(PIO) run -e $(HW_ENV) -t upload

flash-hw: upload-hw serial

test:
	$(PIO) test -e native

clean:
	$(PIO) run --target clean
