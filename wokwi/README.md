# wokwi/ — browser simulation (no hardware needed)

- `diagram.json` — ESP32-S3-DevKitC-1 + green/red LEDs + onboard RGB (NeoPixel
  on GPIO48) + second S3 board acting as the meter (UART cross-wired,
  logic-level RS485 equivalent).
- `meter.ino` — virtual meter firmware: cycles registers 0x03/0x04/0x05 every second
  and prints whatever the tester replies.
- `wokwi.toml` — project config pointing at the PlatformIO build artifacts.

Open in wokwi.com (ESP32-S3 project), upload `firmware.bin` as custom firmware,
press play: watch the tester answer every poll and the LEDs follow. Attach the
logic analyzer to TX/RX/DE to inspect the half-duplex turn-around.
