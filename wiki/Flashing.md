# Flashing — all different ways (v2.8)

![Bring-up terminal](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/terminal.png)

*Sim → sequence → `STATUS?` → flash: the whole bring-up in three commands.*

Pick **one** method. v2.8 ships two firmware lines in one release:
**generic FULL** (8 MB ESP32-S3, FULL dashboard) and **Waveshare**
(ESP32-S3-ETH-8DI-8RO, FULL dashboard on the board pin map). Both report
`FW_VERSION 2.8` and both OTA-check `/releases/latest` (the old Waveshare
`-wsN` prerelease line is retired).

## 1. Ready binaries with esptool (any PC, no IDE) — ONE file, ONE command
1. `pip install esptool`
2. Choose the file for your board from the **v2.8 release assets**:
   - `bms-tester-8mb.bin` — generic ESP32-S3 DevKitC-1 (8 MB), FULL dashboard.
     Merged image (bootloader + partitions + app).
   - `bms-tester-waveshare.bin` — Waveshare ESP32-S3-ETH-8DI-8RO. Merged
     image (bootloader@0x0 + partitions@0x8000 + app@0x10000).
   - `waveshare-firmware.bin` — Waveshare app-only image (for the dashboard
     `/update` upload path on a Waveshare box).
3. Plug in the S3, find the port (COMx / /dev/ttyACM0), then:

```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash 0x0 <FILE>
```

Verify against the SHA-256 sums published in the v2.8 release notes.

## 2. Browser flasher (no installs)
Open an ESP Web Tools flasher (e.g. https://www.espthings.io/tools/esp32-flasher/),
load the ONE merged file for your board at `0x0`, flash, done.

## 3. Separate files (advanced / partial re-flash)
`bootloader.bin` + `partitions.bin` + `firmware.bin` for the classic
three-address flash (`0x0` / `0x8000` / `0x10000`).

## 4. PlatformIO (VS Code)
Open the repo folder, let PIO install, then upload:
- `esp32-s3-devkitc-1` — 8 MB generic board, FULL dashboard (default).
- `s3-waveshare` — Waveshare ESP32-S3-ETH-8DI-8RO (16 MB flash, OPI PSRAM,
  `default_16MB.csv`, TCA9554 relays, FULL dashboard).
- `s3-n16r8` — generic N16R8 (16 MB flash, OPI PSRAM, FULL).
- `s3-classic` / `s3-lite` — CLASSIC / LITE dashboard on 8 MB boards.
All compile the same `src/` — only the dashboard page and board pin map differ.

## 5. Arduino IDE (no PlatformIO)
1. Install Arduino IDE, add `https://espressif.github.io/arduino-esp32/package_esp32_index.json`,
   install **esp32 by Espressif**.
2. Open `arduino/bms_connection_tester/bms_connection_tester.ino`
   (the `.h`/`.cpp` tabs open automatically — identical logic to `src/`).
3. Board **ESP32S3 Dev Module**, **USB CDC On Boot Enabled**, Upload **921600**;
   N16R8 additionally: **Flash Size 16MB**, **PSRAM OPI PSRAM**.
   For the Waveshare board add `-DBOARD_WAVESHARE_8DI8RO=1` to the build flags
   (or just use the PlatformIO `s3-waveshare` env — recommended).
4. Pick the port, Upload.

## First-boot check
1. Red LED (or red RGB on Waveshare) comes on at boot.
2. AP **`BMS-Tester`** appears — join it (password `bms12345`).
3. With a meter polling on A/B, the LED flips green (≤ 1 s after the first
   valid frame).
4. Send `STATUS?` on the USB serial (115200): expect `RED 2.8` silent,
   `GREEN 2.8` while a meter polls.

## After that — no more cables for updates
Join the AP, open the dashboard's **Firmware card**: upload a `.bin` from a
later GitHub release (offline, always works), or enter a hotspot Wi-Fi once
and let the box update itself when idle (OTA checks `/releases/latest`;
generic boxes pull `firmware.bin`, Waveshare boxes pull
`waveshare-firmware.bin`).
