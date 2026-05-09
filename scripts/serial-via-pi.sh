#!/usr/bin/env bash
# Connect to RP2040 USB CDC by SSH-tunneling cat /dev/rp2040-cdc on the Pi.
#
# Usage:  ./scripts/serial-via-pi.sh
# Config: PI_HOST from .env or environment.
# Exit:   Ctrl-C
set -euo pipefail

. "$(dirname "$0")/common.sh"

echo "[mac] tailing /dev/rp2040-cdc on $PI_HOST (Ctrl-C to exit)"
# Pause the systemd socat (it holds the device exclusively), then cat directly.
# `trap` re-enables the bridge on Ctrl-C exit.
ssh -t "$PI_HOST" "
trap 'sudo systemctl start rp2040-serial.service' EXIT
sudo systemctl stop rp2040-serial.service
stty -F /dev/rp2040-cdc 115200 raw -echo
cat /dev/rp2040-cdc
"
