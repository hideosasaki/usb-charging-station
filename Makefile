# USB Charging Station - build & remote-flash wrapper
# Build on Mac, flash via Raspberry Pi Zero W.

.PHONY: build upload serial flash clean help

PIO ?= $(firstword $(wildcard $(HOME)/.platformio/penv/bin/pio) pio)

help:
	@echo "Targets:"
	@echo "  make build   - PlatformIO build (firmware.uf2 in .pio/build/...)"
	@echo "  make upload  - build + scp to Pi + picotool load"
	@echo "  make serial  - connect to Pi's TCP serial bridge (port 5333)"
	@echo "  make flash   - upload then serial"
	@echo "  make clean   - PlatformIO clean"

build:
	$(PIO) run

upload: build
	./scripts/upload-via-pi.sh

serial:
	./scripts/serial-via-pi.sh

flash: upload serial

clean:
	$(PIO) run --target clean
