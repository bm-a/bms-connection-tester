# Changelog

All notable changes to this project are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
