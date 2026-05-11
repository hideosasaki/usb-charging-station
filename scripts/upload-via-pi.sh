#!/usr/bin/env bash
# Upload PlatformIO-built UF2 to RP2040-Zero through Raspberry Pi Zero W.
#
# Usage:
#   ./scripts/upload-via-pi.sh [<path/to/firmware.uf2>]
# Config:
#   PI_HOST is read from .env (project root) or environment.
#   PIO_ENV selects which PlatformIO env's UF2 to upload when no
#   explicit path is given (defaults to waveshare_rp2040_zero).
#   Example: PI_HOST=pi-zero (must match an entry in ~/.ssh/config)
set -euo pipefail

. "$(dirname "$0")/common.sh"

# The default here must match the env name in platformio.ini. If the
# env is ever renamed, update both files (and Makefile) together —
# otherwise this script silently falls back to a stale build path.
PIO_ENV="${PIO_ENV:-waveshare_rp2040_zero}"
FW="${1:-$PROJECT_DIR/.pio/build/$PIO_ENV/firmware.uf2}"
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
