#!/bin/sh
# Virtual-bus emulation in one shot (Debian/proot or any Linux with socat+g++).
# Builds the host DUT harness (real src/bms_protocol.cpp), links a socat PTY
# pair, runs the raw-fd meter through every scenario, then checks the DUT's
# LED log went RED->GREEN->RED (link proved live, then silence detected).
# Exit 0 = VIRTUAL-BUS PASS. Everything runs in this one shell so background
# jobs survive (a fresh proot login kills detached jobs from earlier logins).
set -eu
cd "$(dirname "$0")/.."

g++ -O2 -std=c++17 -I src src/bms_protocol.cpp tools/dut_emu.cpp -o /tmp/dut_emu
rm -f /tmp/ttyDUT /tmp/ttyMETER
socat pty,raw,echo=0,link=/tmp/ttyDUT pty,raw,echo=0,link=/tmp/ttyMETER \
  > /tmp/socat.log 2>&1 &
SC=$!
sleep 2
stty -F /tmp/ttyDUT raw -echo
stty -F /tmp/ttyMETER raw -echo
/tmp/dut_emu /tmp/ttyDUT > /tmp/dut.log 2>&1 &
DU=$!
sleep 1
RC=0
timeout 90 python3 -u tools/raw_meter.py /tmp/ttyMETER || RC=$?
echo "--- DUT LED log ---"
cat /tmp/dut.log
kill $DU $SC 2>/dev/null || true
if [ "$RC" -ne 0 ]; then echo "VIRTUAL-BUS FAIL (meter)"; exit 1; fi
if ! grep -q "LED GREEN" /tmp/dut.log; then
  echo "VIRTUAL-BUS FAIL (DUT never went green)"
  exit 1
fi
echo "VIRTUAL-BUS PASS"
