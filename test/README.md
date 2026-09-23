# test/ — automated tests (152 passing: 103 via pio + 49 web via g++)

![Suite results](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/tests.png)

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
- `test_relay/` (36) — sequencer: boot OFF, sequential stepping, per-mode
  holds, ALL-ON (+ default 50 ms stagger ramp), chase wave + 20 ms
  break-before-make gaps (bitmask weight 1 lit / 0 in gap), relay count
  scoping + live shrink/regrow safety, loop/pause/limit, reverse direction,
  QC counters, all 3 button behaviors, manual override (+ force-in-chase
  idles the wave, RESTART preserves forces), post-stop start dead-band,
  mid-cycle edit latching, polarity helper, millis rollover, debounce edges.
- `test_upload/` (5) — Tasmota-grade update gates: explicit sketch budget,
  per-variant asset pick, exact-basename filename match (suffix-trap proof),
  image-head magic + flash-size-vs-chip matrix, error vocabulary coverage.
- `test_spoof/` (11) — stage-1 ("100") + stage-2 (88.8/88.8/88.8/188) frame
  bytes, checksum self-consistency, custom values, `SpoofPlan` stage
  timing/handoff/cancel/retrigger/rollover, legacy window (frozen) timing.
- `test_meter/` (10) — v2.6 link-gap heuristic: first GREEN opens meter #1,
  RESTARTs on steady GREEN never open meters, cycle+gap = pass, retry loop =
  one verdict, sub-3 s flickers stay, abort+gap = fail, day reset + boot
  restore, 200-meter day boundary, millis-wrap + 50 k-attempt soak, mid-cycle
  gap defers its close until IDLE.
- `test_ota/` (6) — version compare (incl. `2.10 > 2.9`), per-variant asset
  pick, download-URL build + tag sanitizing, auto-check gate matrix.
- `test_web/` (49, g++-only) — real `web_ui.cpp` on host stubs (`Arduino.h`,
  `WiFi.h` + STA/RSSI/IP, `WebServer.h` + request/upload drivers incl.
  pass-last order, `Preferences.h` + commit counter, `Update.h` + single-end
  + failure injection, `ESPmDNS.h`, `esp_system.h`): per-request admin
  password gating, WiFi kill switch + mDNS, portal landing (probes/CNA-UA),
  NVS v2→v3 migration, validation/clamping + named rejects (R9/R10),
  whitespace-tolerant JSON, deferred-save coalescing + reboot flush, relay
  count / chase-sweeps / spoof-pin+save+trigger / loop / labels / counters /
  STA + one-shot test + on-demand join / OTA (+install, URL, interval) /
  Tasmota `/update` upload (gates, rejects, order-independence, streamed-only
  password) / console verbs (`DAYRESET`, meter batch in `STATUS`) /
  `/api/meter` reset-only + link-gap closes over the real `web_tick` path /
  backup v2 + restore v1/v2 / keep-WiFi reset +
  bootcount / info fields, POST fuzz, factory reset, portal redirects.
- `test_system/` (3) — `select_reply()` matrix (golden/stage-1/stage-2/
  disabled/silent paths) + 24 h office-day sim: 86,400 polls, per-reply
  checksum validation, exact 5 s + 10 s spoof stages, write-silence mid-spoof,
  relay schedule + chase probes, hourly noise, green-all-day link + loop
  cycle/limit/counter proof.

Plus `tools/check_web_contract.py` — dashboard JS ↔ firmware route/key gate
(incl. dynamic `lblN` keys, `/api/ota`, `/update` form, new v2.4 endpoints,
per-mode element ids, single-`Update.end(true)` + no-`SIZE_UNKNOWN` rules).

Run: `pio test -e native` (103: all except `test_web`) or `sh ../run_tests.sh`
(full 152: g++ suites + contract + `test_web` + `test_system` + soak).
