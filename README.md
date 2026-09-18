# E-Rickshaw Meter RS485 Connection Tester

A factory-line jig: a small ESP32-S3 box that impersonates the battery's BMS just enough for an
e-rickshaw meter to recognize it. The worker connects two wires and reads two lamps —
**green = wired correctly, red = not**. No buttons, no screens, no reset, ever.

| | |
|---|---|
| Board | ESP32-S3 DevKitC-1 + MAX485 module |
| Current release | **v1.1** (frozen `v1.0` in ZIP + git tag) |
| Tests | **32 / 32 passing** (`run_tests.sh` / `pio test -e native`) |
| Firmware | `firmware/firmware.bin` (277,360 bytes, SHA below) |

## Wiring

| Signal | Connection |
|---|---|
| S3 GPIO17 (TX) | → MAX485 DI |
| S3 GPIO16 (RX) | ← MAX485 RO |
| S3 GPIO4 | → MAX485 DE+RE tied (+10 kΩ pull-down to GND) |
| S3 GPIO10 | → 220 Ω → green LED → GND |
| S3 GPIO11 | → 220 Ω → red LED → GND |
| MAX485 VCC / GND | 3.3 V / common GND with meter |
| MAX485 A/B | → meter A/B (twisted pair) |

Power from USB only — never the traction pack. If green never appears, swap A/B first.

## How it works

The meter polls in the JBD / Xiaoxiang BMS protocol (9600 8N1). The box validates every incoming
frame (start, command, length, checksum, terminator), answers known reads with canned frames
(`0x03` = byte-exact real full-battery capture: 52.0 V, 100 %; `0x04` = 14S cells; `0x05` = name),
stays silent on writes/unknown registers — and turns green whenever *any* valid meter traffic is
seen inside a self-adjusting 2–10 s window. Silence → red, automatically.

## Flash it (pick one)

- **Ready binaries:** `pip install esptool`, then
  `esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 firmware/bootloader.bin 0x8000 firmware/partitions.bin 0x10000 firmware/firmware.bin`
- **PlatformIO:** open this folder in VS Code + PIO, upload `esp32-s3-devkitc-1`.
- **Arduino IDE:** open `arduino/bms_connection_tester/bms_connection_tester.ino`
  (ESP32S3 Dev Module, USB CDC On Boot Enabled).

Details + troubleshooting + full test report: [`RS485-Tester-Report.docx`](RS485-Tester-Report.docx)
(phone-friendly, written for an electronics engineer).

## Verify it

```sh
sh run_tests.sh          # native suite (always) + hardware suite (if a board is attached)
pio test -e native       # 32/32 Unity tests: checksums, golden frame, parser, faults, timing
```

`firmware.bin` SHA-256: `7195161117ef68e3a3cd4c7793539a87c03083b067bde1a6e5b13ae330a376cf`

## Versions

- **v1.0** — 0x03-only responder, fixed 2 s window. Frozen: `../bms-connection-tester-v1.0.zip` + git tag.
- **v1.1** — multi-register (03/04/05) + adaptive window + silence-on-unknown + fault-hardened parser.

## Layout

`src/` firmware · `test/` 32 Unity tests · `tools/` virtual meter + HIL pytest + QEMU script ·
`arduino/` IDE sketch · `firmware/` flash-ready binaries · `wokwi/` browser simulation ·
`captures/` original Docklight recordings.
