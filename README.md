# JBD Smart BMS RS485 Responder + Relay Test Bench — ESP32-S3

A firmware + hardware design that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485, so any compatible meter, display, or host can be exercised without
the real battery pack — **v2.0 adds an 8-relay sequencer** that switches the
meter's own functions in order, driven by a button or a built-in web dashboard.
Status LEDs report link state live: **green = valid BMS traffic seen,
red = bus silent**. No screens needed.

> **New to RS485 or the JBD protocol?** Start with the companion handbook:
> **[jbd-bms-rs485-handbook](https://github.com/bm-a/jbd-bms-rs485-handbook)** —
> how RS485 works, the MAX485 module up close, the JBD frame format with worked
> examples, shopping list with search terms, and a step-by-step build guide.
> (Also known as: JBD BMS emulator, Xiaoxiang BMS simulator/tester, smart BMS
> responder, RS485 battery emulator, e-rickshaw meter tester.)

| | |
|---|---|
| Targets | ESP32-S3 DevKitC-1 (8 MB) + ESP32-S3 N16R8 (16 MB + OPI PSRAM) + MAX485 + 8ch relay |
| Protocol | JBD UART over RS485, 9600 8N1 (registers `0x03`/`0x04`/`0x05`) |
| Releases | **v2.0** current · `v1.2` RGB+N16R8 · `v1.1` fixes below · `v1.0` frozen (ZIP + tag) |
| Tests | **50 / 50 passing** (`pio test -e native` or `sh run_tests.sh`) + 8-day soak |
| Firmware | `firmware/` (8 MB) + `firmware-n16r8/` (16 MB), SHAs below |
| Web UI | Always-on AP `BMS-Tester` → professional dashboard (no office Wi-Fi needed) |

## What v2.0 adds (v1.x base frozen)

- **8 relays, sequential or all-at-once.** R1–R8 on GPIO 5/6/7/8/9/12/13/14 drive
  a SmartElex-style 12 V module (own 12 V supply, common GND, default active-LOW).
  Step delay, hold time, polarity — all on the web page.
- **Button with 3 behaviors** (GPIO15, web-selectable): hold-X-seconds with
  re-press abort, run-to-completion locked, or re-press restarts the cycle.
  10 s hold = factory reset fallback.
- **Fault spoof:** GPIO21 (or web FIRE) makes the meter read 88.8 V / 88.8 A /
  88.8 °C / 188 % on `0x03` for 10 s (configurable 1–120 s + values), then
  auto-reverts. `0x04`/`0x05` never change.
- **Web dashboard:** always-broadcasting AP, login (`admin`/`admin123`, change
  on first login), live relay grid + sequence/spoof/admin cards, NVS persistence.

## Hardware modules — what it runs on

| Module | Role | Key pins / settings |
|---|---|---|
| ESP32-S3 DevKitC-1 / N16R8 | Application MCU (240 MHz LX7, native USB) | UART2: TX = GPIO17, RX = GPIO16 |
| MAX485 (or SP3485) | Half-duplex RS485 transceiver | DI ← TX, RO → RX, DE+RE ← GPIO4 |
| Green / red LEDs + RGB | Link indicator (mutually exclusive) | GPIO10 / GPIO11 via 220 Ω; GPIO48 RGB mirrors |
| 8ch relay module (v2.0) | Switches meter functions in sequence | IN1–8 ← GPIO5/6/7/8/9/12/13/14; **separate 12 V supply** |
| Button (v2.0) | Starts/stops sequences | GPIO15 to GND (pull-up) |
| Spoof input (v2.0) | Triggers 10 s test values | GPIO21 to GND (pull-up) |
| 10 kΩ resistor | Pull-down on GPIO4 | Boots in listen mode, never jams the bus |

Full wiring, relay supply, power (USB for ESP + 12 V for coils), and
troubleshooting in [`docs/MODULES.md`](docs/MODULES.md).
Protocol byte layout and checksum rule in [`docs/PROTOCOL.md`](docs/PROTOCOL.md).
Relay/web guide in the [wiki](../../wiki) (mirrored in [`wiki/`](wiki/)).

## Firmware behavior

- Validates every incoming frame completely — line noise can never fake a link
  (proven: 10 M-byte fuzz, zero emits). Answers `0x03` (52.0 V, 100 %),
  `0x04` (14-cell), `0x05` (name); silent on writes/unknown, still counted live.
- Link window self-adjusts (2–10 s); `STATUS?` replies `GREEN 2.0` / `RED 2.0`.
- Sequencer runs on `millis()` — no `delay()` anywhere; RS485 keeps priority.
- AP `BMS-Tester` is up from every boot; connect any phone/laptop, open the
  dashboard (usually `192.168.4.1`), log in, configure.

## Flash it (pick one)

- **Ready binaries (esptool, any PC):** `pip install esptool`, then for your board:
  `esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 <bootloader.bin> 0x8000 <partitions.bin> 0x10000 <firmware.bin>`
  using `firmware/` (8 MB) or `firmware-n16r8/` (N16R8) — see those READMEs.
- **PlatformIO:** upload `esp32-s3-devkitc-1` (8 MB / Wokwi) or `s3-n16r8`.
- **Arduino IDE:** open `arduino/bms_connection_tester/bms_connection_tester.ino`
  (all tabs open automatically; ESP32S3 Dev Module, USB CDC On Boot Enabled,
  921600; N16R8 also Flash 16MB + OPI PSRAM).

## Verify it

```sh
sh run_tests.sh          # 50/50 Unity + 8-day soak (always); HIL when attached
pio test -e native       # checksums, logic, parser, stress, relay, spoof
pio run -e esp32-s3-devkitc-1 -e s3-n16r8  # both firmware profiles compile
```

Emulator results for v2.0 (Termux + Debian proot):
- Native 50/50 + soak `691040/691040` — PASS (old 32 untouched).
- Virtual bus (socat PTY + real-protocol harness): 03/04/05 golden, silences,
  resync, red-after-silence — PASS.
- Dashboard JS: `node --check` clean.
- Wokwi: discretes + NeoPixel + 8 relay LEDs + button/spoof buttons.
- QEMU-S3: same known Arduino-guest gap (`0x10`/`0x10200C`) — no regression.

`firmware/firmware.bin` SHA-256: see `firmware/README.md` (refreshed for v2.0).
`firmware-n16r8/firmware.bin` SHA-256: see `firmware-n16r8/README.md`.

## Versions

- **v2.0** — 8-relay sequencer + always-on AP dashboard + spoof window + admin auth.
  Responder core frozen (50/50 incl. original 32).
- **v1.2** — onboard RGB mirror (GPIO48) + N16R8 16 MB/OPI build + Wokwi NeoPixel.
- **v1.1** — multi-register (03/04/05) + adaptive window + silence-on-unknown + hardened parser.
- **v1.0** — 0x03-only responder, fixed 2 s window. Frozen: `releases/bms-connection-tester-v1.0.zip` + tag.

See [`CHANGELOG.md`](CHANGELOG.md) for full notes.

## Layout

`src/` firmware (protocol core + relay ctrl + web UI) · `docs/` module + protocol ·
`test/` 50 Unity tests · `tools/` meter sim, HIL pytest, QEMU script, soak, report ·
`arduino/` IDE sketch · `firmware/` 8 MB binaries · `firmware-n16r8/` N16R8 binaries ·
`wokwi/` browser sim (relays + buttons) · `captures/` Docklight recordings ·
`scripts/` Linux/Termux setup · `wiki/` wiki sources · `.github/workflows/` CI.
