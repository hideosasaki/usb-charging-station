#!/usr/bin/env bash
# Upload PlatformIO-built UF2 to RP2040-Zero through Raspberry Pi Zero W.
#
# Usage:
#   ./scripts/upload-via-pi.sh [<path/to/firmware.uf2>]
# Config:
#   PI_HOST is read from .env (project root) or environment.
#   Example: PI_HOST=pi-zero (must match an entry in ~/.ssh/config)
set -euo pipefail

. "$(dirname "$0")/common.sh"

FW="${1:-$PROJECT_DIR/.pio/build/waveshare_rp2040_zero/firmware.uf2}"
PI_TMP="/tmp/usb-charging-station.uf2"

if [ ! -f "$FW" ]; then
    echo "firmware not found: $FW" >&2
    echo "run 'pio run' (or 'make build') first." >&2
    exit 1
fi

echo "[mac] scp $(basename "$FW") -> $PI_HOST:$PI_TMP"
scp -q "$FW" "$PI_HOST:$PI_TMP"

echo "[mac] ssh $PI_HOST '~/bin/rp2040-load.sh $PI_TMP'"
ssh "$PI_HOST" "~/bin/rp2040-load.sh '$PI_TMP'"

echo "[mac] upload finished."
