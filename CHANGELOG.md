# Changelog

All notable changes to this project are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [v2.3.1] — 2026-09-21
### Fixed
- Dashboard save race: the 1 s state refresh overwrote any field the moment
  it lost focus, so clicking Save after editing posted the stale device
  value ("options reset to before one" unless you beat the poll). The 1 s
  tick is now status-only (link/relays/counters/OTA/STA); form fields fill
  once on load and after each successful save, user-edited (dirty) fields
  are never clobbered, and failed fetches no longer kill the tick.
- Same root cause explained the "chase sweeps only 3 relays" bench report:
  the chase engine was correct — the persisted relay count was stale.
  With saves sticking, count/chase/hold edits apply as shown.
- Garbled dashboard letters: pages had no charset; all three now declare
  UTF-8 (`⚡ → ∞ °` render correctly, incl. portal mini-browsers).
- GitHub OTA check never matched: the API pretty-prints `"tag_name": "v2.3"`
  (space after colon) but the parser wanted no space — every check died as
  "bad api reply". Tolerant parse + a dashboard **Install update** button
  (password-gated) so a found release is one tap away.
### Changed
- Login wall removed (WPA2 AP password is the gate). Reboot, factory reset,
  `/update` upload, OTA-admin and AP/admin saves now ask for the admin
  password per request (default `admin123`, changeable; blank = keep).
  Password values no longer appear in `/api/state`.
- Chase hold is now automatic: `Chase sweeps` (default 3, 0 = forever);
  effective hold = sweeps × relays × step, retuned at every start.
  The old `Hold chase ms` field is retired (stale NVS key ignored).
### Added
- Spoof trigger GPIO is configurable on the dashboard (default 21, saved on
  FIRE). Only proven-safe free DIOs are accepted (1, 2, 21, 38–44, 47);
  anything else falls back to 21. The pin re-arms live on change.
- WiFi kill switch: grounding GPIO18 (free, non-strapping) drops the AP +
  portal + server immediately; releasing it brings everything back.
  Default on at boot, debounced like the main button.
### Notes
- `nvs_open failed: NOT_FOUND` once on first boot is benign (read-only
  open before the first commit creates the namespace).

## [v2.3] — 2026-09-21
### Added
- Relay count (first N of 8 participate, web `Relays`, beyond-N forced OFF
  and greyed out) + chase-wave mode (single lit relay sweeping R1→Rn, wraps;
  3rd `rmode`, all 3 button behaviors apply).
- 2-stage spoof: stage 1 ("100" realistic full pack, 5 s) then stage 2
  (88.8/88.8/88.8/188 pattern, 10 s), then auto-revert — both stages fully
  editable (values + seconds each) from the web or pin trigger.
- Per-mode holds in milliseconds (`hseq`/`hch`/`hall`, 0 = forever each;
  ALL-ON default 5 min soak). The old seconds `hold` is gone (it caused the
  "relays won't turn off" confusion at 0).
- Industrial pack: loop + inter-cycle pause + cycle limit (burn-in),
  ALL-ON stagger (inrush ramp), direction fwd/rev, 8 relay labels (QC names
  on tiles), cycle + actuation counters on the dashboard, boot auto-start.
- Persistent logins: "remember this device" (30-day NVS token slots ×4,
  reboot-safe); HttpOnly + SameSite=Lax cookies.
- OTA: manual `/update` firmware upload (works fully offline) + automatic
  GitHub-release checks/installs when the optional STA uplink (phone hotspot)
  is online. AP stays always-on regardless; auto-gate requires idle bench +
  60 s silent bus + no running sequence.
- Live firmware version on the dashboard (from `/api/state`, never stale).
- 38 new tests: **105/105 passing** (70 pio-native incl. new `test_ota`,
  29 web, contract gate). NVS `bms2` v3 with v2→v3 migration tested
  (seconds×1000 fanned to all holds, singles→stage 2, 100-first order).
### Changed
- Config saves no longer stall the loop: handlers mark dirty, `web_tick()`
  commits once after 1.5 s idle (3 rapid saves = 1 flash write, proven by
  test); reboot/reset flush synchronously first.
- `FW_VERSION`/`STATUS?` report `2.3`.
### Fixed
- Host-stub fidelity bug #4: single-char `String::indexOf(' ')` returns
  garbage on the stub (truncated auth cookies depending on token content) —
  cookie parsing now uses the `const char*` overload (caught by 2 web tests).
- Dashboard `handle_state` statement terminated early by a stray `;`, dropping
  the spoof keys from `/api/state` (caught by 2 web tests before release).

## [v2.2] — 2026-09-24
### Fixed
- Web page now actually opens on phones: captive portal (DNS catch-all to
  192.168.4.1) pops the login page on join; unknown URLs redirect to the
  dashboard instead of a dead 404; AP IP pinned to 192.168.4.1 via
  `softAPConfig`. Report + wiki now warn about the classic trap: phones route
  around "no internet" networks — turn mobile data off / stay connected.
### Added
- Portal-redirect flow covered by a new host test (15 web tests total).

## [v2.1] — 2026-09-23
### Added
- 16 website reliability tests (`test_web`, host-executed real `web_ui.cpp`
  on Arduino stubs): auth, session expiry, validation/clamping, NVS round-trip,
  relay/seq/spoof/admin handlers, POST fuzz, factory reset.
- `test_system` 24 h office-day sim: 86,400 polls with per-reply checksum
  validation, exact 10 s spoof window, relay schedule probes, abort/restart,
  hourly noise, write-silence mid-spoof — all green in 0.05 s.
- `tools/check_web_contract.py`: dashboard JS ↔ firmware route/key consistency
  gate (runs in `run_tests.sh` + CI).
- `wokwi/sim.yaml` automation scenario (button → relay pins, spoof → `22 B0`
  on meter console) + token-gated CI sim job; diagram fixed against official
  docs (NeoPixel VDD/VSS, button 1.l/2.l) and upgraded to 8 real relay-module
  parts (npn = energize-on-LOW, matching active-LOW default) with NO-contact
  indicator LEDs.
### Changed
- `select_reply()` extracted to `relay_ctrl` (behavior-identical; main loop
  uses it — now host-covered), `web_setup` resets identity to defaults before
  NVS overlay (deterministic repeated setup).
- `FW_VERSION`/`STATUS?` report `2.1`. Confirmed bench fact: relay idle HIGH,
  ON when LOW = active-LOW default is correct, no polarity change.
### Fixed
- Host-stub fidelity bugs found by the new suites (separate mock clocks per
  TU, NVS clear missing number store) — stubs now match real core semantics
  (`constrain` macro, 2-arg `indexOf`).

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
