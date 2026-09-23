# Changelog

All notable changes to this project are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Every release ships Tasmota-style one-file images (`bms-tester-8mb.bin`,
plus `bms-tester-n16r8.bin` from v1.2 on): `write-flash 0x0 <file>`.

## [v2.7] — 2026-09-23
Release media:
[FULL](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-full.png) ·
[LITE](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-lite.png) ·
[demo GIF](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/demo.gif) ·
[protocol](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/protocol.png) ·
[wiring](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/wiring.png) ·
[terminal](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/terminal.png) ·
[suites](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/tests.png) ·
[tiles](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/tiles.png)
### Added
- Three dashboard variants, one per build (`WEB_UI_VARIANT` in
  `platformio.ini`; only the selected page compiles in, no flash bloat):
  **CLASSIC** (`s3-classic`, variant 0) = v2.6 page byte-verbatim;
  **FULL** (default envs, variant 1) = classic + live inline SVG bench card
  (48 V → bucks → ESP → MAX485 with TX/RX dot → A/B flow → meter readout,
  8 relay blocks glowing green, zero CDN — works on the offline AP) + tile
  glow transitions; **LITE** (`s3-lite`, variant 2) = relay tiles + names +
  LINK pill only (~4.9 KB page). Relay names stay NVS-persistent in all
  three (existing `lbl0..7` + labels card, unchanged wire path).
- Live meter readout keys in `/api/state` (read-only, never saved/restored):
  `mv`/`ma`/`msoc` (tenths + %) mirroring the last `0x03` reply — golden
  52.0 V / 0 A / 100 % or the active spoof stage — feeding the FULL bench
  SVG. Web-contract checker now validates all three page variants
  (endpoints ⊆ routes, ids per-page, union of state keys); new `test_web`
  asserts for `mv:520/ma:0/msoc:100`; all three variants proven compiling
  on host (`-DWEB_UI_VARIANT=0/1/2`).
- OTA assets unchanged (`firmware.bin` / `n16r8-firmware.bin` = FULL builds)
  so on-device updating keeps working on every box; classic/lite are
  manual-flash alternatives.
### Verified
- `pio test -e native` 103/103, `test_web` 49/49, web-contract PASS (3/3
  variants), 30-day soak PASS, OTA decision unit tests 6/6, GH release
  assets present under exact OTA names, download URL resolves HTTP 200.
  ESP-target compile + real OTA pull need hardware (toolchain uninstallable
  in this container; HIL skipped, no device).

