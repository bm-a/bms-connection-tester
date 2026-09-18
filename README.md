# JBD Smart BMS RS485 Responder — ESP32-S3

A firmware + hardware design that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485, so any compatible meter, display, or host can be exercised without
the real battery pack. Two status LEDs report link state live:
**green = valid BMS traffic seen, red = bus silent**. No buttons, no screens.

| | |
|---|---|
| Target | ESP32-S3 DevKitC-1 + MAX485 transceiver |
| Protocol | JBD UART over RS485, 9600 8N1 (registers `0x03`/`0x04`/`0x05`) |
| Releases | **v1.1** current · `v1.0` frozen (ZIP + tag) |
| Tests | **32 / 32 passing** (`pio test -e native` or `sh run_tests.sh`) |
| Firmware | `firmware/firmware.bin` (277,360 bytes, SHA below) |

## Hardware modules — what it runs on

| Module | Role | Key pins / settings |
|---|---|---|
| ESP32-S3 DevKitC-1 | Application MCU (240 MHz LX7, native USB, 8 MB flash) | UART2: TX = GPIO17, RX = GPIO16 |
| MAX485 (or SP3485) | Half-duplex RS485 transceiver | DI ← TX, RO → RX, DE+RE ← GPIO4 (HIGH = talk, LOW = listen) |
| Green / red LEDs | Link indicator (mutually exclusive in firmware) | GPIO10 / GPIO11 via 220 Ω to GND |
| 10 kΩ resistor | Pull-down on GPIO4 | Boots in listen mode, never jams the bus |

Full wiring, power (USB 5 V only, MAX485 at 3.3 V, common ground), and
troubleshooting live in [`docs/MODULES.md`](docs/MODULES.md).
Protocol byte layout and checksum rule live in [`docs/PROTOCOL.md`](docs/PROTOCOL.md).

## Firmware behavior

- Validates every incoming frame completely (start, command, length, checksum,
  terminator) — line noise can never fake a link (proven: 10 M-byte fuzz, zero emits).
- Answers register reads `0x03` (basic info — byte-exact real capture: 52.0 V,
  100 %), `0x04` (14-cell voltages), `0x05` (device name); stays **silent** on
  writes and unknown registers, while still counting them as live traffic.
- Link window self-adjusts (2–10 s) to any host poll cadence; silence → red
  automatically; traffic resumes → green automatically.
- `STATUS?` on USB serial replies `GREEN 1.1` / `RED 1.1` (test/automation hook).

## Typical uses

- E-rickshaw meter wiring check on the assembly line (the original job).
- Bench-testing any JBD-protocol display without a battery.
- RS485 link validation (cable, polarity, termination, ground).

## Flash it (pick one)

- **Ready binaries:** `pip install esptool`, then
  `esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 firmware/bootloader.bin 0x8000 firmware/partitions.bin 0x10000 firmware/firmware.bin`
  (see `firmware/README.md`; browser-flasher alternative included).
- **PlatformIO:** open this folder in VS Code + PIO → upload `esp32-s3-devkitc-1`.
- **Arduino IDE:** open `arduino/bms_connection_tester/bms_connection_tester.ino`
  (ESP32S3 Dev Module, USB CDC On Boot Enabled).

## Verify it

```sh
sh run_tests.sh          # native suite + 8-day soak sim (always); HIL when hardware is attached
pio test -e native       # 32/32 Unity tests: checksums, golden frame, parser, faults, timing
sh scripts/setup-linux.sh    # standard Linux: deps + venv + tests
sh scripts/setup-termux.sh   # Termux/Android: workarounds + tests
```

`firmware.bin` SHA-256: `7195161117ef68e3a3cd4c7793539a87c03083b067bde1a6e5b13ae330a376cf`

## Versions

- **v1.1** — multi-register (03/04/05) + adaptive window + silence-on-unknown + hardened parser.
- **v1.0** — 0x03-only responder, fixed 2 s window. Frozen: `releases/bms-connection-tester-v1.0.zip` + tag.

## Layout

`src/` firmware · `docs/` module + protocol docs · `test/` 32 Unity tests ·
`tools/` virtual meter, HIL pytest, QEMU script, soak sim · `arduino/` IDE sketch ·
`firmware/` flash-ready binaries · `wokwi/` browser simulation ·
`captures/` original Docklight recordings · `scripts/` Linux/Termux setup ·
`.github/workflows/` Linux CI.
