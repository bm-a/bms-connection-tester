# HANDOFF — bms-connection-tester project (full brain dump)

> Written 2026-09-18, before Termux storage compression.
> Last updated 2026-09-21 for **v2.3** (relay count/chase, 2-stage spoof,
> per-mode holds, industrial pack, persistent logins, OTA). Rule going forward:
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
(100 first, then 88.8/88.8/88.8/188); persistent logins; manual + automatic
OTA; coalesced config saves. No screens needed.

- **Origin story:** user's dad runs an e-rickshaw meter assembly line; workers
  needed the real battery or a laptop to verify RS485 wiring. This box replaces
  both. (Repos are now deliberately **generic** — e-rickshaw is one use case.
  The Word report for dad keeps the e-rickshaw framing on purpose.)
- **People:** user = Bhavishya Madan (GitHub `bm-a`). Dad = electronics-strong,
  code-weak; gets status via a WhatsApp Word report, not GitHub.
- **Current release: v2.3** (tag `v2.3`; relay count/chase, 2-stage spoof,
  per-mode ms holds, industrial pack, persistent logins, OTA, save coalescing).
  v1.x responder core FROZEN (parser, option-A, tracker, golden frames, LEDs,
  STATUS?, original 32 tests byte-identical). v1.0 frozen (ZIP + git tag,
  untouched since).

---

## 2. The three GitHub repos (all public, user `bm-a`)

| Repo | URL | Contents | State |
|---|---|---|---|
| `bms-connection-tester` | https://github.com/bm-a/bms-connection-tester | Firmware, 105 tests, docs, binaries, Wokwi, CI, wiki | main pushed; tags `v1.0`–`v2.3`; Releases v1.0 (ZIP) + v1.1 (3 bins + docx) + v1.2/v2.0/v2.1/v2.2/v2.3 (7 assets each); wiki live (7 pages) |
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
| S3 GPIO21 (v2.0) | → spoof trigger to GND (pull-up) |
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
- `FW_VERSION` = `"2.3"`; `STATUS?` replies `GREEN 2.3` / `RED 2.3`
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
  (`cyclesDone()`/`actuations()`).
- `DebouncedInput` (30 ms, edge-once), `SpoofWindow` (legacy single-stage,
  frozen + tested), `SpoofPlan` (v2.3 two-stage: `stage()` → 0/1/2).
- `build_spoof_frame(cfg, stage, out)` — golden copy + patched V/A/SOC/temps
  + recomputed CK (same LEN+DATA rule); `relay_pin_level()` polarity helper.
- `Bms2Config` — all web-tunable values + NVS schema (namespace `bms2`, v3;
  tested v2→v3 migration: seconds×1000 fanned to all holds, singles→stage 2).
- `web_ui` (ESP-only): AP `BMS-Tester` always on (fixed 192.168.4.1 via
  `softAPConfig`, channel changeable), `DNSServer` catch-all captive portal,
  unknown URLs → 302 `/`, session-cookie login (30 min RAM sliding +
  remember-me NVS slots ×4, HttpOnly/SameSite), JSON API + single-page dark
  dashboard with live version + labels + counters (`node --check` clean +
  `check_web_contract.py` gate), deferred-save coalescing (dirty-flag, 1.5 s
  flush, sync-flush on reboot/reset), NVS load/save, optional STA uplink
  (hotspot, 30 s non-blocking try, AP-only fallback), manual `/update`
  firmware upload, factory reset + reboot.
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

## 7. Tests — 105/105 + soak + virtual bus + contract (how to run, what they prove)

- `pio test -e native` → 8 suites: test_checksum (7), test_logic (8),
  test_parser (13), test_stress (4), **test_relay (24), test_spoof (11),
  test_ota (6), test_system (3)**. **Last run: all green (70/70).** Old 32
  byte-identical since v1.1. `test_web` (29) is g++-only (needs `-DARDUINO` +
  stubs) → total **105/105** via `sh run_tests.sh`.
- `test_web` highlights: real `web_ui.cpp` executes on host — login/session/
  remember-me + expiry + slot eviction, NVS v2→v3 migration, validation/NVS/
  API/fuzz/reset, deferred-save coalescing (stub flash-commit counter) +
  reboot flush, count/chase/loop/labels/counters/STA/OTA/`/update` handlers.
  Stub-fidelity bugs it caught (and fixed): per-TU mock clocks (→ `inline`
  shared clock), NVS `clear()` missing the number store, test isolation via
  static `ident` (→ defaults-reset in `web_setup`, behavior-neutral on
  hardware), **#4: single-char `String::indexOf(' ')` returns garbage on the
  stub (truncated auth cookies depending on token content) → cookie parsing
  uses the `const char*` overload**. All four were test-harness or robustness
  fixes, never responder logic.
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
  `lblN` keys (builder loops) + `/api/ota` + `/update` form.
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

