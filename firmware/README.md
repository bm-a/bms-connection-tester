# Ready-to-flash binaries — v2.4 (ESP32-S3 DevKitC-1, 8 MB flash)

Built 2026-09-22 from this exact source (`pio run -e esp32-s3-devkitc-1`,
Xtensa GCC 8.4.0, Arduino 2.0.x). Use for 8 MB boards and for Wokwi.
For 16 MB N16R8 boards use `firmware-n16r8/` instead (same logic, 16 MB map).

Includes the v2.4 bench (Tasmota-grade update path, per-mode relay menu with
chase break-before-make + stop dead-band, spoof save-only, console, config
backup/restore, custom OTA URL, STA uplink test, info card, mDNS) +
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
- `firmware.bin` (999,040 bytes)
  SHA-256: `c34044954f847964734385bdbc2981c119f47dce34b72aec9303bbb1352f0481`
- `bootloader.bin` (15,104 bytes)
  SHA-256: `1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343`
- `partitions.bin` (3,072 bytes)
  SHA-256: `1d9cca96de0fe07ad7fc0648b9878ddecd9ce565e38b589ad20fea698ed4c80c`
- Golden reply bytes (`DD 03 00 1B 14 50 … FC DA 77`), `2.4` version,
  `BMS-Tester` AP name and dashboard strings verified byte-present inside
  `firmware.bin`.

Behavior: boots red (discretes + onboard RGB), relays OFF (no click),
AP `BMS-Tester` up immediately; green ≤ 1 s after first valid JBD poll.
