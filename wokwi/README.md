# wokwi/ — browser simulation (no hardware needed)

![Relay tiles — the sim drives these](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/tiles.png)

*Prefer offline? Same idea, phone-hosted, with 3D:
[local-wokwi/](../local-wokwi/) ([bms-tester-sim](https://github.com/bm-a/bms-tester-sim)).*

- `diagram.json` — ESP32-S3-DevKitC-1 + green/red LEDs + onboard RGB (NeoPixel
  on GPIO48: DIN/VDD/VSS per official docs) + **8 relay-module parts
  (GPIO 5/6/7/8/9/12/13/14, npn = energize-on-LOW like the real bench module,
  NO-contact indicator LEDs)** + button (GPIO15) + spoof button (GPIO21, both `1.l`/`2.l`
  per docs) + second S3 board acting as the meter (UART cross-wired,
  logic-level RS485 equivalent).
- `meter.ino` — virtual meter firmware: cycles registers 0x03/0x04/0x05 every second
  and prints whatever the tester replies.
- `wokwi.toml` — project config pointing at the PlatformIO build artifacts.
- `sim.yaml` — headless automation scenario (needs `WOKWI_CLI_TOKEN`):
  meter replies → button press → relay pins LOW in sequence → spoof press →
  `22 B0` on the meter console. CI runs it when the secret exists.

Open in wokwi.com (ESP32-S3 project), upload `firmware.bin` as custom firmware,
press play: watch the tester answer every poll and the LEDs follow. Press the
button to run the relay sequence (v2.0), the spoof button for 10 s test values
on the meter console. Attach the logic analyzer to TX/RX/DE to inspect the
half-duplex turn-around. (Wi-Fi dashboard is not emulated in Wokwi.)
