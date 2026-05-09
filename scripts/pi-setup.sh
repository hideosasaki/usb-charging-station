#!/usr/bin/env bash
# One-shot setup script for Raspberry Pi Zero W to act as RP2040 flash/serial proxy.
# Run ON THE PI (not on Mac):
#   curl ... | bash    or    scp this file to the Pi and run with sudo.
#
# Idempotent: safe to re-run.
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "Run as a normal user (not root). sudo will be invoked where needed." >&2
    exit 1
fi

echo "[pi-setup] installing packages..."
sudo apt update
sudo apt install -y picotool socat python3 python3-serial udev usbutils

echo "[pi-setup] adding $USER to plugdev,dialout..."
sudo usermod -a -G plugdev,dialout "$USER"

echo "[pi-setup] writing udev rule for RP2040..."
sudo tee /etc/udev/rules.d/99-rp2040.rules >/dev/null <<'EOF'
# RP2040 in BOOTSEL (picoboot)
SUBSYSTEM=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE="0666", GROUP="plugdev", SYMLINK+="rp2040-bootsel"
# RP2040 running Arduino-Pico CDC
SUBSYSTEM=="tty", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000a", MODE="0666", GROUP="plugdev", SYMLINK+="rp2040-cdc"
SUBSYSTEM=="tty", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0009", MODE="0666", GROUP="plugdev", SYMLINK+="rp2040-cdc"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "[pi-setup] writing systemd unit rp2040-serial.service..."
sudo tee /etc/systemd/system/rp2040-serial.service >/dev/null <<EOF
[Unit]
Description=RP2040 USB CDC bridge to TCP 5333
After=network.target

[Service]
Type=simple
User=$USER
ExecStartPre=/bin/sh -c 'while [ ! -e /dev/rp2040-cdc ]; do sleep 0.5; done'
ExecStart=/usr/bin/socat -d -d \\
    TCP-LISTEN:5333,reuseaddr,fork,keepalive \\
    FILE:/dev/rp2040-cdc,b115200,raw,echo=0,crnl
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

echo "[pi-setup] writing sudoers drop-in for passwordless service control..."
sudo tee /etc/sudoers.d/rp2040-serial >/dev/null <<EOF
$USER ALL=(ALL) NOPASSWD: /bin/systemctl stop rp2040-serial.service, /bin/systemctl start rp2040-serial.service
EOF
sudo chmod 0440 /etc/sudoers.d/rp2040-serial

echo "[pi-setup] installing ~/bin/rp2040-load.sh..."
mkdir -p "$HOME/bin"
cat > "$HOME/bin/rp2040-load.sh" <<'EOF'
#!/bin/bash
# Load a UF2 to RP2040 via picotool. Triggers 1200bps touch first if a CDC port is present.
set -euo pipefail
FW="${1:?usage: rp2040-load.sh <firmware.uf2>}"
[ -f "$FW" ] || { echo "no such file: $FW" >&2; exit 1; }

if [ -e /dev/rp2040-cdc ]; then
    echo "[pi] 1200bps touch on /dev/rp2040-cdc"
    sudo systemctl stop rp2040-serial.service || true
    python3 - <<'PY'
import serial, time
try:
    s = serial.Serial("/dev/rp2040-cdc", 1200)
    s.setDTR(False)
    time.sleep(0.1)
    s.close()
except Exception as e:
    print(f"[pi] touch warn: {e}")
PY
    for _ in $(seq 1 20); do
        lsusb -d 2e8a:0003 >/dev/null 2>&1 && break
        sleep 0.25
    done
fi

echo "[pi] picotool load -f -x -v $FW"
picotool load -f -x -v "$FW"

for _ in $(seq 1 40); do
    [ -e /dev/rp2040-cdc ] && break
    sleep 0.25
done
sudo systemctl start rp2040-serial.service || true
echo "[pi] done."
EOF
chmod +x "$HOME/bin/rp2040-load.sh"

echo "[pi-setup] enabling rp2040-serial.service..."
sudo systemctl daemon-reload
sudo systemctl enable rp2040-serial.service
sudo systemctl restart rp2040-serial.service || true

echo
echo "[pi-setup] done."
echo "Next steps:"
echo "  1. log out & back in so plugdev/dialout group membership takes effect"
echo "  2. plug RP2040-Zero with BOOT button held to verify lsusb shows 2e8a:0003"
echo "  3. from Mac: 'make upload' should work end-to-end"
