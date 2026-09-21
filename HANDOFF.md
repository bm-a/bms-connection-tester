# HANDOFF — bms-connection-tester project (full brain dump)

> Written 2026-09-18, before Termux storage compression.
> Audience: the next AI agent (or future me) picking this project up cold.
> Goal: everything needed to resume without re-discovering anything.
> **Read order for a new agent:** §1 → §2 → §5 → §8 → §11.

---

## 1. What this project is (30 seconds)

An ESP32-S3 box that impersonates a **JBD / Xiaoxiang Smart BMS** over RS485 so
compatible meters/displays can be exercised **without the real battery pack**.
Two LEDs: green = valid BMS traffic seen, red = bus silent. No buttons, no UI.

- **Origin story:** user's dad runs an e-rickshaw meter assembly line; workers
  needed the real battery or a laptop to verify RS485 wiring. This box replaces
  both. (Repos are now deliberately **generic** — e-rickshaw is one use case.
  The Word report for dad keeps the e-rickshaw framing on purpose.)
- **People:** user = Bhavishya Madan (GitHub `bm-a`). Dad = electronics-strong,
  code-weak; gets status via a WhatsApp Word report, not GitHub.
- **Current release: v1.1.** v1.0 frozen (ZIP + git tag, untouched since).

---

## 2. The three GitHub repos (all public, user `bm-a`)

| Repo | URL | Contents | State |
|---|---|---|---|
| `bms-connection-tester` | https://github.com/bm-a/bms-connection-tester | Firmware, 32 Unity tests, docs, binaries, Wokwi, CI | main pushed; tags `v1.0`, `v1.1`; GitHub Releases v1.0 (ZIP) + v1.1 (3 bins + docx) |
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
- **Decision: option A** — writes (`0x5A`) and unknown registers get SILENCE
  (never a wrong-register reply), but still refresh the green window.
  This was an explicit user-confirmed choice. Do not change without asking.

---

## 4. Hardware design (frozen — v1.1 needs zero rewiring vs v1.0)

Board: **ESP32-S3 DevKitC-1** (NOT classic ESP32 — S3 has no GPIO25;
S3 GPIO range is 0–21 + 26–48; verified against Espressif docs by research agents).

| Signal | Connection |
|---|---|
| S3 GPIO17 (TX) | → MAX485 DI |
| S3 GPIO16 (RX) | ← MAX485 RO |
| S3 GPIO4 | → MAX485 DE+RE tied (+ 10 kΩ pull-down to GND) |
| S3 GPIO10 | → 220 Ω → green LED → GND |
| S3 GPIO11 | → 220 Ω → red LED → GND |
| MAX485 VCC / GND | 3.3 V (NOT 5 V) / common GND with meter |
| MAX485 A/B | → meter A/B, twisted pair, short run |

Avoided pins: strapping 0/3/45/46, USB-JTAG 19/20, flash/PSRAM 26–37,
console 43/44. UART2 @ 9600 8N1 via GPIO matrix.
Power: USB 5 V only — NEVER the traction pack. Idle ≈ 0.3–0.5 W (radio off).

---

## 5. Firmware architecture

