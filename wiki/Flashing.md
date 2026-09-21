# Flashing — all different ways (v2.1)

Pick **one** method. All four install the same v2.1 logic; only the tool differs.

## 1. Ready binaries with esptool (any PC, no IDE)
1. `pip install esptool`
2. Choose the folder for your board: `firmware/` (8 MB DevKitC-1 / Wokwi)
   or `firmware-n16r8/` (N16R8: 16 MB flash + OPI PSRAM).
3. Plug in the S3, find the port (COMx / /dev/ttyACM0), then:

```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

Verify with the SHA-256 sums in `firmware/README.md` / `firmware-n16r8/README.md`:
8 MB `firmware.bin` = `8c393b37…d7204419c9` (761,536 bytes);
N16R8 `firmware.bin` = `164f43de…63fe062d8` (764,048 bytes).

## 2. Browser flasher (no installs)
Open an ESP Web Tools flasher (e.g. https://www.espthings.io/tools/esp32-flasher/),
load the same three files at `0x0` / `0x8000` / `0x10000`, flash, done.

## 3. PlatformIO (VS Code)
Open the repo folder, let PIO install, then upload:
- `esp32-s3-devkitc-1` for 8 MB boards (and Wokwi artifacts), or
- `s3-n16r8` for N16R8 (16 MB flash, OPI PSRAM, `default_16MB.csv`).
Both compile the same `src/` — verified with Xtensa GCC 8.4.0.

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
Send `STATUS?` on the USB serial (115200): expect `RED 2.1` silent,
`GREEN 2.1` while a meter polls. Then see [[Hardware]] for wiring checks.