`platformio.ini` envs: `esp32-s3-devkitc-1` (8 MB firmware),
`s3-n16r8` (16 MB + OPI PSRAM, `default_16MB.csv`, `-DFW_IS_N16R8=1`),
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
  with `--clobber` when only the report changes). Wiki backend provisioned
  (one browser click); push via the `.wiki.git` clone.
- Re-run `gh release create` only if assets change.

---

## 11. Pending / next steps (ordered)

0. **Post-compaction resume:** say "continue" — todo list is ordered top-down
   (HANDOFF delta → QEMU Stage 0 → A → B → C → D → bench + housekeeping).
1. **Bench test v2.3 with real meter + SmartElex module** (the one thing that
   matters now): (a) responder still green ≤ 1 s, ~52 V/100 %; (b) AP
   `BMS-Tester` visible → login page pops on join (or 192.168.4.1, mobile
   data off), remember-me survives reboot; (c) button runs relay sequence +
   chase, count/limit/loop/labels behave, no boot click; (d) spoof shows
   100/100/100/100 % for 5 s then 88.8/88.8/88.8/188 % for 10 s, then reverts
   (188-shows-100 = meter clamp, expected); (e) per-mode holds auto-OFF;
   (f) manual `/update` upload works (offline); auto-OTA only with STA uplink.
   If the page still won't open: confirm flashed firmware is v2.3
   (`STATUS?` → `2.3`; v1.x has no WiFi at all), AP visible, exact URL/error.
   Needs: 12 V coil supply with common GND.
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
firmware-n16r8/ wokwi/ captures/ docs/ scripts/ wiki/ releases/ .github/
.gitignore .unity/ .pio/`
(`.pio .unity releases/ *.log tc.json .test_*` git-ignored; `releases/` holds
v1.0 ZIP + 2 git bundles — offline full-history backup, verified by test-clone.
Back it up before ANY storage compaction: it exists nowhere else.)
Wiki canonical sources live in `wiki/` (Home, Flashing, Hardware, Protocol,
Emulators, Versions, Relays) and are pushed to the `.wiki.git` backend.
`src/ota.*` (OTA decisions) + `test/test_ota/` + `test/test_web/Update.h`
(upload stub) are v2.3 additions; `test_web` stubs now also cover STA
(`WiFi.h`), flash-commit counting (`Preferences.h`), and upload handlers
(`WebServer.h`).
Termux home `archive/` — pre-existing clutter, leave alone.
Emulator work (`/opt/qemu-src` 1.1 GB, `/opt/qemu-s3` 94 MB, `/opt/pioenv`
76 MB, `/root/.platformio` large) is inside the Debian container — see §8
before wiping proot data. Wiping it costs ~1 h rebuild (QEMU 20–40 min +
PIO packages + Xtensa tarball workaround); losing `.pio` (102 MB) is free
(re-downloads).

**Release checklist — EVERY feature release MUST do all of these in the
release commit (lesson learned 2026-09-22: HANDOFF body went stale at v1.1
while README/CHANGELOG moved on — never again):**
1. `src/` + `arduino/` tabs identical (diff-check every file, incl. new `ota.*`).
2. `VERSION` + `FW_VERSION` + `STATUS?` strings bumped together.
3. `CHANGELOG.md` new entry; `README.md` versions/table/SHAs; `docs/MODULES.md`
   pins; `llms.txt` counts + code map; `test/README.md` counts;
   `tools/README.md` if helpers changed; `arduino/README.md` wiring/tabs;
   `wokwi/README.md` if diagram changed.
4. `firmware/` + `firmware-n16r8/` rebuilt from exact source + SHAs in READMEs;
   `RS485-Tester-Report.docx` regenerated via `make_report.py`.
5. `wiki/` sources updated + pushed to `.wiki.git`.
6. **This HANDOFF**: update §§1–5, 7–12 in the body (no addendum-only updates),
   then commit → tag → push → `gh release create` (n16r8 assets renamed) →
   confirm CI green.

Resume checklist: `ls /opt/qemu-src/build/qemu-system-xtensa && pio --version`
(toolchain-alive check — if empty, rebuild per §8) → `git log --oneline |
head -3` → `git status --short` → `pio test -e native` (Termux) →
`sh run_tests.sh` → `sh tools/virtual_bus.sh` (in proot) → compare
against §7 numbers (105/105 + contract).
