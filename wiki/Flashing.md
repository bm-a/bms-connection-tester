# Flashing — all different ways (v2.7)

![Bring-up terminal](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/terminal.png)

*Sim → sequence → `STATUS?` → flash: the whole bring-up in three commands.*

Pick **one** method. All install the same v2.7 logic; only the tool differs.

## 1. Ready binaries with esptool (any PC, no IDE) — ONE file, ONE command
1. `pip install esptool`
2. Choose the file for your board: `firmware/bms-tester-8mb.bin`
   (8 MB DevKitC-1 / Wokwi) or `firmware-n16r8/bms-tester-n16r8.bin`
   (N16R8: 16 MB flash + OPI PSRAM). Each is the full image
   (bootloader + partitions + app) — no multi-file juggling.
3. Plug in the S3, find the port (COMx / /dev/ttyACM0), then:

```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash 0x0 <FILE>
```

Verify with the SHA-256 sums in `firmware/README.md` / `firmware-n16r8/README.md`:
8 MB merged = `ba08f166…8727b9` (1,070,800 bytes);
N16R8 merged = `847f6c94…b82842` (1,073,328 bytes).

## 2. Browser flasher (no installs)
Open an ESP Web Tools flasher (e.g. https://www.espthings.io/tools/esp32-flasher/),
load the ONE merged file at `0x0`, flash, done.

## 3. Separate files (advanced / partial re-flash)
The folders also carry `bootloader.bin` + `partitions.bin` + `firmware.bin`
for the classic three-address flash (`0x0` / `0x8000` / `0x10000`).

## 3. PlatformIO (VS Code)
Open the repo folder, let PIO install, then upload:
- `esp32-s3-devkitc-1` for 8 MB boards (FULL dashboard), or
- `s3-n16r8` for N16R8 (16 MB flash, OPI PSRAM, `default_16MB.csv`, FULL), or
- `s3-classic` / `s3-lite` for the CLASSIC / LITE dashboard on 8 MB boards.
All compile the same `src/` — only the dashboard page differs.

## 4. Arduino IDE (no PlatformIO)
1. Install Arduino IDE, add `https://espressif.github.io/arduino-esp32/package_esp32_index.json`,
   install **esp32 by Espressif**.
2. Open `arduino/bms_connection_tester/bms_connection_tester.ino`
   (the `.h`/`.cpp` tabs open automatically — identical logic to `src/`).
3. Board **ESP32S3 Dev Module**, **USB CDC On Boot Enabled**, Upload **921600**;
   N16R8 additionally: **Flash Size 16MB**, **PSRAM OPI PSRAM**.
4. Pick the port, Upload. On boot the red LED (and red RGB) comes on;
   green follows the first meter poll.

## After flashing
Send `STATUS?` on the USB serial (115200): expect `RED 2.7` silent,
`GREEN 2.7` while a meter polls. Then see [[Hardware]] for wiring checks.

## After that — no more cables for updates
Join the AP, open the dashboard's **Firmware card**: upload a `.bin` from a
later GitHub release (offline, always works), or enter a hotspot Wi-Fi once
and let the box update itself when idle.
