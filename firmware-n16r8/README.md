# Ready-to-flash binaries — v2.5 (ESP32-S3 N16R8, 16 MB flash + OPI PSRAM)

Built 2026-09-22 from this exact source (`pio run -e s3-n16r8`,
Xtensa GCC 8.4.0, Arduino 2.0.x). Same firmware logic as `firmware/`,
only the flash/PSRAM map differs. Use for N16R8 boards (16 MB flash,
8 MB octal PSRAM, onboard WS2812 on GPIO48).

Includes the v2.5 bench (trigger save, portal landing, structured config,
on-demand STA, Tasmota-grade update path, per-mode relay menu, console,
backup/restore, info card, mDNS) +
always-on AP dashboard (`BMS-Tester`, captive portal, no login wall) +
2-stage spoof (100 first, then 88.8/188) + manual/auto OTA. v1.x responder
behavior frozen.

## Flash with esptool (any PC, no IDE needed)
1. `pip install esptool`
2. Plug in the S3, find the port (COMx / /dev/ttyACM0), then:

```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

## Or browser flash (no installs)
Open an ESP Web Tools flasher (e.g. https://www.espthings.io/tools/esp32-flasher/),
load the three files at the addresses above, flash, done.

## Verify
- `firmware.bin` (1,004,720 bytes)
  SHA-256: `c2d8d2287c53a06e147a04b9e17ab8f1b6cf81c09a7976db7e9b4923053f4620`
- `bootloader.bin` (15,104 bytes)
  SHA-256: `1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343`
- `partitions.bin` (3,072 bytes)
  SHA-256: `bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce`
- Golden reply bytes, `2.5` version, `BMS-Tester` AP name and dashboard
  strings verified byte-present inside `firmware.bin`.

Behavior: identical to the 8 MB build — boots red, relays OFF, AP up,
green on valid JBD polls (discretes + onboard RGB mirror).
