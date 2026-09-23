# HANDOFF — bms-connection-tester project (full brain dump)

> Written 2026-09-18, before Termux storage compression.
> Last updated 2026-09-23 for **v2.7** (three dashboard variants FULL/CLASSIC/
> LITE via `WEB_UI_VARIANT`, live SVG bench + `mv/ma/msoc` readout, persistent
> relay names everywhere, offline 3D bench `local-wokwi/` + new public repo
> `bms-tester-sim`, enclosure/GX16 docs, release snapshots + demo GIF;
> 103/103 + 49/49 + contract 3/3 + soak green; v2.7 tag + release live WITH A
> GAP: no firmware bins — no ESP toolchain in this container, OTA v2.6→v2.7
> checks fine but installs 404 until bins are built+uploaded on the bench PC).
> COMPACTION CHECKPOINT 2026-09-21: user compacting Termux storage. All 3
> repos verified pushed + clean (main `2b23226` + tag `v2.3` upstream, release
> live, CI green; handbook `ba3ac6f`; emulator `1fd0b2a`). `releases/` (564 KB,
> gitignored offline backup) must be copied somewhere compaction won't touch.
> If `/opt/*` inside Debian is gone post-compaction, rebuild order is
> pioenv → PIO packages (+Xtensa tarball workaround) → QEMU (sanitized PATH).
> §12's release checklist REQUIRES updating this file in the same commit as
> any feature — a body left stale behind an addendum is a bug (2026-09-22).
> Audience: the next AI agent (or future me) picking this project up cold.
> Goal: everything needed to resume without re-discovering anything.
> **Read order for a new agent:** §1 → §2 → §5 → §8 → §11.

---

## 1. What this project is (30 seconds)

An ESP32-S3 box that impersonates a **JBD / Xiaoxiang Smart BMS** over RS485 so
compatible meters/displays can be exercised **without the real battery pack**.
Two LEDs: green = valid BMS traffic seen, red = bus silent.
**v2.3 is the relay test bench grown up**: relay count (first N) + chase-wave
mode, per-mode millisecond holds, loop/pause/cycle-limit burn-in, ALL-ON
stagger, direction, relay labels, QC counters, boot auto-start; 2-stage spoof
(100 first, then 88.8/88.8/88.8/188); manual + automatic
OTA; coalesced config saves. No screens needed.
**v2.3.1 (bench patch)**: login wall REMOVED (per-request admin password for
reboot/reset/upload/OTA-admin/AP saves); 1 s tick is status-only (forms fill
on load + after saves); UTF-8 pages; chase auto-hold sweeps (retires `hch`);
configurable spoof trigger GPIO (safe-list clamped); WiFi kill switch on
GPIO18; fixed GitHub tag parse + dashboard Install button.

- **Origin story:** user's dad runs an e-rickshaw meter assembly line; workers
  needed the real battery or a laptop to verify RS485 wiring. This box replaces
  both. (Repos are now deliberately **generic** — e-rickshaw is one use case.
  The Word report for dad keeps the e-rickshaw framing on purpose.)
- **People:** user = Bhavishya Madan (GitHub `bm-a`). Dad = electronics-strong,
  code-weak; gets status via a WhatsApp Word report, not GitHub.
- **Current release: v2.7** (three dashboard variants: FULL default with live
  inline SVG bench card + `mv/ma/msoc` meter readout, CLASSIC = v2.6 page
  byte-verbatim, LITE = relays+names+link; relay names NVS-persistent in all;
  OTA assets stay FULL; everything else = v2.6 behavior).
  v1.x responder core FROZEN (parser, option-A, tracker, golden frames, LEDs,
  STATUS?, original 32 tests byte-identical). v1.0 frozen (ZIP + git tag,
  untouched since).

---

## 2. The GitHub repos (all public, user `bm-a`)

| Repo | URL | Contents | State |
|---|---|---|---|
| `bms-connection-tester` | https://github.com/bm-a/bms-connection-tester | Firmware, 152 tests, docs, binaries, Wokwi, CI, wiki | main pushed; tags `v1.0`–`v2.7`; Releases: every release ≤ v2.6 carries Tasmota-style one-file images next to the separate files — v1.0 (ZIP+1), v1.1 (4+1), v1.2–v2.6 (7+2 = 9 each, v2.6 incl. docx); **v2.7 carries dash-full.png + dash-lite.png + demo.gif ONLY (no bins — see §11.2)**; wiki live (8 pages) |
| `bms-tester-sim` | https://github.com/bm-a/bms-tester-sim | Offline 3D bench simulator (subtree of `local-wokwi/`): ESP32-S3 + MAX485 + photo meter + 8 relays + GX16, electron-flow viz, pressable buttons, relay board + persistent names, verbatim ESP page embed, `shots.py` snapshots | Pushed (subtree, main) |
| `jbd-bms-rs485-handbook` | https://github.com/bm-a/jbd-bms-rs485-handbook | Electronics explainer: RS485, MAX485, S3 pins, JBD protocol, build guide, FAQ + `llms.txt` | Pushed (single commit + later edits if any — check `git log`) |
| `esp32s3-qemu-arm64` | https://github.com/bm-a/esp32s3-qemu-arm64 | Build/run scripts for Espressif QEMU on ARM64, M25P80 flash patches, Termux/proot notes | Pushed (2 commits) |

Cross-links: main README ↔ handbook; emulator README references both.
Topics set for search (main: 15 topics incl. `jbd-bms`, `rs485`, `bms-emulator`,
`xiaoxiang`, `hardware-in-the-loop`; handbook: 7; emulator: 4).
`llms.txt` (AI-agent summary standard) exists in main + handbook repos.

**Local clones** (Termux home, source of truth — push before compressing!):
- `/data/data/com.termux/files/home/bms-connection-tester` (this file lives here)
- `/data/data/com.termux/files/home/jbd-bms-rs485-handbook`
- `/data/data/com.termux/files/home/esp32s3-qemu-arm64`

---

## 3. Version history

- **v1.0** (`ff4044d`, tag `v1.0`): 0x03-only responder, fixed 2 s green window,
  11/11 tests, S3 firmware built. Frozen in
  `releases/bms-connection-tester-v1.0.zip` (22 files, git-ignored) + tag.
- **v1.1** (tags moved forward with each finalization; final commits
  `1d2a710` → `1593231` → `183aa32` → `10be208` → `8fd3d99`, tag `v1.1`):
  multi-register (03/04/05) + adaptive window + silence-on-unknown (option A) +
  hardened streaming parser; **32/32 tests**; S3 firmware rebuilt;
  Arduino sketch; flash binaries; docs; CI; generic repo identity.
