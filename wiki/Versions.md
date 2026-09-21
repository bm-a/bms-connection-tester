# Versions / Changelog (Keep a Changelog)

## [v2.0] — 2026-09-22
### Added
- 8-relay sequencer (sequential/all-ON, 3 button behaviors, boot-safe,
  polarity toggle); always-on AP dashboard (auth, NVS, admin reset);
  spoof window (88.8/88.8/88.8/188 on `0x03`, configurable, auto-revert).
- 18 new tests (**50/50**); Wokwi relay LEDs + buttons; CI builds both envs.
### Changed
- `FW_VERSION`/`STATUS?` → `2.0`; radio on (AP always broadcasting).
  v1.x responder core frozen and re-proven.

## [v1.2] — 2026-09-21
### Added
- Onboard WS2812 RGB mirror (GPIO48, built-in `neopixelWrite`, brightness 32):
  same green/red state as the discretes, zero extra wiring, driven from the
  250 ms eval.
- N16R8 build (`s3-n16r8`: 16 MB flash, OPI PSRAM, `default_16MB.csv`);
  8 MB env kept for Wokwi. Arduino IDE menu documented.
- Wokwi NeoPixel part on GPIO48; `firmware-n16r8/` ready-to-flash triple.
### Verified
- 32/32 + soak green; both envs compile; virtual-bus PASS; QEMU reproduces the
  known gap (no regression); golden bytes + `STATUS?` strings byte-present in
  both `firmware.bin` images.
### Changed
- `FW_VERSION`/`STATUS?` → `1.2`; MODULES/arduino/wokwi/src READMEs, llms.txt.

## [v1.1] — 2026-09-18
### Added
- Multi-register canned replies (`0x03` golden, `0x04` 14S, `0x05` name);
  silence on writes/unknown (option A); adaptive 2–10 s window; streaming
  checksum-verified parser; 21 new tests (**32/32**); `arduino/`, `firmware/`,
  `captures/`, `virtual_meter.py` modes, `soak_sim.cpp`, Wokwi meter.
### Fixed
- Parser emits only on CK success; Arduino `B1` macro collision (`JST_*`).

## [v1.0] — 2026-09-18
### Added
- 0x03-only responder, fixed 2 s window, green/red LEDs, `STATUS?`,
  11/11 tests, S3 binary, Word report. Frozen as
  `releases/bms-connection-tester-v1.0.zip` + tag `v1.0`.