```
src/bms_protocol.h / .cpp   hardware-independent core (also compiled on host)
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
- `FW_VERSION` = `"1.1"`; `STATUS?` replies `GREEN 1.1` / `RED 1.1`
  (first token stable — HIL test splits on whitespace).

Canned frames (frozen literals, NEVER recomputed at runtime):
| Reg | Len | Content | CK |
|---|---|---|---|
| 0x03 | 34 | byte-exact real capture: 52.0 V, 0 A, 100 Ah/100 Ah, RSOC 100 %, 14S, 2×25.0 °C | FC DA |
| 0x04 | 35 | 14 × 0x0E82 (3714 mV) = 52.0 V, consistent with 0x03 | F8 04 |
| 0x05 | 19 | ASCII `TEST-14S100A` | FC FD |

`main.cpp` loop: drain Serial2 → parser → `note_poll` + reply (DE HIGH →
write → `flush(true)` → 1.5 ms guard → DE LOW → drain stale RX → parser reset);
250 ms LED eval; `STATUS?` handler. Boots red. No `delay()` in hot path.
(No `delay(1)` power tweak — deliberately deferred to a possible v1.2.)

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

## 7. Tests — 32/32 + soak (how to run, what they prove)

- `pio test -e native` → 4 suites: test_checksum (7), test_logic (8),
  test_parser (13), test_stress (4). **Last CI run: all green.**
- `sh run_tests.sh` → same suites via g++ fallback (needed on Termux/Android
  where `pio test` historically crashed — since fixed via pyserial patch, but
  fallback kept) + **soak sim** + HIL (auto-skip without hardware).
- Highlights: exhaustive 1,785 single-byte corruptions (0 false frames; also
  asserts `DD 5A 03…` is a *valid write* → silent); 10 M fuzz (0 emits);
  cadence×register sweep (100 ms–10 s × 03/04/05 + write/unknown interleave);
  bus saturation (5,000 back-to-back polls); 1 s-green / 2 s-red / self-heal /
  rollover; on-target S3 test ELFs link (run only with hardware).
- `tools/soak_sim.cpp` — 8 simulated days: **691,040 polls, all answered**,
  noise rejected, silence→red, millis() wrap crossed mid-run, green-on-resume.
  Runs in ~0.04 s. Wired into run_tests.sh.
- `tools/virtual_meter.py` — scenario modes (reg/period/jitter/mode).
- `tools/test_hardware.py` — HIL pytest; needs `HIL_BUS_PORT` + `HIL_CDC_PORT`.
- Past bugs the suite caught (proof it works): parser emit-on-bad-CK,
  author's own wrong vector (FEEF not FEFF), Arduino `B1` macro collision,
  missing `<cstring>` (Debian GCC 14), missing `<cstdio>` (CI Ubuntu GCC —
  Termux clang had silently accepted both).

---

## 8. Build system & environments

`platformio.ini` envs: `esp32-s3-devkitc-1` (firmware), `native` (host tests,
`build_src_filter = +<bms_protocol.cpp>`), `s3_tests` (on-target test ELFs).
Pinned reality: espressif32@7.1.3, Xtensa GCC 8.4.0 (esp-2021r2-patch5),
Arduino 2.0.x (IDF 4.4 based). `firmware.bin` = 277,360 bytes,
SHA-256 `7195161117ef68e3a3cd4c7793539a87c03083b067bde1a6e5b13ae330a376cf`.
`firmware/` also holds bootloader.bin + partitions.bin + esptool README
(flash addrs 0x0 / 0x8000 / 0x10000). `arduino/` = same firmware as IDE sketch.
`.github/workflows/ci.yml` runs native + firmware + soak on Ubuntu (green).

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
4. QEMU JIT cannot run inside proot (glib `g_quark_init` abort in prebuilt;
   source-built binary runs but see §9). Build in proot, RUN on real Linux.
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
After all three, remaining trace per boot loop: `M25P80: Unknown cmd 0x10`
(~25×) + `Invalid read at addr 0x10200C … region 'er'` (~99×) → still asserts.
Hypothesis: DIO-era command bytes hitting the single-line SSI model + an
unmodeled register region. IDF-based guests are unaffected per Espressif CI;
Arduino guests were never QEMU-supported. Verdict: **emulator gap, not firmware
— our code is never reached.** Practical emulation path = Wokwi (`wokwi/`
scaffolded; meter cycles 03/04/05; needs browser/token, never executed here).
`tools/run_qemu_s3.sh` = one-command boot test for real Linux/Mac.
Next agent: to continue, decode what `0x10` is in DIO context (possible status-
register or continuous-read mode byte) and identify the `0x10200C` peripheral;
or deprioritize — host tests + soak already prove the firmware.

---

## 10. GitHub access & pushing (as of 2026-09-18)

- Account `bm-a` (Bhavishya Madan). NO Developer Program needed — pushing only
  needs a normal account + classic PAT with `repo` scope.
- A PAT (`ghp_…`, full permissions) was **pasted in chat**. It is NOT stored in
  any file (verified: only used via `gh auth login --with-token` inside the
  Debian container). **Recommend the user revoke/rotate it** — chat logs persist.
- `gh` lives ONLY in the Debian container (`/usr/bin/gh`, auth in container
  root's config). Push from there: `proot-distro login debian -- bash -c 'cd
  <repo> && git push origin main'`. (Termux-shell push fails: no credential
  helper.) `gh auth setup-git` was run once in Debian — re-run if pushes fail.
