# USB Charging Station - build & remote-flash wrapper
# Build on Mac, flash via Raspberry Pi Zero W.
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
	@echo "  make upload      - build + scp to Pi + picotool load"
	@echo "  make serial      - connect to Pi's TCP serial bridge (port 5333)"
	@echo "  make flash       - upload then serial"
	@echo ""
	@echo "Targets (SW3518 firmware):"
	@echo "  make build-hw    - PlatformIO build for $(HW_ENV)"
	@echo "  make upload-hw   - build-hw + flash to Pi-attached board"
	@echo "  make flash-hw    - upload-hw then serial"
	@echo ""
	@echo "Other:"
	@echo "  make test        - pio test -e native (host-side unit tests)"
	@echo "  make clean       - PlatformIO clean"
	@echo ""
	@echo "Override env explicitly: make build PIO_ENV=<env-name>"

build:
	$(PIO) run -e $(PIO_ENV)

upload: build
	PIO_ENV=$(PIO_ENV) ./scripts/upload-via-pi.sh

serial:
	./scripts/serial-via-pi.sh

flash: upload serial

build-hw:
	$(PIO) run -e $(HW_ENV)

upload-hw: build-hw
	PIO_ENV=$(HW_ENV) ./scripts/upload-via-pi.sh

flash-hw: upload-hw serial

test:
	$(PIO) test -e native

clean:
	$(PIO) run --target clean
