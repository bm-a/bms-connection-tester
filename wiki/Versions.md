# Versions / Changelog (Keep a Changelog)

## [v2.8] — 2026-09-27
### Meter counter: 5 s settle fumble guard
- A link gap ≥ 3 s now only **arms** a reseat candidate; the new meter
  **commits** after GREEN holds 5 s (`LINK_SETTLE_NEW_METER_MS`). A scratchy
  reseat (seat 4 s, pull, seat properly) no longer double-counts one unit.
- The closing meter's verdict is **snapshotted at arm time**, so an eager
  START during the settle window attributes to the new meter; a RED before
  the settle merges the window's activity back (same meter all along).
- A yank mid-cycle **invalidates the test**: the previous meter closes as
  fail (an interrupted test was never honestly a pass).
- Dashboard Meters-today note updated; `test_meter` grows fumble +
  eager-start cases (12/12).

### Unified release line
- One v2.8 tag ships **generic FULL** (8 MB ESP32-S3) + **Waveshare**
  (ESP32-S3-ETH-8DI-8RO) firmware. The Waveshare `-wsN` prerelease line is
  retired: both boards report `FW_VERSION 2.8` and both OTA-check
  `/releases/latest`, each downloading its own variant asset
  (`firmware.bin` / `waveshare-firmware.bin`).

### Docs from scratch
- In-depth README rewrite; two new from-scratch build guides:
  `docs/BUILD-DIY.md` (generic board, build it yourself) and
  `docs/BUILD-WAVESHARE.md` (all-in-one board); this wiki overhauled page
  by page (new [[Building]] page).

### Verified
- **166/166** native tests (`test_meter` 12/12, `test_web` 49/49,
  `test_waveshare` 12/12), 3-variant web contract PASS, 30-day soak PASS.

## [v2.7] — 2026-09-23
### Dashboard variants at a glance

| FULL (default) | LITE |
|---|---|
| ![FULL](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-full.png) | ![LITE](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-lite.png) |
| bench SVG + tiles + meters + all cards | tiles + names + LINK pill only |

![Sequential + spoof demo](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/demo.gif)

### Added
- Three dashboard variants, one per build (`WEB_UI_VARIANT`):
  CLASSIC (`s3-classic`) = v2.6 page verbatim; FULL (default) = classic +
  live inline SVG bench card + tile glow + meter readout (`mv/ma/msoc`);
  LITE (`s3-lite`) = relay tiles + names + LINK pill only. Relay names
  NVS-persistent in all three. OTA assets stay FULL builds.
- Proven: 103/103 native, 49/49 web, contract 3/3, soak PASS, OTA units 6/6,
  GH assets present, download URL HTTP 200. ESP compile + real OTA pull
  need hardware (HIL skipped).

## [v2.7-ws1] — 2026-09-26 (Waveshare board line, retired in v2.8)
### Added
- Waveshare ESP32-S3-ETH-8DI-8RO board variant (`s3-waveshare`): relays via
  TCA9554PWR @ I2C `0x20` (SDA42/SCL41, HIGH bit = ON), isolated RS485
  TX17/RX18 with hardware auto-direction, BOOT (GPIO0) = START/STOP,
  DI1 (GPIO4) = spoof trigger, DI2 (GPIO5) = WiFi kill, RGB on GPIO38.
  Own OTA line (`FW_VERSION 2.7-ws1`, `waveshare-firmware.bin`,
  `v2.7-wsN` prerelease tags). Proper 1.04 MB merged image
  (bootloader@0x0 + partitions@0x8000 + app@0x10000).
- Superseded by the unified v2.8 release (no more `-wsN` tags).

## [v2.6] — 2026-09-22
### Added
- Daily meter-test counting (software-only estimate, no button, no new
  GPIO): `MeterBatch` (meters/attempts/pass/fail in RAM, flat NVS
  `m_met/m_att/m_ps/m_fl` flushed on close/reset only), fed from the live
  link state every loop — RED gap ≥ 3 s closed by GREEN = reseat = new
  meter; steady GREEN across RESTARTs = same meter; flickers stay; mid-cycle
  gaps defer to IDLE. Manual New-day reset (no RTC; boot persists, backups
  exclude counters, seated unit re-opens as #1). Console `DAYRESET`.
- Round link dots (green/red) in the dashboard header beside the LINK pill.
- One-file flash images: `firmware/bms-tester-8mb.bin` +
  `firmware-n16r8/bms-tester-n16r8.bin` (merged bootloader+partitions+app,
  `write-flash 0x0 <file>`, structure-verified).
- Spoof edit+save confirmed as the existing two-stage feature (no new mode):
  stage 1 = all-1s (100), stage 2 = all-8s (88.8/188), both editable.
- 10-test `test_meter` suite + 5 web meter tests + 8 emu HTTP checks
  (plus new `/__bus` emu control for the RS485 bus state).

## [v2.5] — 2026-09-22
### Fixed
- Multipart upload auth: the done handler demanded streamed field AND parsed
  arg, but real servers never populate args for multipart — every real
  upload 403'd (host stub had masked it). Streamed field alone now decides.
- JSON parser rejected `"key": value` whitespace (python-requests style) —
  same bug class as the v2.3.1 tag-space parse. All helpers tolerant now.
- On-demand STA join: check/install/URL join with saved creds themselves
  (test-then-install finally works end to end).
### Added
- Trigger group (enable + GPIO + polarity + Save-trigger), captive-portal
  landing page for phones, structured config schema (`docs/CONFIG-SCHEMA.md`,
  v1+v2 backups), socket emulation harness (`tools/fw_emu`, 64/64 over real
  HTTP), 30-day soak (2.59 M polls, 309 k cycles, 30 NVS commits),
  `docs/EMULATION-v2.5.md` per-feature report.
- 135/135 host tests (44 web) + contract + soak + virtual-bus + both PIO envs.

## [v2.4] — 2026-09-22
### Fixed
- Firmware upload rebuilt Tasmota-style: exact variant-asset match, explicit
  sketch budget, image-head gate, single finalize, progress bar, named errors
  (a wrong file or password changes nothing).
### Changed
- Relay timing floors (step ≥ 100 ms default 250, stagger default 50,
  pause 500–60000 default 2000); chase gains a fixed 20 ms break-before-make;
  START refused 0.5 s after STOP; mode switch needs IDLE; loop needs a
  finite hold; count-shrink acts immediately; RESTART keeps tile forces.
- Dashboard shows only the active mode's fields; spoof card has Save-only;
  OTA interval editable; AP/STA saves offer Save + reboot.
### Added
- Web console, config backup/restore (passwords never exported), custom OTA
  URL + Upgrade-from-URL, one-shot STA uplink test (no reboot), Information
  card (variant/flash/sketch/heap/uptime/bootcount/reset reason/RSSI/pin
  map), mDNS `bmstester.local`, keep-WiFi reset, boot-counter reset.
- 134/134 tests (36 relay incl. R18–R31, 41 web, 5 upload gates) + contract
  (single-end rule, no SIZE_UNKNOWN) + soak + virtual-bus + both PIO envs.

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
