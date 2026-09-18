#!/bin/sh
# Setup for standard Linux (Debian/Ubuntu, x86_64 or ARM64, glibc).
# Installs toolchain deps, PlatformIO, and runs the full native suite.
set -eu
sudo apt-get update -qq
sudo apt-get install -y -q python3 python3-pip python3-venv git curl \
  build-essential socat esptool 2>&1 | tail -1
python3 -m venv --system-site-packages .venv 2>/dev/null || python3 -m venv .venv
. .venv/bin/activate
pip install -q platformio pytest pyserial esptool
pio test -e native
echo "Linux setup OK. Build firmware:  pio run -e esp32-s3-devkitc-1"
echo "Flash:  esptool.py --chip esp32s3 --port PORT write-flash 0x0 firmware/bootloader.bin 0x8000 firmware/partitions.bin 0x10000 firmware/firmware.bin"
