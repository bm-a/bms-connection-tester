#!/bin/sh
# Test runner: native suite always, hardware suite only with a device.
# NOTE (Termux/Android): `pio test` currently fails on Android itself
# (pyserial has no Android tty implementation; port auto-detect aborts
# before compiling). So this script builds the Unity tests directly
# with g++ + Unity sources in .unity/ (same asserts as `pio test`).
set -eu
cd "$(dirname "$0")"

UNITY_VER=""
if [ -f .unity/unity.c ]; then
  UNITY_VER="local .unity/"
else
  echo "fetching Unity test framework..."
  mkdir -p .unity
  for f in unity.h unity.c unity_internals.h; do
    curl -sL --max-time 60 \
      "https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/$f" \
      -o ".unity/$f"
  done
fi

echo "=== native: test_checksum (Unity) ==="
g++ -std=c++17 -I src -I .unity src/bms_protocol.cpp test/test_checksum/test_checksum.cpp .unity/unity.c -o .test_checksum
./.test_checksum

echo "=== native: test_logic (Unity) ==="
g++ -std=c++17 -I src -I .unity src/bms_protocol.cpp test/test_logic/test_logic.cpp .unity/unity.c -o .test_logic
./.test_logic

echo "=== pio test -e native (upstream wrapper; expected to fail on Android) ==="
if command -v pio >/dev/null 2>&1 && pio test -e native 2>&1 | tail -5; then
  :
fi

echo "=== hardware-in-loop (pytest, needs real ESP32-S3 + RS485 adapter) ==="
if [ -n "${HIL_BUS_PORT:-}" ] && [ -n "${HIL_CDC_PORT:-}" ] \
   && [ -e "$HIL_BUS_PORT" ] && [ -e "$HIL_CDC_PORT" ]; then
  python3 -m pytest tools/test_hardware.py -v
else
  echo "SKIP: set HIL_BUS_PORT + HIL_CDC_PORT to run HIL (no device detected)"
fi

echo "=== virtual-bus demo hint ==="
echo "socat pty pair + python3 tools/virtual_meter.py <port> (needs socat + pyserial)"
echo "ALL NATIVE TESTS DONE"
