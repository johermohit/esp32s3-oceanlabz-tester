#!/bin/bash
# One-command build, flash and serial monitor for macOS/Linux.
set -e
cd "$(dirname "$0")/firmware_source"
command -v python3 >/dev/null || { echo "python3 missing. Run: xcode-select --install"; exit 1; }
[ -d ../.venv ] || python3 -m venv ../.venv
source ../.venv/bin/activate
pip install -q platformio
echo "Plug in the board using the NATIVE USB-C port (the other one if nothing shows). Building and flashing..."
pio run -t upload || { echo "Upload failed. Hold BOOT, tap RESET, release BOOT, then run this script again."; exit 1; }
echo
echo "Flashed. Serial monitor open. Type one letter and press Enter: s=I2C scan, m=mic, t=tone, d=display, q=try pin presets, b=backlight, h=help"
echo "Copy the output and send it back. Quit with Ctrl+C."
pio device monitor -b 115200
