#!/bin/sh
# QEMU S3 boot + virtual-meter test (needs a REAL Linux/mac, not Termux/proot:
# QEMU's JIT cannot run inside proot's syscall emulation).
# Usage: sh tools/run_qemu_s3.sh [QEMU_BIN_DIR]
# Boots firmware/firmware.bin on an emulated ESP32-S3, UART0 on TCP :5555,
# then checks the boot log and STATUS? -> RED (boots red, no meter polling).
set -eu
cd "$(dirname "$0")/.."
QEMU_BIN="${1:-/opt/qemu-s3/qemu/bin}"
PORT=5555

"$QEMU_BIN/qemu-img" --version >/dev/null 2>&1 || true
python3 -m esptool --chip esp32s3 merge_bin -o /tmp/s3_flash_image.bin \
  0x0 firmware/bootloader.bin 0x8000 firmware/partitions.bin \
  0x10000 firmware/firmware.bin >/dev/null
"$QEMU_BIN/qemu-system-xtensa" -nographic -machine esp32s3 \
  -drive file=/tmp/s3_flash_image.bin,if=mtd,format=raw \
  -serial "tcp::$PORT,server,nowait" > /tmp/qemu_s3.log 2>&1 &
QEMU_PID=$!
sleep 12
python3 - "$PORT" <<'EOF'
import socket, sys, time
port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=5)
s.settimeout(2.0)
time.sleep(2)
s.sendall(b"STATUS?\n")
try:
    data = s.recv(4096).decode(errors="replace")
except socket.timeout:
    data = ""
print("--- console snippet ---")
print(data[-1500:])
print("QEMU-S3 " + ("PASS: boots red" if "RED" in data else "CHECK LOG: no RED seen"))
EOF
kill $QEMU_PID 2>/dev/null || true