- Commit identity used: `bm-a` / `bm-a@users.noreply.github.com`.
- Releases: v1.0 (ZIP asset), v1.1 (firmware×3 + docx). Re-run
  `gh release create` only if assets change.

---

## 11. Pending / next steps (ordered)

1. **Bench test with a real meter** (the one thing that matters): green ≤ 1 s,
   meter shows ~52 V/100 %, red ~2 s after unplug. Report anomalies → likely a
   one-constant fix (window) or an extra register reply.
2. **HIL pytest** (`HIL_BUS_PORT`/`HIL_CDC_PORT`) + Wokwi run once hardware/PC
   available. QEMU `0x10`/`0x10200C` investigation (see §9) — optional.
3. **Possible v1.2:** `delay(1)` at end of `loop()` (~30 % power cut); re-tag,
   refresh `firmware/`, report.
4. **Housekeeping:** rotate the chat-exposed PAT; re-check CI after any push
   (Node 20 deprecation warnings are non-fatal noise); Google indexes new repos
   with days of lag — normal.

---

## 12. File inventory (project root + home)

Project: `platformio.ini README.md CHANGELOG.md LICENSE VERSION HANDOFF.md
RS485-Tester-Report.docx run_tests.sh src/ test/ tools/ arduino/ firmware/
wokwi/ captures/ docs/ scripts/ releases/ .github/ .gitignore .unity/ .pio/`
(`.pio .unity releases/ *.log tc.json .test_*` git-ignored; `releases/` holds
v1.0 ZIP + 2 git bundles — offline full-history backup, verified by test-clone.)
Termux home `archive/` (backups/logs/installers/bento-dev + ORGANIZE_LOG.txt
with undo commands) — pre-existing clutter tidied 2026-09-18; active
`bento_final.js`, `status_server6.py` etc. deliberately left in place.
Emulator work (`/opt/qemu-src`, `/opt/qemu-s3`, `/opt/pioenv`) is inside the
Debian container — see §8 before wiping proot data.

Resume checklist: `git log --oneline | head -3` → `git status --short` →
`pio test -e native` (Termux) → `sh run_tests.sh` → compare against §7 numbers.

---

## 13. v2.0 addendum (2026-09-22, relay test bench — base frozen)

- v1.x responder core (parser, option-A, tracker, golden frames, LEDs,
  STATUS?, 32 tests, soak) untouched and re-proven: native **50/50**
  (old 32 + `test_relay` 12 + `test_spoof` 6), soak green, virtual-bus PASS.
- New: `src/relay_ctrl.*` (sequencer, 3 button modes, debounce, spoof window
  + frame builder, config struct), `src/web_ui.*` (always-on AP `BMS-Tester`,
  login, dark dashboard, NVS `bms2`, factory reset), integrated in
  `src/main.cpp` (+ identical `.ino`, + IDE tabs). `FW_VERSION` = `"2.0"`.
- GPIOs added: relays R1–R8 = 5/6/7/8/9/12/13/14 (SmartElex 12 V module, own
  12 V supply, common GND, active-LOW default), button = 15, spoof = 21.
- Builds: both envs green in proot (one fix: WebServer 2.0.x `collectHeaders`
  array form). `firmware/` 761,408 B, `firmware-n16r8/` 763,920 B.
- Wokwi: +8 relay LEDs + 2 pushbuttons. QEMU: same known `0x10`/`0x10200C`
  gap (22/84 hits), no regression. Dashboard JS: `node --check` clean.
- Release: tag `v2.0`; assets = 8 MB triple + `n16r8-` triple
  (renamed: GitHub forbids duplicate asset names) + docx. Wiki: +`Relays.md`.
