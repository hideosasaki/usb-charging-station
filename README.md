# USB Charging Station

A shelf-top 3-port USB charging station built around an RP2040-Zero, three SW3518 USB-PD controllers, and a 2.4" ILI9341 TFT for local visualization. Powered from a 12 V LiFePO4 system; no Wi-Fi or cloud integration.

## Quick Start

```bash
# 1. Build (on the dev machine)
make build

# 2. Flash via the USB-relay Pi
make upload

# 3. Open the serial monitor (relayed through the Pi)
make serial
```

The build runs locally; flashing and serial I/O are tunneled over SSH to a Raspberry Pi that hosts the RP2040-Zero over USB. This keeps the dev machine free of the USB tether. See [docs/remote-dev-setup.md](docs/remote-dev-setup.md) for setup.

## Layout

```
.
├── platformio.ini       # Earle Philhower core
├── Makefile             # build / upload / serial wrappers
├── src/main.cpp         # firmware entry point
├── scripts/             # remote-flash helpers (Mac and Pi side)
├── .env.example         # PI_HOST configuration template
├── .ssh-config.example  # ~/.ssh/config snippet
└── docs/                # design notes (Japanese, internal)
```

## Hardware

- Waveshare RP2040-Zero
- 3 × SW3518 (PD/QC charging controllers, not yet on hand)
- ILI9341 2.4" TFT (touch unused)
- 12 V LiFePO4 input from a separate v5 power system
