# Changelog

All notable changes to this project are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [v2.0] — 2026-09-22
### Added
- 8-relay sequencer (`src/relay_ctrl.*`, host-tested): SEQUENTIAL R1→R8 with
  configurable step delay, or ALL-ON mode; 3 web-selectable button behaviors
  (hold-X-s + re-press abort / run-to-completion locked / re-press restarts);
  web per-relay overrides + START/STOP ALL; boot-safe OFF-before-pinMode;
  active-LOW default with web polarity toggle (SmartElex 12 V class).
- Always-on WiFi AP web UI (`src/web_ui.*`, ESP-only): dark professional
  dashboard (relays grid, sequence config, fault spoof, admin), login session
  with configurable admin user/password, NVS persistence, factory reset via
  Admin page or 10 s button long-press. AP defaults `BMS-Tester`/`bms12345`,
  channel 6 — all changeable; works fully offline (no office network needed).
- Spoof window: pin (GPIO21) or web FIRE shows 88.8 V / 88.8 A / 88.8 °C /
  188 % on `0x03` for a configurable 1–120 s (default 10 s), then auto-reverts;
  values/duration/source all web-configurable; frozen checksum rule applied.
- 18 new tests (`test_relay` 12, `test_spoof` 6): **50/50 passing**; old 32
  untouched. Wokwi relay LEDs + buttons; CI builds both firmware envs.
### Changed
- `FW_VERSION`/`STATUS?` report `2.0`. Radio now on (AP always broadcasting);
  power guidance updated. v1.x responder behavior frozen and re-proven
  (native + virtual-bus + soak all green on the same source).
### Fixed
- WebServer 2.0.x `collectHeaders` array-form call (caught by firmware build).

## [v1.2] — 2026-09-21
### Added
- Onboard WS2812 RGB mirror (GPIO48 via built-in `neopixelWrite`, no extra
  library): shows the same green/red link state as the external LEDs, so the
  box works with zero LED wiring. Brightness 32/255, driven from the 250 ms
  eval (never the hot RX path).
- ESP32-S3 N16R8 build (`pio run -e s3-n16r8`): 16 MB flash + OPI PSRAM +
  `default_16MB.csv` partitions; `esp32-s3-devkitc-1` (8 MB) kept for Wokwi.
  Arduino IDE settings documented (Flash 16MB + OPI PSRAM + USB CDC Enabled).
- Wokwi NeoPixel part (`rgb1` on GPIO48) next to the discrete LEDs; meter
  stimulus unchanged (cycles 03/04/05 @ 1 s).
### Verified
- 32/32 native tests + 8-day soak still green (protocol core untouched).
- Both firmware envs compile (Xtensa GCC 8.4.0): 8 MB profile + N16R8 profile.
- Virtual-bus emulation (socat PTY pair + host DUT harness linking the real
  `bms_protocol.cpp`): 03/04/05 golden byte-exact, silence on write/unknown,
  noise resync, red-after-silence — PASS.
- QEMU-S3 boot with v1.2 firmware reproduces the known emulator gap
  (`Unknown cmd 0x10` + `0x10200C` reads, Arduino-guest flash assert) — no
  firmware regression, functional proof via Wokwi instead.
### Changed
- `FW_VERSION` / `STATUS?` report `1.2`; `docs/MODULES.md`, `arduino/README.md`,
  `wokwi/README.md`, `src/README.md`, `llms.txt` updated for RGB + N16R8.

## [v1.1] — 2026-09-18
### Added
- Multi-register support: canned replies for `0x03` (golden capture), `0x04`
  (14S cell voltages), `0x05` (device name); silence on writes/unknown (option A).
- Adaptive green window (2 s floor, 10 s cap) — any poll cadence shows steady green.
- Streaming checksum-verified JBD parser with noise re-sync and overlong rejection.
- 21 new tests (parser, dispatcher, adaptive tracker, 10 M fuzz, exhaustive
  corruptions, cadence sweep, bus saturation, 8-day soak): **32/32 passing**.
- `arduino/` IDE sketch, `firmware/` ready-to-flash binaries, `captures/` ground truth,
  `tools/virtual_meter.py` scenario modes, `tools/soak_sim.cpp`, Wokwi meter cycling 03/04/05.
### Fixed
- Parser gated emission on checksum success (corrupt + trailing `0x77` no longer emits).
- Arduino `binary.h` `B1` macro collision (states renamed `JST_*`).

## [v1.0] — 2026-09-18
### Added
- Initial release: 0x03-only responder, fixed 2 s window, green/red LEDs,
  `STATUS?` debug command, 11/11 tests, S3 firmware binary, Word report.
- Frozen as `releases/bms-connection-tester-v1.0.zip` and git tag `v1.0`.