- **v1.2** (`67e9ebe`, tag `v1.2`): onboard WS2812 RGB mirror (GPIO48,
  `neopixelWrite`, no extra lib) + N16R8 build (`s3-n16r8`: 16 MB flash, OPI
  PSRAM) + Wokwi NeoPixel; `firmware/` + `firmware-n16r8/` triples; 32/32 +
  soak green; virtual-bus PASS; QEMU known gap re-confirmed.
- **v2.0** (`71b0bc6`, tag `v2.0`): relay test bench — 8-relay sequencer
  (`src/relay_ctrl.*`: sequential/all-ON, 3 button behaviors, debounce, hold
  timer, boot-safe OFF-first, polarity toggle), always-on AP dashboard
  (`src/web_ui.*`: dark UI, login, NVS `bms2`, admin reset, `BMS-Tester`),
  spoof window (88.8/88.8/88.8/188 on `0x03`, configurable, auto-revert);
  18 new tests (**50/50** at the time); Wokwi relay LEDs + buttons; CI builds both envs.
  Responder core frozen and re-proven on the same source.
- **v2.1** (tag `v2.1`): website reliability, zero behavior change —
  14 host-executed web tests (real `web_ui.cpp` on Arduino stubs:
  `test/test_web/Arduino.h|WiFi.h|WebServer.h|Preferences.h`),
  `tools/check_web_contract.py` gate, `select_reply()` extracted host-covered,
  `test_system` 24 h office-day sim (86,400 polls, per-reply CK validation),
  Wokwi diagram fixed per official docs + 8 relay-module parts + `sim.yaml`
  automation with token-gated CI job. Bench-confirmed relay idle HIGH /
  ON-when-LOW = active-LOW default stands. **66/66 total** (52 pio + 14 web).
- **v2.2** (tag `v2.2`): user reported the page never opened on their phone.
  Root causes ranked: (1) no captive portal — phones show "no internet" and
  route via mobile data / never pop a page; (2) unknown URLs 404'd; (3) AP IP
  only implicit. Fix: `DNSServer` catch-all to `WiFi.softAPIP()` (login pops
  on join), `onNotFound` → 302 `/`, explicit
  `softAPConfig(192.168.4.1/24)`; report + wiki warn to turn mobile data off.
  Portal flow host-tested (15 web tests). **67/67 total**. Other bench
  possibility (old v1.x firmware flashed = no WiFi at all) left as a
  diagnostic question to the user: is `BMS-Tester` visible? what does
  `STATUS?` report?
- **v2.3** (tag `v2.3`): relay count 1–8 (beyond-N forced OFF + greyed) +
  chase-wave mode (3rd `rmode`, single lit sweep, wraps, all button modes) +
  2-stage spoof (stage 1 "100" 5 s → stage 2 88.8/188 10 s, both editable) +
  per-mode ms holds (`hseq`/`hch`/`hall`, 0 = forever each) + industrial pack
  (loop + pause + cycle limit, ALL-ON stagger, fwd/rev, 8 labels, cycle/act
  counters, boot auto-start) + persistent logins (remember-me, 30-day NVS
  slots ×4) + OTA (offline `/update` upload + auto GitHub checks/installs over
  optional STA uplink) + coalesced config saves (dirty-flag, 1.5 s flush) +
  live dashboard version. NVS `bms2` v3 (tested v2→v3 migration). Bench answers
  baked in: 188-shows-100 = meter-side clamp (proven), relays-stuck-on = hold
  0 = forever (per-mode holds now explicit). **105/105 total** (70 pio +
  29 web + contract).
- **v2.3.1** (bench patch, this release): login wall REMOVED (per-request
  admin password gates reboot/reset/`/update`/OTA-admin/AP saves; passwords
  scrubbed from `/api/state`); 1 s tick is status-only (forms fill on load +
  after saves; dirty-tracking + fetch-failure tolerance); UTF-8 on all pages;
  chase auto-hold `swp` (retires `hch`); spoof trigger GPIO configurable
  (allowlist-clamped, NVS `spin`, live re-arm); WiFi kill switch on GPIO18;
  fixed GitHub tag parse (space-tolerant) + dashboard Install button.
  **103/103 total** (77 pio + 26 web + contract).
- **v2.4** (this release): Tasmota-grade `/update` (`fw_upload.h` gates:
  exact asset basename, explicit budget, image-head check, single finalize,
  named errors) + OTA-pull same gates + custom URL; per-mode relay menu
  (chase 20 ms BBM, 500 ms stop dead-band, step ≥ 100 dflt 250 / stagger
  dflt 50 / pause 500–60000 dflt 2000, R9/R10 rejects, live count-shrink,
  RESTART keeps forces); spoof Save-only; console; backup/restore (no
  passwords); STA one-shot test; info card (variant/flash/sketch/heap/
  uptime/bootcount/reset-reason/RSSI/pin map); mDNS `bmstester.local`;
  keep-WiFi + bootcount resets; editable OTA cadence.
  **134/134 total** (93 pio + 41 web, incl. 5 upload-gate tests).
- **v2.5** (this release): trigger group (enable+GPIO+polarity+Save, `sinv`
  wired end to end); captive-portal landing (probes/CNA-UA get button +
  Safari steps, rest 302s to `/`); structured config (`docs/CONFIG-SCHEMA.md`,
  v2 sectioned + v1 flat backups); on-demand STA join for check/install/URL;
  whitespace-tolerant JSON core; socket harness (`tools/fw_emu`, 54 checks)
  + 48 h run (10 checks) + 30-day soak (2.59 M polls, 309 k cycles, 30 NVS
  commits) + `docs/EMULATION-v2.5.md`. Harness-caught fixes: multipart-arg
  403s, whitespace rejects, test-then-install gap, missing `seq.begin()` in
  emu, console ok-masking. **137/137 total** (93 pio + 44 web).
- **v2.6** (this release): software-only daily meter estimate — `MeterBatch`
  (meters/attempts/pass/fail in RAM, flat NVS `m_met/m_att/m_ps/m_fl`
  flushed on close/reset only), fed from the live link state in `web_tick()`:
  RED gap ≥ 3 s (`LINK_GAP_NEW_METER_MS`) closed by GREEN = reseat = new
  meter; steady GREEN across RESTARTs = same meter; sub-3 s flickers stay;
  mid-cycle gaps defer to IDLE; manual New-day reset (no RTC; boot persists
  via `begin()` amnesia + NVS overlay; seated unit re-opens as #1; backups
  exclude counters); console `DAYRESET`. Round link dots (`#dotG/#dotR`) in
  the dashboard header. One-file merged flash images per board
  (`bms-tester-8mb.bin` / `bms-tester-n16r8.bin`, `merge_bin`,
  structure-verified). `test_meter` (10) added to pio native filter (103);
  `/__bus` emu control for the RS485 state (the STA `__link` never touched
  bus LEDs). Harness-caught: dayReset seat-count, begin-vs-dayReset link
  amnesia, STA/bus link conflation. **152/152 total** (103 pio + 49 web);
  emu 62/62 + 10/10 soak; v2.6 docx.
