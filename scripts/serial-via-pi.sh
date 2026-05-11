#!/usr/bin/env bash
# Connect to the RP2040 USB CDC bidirectionally via the Pi's socat
# TCP bridge (rp2040-serial.service exposes /dev/rp2040-cdc on TCP 5333).
#
# Both directions: serial output streams to stdout, and any text typed
# (followed by LF) is sent to the firmware's serial_cmd parser.
# Examples:
#   bl 26
#   port 0 detach
#   scenario 1 A
#   status
#
# Usage:  ./scripts/serial-via-pi.sh
# Config: PI_HOST from .env or environment; PI_PORT defaults to 5333.
# Exit:   Ctrl-C
set -euo pipefail

. "$(dirname "$0")/common.sh"

: "${PI_PORT:=5333}"

echo "[mac] connecting to $PI_HOST:$PI_PORT (Ctrl-C to exit)"
exec nc "$PI_HOST" "$PI_PORT"
