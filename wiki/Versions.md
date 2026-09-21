# Versions / Changelog (Keep a Changelog)

## [v2.3.1] — 2026-09-21
### Fixed
- Saves stick: the 1 s tick is status-only; forms fill on load + after saves.
- UTF-8 on all pages (garbled letters gone). OTA check parse + Install button.
### Changed
- No login wall (WPA2 is the gate); reboot/reset/upload/OTA-admin/AP saves
  ask the admin password per request (default `admin123`).
- Chase hold is automatic (`Chase sweeps`, default 3, 0 = forever).
### Added
- Spoof trigger GPIO configurable (safe pins only, else 21). WiFi kill
  switch: ground GPIO18 to drop the AP, release to restore.
- 103/103 tests.

## [v2.3] — 2026-09-21
### Added
- Relay count (first N of 8) + chase-wave mode (3rd sequence mode) +
  2-stage spoof (100 first 5 s, then 88.8/88.8/88.8/188 10 s, both editable).
- Per-mode holds in ms (sequential / chase / ALL-ON soak); industrial pack:
  loop + pause + cycle limit, ALL-ON stagger, direction, relay labels,
  cycle/actuation counters, boot auto-start.
- Persistent logins (remember-me, 30-day NVS slots); OTA via offline
  `/update` upload or automatic GitHub updates over the optional STA uplink.
- 38 new tests (**105/105**); NVS `bms2` v3 with tested v2→v3 migration;
  coalesced config saves (no loop stalls).
### Changed
- `FW_VERSION`/`STATUS?` → `2.3`; dashboard version is live from the device.

## [v2.2] — 2026-09-24
### Fixed
- Login page pops automatically on join (captive portal DNS catch-all +
  unknown-URL redirect); AP IP pinned to 192.168.4.1. Report + wiki warn:
  turn mobile data off, phones route around "no internet" networks.
### Added
- Portal-redirect flow host test (15 web tests).

## [v2.1] — 2026-09-23
### Added
- 14 host-executed website tests + JS↔firmware contract gate + 24 h office-day
  sim (86,400 polls, per-reply checksum validation) + Wokwi automation scenario
  (`sim.yaml`) with token-gated CI sim job.
- Diagram verified against official docs and fixed (NeoPixel VDD/VSS, button
  1.l/2.l); 8 real relay-module parts with NO-contact indicators.
### Changed
- `select_reply()` host-covered refactor (behavior-identical); `FW_VERSION` →
  `2.1`. Bench-confirmed: relay idle HIGH, ON-when-LOW = active-LOW default.

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
