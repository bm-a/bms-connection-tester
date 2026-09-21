#!/bin/sh
# QEMU-S3 Stage-0 exact boot-test harness (run INSIDE proot-debian or real Linux).
# NOTE 2026-09-21: QEMU 9.2.2 DOES execute guests inside proot-debian on this
# phone (ESP-ROM output + guest_errors observed) — the old "JIT cannot run in
# proot" note is stale for this setup. QEMU prints unknown SPI cmds as
# "M25P80: Unknown cmd %x" (hex, no 0x prefix) — cmd "10" IS 0x10.
# NOTE: firmware STATUS?/logs live on USB-Serial (Serial), NOT UART0 — UART0
# shows only ROM output. Boot-past-flash-init is therefore observed as:
# zero flash errors + reboot loop STOPS (Stage B adds GPIO proof).
# Usage: sh tools/qemu_boot_test.sh [n16r8|plain] [timeout_s]
#   BASELINE=1  record current failure signature, always exit 0.
#   TRACE_EVTS="m25p80_command_decoded"  log every flash transaction
#               (via -d trace:, the reliable route) to $LOGDIR/trace.log.
#   default     EXACT TEST: PASS iff zero 'Unknown cmd 0x10' AND zero '0x10200C'
#               in guest_errors (Stage-A gate), exit 1 otherwise.
# Env overrides: QEMU_BIN, ESPTOOL, LOGDIR, PORT (tcp serial, off by default).
set -eu
cd "$(dirname "$0")/.."
VARIANT="${1:-n16r8}"
TIMEOUT_S="${2:-45}"
LOGDIR="${LOGDIR:-/tmp/qemu_boot_test}"
PORT="${PORT:-}"

if [ "$VARIANT" = "plain" ]; then FWDIR="firmware"; else FWDIR="firmware-n16r8"; fi
for f in bootloader.bin partitions.bin firmware.bin; do
  [ -f "$FWDIR/$f" ] || { echo "ENV-FAIL: missing $FWDIR/$f"; exit 2; }
done

if [ -n "${QEMU_BIN:-}" ]; then :;
elif [ -x /opt/qemu-src/build/qemu-system-xtensa ]; then
  QEMU_BIN=/opt/qemu-src/build/qemu-system-xtensa
elif [ -x /opt/qemu-s3/qemu/bin/qemu-system-xtensa ]; then
  QEMU_BIN=/opt/qemu-s3/qemu/bin/qemu-system-xtensa
else
  echo "ENV-FAIL: no qemu-system-xtensa (set QEMU_BIN)"; exit 2
fi
if [ -n "${ESPTOOL:-}" ]; then :;
elif [ -x /opt/pioenv/bin/esptool ]; then ESPTOOL=/opt/pioenv/bin/esptool
else ESPTOOL="python3 -m esptool"; fi

mkdir -p "$LOGDIR"
FLASH="$LOGDIR/s3_flash_image.bin"
GUESTLOG="$LOGDIR/guest_errors.log"
UARTLOG="$LOGDIR/uart0.log"
rm -f "$FLASH" "$GUESTLOG" "$UARTLOG"

$ESPTOOL --chip esp32s3 merge_bin --fill-flash-size 8MB -o "$FLASH" \
  0x0 "$FWDIR/bootloader.bin" 0x8000 "$FWDIR/partitions.bin" \
  0x10000 "$FWDIR/firmware.bin" >/dev/null
SIZE=$(wc -c < "$FLASH")
echo "flash image: $SIZE bytes (expect 8388608 for GD25Q64 selection)"

SERIAL="file:$UARTLOG"
[ -n "$PORT" ] && SERIAL="tcp::$PORT,server,nowait"
DOPTS="guest_errors"
if [ -n "${TRACE_EVTS:-}" ]; then
  EVTS=$(printf "%s" "$TRACE_EVTS" | tr ',' '\n' | sed 's/^/trace:/' | paste -sd,)
  DOPTS="guest_errors,$EVTS"
fi
# shellcheck disable=SC2086
timeout "${TIMEOUT_S}s" "$QEMU_BIN" -nographic -machine esp32s3 \
  -drive file="$FLASH",if=mtd,format=raw \
  -d "$DOPTS" -D "$GUESTLOG" \
  -serial "$SERIAL" >"$LOGDIR/qemu_stdout.log" 2>"$LOGDIR/trace.log" || true

C10=$(grep -c -E "Unknown cmd (0x)?10$" "$GUESTLOG" 2>/dev/null || true)
C10u=$(grep -c "Unknown cmd" "$GUESTLOG" 2>/dev/null || true)
C102=$(grep -c "0x10200C" "$GUESTLOG" 2>/dev/null || true)
CER=$(grep -c "region .er" "$GUESTLOG" 2>/dev/null || true)
ASSERT=$(cat "$UARTLOG" "$LOGDIR/qemu_stdout.log" 2>/dev/null | grep -c -i -E "assert|abort|guru meditation" || true)
BOOTS=$(grep -c "rst:0x1 (POWERON)" "$UARTLOG" 2>/dev/null || true)
REBOOTS=$(grep -c "rst:0x3 (RTC_SW_SYS_RST)" "$UARTLOG" 2>/dev/null || true)

echo "--- Stage-0 signature ($VARIANT, ${TIMEOUT_S}s) ---"
echo "Unknown cmd 0x10 : $C10 (any unknown cmd: $C10u)"
echo "0x10200C reads   : $C102 (region 'er': $CER)"
echo "assert/abort     : $ASSERT"
echo "ROM boots / reboots: $BOOTS / $REBOOTS (reboots>0 = app crash loop)"
echo "logs: $LOGDIR"

if [ -n "${BASELINE:-}" ]; then echo "BASELINE recorded."; exit 0; fi
if [ "$C10" = "0" ] && [ "$C102" = "0" ]; then
  echo "PASS: flash model clean (no 0x10, no 0x10200C)"
  exit 0
else
  echo "FAIL: flash-model gaps remain (Stage-A work needed)"
  exit 1
fi
