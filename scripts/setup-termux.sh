#!/bin/sh
# Setup for Termux on Android (no root, bionic libc, phone storage only).
# Works around three Termux-specific issues automatically:
#  1. pyserial has no Android port lister -> one-line patch (see
#     ../esp32s3-qemu-arm64/patches/pyserial-android-listports.patch).
#  2. PIO's Xtensa toolchain tarball fails on rename() under proot ->
#     extract by hand with tar (documented in emulator repo README).
#  3. `pio test`/`pio run` for ESP32 cannot run on bionic at all ->
#     use this repo's g++ fallback in run_tests.sh (same Unity asserts).
set -eu
pkg install -y -q python clang git curl socat 2>&1 | tail -1
pip install -q platformio pytest pyserial
python3 - <<'EOF'
import sys
print("pyserial platform:", sys.platform)
try:
    from serial.tools import list_ports
    print("ports visible:", [p.device for p in list_ports.comports()])
except ImportError as e:
    print("PATCH NEEDED:", e)
    print("Apply: edit $(python3 -c 'import serial.tools.list_ports_posix as m; print(m.__file__)')")
    print("  change:  if plat[:5] == 'linux':")
    print("  to:      if plat[:5] == 'linux' or plat[:7] == 'android':")
EOF
sh run_tests.sh
echo "Termux setup OK."
