# Ready-to-flash binaries — v2.8 (ESP32-S3 N16R8, 16 MB flash + OPI PSRAM)

![Bring-up terminal](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/terminal.png)

Built 2026-09-27 from this exact source (`pio run -e s3-n16r8`,
Xtensa GCC 8.4.0, Arduino 2.0.x). Same firmware logic as `firmware/`,
only the flash/PSRAM map differs. Use for N16R8 boards (16 MB flash,
8 MB octal PSRAM, onboard WS2812 on GPIO48).

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
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash 0x0 bms-tester-n16r8.bin
```

`bms-tester-n16r8.bin` is the merged image (bootloader + partitions + app
at their offsets) — one file, flashed to `0x0`, done.

## Or browser flash (no installs)
Open an ESP Web Tools flasher (e.g. https://www.espthings.io/tools/esp32-flasher/),
load `bms-tester-n16r8.bin` at `0x0`, flash, done.

## Or the 3 separate files (advanced / partial re-flash)
```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

## Verify
- `bms-tester-n16r8.bin` (1,078,304 bytes)
  SHA-256: `748355908e000215ef78ca9221db47167609278d3c90cba1361621e35c4f5676`
- `firmware.bin` (1,012,768 bytes)
  SHA-256: `e805e98220763dfd275b4cdfbe9d61e17d394e9abb99e5dd0ff03bf349c90aa6`
- `bootloader.bin` (15,104 bytes)
  SHA-256: `1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343`
- `partitions.bin` (3,072 bytes)
  SHA-256: `bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce`
- Golden reply bytes, `2.8` version, `BMS-Tester` AP name and dashboard
  strings verified byte-present inside `firmware.bin`; merged image verified
  (bootloader `E9` @0x0, partition magic @0x8000, app `E9` @0x10000).

Behavior: identical to the 8 MB build — boots red, relays OFF, AP up,
green on valid JBD polls (discretes + onboard RGB mirror).
