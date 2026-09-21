# JBD Smart BMS RS485 Responder — ESP32-S3

A firmware + hardware design that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485, so any compatible meter, display, or host can be exercised without
the real battery pack. Status LEDs report link state live:
**green = valid BMS traffic seen, red = bus silent**. No buttons, no screens.

> **New to RS485 or the JBD protocol?** Start with the companion handbook:
> **[jbd-bms-rs485-handbook](https://github.com/bm-a/jbd-bms-rs485-handbook)** —
> how RS485 works, the MAX485 module up close, the JBD frame format with worked
> examples, shopping list with search terms, and a step-by-step build guide.
> (Also known as: JBD BMS emulator, Xiaoxiang BMS simulator/tester, smart BMS
> responder, RS485 battery emulator, e-rickshaw meter tester.)

| | |
|---|---|
| Targets | ESP32-S3 DevKitC-1 (8 MB) + ESP32-S3 N16R8 (16 MB + OPI PSRAM) + MAX485 |
| Protocol | JBD UART over RS485, 9600 8N1 (registers `0x03`/`0x04`/`0x05`) |
| Releases | **v1.2** current · `v1.1` fixes below · `v1.0` frozen (ZIP + tag) |
| Tests | **32 / 32 passing** (`pio test -e native` or `sh run_tests.sh`) + 8-day soak |
| Firmware | `firmware/` (8 MB) + `firmware-n16r8/` (16 MB), SHAs below |

## Hardware modules — what it runs on

| Module | Role | Key pins / settings |
|---|---|---|
| ESP32-S3 DevKitC-1 / N16R8 | Application MCU (240 MHz LX7, native USB, 8/16 MB flash) | UART2: TX = GPIO17, RX = GPIO16 |
| MAX485 (or SP3485) | Half-duplex RS485 transceiver | DI ← TX, RO → RX, DE+RE ← GPIO4 (HIGH = talk, LOW = listen) |
| Green / red LEDs | Link indicator (mutually exclusive in firmware) | GPIO10 / GPIO11 via 220 Ω to GND |
| Onboard RGB (new in v1.2) | Mirrors the discretes, zero wiring | GPIO48 WS2812 via `neopixelWrite`, brightness 32/255 |
| 10 kΩ resistor | Pull-down on GPIO4 | Boots in listen mode, never jams the bus |

Full wiring, power (USB 5 V only, MAX485 at 3.3 V, common ground), N16R8
flash/PSRAM map, and troubleshooting live in [`docs/MODULES.md`](docs/MODULES.md).
Protocol byte layout and checksum rule live in [`docs/PROTOCOL.md`](docs/PROTOCOL.md).
Extra guides live in the [wiki](../../wiki) (mirrored in [`wiki/`](wiki/)).

## Firmware behavior

- Validates every incoming frame completely (start, command, length, checksum,
  terminator) — line noise can never fake a link (proven: 10 M-byte fuzz, zero emits).
- Answers register reads `0x03` (basic info — byte-exact real capture: 52.0 V,
  100 %), `0x04` (14-cell voltages), `0x05` (device name); stays **silent** on
  writes and unknown registers, while still counting them as live traffic.
- Link window self-adjusts (2–10 s) to any host poll cadence; silence → red
  automatically; traffic resumes → green automatically.
- Discretes (GPIO10/11) + onboard RGB (GPIO48) always show the same state;
  RGB needs no library (`neopixelWrite`) and is driven from the 250 ms eval,
  never the hot RX path.
- `STATUS?` on USB serial replies `GREEN 1.2` / `RED 1.2` (test/automation hook).

## Typical uses

- E-rickshaw meter wiring check on the assembly line (the original job).
- Bench-testing any JBD-protocol display without a battery.
- RS485 link validation (cable, polarity, termination, ground).

## Flash it (pick one)

- **Ready binaries (esptool, any PC):** `pip install esptool`, then for your board:
  `esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 <bootloader.bin> 0x8000 <partitions.bin> 0x10000 <firmware.bin>`
  using `firmware/` (8 MB boards) or `firmware-n16r8/` (N16R8) — see those READMEs.
  Browser-flasher alternative in the same READMEs.
- **PlatformIO:** open this folder in VS Code + PIO → upload `esp32-s3-devkitc-1`
  (8 MB / Wokwi) or `s3-n16r8` (16 MB flash + OPI PSRAM).
- **Arduino IDE:** open `arduino/bms_connection_tester/bms_connection_tester.ino`
  (ESP32S3 Dev Module, USB CDC On Boot Enabled, 921600; N16R8 also needs
  Flash Size 16MB + PSRAM OPI PSRAM).

## Verify it

```sh
sh run_tests.sh          # native suite + 8-day soak sim (always); HIL when hardware is attached
pio test -e native       # 32/32 Unity tests: checksums, golden frame, parser, faults, timing
pio run -e esp32-s3-devkitc-1 -e s3-n16r8  # both firmware profiles compile
sh scripts/setup-linux.sh    # standard Linux: deps + venv + tests
sh scripts/setup-termux.sh   # Termux/Android: workarounds + tests
```

Emulator results for v1.2 (Termux + Debian proot, 2026-09-21):
- Native 32/32 + soak `691040 polls / 691040 replies`, millis-wrap crossed — PASS.
- Virtual bus (socat PTY pair + host DUT harness linking the real
  `bms_protocol.cpp`): 03/04/05 golden byte-exact, silence on write/unknown,
  noise resync, red-after-silence — PASS.
- Wokwi: discretes + new NeoPixel on GPIO48 follow the meter (cycles 03/04/05).
- QEMU-S3: reproduces the known Arduino-guest gap (`Unknown cmd 0x10`,
  `0x10200C` reads, flash-init assert) — no firmware regression; Wokwi is the
  functional proof (see `docs/`, wiki Emulators page).

`firmware/firmware.bin` (8 MB, 277,376 bytes) SHA-256:
`23bf9f8617984bc4c9b6d1be67dd5c6b4d3a6855d6177e355dd64e12c755bc6e`
`firmware-n16r8/firmware.bin` (16 MB, 279,840 bytes) SHA-256:
`c7761eb50a54e7e2f0a11a4cef9156e5efdd99f144030a565977ec356fc7d531`

## Versions

- **v1.2** — onboard RGB mirror (GPIO48) + N16R8 16 MB/OPI build + Wokwi NeoPixel
  + refreshed binaries/docs. Protocol core untouched (32/32 + soak still green).
- **v1.1** — multi-register (03/04/05) + adaptive window + silence-on-unknown + hardened parser.
- **v1.0** — 0x03-only responder, fixed 2 s window. Frozen: `releases/bms-connection-tester-v1.0.zip` + tag.

See [`CHANGELOG.md`](CHANGELOG.md) for the full per-version notes.

## Layout

`src/` firmware · `docs/` module + protocol docs · `test/` 32 Unity tests ·
`tools/` virtual meter, HIL pytest, QEMU script, soak sim, report generator ·
`arduino/` IDE sketch · `firmware/` 8 MB binaries · `firmware-n16r8/` N16R8 binaries ·
`wokwi/` browser simulation (now with RGB) · `captures/` Docklight recordings ·
`scripts/` Linux/Termux setup · `wiki/` wiki sources (published to the GitHub wiki) ·
`.github/workflows/` Linux CI.
