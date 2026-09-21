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

echo "=== native: test_parser (Unity) ==="
g++ -std=c++17 -I src -I .unity src/bms_protocol.cpp test/test_parser/test_parser.cpp .unity/unity.c -o .test_parser
./.test_parser

echo "=== native: test_stress (Unity, extended data) ==="
g++ -O2 -std=c++17 -I src -I .unity src/bms_protocol.cpp test/test_stress/test_stress.cpp .unity/unity.c -o .test_stress
./.test_stress
rm -f .test_stress

echo "=== native: test_relay (Unity, v2 sequencer) ==="
g++ -std=c++17 -I src -I .unity src/bms_protocol.cpp src/relay_ctrl.cpp test/test_relay/test_relay.cpp .unity/unity.c -o .test_relay
./.test_relay

echo "=== native: test_spoof (Unity, v2 spoof frame + window) ==="
g++ -std=c++17 -I src -I .unity src/bms_protocol.cpp src/relay_ctrl.cpp test/test_spoof/test_spoof.cpp .unity/unity.c -o .test_spoof
./.test_spoof
rm -f .test_relay .test_spoof

echo "=== native: test_ota (Unity, v2.3 OTA decision logic) ==="
g++ -std=c++17 -I src -I .unity src/ota.cpp test/test_ota/test_ota.cpp .unity/unity.c -o .test_ota
./.test_ota
rm -f .test_ota

echo "=== native: test_upload (Unity, v2.4 Tasmota-grade update gates) ==="
g++ -std=c++17 -I src -I .unity test/test_upload/test_upload.cpp .unity/unity.c -o .test_upload
./.test_upload
rm -f .test_upload

echo "=== web contract (dashboard JS vs web_ui.cpp routes/keys) ==="
python3 tools/check_web_contract.py

echo "=== native: test_web (Unity, website logic on host stubs) ==="
g++ -std=c++17 -DARDUINO -I test/test_web -I src -I .unity src/bms_protocol.cpp src/relay_ctrl.cpp src/web_ui.cpp test/test_web/test_web.cpp .unity/unity.c -o .test_web
./.test_web
rm -f .test_web

echo "=== native: test_system (Unity, 24h office-day reliability sim) ==="
g++ -O2 -std=c++17 -I src -I .unity src/bms_protocol.cpp src/relay_ctrl.cpp test/test_system/test_system.cpp .unity/unity.c -o .test_system
./.test_system
rm -f .test_system

echo "=== soak: 8-day continuous-run simulation ==="
g++ -O2 -std=c++17 -I src src/bms_protocol.cpp tools/soak_sim.cpp -o .soak_sim
./.soak_sim
rm -f .soak_sim .test_checksum .test_logic .test_parser

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