## [v2.6] — 2026-09-22
### Added
- Daily meter-test counting, software-only estimate (no button, no new
  GPIO): `MeterBatch` (meters/attempts/pass/fail in RAM, flat NVS
  `m_met/m_att/m_ps/m_fl` flushed on close/reset only), fed from the live
  link state in `web_tick()` every loop. RED gap ≥ 3 s (`LINK_GAP_NEW_METER_MS`)
  closed by GREEN = reseat = new meter; steady GREEN across RESTARTs/retries
  = same meter; sub-3 s flickers stay on the same meter; a gap that elapses
  mid-cycle defers its close until IDLE so the in-flight verdict lands on the
  right meter. Manual New-day reset (no RTC on the box; boot persists via
  `begin()` amnesia + NVS overlay; a seated unit re-opens as meter #1; an
  empty bench stays 0 until first GREEN). Backups deliberately exclude
  counters (a restore must not resurrect yesterday's tallies). Console
  `DAYRESET` (admin-gated); `STATUS` prints the batch (`met=/att=/ps=/fl=`).
- Round link dots (green/red, `#dotG`/`#dotR`) in the dashboard header beside
  the LINK pill, driven by the same state — plus a "Meters today (approx)"
  card (live tallies + honest "estimate, no button needed" note + New-day
  reset). No `NEXT METER` button anywhere (a button version was built, then
  removed in the same release — the heuristic is the shipped behavior).
- One-file merged flash images per board (`firmware/bms-tester-8mb.bin`,
  `firmware-n16r8/bms-tester-n16r8.bin`: bootloader + partitions + app via
  `merge_bin`, structure-verified `E9`@0x0 / partition magic@0x8000 / app
  `E9`@0x10000, version + AP strings byte-present). Rolled out uniformly to
  every release v1.0–v2.5 (built from each tag's own source, old 3-file sets
  kept as the advanced path).
- `test/test_meter/` (10 Unity tests: first-GREEN opens #1, RESTARTs never
  open meters, cycle+gap = pass, retry loop = one verdict, flickers stay,
  abort+gap = fail, day reset + boot restore, 200-meter day boundary,
  millis-wrap + 50 k-attempt soak, mid-cycle gap defers to IDLE) — wired
  into the `pio test -e native` filter (now 10 suites, 103 tests).
- 5 web meter tests (link gaps driven over the real `web_tick` path, busy
  deferral, NVS persist-across-reboot, console verbs, dashboard surface incl.
  "no NEXT button" assertion) + 8 emu HTTP checks + new `/__bus` control-port
  endpoint driving the RS485 bus LEDs (the STA `__link` never touched them).
### Changed
- `FW_VERSION`/`STATUS?` → `2.6`. `/api/meter` is reset-only (`{"cmd":"reset"}`);
  `/api/state` exposes `m_met/m_att/m_ps/m_fl`; console help lists `DAYRESET`.
- `docs/CONFIG-SCHEMA.md` gains `meters.*`; `docs/MODULES.md` notes v2.6 adds
  no GPIO; `docs/PROTOCOL.md` confirms no wire change; wiki (Dashboard,
  Relays, Hardware, Flashing, Emulators, Home, Versions) + `llms.txt` +
  READMEs + `test/README.md` + `tools/README.md` + `arduino/README.md` all
  describe v2.6; `RS485-Tester-Report.docx` regenerated v2.6 (also fixes stale
  v2.5-report bits: login-page instructions dead since v2.3.1, `STATUS? 2.3`,
  8-day soak numbers, v2.4 folder map).
### Verified
- 152/152 (`run_tests.sh`: 7+8+13+4+36+11+10+6+5 Unity + contract + 49 web +
  3 system) + 30-day soak (2,591,400 polls, 309,625 cycles, wrap crossed,
  30 NVS commits) + both PIO envs SUCCESS + emu 62/62 + 48 h soak 10/10.

## [v2.5] — 2026-09-22
### Fixed
- Multipart upload auth: done handler demanded streamed field AND parsed
  arg — real servers never populate args for multipart, so every real
  upload 403'd (host stub masked it by injecting args). Streamed field
  alone now decides; security unchanged (wrong password still aborts).
- JSON helpers rejected `"key": value` whitespace (python-requests style);
  all three (`has`/`jnum`/`jstr`) share a tolerant core now (same bug class
  as the v2.3.1 OTA tag-space parse).
- Test-then-install was broken by design (one-shot STA test drops its link,
  but installs demanded a live link): check/install/URL now join with saved
  creds themselves (15 s, Tasmota-style blocking).
### Added
- Trigger group (enable + GPIO + polarity + Save-trigger; `sinv` finally
  wired end to end), captive-portal landing for phones (probes/CNA-UA get
  button + Safari steps, rest 302s to `/`), structured config schema
  (`docs/CONFIG-SCHEMA.md`, v1+v2 backups), socket emulation harness
  (`tools/fw_emu`: real handlers over real HTTP, 54 checks), 48 h
  end-to-end run (10 checks), 30-day soak (2.59 M polls, 309 k cycles,
  30 NVS commits), `docs/EMULATION-v2.5.md` per-feature report.
### Verified
- 137/137 (93 pio + 44 web) + contract + soak + virtual-bus + both PIO envs.

## [v2.4] — 2026-09-22
### Fixed
- Firmware upload rebuilt Tasmota-style (`src/fw_upload.h` gates, host-tested):
  exact variant-asset basename match (the old suffix idea would cross-flash —
  caught by its own unit test), explicit sketch budget instead of
  `UPDATE_SIZE_UNKNOWN`, `0xE9` magic + flash-size-vs-chip head gate, ONE
  `Update.end()` in the done handler (the old double-end lied about success),
  order-independent password collection, progress bar, named errors. A wrong
  file or password stages nothing and changes nothing.
- Relay review R1–R31 folded in: chase break-before-make (20 ms all-OFF gap;
  release is slower than pull-in), 500 ms post-stop start dead-band, live
  count-shrink safety, force-in-chase idles the wave, RESTART keeps forces,
  run-register snapshots (mid-cycle edits apply next cycle), timing floors
  (step ≥ 100 ms, stagger default 50, pause 500–60000 ms).
- Real firmware builds caught two host-stub lies: `Update.write` takes
  `uint8_t*` (not `char*`), and `String += char` binds the int overload
  (decimal garbage in the streamed password) — firmware appends via 1-char
  C string now.
### Changed
- Dashboard shows only the active mode's fields (per-mode menu); spoof card
  has Save-only next to FIRE; OTA check interval editable; AP/STA saves
  offer Save vs Save + reboot.
### Added
- Web console (`START STOP FIRE CANCEL STATUS UPTIME VERSION REBOOT RESET
  HELP`; hardware verbs admin-gated), config backup/restore (sectioned v2,
  passwords never exported), custom OTA URL + Upgrade-from-URL, one-shot STA
  uplink test (30 s, no reboot), Information card (variant/flash/sketch/heap/
  uptime/bootcount/reset reason/RSSI/MAC/pin map), mDNS `bmstester.local`,
  keep-WiFi reset, boot-counter reset.
### Verified
- 134/134 (93 pio incl. 5 upload-gate tests + 41 web) + contract (single-end
  rule, no-`SIZE_UNKNOWN`, portal-surface rules) + soak + virtual-bus + both
  PIO envs.

## [v2.3.1] — 2026-09-21
### Fixed
- Saves stick: the 1 s tick is status-only; forms fill on load + after saves
  (dirty-tracking + fetch-failure tolerance); user edits never clobbered.
- UTF-8 on all pages (garbled letters gone); OTA check tag parse tolerates
  the API's space-after-colon + dashboard Install button works.
### Changed
- Login wall REMOVED (WPA2 is the gate): reboot/reset/upload/OTA-admin/AP
  saves ask the admin password per request (default `admin123`); passwords
  scrubbed from `/api/state`.
- Chase hold is automatic (`Chase sweeps`, default 3, 0 = forever).
### Added
- Spoof trigger GPIO configurable (safe-pin allowlist, else 21, live re-arm).
  WiFi kill switch: ground GPIO18 to drop AP+portal+server, release restores.
### Removed
- Session logins + remember-me NVS slots; legacy `hch` hold (replaced by
  `swp` auto-hold — upgraders keep 3 sweeps).
### Verified
- 103/103 (77 pio + 26 web) + contract + soak.

## [v2.3] — 2026-09-21
### Added
- Relay count (first N of 8; beyond-N forced OFF + greyed tiles) + chase-wave
  mode (3rd sequence mode: single lit relay sweeping R1→Rn→R1, wraps, both
  directions, all button modes apply).
- 2-stage spoof (stage 1 "100" 5 s → stage 2 88.8/88.8/88.8/188 10 s; values
  + seconds both editable; upgraders' single-stage values migrate to stage 2).
- Per-mode holds in ms (`hseq`/`hall`, 0 = forever each); industrial pack:
  loop + pause + cycle limit, ALL-ON stagger (inrush ramp), direction, 8
  relay labels, cycle/actuation QC counters, boot auto-start.
- Persistent logins (remember-me, 30-day NVS slots ×4); OTA via offline
  `/update` upload or automatic GitHub checks/installs over the optional STA
  uplink; coalesced config saves (dirty-flag, 1.5 s flush — no loop stalls).
- NVS `bms2` v3 with tested v2→v3 migration (seconds×1000 fanned to holds).
- 38 new tests (**105/105**: 70 pio + 29 web; contract gate + soak alongside).
### Changed
- `FW_VERSION`/`STATUS?` → `2.3`; dashboard version is live from the device.

## [v2.2] — 2026-09-21
### Fixed
- Login page pops automatically on join (captive portal DNS catch-all +
  unknown-URL 302 redirect); AP IP pinned to 192.168.4.1. Report + wiki warn:
  turn mobile data off — phones route around "no internet" networks.
### Added
- Portal-redirect flow host test (15 web tests → **67/67** total).
### Changed
- `FW_VERSION`/`STATUS?` → `2.2`.

## [v2.1] — 2026-09-21
### Added
- 14 host-executed website tests + JS↔firmware contract gate
  (`tools/check_web_contract.py`) + 24 h office-day sim (86,400 polls,
  per-reply checksum validation) + Wokwi automation scenario (`sim.yaml`)
  with token-gated CI sim job (**66/66** total).
- Wokwi diagram verified against official docs and fixed (NeoPixel VDD/VSS,
  button pins); 8 real relay-module parts with NO-contact indicator LEDs.
### Changed
- `select_reply()` host-covered refactor (behavior-identical).
  `FW_VERSION`/`STATUS?` → `2.1`. Bench-confirmed: relay idle HIGH,
  ON-when-LOW = active-LOW default.

## [v2.0] — 2026-09-21
### Added
- 8-relay sequencer (sequential/all-ON, 3 button behaviors: hold-abort /
  run-lock / restart; boot-safe OFF-first drive; polarity toggle); always-on
  AP dashboard (login auth, NVS persistence, admin reset); spoof window
  (88.8/88.8/88.8/188 on `0x03`, configurable duration, auto-revert).
- 18 new tests (**50/50**); Wokwi relay LEDs + buttons; CI builds both envs.
### Changed
- `FW_VERSION`/`STATUS?` → `2.0`; radio on (AP always broadcasting).
  v1.x responder core frozen and re-proven.

## [v1.2] — 2026-09-21
### Added
- Onboard WS2812 RGB mirror (GPIO48, built-in `neopixelWrite`, brightness 32):
  same green/red state as the discretes, zero extra wiring, driven from the
  250 ms eval (never the hot RX loop).
- N16R8 build (`s3-n16r8`: 16 MB flash, OPI PSRAM, `default_16MB.csv`);
  8 MB env kept for Wokwi. Wokwi NeoPixel part on GPIO48; `firmware-n16r8/`
  ready-to-flash triple; Arduino IDE board menu documented.
### Changed
- `FW_VERSION`/`STATUS?` → `1.2`; MODULES/arduino/wokwi/src READMEs, llms.txt.
### Verified
- 32/32 + soak green; both envs compile; virtual-bus PASS; golden bytes +
  `STATUS?` strings byte-present in both `firmware.bin` images.

## [v1.1] — 2026-09-18
### Added
- Multi-register canned replies (`0x03` golden 52.0 V/100 %, `0x04` 14×3714 mV,
  `0x05` name); silence on writes/unknown (option A — explicit user-confirmed
  choice); adaptive 2–10 s link window (EMA of poll intervals); streaming
  checksum-verified parser with noise re-sync; 21 new tests (**32/32**);
  `arduino/`, `firmware/` triple, `captures/`, `virtual_meter.py` scenario
  modes, `soak_sim.cpp`, Wokwi meter.
### Fixed
- Parser emits only on checksum success (a corrupt-CK + trailing `0x77` used
  to emit — gated, now covered by test); Arduino `B1` macro collision
  (parser states renamed `JST_*`, broke the S3 build once).

## [v1.0] — 2026-09-18
### Added
- 0x03-only responder, fixed 2 s link window, green/red discrete LEDs,
  USB-serial `STATUS?` (`GREEN`/`RED`), 11/11 tests, S3 binary, Word report.
  Frozen as `releases/bms-connection-tester-v1.0.zip` + tag `v1.0`; responder
  core untouched by all v2.x work (original tests byte-identical).
