# test/ — automated tests (103 passing: 77 via pio + 26 web via g++)

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
- `test_relay/` (24) — sequencer: boot OFF, sequential stepping, per-mode ms
  holds, ALL-ON, chase wave, relay count scoping, loop/pause/limit, reverse
  direction, ALL-ON stagger, QC counters, all 3 button behaviors, manual
  override, polarity helper, millis rollover, debounce edges.
- `test_spoof/` (11) — stage-1 ("100") + stage-2 (88.8/88.8/88.8/188) frame
  bytes, checksum self-consistency, custom values, `SpoofPlan` stage
  timing/handoff/cancel/retrigger/rollover, legacy window (frozen) timing.
- `test_ota/` (6) — version compare (incl. `2.10 > 2.9`), per-variant asset
  pick, download-URL build + tag sanitizing, auto-check gate matrix.
- `test_web/` (26, g++-only) — real `web_ui.cpp` on host stubs (`Arduino.h`,
  `WiFi.h` + STA, `WebServer.h` + request/upload drivers, `Preferences.h` +
  commit counter, `Update.h`, `DNSServer.h`): per-request admin password
  gating, WiFi kill switch, NVS v2→v3 migration,
  validation/clamping, deferred-save coalescing + reboot flush, relay count /
  chase-sweeps / spoof-pin / loop / labels / counters / STA / OTA (+install) /
  `/update` upload handlers, POST fuzz, factory reset, portal redirects.
- `test_system/` (3) — `select_reply()` matrix (golden/stage-1/stage-2/
  disabled/silent paths) + 24 h office-day sim: 86,400 polls, per-reply
  checksum validation, exact 5 s + 10 s spoof stages, write-silence mid-spoof,
  relay schedule + chase probes, hourly noise, green-all-day link + loop
  cycle/limit/counter proof.

Plus `tools/check_web_contract.py` — dashboard JS ↔ firmware route/key gate
(incl. dynamic `lblN` keys, `/api/ota`, `/update` form).

Run: `pio test -e native` (70: all except `test_web`) or `sh ../run_tests.sh`
(full 103: g++ suites + contract + `test_web` + `test_system` + soak).
