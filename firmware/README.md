# Ready-to-flash binaries — v1.2 (ESP32-S3 DevKitC-1, 8 MB flash)

Built 2026-09-21 from this exact source (`pio run -e esp32-s3-devkitc-1`,
Xtensa GCC 8.4.0, Arduino 2.0.x). Use for 8 MB boards and for Wokwi.
For 16 MB N16R8 boards use `firmware-n16r8/` instead (same logic, 16 MB map).

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
- `firmware.bin` (277,376 bytes)
  SHA-256: `23bf9f8617984bc4c9b6d1be67dd5c6b4d3a6855d6177e355dd64e12c755bc6e`
- `bootloader.bin` (15,104 bytes)
  SHA-256: `1776e4dd896a69d0a5c2e79957b0e2a88aa4129b1381d6478683515a1f6af343`
- `partitions.bin` (3,072 bytes)
  SHA-256: `1d9cca96de0fe07ad7fc0648b9878ddecd9ce565e38b589ad20fea698ed4c80c`
- Golden reply bytes (`DD 03 00 1B 14 50 … FC DA 77`) and `STATUS?`
  `GREEN`/`RED` strings verified byte-present inside `firmware.bin`.

Behavior: boots red (discretes + onboard RGB), green ≤ 1 s after first valid
JBD poll, red again after the adaptive window (2–10 s) of silence.
