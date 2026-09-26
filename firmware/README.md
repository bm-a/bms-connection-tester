# Ready-to-flash binaries — v2.8 (ESP32-S3 DevKitC-1, 8 MB flash)

![Bring-up terminal](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/terminal.png)

Built 2026-09-27 from this exact source (`pio run -e esp32-s3-devkitc-1`,
Xtensa GCC 8.4.0, Arduino 2.0.x). Use for 8 MB boards and for Wokwi.
For 16 MB N16R8 boards use `firmware-n16r8/` instead (same logic, 16 MB map).

Includes the v2.8 bench (meter counter with 5 s settle fumble guard,
daily meter heuristic, round link dots, trigger save, portal landing,
structured config, on-demand STA, Tasmota-grade update path, per-mode
relay menu, console, backup/restore, info card,
mDNS) + always-on AP dashboard (`BMS-Tester`, captive portal, no login
wall) + 2-stage spoof (100 first, then 88.8/188) + manual/auto OTA.
v1.x responder behavior frozen.

## Flash with esptool — ONE file, ONE command (any PC, no IDE needed)
1. `pip install esptool`
2. Plug in the S3, find the port (COMx / /dev/ttyACM0), then:

```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash 0x0 bms-tester-8mb.bin
```

`bms-tester-8mb.bin` is the merged image (bootloader + partitions + app at
their offsets) — one file, flashed to `0x0`, done.

## Or browser flash (no installs)
Open an ESP Web Tools flasher (e.g. https://www.espthings.io/tools/esp32-flasher/),
load `bms-tester-8mb.bin` at `0x0`, flash, done.

## Or the 3 separate files (advanced / partial re-flash)
```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

## Verify
- `bms-tester-8mb.bin` (1,075,808 bytes)
  SHA-256: `28248551fa8d32e061f75d46cb2bcd4559d1c40dd750e2422b412bf5353a98f8`
- `firmware.bin` (1,010,272 bytes)
  SHA-256: `27d5e25c86c0a97b383a55b326311cdd32296ec4772bbc243d645a4fe25616a8`
- `bootloader.bin` (15,104 bytes)
  SHA-256: `1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343`
- `partitions.bin` (3,072 bytes)
  SHA-256: `1d9cca96de0fe07ad7fc0648b9878ddecd9ce565e38b589ad20fea698ed4c80c`
- Golden reply bytes (`DD 03 00 1B 14 50 … FC DA 77`), `2.8` version,
  `BMS-Tester` AP name and dashboard strings verified byte-present inside
  `firmware.bin`; merged image verified (bootloader `E9` @0x0, partition
  magic @0x8000, app `E9` @0x10000).

Behavior: boots red (discretes + onboard RGB), relays OFF (no click),
AP `BMS-Tester` up immediately; green ≤ 1 s after first valid JBD poll.