- **v2.7** (this release): three dashboard variants, one per build
  (`WEB_UI_VARIANT` in `platformio.ini`, only the selected page compiles in):
  FULL (default envs) = v2.6 page + live inline SVG bench card (48V→bucks→
  ESP→MAX485 DE dot→animated A/B flow→meter readout, 8 glowing relay blocks,
  zero CDN) + tile glow transitions; CLASSIC (`s3-classic`) = v2.6 page
  byte-verbatim (19,799 B, diff-proven); LITE (`s3-lite`) = relay tiles +
  names + LINK pill (~4.9 KB page). Relay names NVS-persistent in all three
  (existing `lbl0..7` path, untouched). Live meter readout keys in
  `/api/state` (read-only, never saved/restored): `mv/ma/msoc` mirroring the
  last `0x03` reply (golden 520/0/100 or active spoof stage). Contract checker
  validates all 3 variants (endpoints ⊆ routes, per-page ids incl. dynamic
  `bench_rN`, union of state keys); `test_web` asserts `mv:520/ma:0/msoc:100`;
  all 3 variants proven compiling on host; FULL+LITE JS `node --check` clean.
  OTA assets unchanged (FULL builds) — but v2.7 bins NOT built here (no ESP
  toolchain in container): release carries images/GIF only, OTA v2.6→v2.7
  checks fine and installs 404-safe until §11.2 is done. Sim (`local-wokwi/`,
  subtree-pushed as `bms-tester-sim`): relay board row + `labels.json`
  persistence synced into `lbl0..7`, live-GitHub OTA check
  (`ota_cmp` port + asset-presence gate, reproduces the 404 path), `shots.py`
  snapshots + demo GIF (cairosvg+ffmpeg). Enclosure/GX16 docs (`enclosure/`,
  dual-buck tree, `DAD:` placeholders for dad's sizing). Real meter photos in
  `Actual Meter Image/` (meter cluster + segment close-up, also 3D textures).
  **152/152 total** (103 pio + 49 web); emu 62/62 earlier; release snapshots
  inspected before upload. Release media (v2.7 tag): dash-full.png,
  dash-lite.png, demo.gif, protocol.png, wiring.png, terminal.png, tests.png,
  tiles.png — every docs/wiki page embeds the relevant ones.
- **Decision: option A** — writes (`0x5A`) and unknown registers get SILENCE
  (never a wrong-register reply), but still refresh the green window.
  This was an explicit user-confirmed choice. Do not change without asking.

---

## 4. Hardware design (v2.0 map — v1.x wiring unchanged, only ADDS pins; v2.3 adds NO pins)

Board: **ESP32-S3 DevKitC-1 (8 MB) or N16R8 (16 MB + OPI PSRAM)** (NOT classic
ESP32 — S3 has no GPIO25; S3 GPIO range is 0–21 + 26–48).

| Signal | Connection |
|---|---|
| S3 GPIO17 (TX) | → MAX485 DI |
| S3 GPIO16 (RX) | ← MAX485 RO |
| S3 GPIO4 | → MAX485 DE+RE tied (+ 10 kΩ pull-down to GND) |
| S3 GPIO10 | → 220 Ω → green LED → GND |
| S3 GPIO11 | → 220 Ω → red LED → GND |
| S3 GPIO48 | onboard WS2812 RGB (mirrors LEDs, v1.2+) |
| S3 GPIO5/6/7/8/9/12/13/14 (v2.0) | → relay IN1–IN8 (SmartElex 12 V, own 12 V supply, common GND, active-LOW default) |
| S3 GPIO15 (v2.0) | → button to GND (pull-up; 10 s hold = factory reset) |
| S3 GPIO21 (v2.0) | → spoof trigger to GND (pull-up; v2.3.1: configurable, see below) |
| S3 GPIO18 (v2.3.1) | → WiFi kill to GND (pull-up; grounded = AP+portal+server off) |
| *(v2.6 adds NO pins — meter counting is pure software)* | |
| MAX485 VCC / GND | 3.3 V (NOT 5 V) / common GND with meter |
| MAX485 A/B | → meter A/B, twisted pair, short run |

Avoided pins: strapping 0/3/45/46, USB-JTAG 19/20, flash/PSRAM 26–37,
console 43/44. UART2 @ 9600 8N1 via GPIO matrix.
Power: USB 5 V for ESP + MAX485 — NEVER the traction pack. v1.x idle ≈ 0.3–0.5 W
(radio off). v2.0 keeps the Wi-Fi AP always on (~1 W-class — use a real charger)
and relay coils run on their own 12 V supply (common GND).

---

## 5. Firmware architecture

```
src/bms_protocol.h / .cpp   hardware-independent core, FROZEN (also on host)
src/relay_ctrl.h / .cpp     HW-independent bench add-on (also on host)
src/ota.h / .cpp            HW-independent OTA decisions (also on host)
src/web_ui.h / .cpp         ESP-only (WiFi/WebServer/Preferences)
src/main.cpp                Arduino sketch (ESP32-S3 only)
```

Core pieces (`bms_protocol.*`):
- `jbd_checksum(buf, len)` — generic `0x10000 − sum`, caller chooses slice.
- `JbdParser` — streaming state machine (states renamed `JST_*` because
  Arduino's `binary.h` `#define`s `B1` — this broke the S3 build once).
  Validates start/cmd/len/data/checksum/terminator; re-syncs on noise;
  rejects overlong (`JBD_MAX_DATA` = 64). Emission gated on checksum success
  (a real bug caught by tests: corrupt-CK + trailing 0x77 used to emit).
- `reply_for(reg, is_write, len)` — 0x03/04/05 read → canned frame; else NULL.
- `PollTracker` — EMA of poll intervals; threshold = clamp(2×EMA+500, 2 s, 10 s).
- `matches_request()` + `connection_active()` — v1.0 compat, kept for tests.
- `FW_VERSION` = `"2.7"`; `STATUS?` replies `GREEN 2.7` / `RED 2.7`
  (first token stable — HIL test splits on whitespace).

Canned frames (frozen literals, NEVER recomputed at runtime):
| Reg | Len | Content | CK |
|---|---|---|---|
| 0x03 | 34 | byte-exact real capture: 52.0 V, 0 A, 100 Ah/100 Ah, RSOC 100 %, 14S, 2×25.0 °C | FC DA |
| 0x04 | 35 | 14 × 0x0E82 (3714 mV) = 52.0 V, consistent with 0x03 | F8 04 |
| 0x05 | 19 | ASCII `TEST-14S100A` | FC FD |

`main.cpp` loop (v2.0 order): button debounce (press event + 10 s reset hold) →
spoof-pin edge → `seq.tick()` + `apply_relays()` (change-only writes) +
`web_tick()` (non-blocking) → FROZEN RS485 drain→parser→`note_poll`+reply
(DE HIGH → write → `flush(true)` → 1.5 ms guard → DE LOW → drain → reset),
with one additive select: reg `0x03` + spoof window active → `spoofFrame`,
else frozen `reply_for` → 250 ms LED eval → `STATUS?`. Boots red, relays OFF
(OFF level driven BEFORE `pinMode` — no boot click). No `delay()` in hot path.
(No `delay(1)` power tweak — obsolete: radio is on in v2.0.)

v2.0 pieces (`relay_ctrl.*`, host-tested):
- `RelaySequencer` — `millis()` state machine (IDLE/RUNNING/HOLD/CHASE/PAUSE),
  sequential stepping with catch-up, per-mode ms holds (0 = forever), relay
  count scoping, manual force mask, rollover-safe compares.
  `handle_button_press()` maps the 3 web modes
  (HOLD_ABORT / RUN_LOCK / RESTART). v2.3: chase sweep, loop + pause +
  cycle limit, ALL-ON stagger, fwd/rev direction, QC counters
  (`cyclesDone()`/`actuations()`). v2.4: chase 20 ms break-before-make,
  500 ms post-stop start dead-band, run-register snapshots (timing latched,
  count-shrink live), force-in-chase idles the wave, RESTART keeps forces,
  step ≥ 100 dflt 250 / stagger dflt 50 / pause 500–60000 dflt 2000. v2.6:
  `MeterBatch` link-gap heuristic (`noteLink`/`pollIdle`/`closeMeter`,
  `begin()` amnesia vs `dayReset()` seat-count), accepted starts open
  attempts (`startImpl` flag keeps loop-restarts out), `cycleDone` latches
  pass. Never touches relays — pure counters.
- `DebouncedInput` (30 ms, edge-once), `SpoofWindow` (legacy single-stage,
  frozen + tested), `SpoofPlan` (v2.3 two-stage: `stage()` → 0/1/2).
- `build_spoof_frame(cfg, stage, out)` — golden copy + patched V/A/SOC/temps
  + recomputed CK (same LEN+DATA rule); `relay_pin_level()` polarity helper.
- `Bms2Config` — all web-tunable values + NVS schema (namespace `bms2`, v3;
  tested v2→v3 migration: seconds×1000 fanned to all holds, singles→stage 2).
- `web_ui` (ESP-only): AP `BMS-Tester` always on (fixed 192.168.4.1 via
  `softAPConfig`, channel changeable), `DNSServer` catch-all captive portal,
  unknown URLs → 302 `/`, **no login wall (v2.3.1; WPA2 is the gate)**,
  per-request admin password gates reboot/reset/`/update`/OTA-admin/AP
  saves (passwords never in `/api/state`), JSON API + single-page dark
  dashboard — 1 s tick is status-only, forms fill on load + after saves
  (`node --check` clean + `check_web_contract.py` gate), deferred-save
  coalescing (dirty-flag, 1.5 s flush, sync-flush on reboot/reset), NVS
  load/save, optional STA uplink + one-shot test (30 s, no reboot)
  (hotspot, 30 s non-blocking try, AP-only fallback), Tasmota-grade manual
  `/update` firmware upload (exact asset, explicit budget, image-head gate,
  single finalize, named errors, progress), OTA check + Install + custom URL,
  console, config backup/restore (no passwords), info card, mDNS
  `bmstester.local`, factory reset + keep-WiFi reset + bootcount reset +
  reboot (one `web_reboot_now` path), boot counter + reset reason,
  portal landing for probes/CNA (button + Safari steps, rest 302s),
  on-demand STA join for check/install/URL, whitespace-tolerant JSON,
  v2.6 meter card (meters/attempts/pass/fail + New-day reset) + round link
  dots (`#dotG/#dotR`) + `/api/meter` (reset-only) + console `DAYRESET` +
  `STATUS` batch + NVS `m_met/m_att/m_ps/m_fl` (close/reset flush, boot
  restore, excluded from backups) + `meter_feed()` in `web_tick()`,
  WiFi kill switch (`web_wifi_set`, GPIO18, debounced in `main.cpp`),
  v2.7 `WEB_UI_VARIANT` (0 CLASSIC / 1 FULL default / 2 LITE — `handle_root`
  serves the compiled-in page; OTA assets stay FULL) + FULL bench SVG card
  (`bench_r0..7`, `bench_de`, `flowAB`, `bench_mv/link`) + read-only
  `mv/ma/msoc` in `handle_state` (golden vs spoof stage).
- `ota` (`ota.h`, host-tested pure logic; network in `main.cpp`): semver
  compare, per-variant asset pick (`-DFW_IS_N16R8=1` on the n16r8 env —
  quoted `-D` strings do not survive the flag pipeline), safe download URLs,
  idle-only auto-check gate; install streams via HTTPClient + `Update`
  (no 700 KB RAM copy).
- Reply selection lives in `select_reply()` (`relay_ctrl`, host-tested in
  `test_system`); `main.cpp` calls it — no inline selection logic.

---

## 6. Protocol findings (do not re-derive — verified)

- JBD UART, 9600 8N1, half-duplex. Request `DD A5 03 00 FF FD 77`.
- **Checksum rule:** `0x10000 − sum`, big-endian. Requests cover REG+LEN+DATA
  (A5/5A excluded). **Responses cover LEN_HI+LEN_LO+DATA — echoed CMD excluded.**
  Including it gives FCD7 vs recorded FCDA (off by exactly 0x03). An early spec
  note said otherwise; the captures prove it. Verified on 6 vectors:
  FFFD (req), FCDA (100 %), FCA8 (50 %), FA86 (90 %/45 °C),
  F65A (2nd Docklight 0x2A variant), F658 (handoff 49 % frame).
- **Anomalies (transcription errors, EXCLUDED from vectors, documented in code):**
  charging frame (F386 vs computed FA41, off 0xBB), discharge frame
  (FBCD vs FBBC, off 0x11).
- Ground truth: `captures/SOC-DOCKLIGHT.xlsx` (rows: 4 req, 7 SOC100, 10 SOC50,
  15 90 %/45C, 21 2nd-0x2A variant, 27 req+Modbus, 30 Modbus `01 03 00 00 00 1D`,
  33 49 %/31C, 39 charge, 44 discharge). Modbus on the bus is out of scope.
- Golden reply + request bytes verified byte-present inside `firmware.bin`.

---

## 7. Tests — 152/152 + soak + virtual bus + contract + socket emu (how to run, what they prove)

- `pio test -e native` → 10 suites: test_checksum (7), test_logic (8),
  test_parser (13), test_stress (4), **test_relay (36), test_spoof (11),
  test_meter (10), test_ota (6), test_upload (5), test_system (3)**.
  **Last run: all green (103/103).** Old 32 byte-identical since v1.1.
  `test_web` (49) is g++-only (needs `-DARDUINO` + stubs) → total **152/152**
  via `sh run_tests.sh`.
- `tools/fw_emu/`: socket harness (real handlers, real HTTP, virtual time):
  `drive_emu.py` 62 checks + `drive_soak.py` 10 checks (48 virtual hours).
  Boot `fw_emu` (built from `emu_main.cpp`), run both drivers. `w3m -dump`
  renders verify pages. `/__bus` drives the RS485 bus LEDs + meter heuristic
  (`/__link` is STA-only — conflating them cost a debugging round in v2.6).
  See `docs/EMULATION-v2.5.md` (v2.5 per-feature report; v2.6 deltas in wiki
  Versions + CHANGELOG).
- `test_web` highlights: real `web_ui.cpp` executes on host — per-request
  admin password gates (admin/OTA/update incl. multipart field plumbing),
  WiFi kill-switch transitions, spoof-pin clamp + persist, OTA install gate,
  NVS v2→v3 migration, validation/NVS/
  API/fuzz/reset, deferred-save coalescing (stub flash-commit counter) +
  reboot flush, count/chase-sweeps/loop/labels/counters/STA/OTA/`/update` handlers.
  Stub-fidelity bugs it caught (and fixed): per-TU mock clocks (→ `inline`
  shared clock), NVS `clear()` missing the number store, test isolation via
  static `ident` (→ defaults-reset in `web_setup`, behavior-neutral on
  hardware), **host `String += char` binds `String(int)` (decimal garbage
  in the upload password) → firmware appends via 1-char C string**,
  **v2.3.1: `hch`/`a_user`/login expectations retired with the remodel**.
  All were test-harness or robustness fixes, never responder logic.
- `test_ota` highlights: semver matrix (`2.10 > 2.9`, `v`-tolerant, garbage
  fail-closed), per-variant asset pick, URL tag sanitizing, gate matrix
  (auto/STA/sequence/bus-silence/interval/rollover).
- `test_system` highlights: `select_reply()` matrix (golden/stage-1/stage-2/
  disabled/null-frame/04-05/write/unknown) + 24 h office-day sim (86,400
  polls, every reply CK-validated, exact 5 s + 10 s spoof stages, relay +
  chase schedule probes, write-silence mid-spoof, green-all-day) + loop
  cycle/limit/counter proof. Runs in ~0.05 s.
- `tools/check_web_contract.py` — JS endpoints/ids/state-keys/POST-keys vs
  firmware routes, exit 0 = PASS (CI `web` job + run_tests.sh). Knows dynamic
  `lblN` keys (builder loops) + `/api/ota` + `/update` form. v2.7: validates
  all 3 PAGE_DASH variants (per-page endpoints/ids incl. dynamic `bench_rN`,
  union of state keys incl. `mv/ma/msoc`).
- `sh run_tests.sh` → same suites via g++ fallback + **soak sim** + HIL
  (auto-skip without hardware).
- New-suite highlights: sequential stepping/timing, per-mode ms holds,
  ALL-ON, chase wave + count scoping, loop/pause/limit, reverse direction,
  ALL-ON stagger, QC counters, all 3 button modes, abort/restart mid-cycle,
  manual override, polarity map, debounce edges, sequencer+window rollover;
  spoof stage-1/2 bytes (2710/64 vs 22B0/22B0/0E23/BC), CK self-consistency,
  custom values, plan timing/handoff/cancel/retrigger. Past real bug caught:
  debounce test asserted the wrong return polarity (test bug, not code —
  fixed before commit); v2.3: hold-expiry test assumed hold starts at last
  step (it starts at the completing tick — fixed).
- v2.3 pre-release catches (both fixed before release): auth-cookie
  truncation (stub `indexOf(' ')`, above) and a stray `;` ending the
  `handle_state` builder early (spoof keys missing from `/api/state`) —
  each caught by 2 web tests. Lesson: keep the web suite at full strength;
  it is the dashboard's only emulator.
- Old highlights (still green): exhaustive 1,785 single-byte corruptions
  (0 false frames); 10 M fuzz (0 emits); cadence×register sweep;
  bus saturation (5,000); 1 s-green / 2 s-red / self-heal / rollover.
- `tools/soak_sim.cpp` — 8 simulated days: **691,040 polls, all answered**,
  noise rejected, silence→red, millis() wrap crossed mid-run, green-on-resume.
- `tools/virtual_bus.sh` — one-shot PTY emulation (Debian/proot, socat+g++):
  builds `tools/dut_emu.cpp` (real `bms_protocol.cpp` over a PTY), runs
  `tools/raw_meter.py` (raw-fd I/O — pyserial modem ioctls fail on proot PTYs)
  through 03/04/05 golden + write/unknown silence + noise resync + 3 s
  silence, asserts meter PASS + DUT RED→GREEN→RED. Exit 0 = PASS.
  (Earlier sessions kept these harnesses in scratch; v2.0 promoted them in-repo.)
- Dashboard JS: `node --check` clean (Termux node). Wokwi diagram validated
  as JSON (18 parts, 33 wires); browser run is manual (never executed here).
- `tools/virtual_meter.py` — scenario modes (real serial ports).
- `tools/test_hardware.py` — HIL pytest; needs `HIL_BUS_PORT` + `HIL_CDC_PORT`.
- Past bugs the suite caught (proof it works): parser emit-on-bad-CK,
  author's own wrong vector (FEEF not FEFF), Arduino `B1` macro collision,
  missing `<cstring>` (Debian GCC 14), missing `<cstdio>` (CI Ubuntu GCC —
  Termux clang had silently accepted both).

---

## 8. Build system & environments

`platformio.ini` envs: `esp32-s3-devkitc-1` (8 MB firmware, FULL dashboard),
`s3-n16r8` (16 MB + OPI PSRAM, `default_16MB.csv`, `-DFW_IS_N16R8=1`, FULL),
`s3-classic` (`-DWEB_UI_VARIANT=0`), `s3-lite` (`-DWEB_UI_VARIANT=2`),
`native` (host tests,
`build_src_filter = +<bms_protocol.cpp> +<relay_ctrl.cpp> +<ota.cpp>`,
`test_filter` = 8 suites), `s3_tests` (on-target ELFs).
Pinned reality: espressif32@7.1.3, Xtensa GCC 8.4.0 (esp-2021r2-patch5),
Arduino 2.0.x (IDF 4.4 based). v2.3 `firmware.bin` = 955,040 bytes
(`65eb652e…ae35d36a`); `firmware-n16r8/firmware.bin` = 957,520 bytes
(`71d39bd7…df36d2`). Golden bytes + `2.3` + `BMS-Tester` + dashboard
strings verified byte-present in both. `firmware/` + `firmware-n16r8/` hold
bootloader + partitions + esptool READMEs (flash 0x0 / 0x8000 / 0x10000).
`arduino/` = same firmware as IDE sketch (9 tabs incl. new files).
`.github/workflows/ci.yml` runs native (70) + web (contract + test_web) +
**both firmware envs** + soak + token-gated `wokwi-sim` (green; sim skipped
without `WOKWI_CLI_TOKEN` secret).

**CRITICAL — two homes:** Termux home (`/data/data/com.termux/files/home`,
shared into proot containers) vs container-local roots. These live INSIDE the
Debian container (`…/containers/debian/rootfs/…`), NOT in Termux home —
back them up separately if they matter:
- `/opt/pioenv` (venv: platformio, esptool), `/opt/qemu-src` (Espressif QEMU
  git + `build/qemu-system-xtensa`, OUR patches applied, see §9),
  `/opt/qemu-s3` (flash images, boot logs), `/root/.platformio` (Debian-side
  packages incl. hand-extracted Xtensa toolchain).
- Ubuntu container holds only a QEMU binary copy (`…/ubuntu/rootfs/opt`).

**Termux/Android quirks (all solved, scripts exist):**
1. pyserial has no Android port lister → 1-line patch
   (`plat[:5]=='linux' or plat[:7]=='android'` in `list_ports_posix.py`;
   patch file in emulator repo). Without it EVERY `pio` cmd crashes on Termux.
2. PIO Xtensa tarball fails on `rename()` under proot (hardlinks, EINVAL) →
   `tar -xzf` the cached tarball into the package dir by hand.
3. Meson sniffs Termux's bionic headers (`-I…/termux/files/usr/include` baked
   into build.ninja) → configure AND build with sanitized
   `PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`.
4. ~~QEMU JIT cannot run inside proot~~ STALE 2026-09-21: QEMU 9.2.2
   source-built DOES execute guests inside proot-debian on this phone
   (ESP-ROM output + guest_errors captured). Only the official prebuilt
   aborts (static-glib `g_quark_init`). Build AND run in proot-debian.
5. v2.3 build lessons: quoted `-D` strings do not survive `platformio.ini`
   (`-DFW_VARIANT="n16r8"` arrives unquoted → numeric `-DFW_IS_N16R8=1`
   instead); Arduino-ESP32 2.0.x `HTTPUpdate` has no `followRedirects` →
   OTA install streams via `HTTPClient` (`setFollowRedirects`) + `Update`.
- Termux shell: aarch64 Android 12, clang 21, Python 3.14, `pio` 6.2.0 installed
  (native tests OK). Debian container: Python 3.13, GCC 14, `gh` 2.46.0.
- `scripts/setup-linux.sh` (apt + venv + tests) vs `scripts/setup-termux.sh`
  (pkg + workarounds + tests) — the "linux version + termux version" request.

**v2.4 build-environment discovery (2026-09-22, keep this):** Python 3.14
reports `android_aarch64`, for which the registry has NO toolchain builds —
Termux-side `pio run` fails at package resolution even though the registry
is reachable. Fixes, in order:
1. `export PLATFORMIO_SYSTEM_TYPE=linux_aarch64` (official override) → the
   cached-version toolchain + framework download fine (binaries run under
   proot-Debian glibc; they canNOT exec on Termux bionic — `cannot execute:
   required file not found`).
2. So firmware compiles ONLY inside proot-debian:
   `proot-distro login debian -- sh -c 'export PATH=/root/.local/bin:$PATH;
   pio run -d <project> -e esp32-s3-devkitc-1 -e s3-n16r8'`
   (Debian has its own pio core at `/root/.local/bin/pio` + warm
   `/root/.platformio`; Termux home is visible inside proot at the same path).
3. `tool-esptoolpy` needs hand-provisioning (its auto pip step builds
   `cryptography` from source → no Rust here): esptool 4.11 sdist +
   `package.json` (v2.41100.260830) + `_contrib` with pure-python
   pyserial/intelhex/ecdsa/six, PLUS a 1-line local patch
   (`list_ports_posix.py`: accept `sys.platform == 'android'` like linux —
   pyserial 3.5 predates Android). Secure-boot signing (the only
   cryptography consumer) is unused by our builds. If `packages/` gets wiped
   again, redo §8 steps 1–3 (framework/toolchain re-download; esptoolpy
   re-assemble).
4. `tools/virtual_bus.sh` hardcodes `/tmp` (read-only/absent on Termux) →
   run it in proot-debian (socat + pyserial present there).

---

## 9. QEMU saga (complete, honest state)

Goal was 3/3 emulation layers; achieved 2.5/3. QEMU-S3 built FROM SOURCE on
the phone (official ARM64 prebuilt aborts under proot — gdb proved static-glib
`g_quark_init` assert) and boots OUR firmware to the Arduino flash-init step,
where it asserts at IDF-4.4.8 `startup.c:328`
(`assert(esp_flash_init_default_chip() == ESP_OK)`).

Traced with `-d guest_errors` + fixed two REAL upstream flash-model bugs
(patch: `esp32s3-qemu-arm64/patches/qemu-m25p80-rdid-sfdp-gd25q64.patch`):
1. RDID `0x90`/`0xAB` unanswered for non-SST parts → now generic (JEDEC-based).
2. GD25Q64 (8 MB image part) had no SFDP → `0x5A` failed → added 256-B table.
3. `0x77` Set-Burst-with-Wrap → accept-and-ignore (trace-confirmed gone).
Stage-0 baseline 2026-09-21 (`tools/qemu_boot_test.sh`, v2.3 n16r8 image,
45 s, IN proot-debian — JIT-stale note above is dead): **81×
`Unknown cmd 0x10` (QEMU prints `%x` without prefix — "10" IS 0x10),
0× `0x10200C`, 1 POWERON + 81 RTC_SW resets** — i.e. exactly ONE cmd-0x10
per boot attempt, then crash-loop; the 0x77 patch killed the whole 0x10200C
class. UART0 shows only ROM output (repeating `mode:DIO … entry 0x403c98d0`).
Observability constraint: firmware `STATUS?`/logs live on USB-Serial, NOT
UART0 — so Stage-A pass = zero flash errors + reboot loop STOPS (+ Stage-B
GPIO proof), not a literal `STATUS?→RED` over UART0.
Stage-A trace (`TRACE_EVTS="m25p80_command_decoded"`, full 26-cmd iteration):
`AB 9F 05 05 9F 5A 05 05 35 00 05 05 35 00 06 31 02 05 35 00 77 00 00 00 10 04`
— driver runs init TO COMPLETION (ends WRDI `04`); `77 00 00 00 10` =
Winbond-style burst-wrap disable (3× dummy `00` + wrap byte `10` = wrap
DISABLED, fire-and-forget, zero functional damage); `31`=WRSR2,
`02`=PAGE PROGRAM at boot is suspicious (possible WRSR2-data desync:
GIGADEVICE consumes 0 data bytes?).
Stage-A ROOT CAUSE (GDB + disassembly, 2026-09-21): the `02` is the WRSR2
*data byte* misdecoded — GD consumed 0 bytes, QE write lost, read-back saw
QE=0 → `startup.c:328` assert path. Fix (NOT yet upstreamed to the emulator
repo — patch file `esp32s3-qemu-arm64/patches/` + applied to `/opt/qemu-src`
working tree only): WRSR2 consumes/persists the QE bit for GIGADEVICE +
`0x77` consumes 3 dummy + wrap byte. Result: flash model 100 % clean
(`qemu_boot_test.sh` PASS, 0/0) — **but the guest STILL reset-loops**.
Deeper GDB: reset is a genuine guest `SW_SYS_RESET` (`esp_restart_noos` at
bootloader `0x403cdb08`, called from entry+0x15) because **`bootloader_init`
returns `0x0a`** (SITE1 breakpoint; load-boot-image + index paths never
reached). So the 2nd-stage bootloader itself bails before the app starts;
UART0 stays ROM-only (app logs are USB-Serial). PARKED here — bench bugs
took priority. Resume: figure out which `bootloader_init` step returns
0x0A (clock? flash-ID? partition-read via XIP?) — disassemble forward from
`0x403c9938`, or catch the failing call with GDB breakpoints. Temp S3DBG
`mem_io_pc→cc->get_pc` debug print is UNCOMMITTED in `/opt/qemu-src`
(`hw/misc/esp32s3_rtc_cntl.c`) — revert before any upstream patch submission.
Next agent: to continue, decode what `0x10` is in DIO context (possible status-
register or continuous-read mode byte); or deprioritize — host tests + soak
already prove the firmware.
Hypothesis: DIO-era command bytes hitting the single-line SSI model + an
unmodeled register region. IDF-based guests are unaffected per Espressif CI;
Arduino guests were never QEMU-supported. Verdict: **emulator gap, not firmware
— our code is never reached.** Re-confirmed on v2.1 image (same signature);
v2.3 baseline above via `tools/qemu_boot_test.sh` (exact test: PASS iff zero
0x10 + zero 0x10200C; `BASELINE=1` records signature).
Practical emulation = Wokwi (`wokwi/` v2.1: verified pins, relay modules,
`sim.yaml` automation; browser run manual, headless needs `WOKWI_CLI_TOKEN`).
`tools/run_qemu_s3.sh` = one-command boot test for real Linux/Mac.
Next agent: to continue, decode what `0x10` is in DIO context (possible status-
register or continuous-read mode byte) and identify the `0x10200C` peripheral;
or deprioritize — host tests + soak already prove the firmware.

---

## 10. GitHub access & pushing

- Account `bm-a` (Bhavishya Madan). Pushing needs a normal account + classic
  PAT with `repo` scope. `gh` authenticated (Termux `~/.config/gh/hosts.yml`,
  0600). Plain `git push` / `git tag` / `gh release create` work from the
  Termux shell via the gh credential helper — verified for v1.2 + v2.0.
  (Old note: pushes once had to go via the Debian container; no longer true.
  If a push ever fails with auth errors, `gh auth status` first, then
  `gh auth setup-git`.)
- A PAT (`ghp_…`, full permissions) was **pasted in chat on 2026-09-18**.
  It is NOT stored in any file. **Still recommend the user revoke/rotate it**
  — chat logs persist. (Outstanding since v1.1.)
- Commit identity: `bm-a` / `bm-a@users.noreply.github.com`.
- Releases: v1.0 (ZIP), v1.1 (3 bins + docx), v1.2 + v2.0 + v2.1 + v2.2 + v2.3
  (7 assets each: 8 MB triple plain-named + `n16r8-` triple — GitHub forbids
  duplicate asset names, so N16R8 files are uploaded renamed; docx re-uploaded
  with `--clobber` when only the report changes). v2.7: tag + release live
  with dash-full.png + dash-lite.png + demo.gif (inspected before upload);
  firmware bins PENDING bench-PC build (§11.2). Wiki backend provisioned
  (one browser click); push via the `.wiki.git` clone.
- Re-run `gh release create` only if assets change.

---

## 11. Pending / next steps (ordered)

0. **Post-compaction resume:** say "continue" — todo list is ordered top-down
   (HANDOFF delta → QEMU Stage 0 → A → B → C → D → bench + housekeeping).
1. **Bench test v2.7 with real meter + SmartElex module** (same list as the
   v2.6 bench plan, plus): FULL bench SVG live on the phone (relays glow,
   A/B flow pulses, meter readout follows golden→spoof), LITE/CLASSIC smoke
   (tiles + names + link), `STATUS?` → `2.7`, relay names survive reboot (NVS).
   Full list: (a) responder still green ≤ 1 s, ~52 V/100 %; (b) join
   from an iPhone → mini-browser shows the landing page → Open Dashboard in
   Safari works (or manual 192.168.4.1); Android same via Chrome; (c) trigger
   Save stores pin/enable/polarity with no fire; polarity flip changes the
   physical-switch sense; (d) per-mode menu + chase + dead-band + rejects as
   v2.4; (e) one-file flash per board (`write-flash 0x0 bms-tester-*.bin`) →
   boots → `STATUS?` → `2.7`; (f) STA test → on-demand URL upgrade installs;
   (g) GPIO18 kill still kills; (h) meter card: first GREEN = #1, reseat gap
   ≥ 3 s opens #2 with pass/fail verdict, retries don't count, New-day
   resets.
   If the page still won't open: confirm flashed firmware is v2.7
   (`STATUS?` → `2.7`; v1.x has no WiFi at all), AP visible, exact URL/error.
   Needs: 12 V coil supply with common GND; 48 V rail per relay COM, each NO
   to its OWN load (never two outputs on different potentials).
2. **Build + upload v2.7 bins (bench PC with ESP toolchain — BLOCKS OTA):**
   OTA v2.6→v2.7 tested 2026-09-23: check phase WORKS (box sees v2.7, decision
   units 6/6, live-GH verified), install phase 404s (`firmware.bin` /
   `n16r8-firmware.bin` absent from the v2.7 release — no toolchain in this
   container) and fails safe (`install http 404`, box keeps running). The sim
   reproduces this exactly (live GH check + asset gate). To close:
   `pio run -e esp32-s3-devkitc-1 -e s3-n16r8 -e s3-classic -e s3-lite`
   (proot-debian per §8) → merge per `firmware/README.md` → 
   `gh release upload v2.7 <bins>` → re-run a box Check→Install to prove the
   pull end to end (STA uplink + TLS + flash write are hardware-only).
2. **Add `WOKWI_CLI_TOKEN` repo secret** → token-gated CI sim proves the real
   firmware headless (button→pins, spoof→`22 B0`); also answers whether
   `WiFi.softAP` boots in the sim (dashboard HTTP itself has no emulator —
   covered by `test_web` instead).
3. **HIL pytest** + manual Wokwi run + dashboard browser pass once hardware/PC
   available.
4. **QEMU full-S3 program (user-approved, staged, phone-proot testing):**
   Stage 0 exact-test harness (`tools/qemu_boot_test.sh`: assert zero
   `Unknown cmd 0x10` + zero `0x10200C` + `STATUS?` → `RED`); Stage A boot
   past flash-init (identify `er` via `info mtree`, byte-level SPI trace,
   patch as `patches/qemu-s3-flash-part2.patch` in the emulator repo);
   Stage B GPIO observability; Stage C UART2/RS485 via `virtual_bus.sh`;
   Stage D Wi-Fi verdict spike (prototype or declare Wokwi the path).
   Signatures: ~22× `0x10` interleaved with ~84× `0x10200C`, then
   `startup.c:328` assert loop. Stop rules: unmodeled-peripheral wall →
   bank evidence + options (no silent grind); proot unexecutable → CI
   boot-smoke instead. (Old "Possible v2.1" ideas are DONE in v2.3:
   relay labels, STA uplink, `delay(1)` obsolete.)
5. **Housekeeping:** rotate the chat-exposed PAT (outstanding since 2026-09-18);
   re-check CI after any push; Google index lag is normal.

---

## 12. File inventory + RELEASE CHECKLIST (read this before any release)

Project: `platformio.ini README.md CHANGELOG.md LICENSE VERSION HANDOFF.md
RS485-Tester-Report.docx run_tests.sh src/ test/ tools/ arduino/ firmware/
firmware-n16r8/ wokwi/ local-wokwi/ enclosure/ captures/ docs/ scripts/ wiki/
releases/ .github/ .gitignore .unity/ .pio/`
(`.pio .unity releases/ *.log tc.json .test_* local-wokwi/labels.json
local-wokwi/server*.log local-wokwi/shots/` git-ignored; `releases/` holds
v1.0 ZIP + 2 git bundles — offline full-history backup, verified by test-clone.
Back it up before ANY storage compaction: it exists nowhere else.)
Wiki canonical sources live in `wiki/` (Home, Flashing, Hardware, Protocol,
Emulators, Versions, Relays, Dashboard) and are pushed to the `.wiki.git` backend.
`Actual Meter Image/` holds the meter cluster + segment photos (also 3D textures
in the sim). `src/ota.*` (OTA decisions) + `test/test_ota/` + `test/test_web/Update.h`
(upload stub) are v2.3 additions; v2.4 adds `src/fw_upload.h` +
`test/test_upload/` + stubs `ESPmDNS.h`/`esp_system.h` (info card/mDNS);
v2.5 adds `tools/fw_emu/` (socket harness + drivers) + `docs/CONFIG-SCHEMA.md`
+ `docs/EMULATION-v2.5.md` + stub `uri()` (portal tests); v2.6 adds
`test/test_meter/` + `/__bus` control + merged `bms-tester-*.bin` images;
v2.7 adds `WEB_UI_VARIANT` pages + `mv/ma/msoc` + `s3-classic`/`s3-lite` envs
+ `local-wokwi/` (sim server, Three.js bench, verbatim ESP embed, `shots.py`)
+ `enclosure/` (IP65/GX16/dual-buck docs).
Termux home `archive/` — pre-existing clutter, leave alone.
Emulator work (`/opt/qemu-src` 1.1 GB, `/opt/qemu-s3` 94 MB, `/opt/pioenv`
76 MB, `/root/.platformio` large) is inside the Debian container — see §8
before wiping proot data. Wiping it costs ~1 h rebuild (QEMU 20–40 min +
PIO packages + Xtensa tarball workaround); losing `.pio` (102 MB) is free
(re-downloads).

**Release checklist — EVERY feature release MUST do all of these in the
release commit (lesson learned 2026-09-22: HANDOFF body went stale at v1.1
while README/CHANGELOG moved on — never again):**
1. `src/` + `arduino/` tabs identical (diff-check every file, incl. new `fw_upload.h`).
2. `VERSION` + `FW_VERSION` + `STATUS?` strings bumped together.
3. `CHANGELOG.md` new entry; `README.md` versions/table/SHAs; `docs/MODULES.md`
   pins; `llms.txt` counts + code map; `test/README.md` counts;
   `tools/README.md` if helpers changed; `arduino/README.md` wiring/tabs;
   `wokwi/README.md` if diagram changed; `platformio.ini` test_filter for new
   suites; `tools/make_report.py` strings + regen docx.
4. `firmware/` + `firmware-n16r8/` rebuilt from exact source + SHAs in READMEs;
   `RS485-Tester-Report.docx` regenerated via `make_report.py`.
5. `wiki/` sources updated + pushed to `.wiki.git`.
6. **This HANDOFF**: update §§1–5, 7–12 in the body (no addendum-only updates),
   then commit → tag → push → `gh release create` (n16r8 assets renamed) →
   confirm CI green.

Resume checklist: `ls /opt/qemu-src/build/qemu-system-xtensa && pio --version`
(toolchain-alive check — if empty, rebuild per §8) → `git log --oneline |
head -3` → `git status --short` → `pio test -e native` (Termux) →
`sh run_tests.sh` → `sh tools/virtual_bus.sh` (in proot — /tmp) → emu
drivers (`fw_emu` + `drive_emu.py` + `drive_soak.py`, ports 18080/18081) →
compare against §7 numbers (152/152 + contract + 72/72 emu). Firmware builds
run ONLY in proot-debian per §8 (Termux pio = host tests only).
