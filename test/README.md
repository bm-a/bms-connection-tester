# test/ — automated tests (66 passing: 52 via pio + 14 web via g++)

Each subdirectory is an independent Unity test app (PlatformIO convention),
also compilable with plain `g++` (see `run_tests.sh` fallback).
`test_web` is g++-only (needs `-DARDUINO` + host stubs in its own folder).

- `test_checksum/` (7) — checksum vectors incl. real captures (`FFFD`, `FCDA`,
  `FCA8`, `FA86`, `F65A` from `captures/SOC-DOCKLIGHT.xlsx`), golden-frame
  byte-exactness, strict matcher vs 1-byte corruptions.
- `test_logic/` (8) — adaptive green window: boot red, green on polls, red after
  silence, self-heal, slow-poll adaptation, 2 s floor / 10 s cap, rollover safety.
- `test_parser/` (13) — full-frame parser (reads/writes/split/corrupt/resync/
  overlong), dispatcher option-A silence, canned-frame checksum self-consistency,
  1 M-byte fuzz (zero false frames), fast + slow poll soaks.
- `test_stress/` (4) — exhaustive 1,785 single-byte corruptions (zero false
  frames), 10 M-byte fuzz (zero emits), cadence×register sweep, 5,000-frame
  bus saturation.
- `test_relay/` (12) — v2.0 sequencer: boot OFF, sequential stepping, hold
  expiry/forever, ALL-ON mode, all 3 button behaviors, manual override,
  polarity helper, millis rollover, debounce edges.
- `test_spoof/` (6) — v2.0 spoof frame bytes (88.8/88.8/88.8/188), checksum
  self-consistency, custom values, window timing/cancel/retrigger, rollover.
- `test_web/` (14, g++-only) — real `web_ui.cpp` on host stubs (`Arduino.h`,
  `WiFi.h`, `WebServer.h` + request driver, `Preferences.h`): login/session,
  auth gates, validation/clamping, NVS round-trip, relay/seq/spoof/admin
  handlers, session expiry, logout, POST fuzz, factory reset.
- `test_system/` (2) — `select_reply()` matrix (golden/spoof/disabled/silent
  paths) + 24 h office-day sim: 86,400 polls, per-reply checksum validation,
  exact 10 s spoof window, write-silence mid-spoof, relay schedule probes,
  hourly noise, green-all-day link.

Plus `tools/check_web_contract.py` — dashboard JS ↔ firmware route/key gate.

Run: `pio test -e native` (52: all except `test_web`) or `sh ../run_tests.sh`
(full 66: g++ suites + contract + `test_web` + `test_system` + soak).
