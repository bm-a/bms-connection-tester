# Flash with Arduino IDE (no PlatformIO needed)

## 1. Install support
1. Install Arduino IDE from arduino.cc.
2. File → Preferences → Additional Boards Manager URLs, add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Tools → Board → Boards Manager → install **“esp32 by Espressif”**.

## 2. Open and configure
1. Open `bms_connection_tester.ino` (the other two files open as tabs automatically).
2. Tools → Board → **“ESP32S3 Dev Module”**.
3. Tools → USB CDC On Boot → **Enabled**.
4. Tools → Upload Speed → **921600**.
5. For N16R8 boards: Tools → Flash Size → **“16MB”**, Tools → PSRAM → **“OPI PSRAM”**.

## 3. Wire first (or at least the LEDs), then upload
- GPIO10 → 220Ω → green LED → GND ; GPIO11 → 220Ω → red LED → GND.
  Onboard RGB (GPIO48) mirrors both automatically — no wiring needed.
- GPIO17→MAX485 DI, GPIO16→MAX485 RO, GPIO4→DE+RE (+10k pull-down), 3.3V, GND, A/B→meter.

## 4. Upload
Connect the S3 with a DATA USB cable, pick the COM port, press Upload.
On boot the red LED comes on; it turns green as soon as the meter polls.

## Same firmware as the PlatformIO project
This sketch is identical logic to `src/main.cpp` in the PlatformIO
project — same pins, same canned SOC100% reply, same 1-second green/red vote.
PlatformIO: `esp32-s3-devkitc-1` (8 MB, Wokwi) or `s3-n16r8` (16 MB + OPI PSRAM).
