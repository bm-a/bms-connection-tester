# JBD Smart BMS RS485 Responder + Relay Test Bench — ESP32-S3

A firmware + hardware design that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485, so any compatible meter, display, or host can be exercised without
the real battery pack — **v2.0 adds an 8-relay sequencer** that switches the
meter's own functions in order, driven by a button or a built-in web dashboard.
Status LEDs report link state live: **green = valid BMS traffic seen,
red = bus silent**. No screens needed.

> **New to RS485 or the JBD protocol?** Start with the companion handbook:
> **[jbd-bms-rs485-handbook](https://github.com/bm-a/jbd-bms-rs485-handbook)** —
> how RS485 works, the MAX485 module up close, the JBD frame format with worked
> examples, shopping list with search terms, and a step-by-step build guide.
> (Also known as: JBD BMS emulator, Xiaoxiang BMS simulator/tester, smart BMS
> responder, RS485 battery emulator, e-rickshaw meter tester.)
>
> **No hardware handy?** Run the offline 3D bench simulator:
> **[bms-tester-sim](https://github.com/bm-a/bms-tester-sim)** — ESP32-S3 +
> MAX485 + meter + relays in the browser, exact ESP dashboard embedded.

| | |
|---|---|
| Targets | ESP32-S3 DevKitC-1 (8 MB) + ESP32-S3 N16R8 (16 MB + OPI PSRAM) + MAX485 + 8ch relay · Waveshare ESP32-S3-ETH-8DI-8RO board variant ([docs/waveshare.md](docs/waveshare.md)) |
| Protocol | JBD UART over RS485, 9600 8N1 (registers `0x03`/`0x04`/`0x05`) |
| Releases | **v2.7** current (3 dashboard variants: FULL default, `s3-classic`, `s3-lite`) · **v2.7-ws1** Waveshare ESP32-S3-ETH-8DI-8RO ([docs/waveshare.md](docs/waveshare.md)) · `v2.6` meter estimate · `v2.5` portal/schema/harness · `v2.4` Tasmota update · `v2.3.1` bench patch · `v2.3` relay bench · `v2.2` captive portal · `v2.1` web reliability · `v2.0` relay bench · `v1.2` RGB+N16R8 · `v1.0` frozen (ZIP + tag) |
| Tests | **164 passing** via `sh run_tests.sh` (103 native + 49 web + 12 Waveshare board-backend) + 30-day soak + 3-variant web contract |
| Firmware | `firmware/` (8 MB) + `firmware-n16r8/` (16 MB), SHAs below |
| Web UI | Always-on AP `BMS-Tester` → professional dashboard in 3 variants (FULL default, `s3-classic`, `s3-lite`) — no office Wi-Fi needed |

## Dashboard (v2.7)

Three builds, one per `WEB_UI_VARIANT` — relay names are NVS-persistent in
all three, and OTA always pulls FULL so updating never strands a box.

| Variant | Build | What you get |
|---|---|---|
| **FULL** (default) | `esp32-s3-devkitc-1`, `s3-n16r8` | Everything: live SVG bench card, relay tiles + names, meters-today, sequence config, spoof, OTA, console |
| CLASSIC | `s3-classic` | v2.6 page byte-verbatim |
| LITE | `s3-lite` | Relay tiles + names + LINK pill only (~4.9 KB page) |

![FULL dashboard snapshot](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-full.png)

*FULL variant: LINK pill + cycle counters, live bench strip (48V → ESP → MAX485 → meter readout), glowing relay tiles, meters-today — rendered from live sim state.*

![Sequential run + spoof demo](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/demo.gif)

*R1→R8 sequential run, then a spoof FIRE driving the meter readout off golden — the same flow the 3D bench shows live.*

![LITE dashboard snapshot](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-lite.png)

*LITE variant: relays + names + link, nothing else — smallest flash footprint.*

## What v2.7 adds (three dashboards, live bench SVG, sim relay board)

- **Three dashboard variants, one per build** (`WEB_UI_VARIANT` in
  `platformio.ini`; only the selected page compiles in): FULL (default —
  classic + live inline SVG bench + tile glow + meter readout, zero CDN),
  CLASSIC (`s3-classic`, v2.6 page byte-verbatim), LITE (`s3-lite`, relay
  tiles + names + LINK pill). Relay names NVS-persistent everywhere; OTA
  assets stay FULL.
- **Live meter readout** (`mv`/`ma`/`msoc` in `/api/state`, read-only):
  mirrors the last `0x03` reply — golden or active spoof stage.
- **Offline 3D bench** (`local-wokwi/`, also standalone at
  [bms-tester-sim](https://github.com/bm-a/bms-tester-sim)): one ESP +
  MAX485 + photo-textured meter + 8 relays + GX16 sockets, electron-flow
  viz sized by live rail current, pressable buttons (incl. factory-reset
  hold + OTA check), relay board row with persistent names, verbatim ESP
  page embedded below.

## What v2.6 adds (daily meter estimate, round link dots, one-file flash)

- **Daily meter counting, software-only estimate** (no button, no new GPIO):
  the box watches link gaps — RED ≥ 3 s closed by GREEN = reseat = new
  meter; steady GREEN across RESTARTs/retries = same meter; sub-3 s
  flickers stay; mid-cycle gaps defer to IDLE. Dashboard card shows
  meters/attempts/pass/fail (pass = a full cycle before the swap) with a
  manual New-day reset (no RTC; boot persists; backups exclude counters).
- **Round link dots** (green/red) in the dashboard header beside the LINK pill.
- **One-file flash images**: `firmware/bms-tester-8mb.bin` +
  `firmware-n16r8/bms-tester-n16r8.bin` (merged bootloader+partitions+app,
  single `write-flash 0x0` command, structure-verified).
- **Proof**: `test_meter` (10 Unity tests) + 5 web meter tests + 8 emu HTTP
  checks over the new `/__bus` control; 62/62 emu + 10/10 soak + 152/152 total.

## What v2.5 adds (trigger save, portal landing, structured config, emulation)

- **Trigger group** on the spoof card: enable + GPIO + polarity with a
  dedicated Save (no firing) — physical-switch users finally have a save
  path, and `sinv` is wired end to end (was NVS-only).
- **Captive-portal landing**: phones that pop the mini-browser get a slim
  page with a big Open-Dashboard button + Safari/Chrome steps; everything
  else still 302s to `/`. Dismissing the popup no longer matters — the
  session lives in the real browser.
- **Structured config** (`docs/CONFIG-SCHEMA.md`): sectioned v2 backups
  (relays/spoof/trigger/network/ota/meta, passwords in no section), v1 flat
  backups still migrate, one shared validation table.
- **Fixed by emulation**: multipart-auth 403s, whitespace-JSON rejects,
  test-then-install dead end (check/install/URL now join on demand).
- **Proof**: socket harness (real handlers, real HTTP) 54 checks + 48 h run
  10 checks + 30-day soak (2.59 M polls, 309 k cycles, 30 NVS commits) —
  see `docs/EMULATION-v2.5.md`. Text-browser renders (`w3m`) verify pages.

## What v2.4 adds (Tasmota-grade update + relay review R1–R31)

- **Firmware upload rebuilt Tasmota-style** (`/update` works fully offline):
  exact variant-asset match (no 8 MB ↔ N16R8 cross-flash), explicit sketch
  budget (never `SIZE_UNKNOWN`), `0xE9` magic + flash-size-vs-chip gate on
  the first bytes, ONE finalize in the done handler, progress bar, and every
  failure names its cause. A wrong file or password changes nothing — the box
  keeps running. OTA-pull installs enforce the same gates; custom firmware
  URL (Tasmota OtaUrl) + Upgrade-from-URL supported.
- **Per-mode relay menu**: the Sequence card shows only the active mode's
  fields (Sequential: step/hold/dir; Chase: step/sweeps/dir; All-ON:
  hold/stagger). Chase gains a fixed 20 ms break-before-make (release is
  slower than pull-in); START is refused 0.5 s after STOP (relay settle);
  mode switch needs IDLE; loop needs a finite hold; count-shrink acts
  immediately (stale forces cleared, never resurrected); RESTART keeps tile
  forces. Timing floors: step ≥ 100 ms (default 250), stagger default 50 ms,
  pause 500–60000 ms (default 2000).
- **Spoof Save-only** (stage values without firing), **web console**
  (`START STOP FIRE CANCEL STATUS UPTIME VERSION REBOOT RESET HELP`),
  **config backup/restore** JSON (passwords never exported), **one-shot STA
  uplink test** (joins ≤ 30 s without rebooting, reports RSSI/IP, drops back
  to AP-only), **Information card** (variant/flash/sketch/heap/uptime/boot
  count/reset reason/RSSI/pin map), **mDNS** (`bmstester.local`), keep-WiFi
  reset, boot-counter reset, editable OTA check cadence.

## What v2.3.1 fixes/adds (bench-driven patch on v2.3)

- **No more login wall.** The dashboard is open on the WPA2 AP; reboot,
  factory reset, `/update` upload, OTA-admin and AP/admin saves ask for the
  admin password per request (default `admin123`). Passwords never appear
  in `/api/state`.
- **Saves stick.** The 1 s tick is status-only now; form fields fill on load
  and after saves, never mid-typing. UTF-8 declared on all pages.
- **Chase auto-hold** (`Chase sweeps`, default 3, 0 = forever) retunes with
  relay count × step. **Spoof trigger GPIO** configurable (safe pins only).
- **WiFi kill switch:** ground GPIO18 to drop the AP, release to restore.
- **OTA that works:** tolerant release-tag parse + dashboard Install button.

## What v2.3 adds (v1.x base still frozen)

- **Relay count + chase wave.** First N of 8 relays participate (beyond-N forced
  OFF); new CHASE mode sweeps a single lit relay R1→Rn. Per-mode holds in
  milliseconds (sequential / chase / ALL-ON soak).
- **2-stage spoof:** stage 1 shows realistic 100/100/100/100 % first, then
  stage 2 shows the 88.8/88.8/88.8/188 pattern — both stages fully editable.
- **Industrial pack:** loop + inter-cycle pause + cycle limit (burn-in racks),
  ALL-ON stagger (inrush ramp), direction fwd/rev, 8 relay labels, cycle +
  actuation counters, boot auto-start.
- **Persistent logins** ("remember this device", reboot-safe) + **OTA**
  (offline `/update` upload + automatic GitHub updates over the optional STA
  uplink). Config saves no longer stall the loop (coalesced flash writes).

## What v2.0 adds (v1.x base frozen)

- **8 relays, sequential or all-at-once.** R1–R8 on GPIO 5/6/7/8/9/12/13/14 drive
  a SmartElex-style 12 V module (own 12 V supply, common GND, default active-LOW).
  Step delay, hold time, polarity — all on the web page.
- **Button with 3 behaviors** (GPIO15, web-selectable): hold-X-seconds with
  re-press abort, run-to-completion locked, or re-press restarts the cycle.
  10 s hold = factory reset fallback.
- **Fault spoof:** GPIO21 (or web FIRE) makes the meter read 88.8 V / 88.8 A /
  88.8 °C / 188 % on `0x03` for 10 s (configurable 1–120 s + values), then
  auto-reverts. `0x04`/`0x05` never change.
- **Web dashboard:** always-broadcasting AP, login (`admin`/`admin123`, change
  on first login), live relay grid + sequence/spoof/admin cards, NVS persistence.

 ## Hardware modules — what it runs on

![Power and signal tree](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/wiring.png)

| Module | Role | Key pins / settings |
|---|---|---|
| ESP32-S3 DevKitC-1 / N16R8 | Application MCU (240 MHz LX7, native USB) | UART2: TX = GPIO17, RX = GPIO16 |
| MAX485 (or SP3485) | Half-duplex RS485 transceiver | DI ← TX, RO → RX, DE+RE ← GPIO4 |
| Green / red LEDs + RGB | Link indicator (mutually exclusive) | GPIO10 / GPIO11 via 220 Ω; GPIO48 RGB mirrors |
| 8ch relay module (v2.0) | Switches meter functions in sequence | IN1–8 ← GPIO5/6/7/8/9/12/13/14; **separate 12 V supply** |
| Button (v2.0) | Starts/stops sequences | GPIO15 to GND (pull-up) |
| Spoof input (v2.0) | Triggers 10 s test values | GPIO21 to GND (pull-up) |
| 10 kΩ resistor | Pull-down on GPIO4 | Boots in listen mode, never jams the bus |

Full wiring, relay supply, power (USB for ESP + 12 V for coils), and
troubleshooting in [`docs/MODULES.md`](docs/MODULES.md).
Protocol byte layout and checksum rule in [`docs/PROTOCOL.md`](docs/PROTOCOL.md).
Relay/web guide in the [wiki](../../wiki) (mirrored in [`wiki/`](wiki/)).

## Firmware behavior

- Validates every incoming frame completely — line noise can never fake a link
  (proven: 10 M-byte fuzz, zero emits). Answers `0x03` (52.0 V, 100 %),
  `0x04` (14-cell), `0x05` (name); silent on writes/unknown, still counted live.
- Link window self-adjusts (2–10 s); `STATUS?` replies `GREEN 2.7` / `RED 2.7`.
- Joining the AP pops the dashboard automatically (captive portal, fixed 192.168.4.1); turn mobile data off if the phone routes around it.
- Sequencer runs on `millis()` — no `delay()` anywhere; RS485 keeps priority.
- AP `BMS-Tester` is up from every boot; connect any phone/laptop, open the
  dashboard (usually `192.168.4.1`), configure. Ground GPIO18 to kill WiFi.

## Flash it (pick one)

- **Ready binaries (esptool, any PC):** `pip install esptool`, then for your board:
  `esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 <bootloader.bin> 0x8000 <partitions.bin> 0x10000 <firmware.bin>`
  using `firmware/` (8 MB) or `firmware-n16r8/` (N16R8) — see those READMEs.
  For the **Waveshare ESP32-S3-ETH-8DI-8RO**: flash the `bms-tester-waveshare.bin`
  merged image at `0x0` from the `v2.7-ws1` prerelease ([docs/waveshare.md](docs/waveshare.md)).
- **PlatformIO:** upload `esp32-s3-devkitc-1` (8 MB / Wokwi), `s3-n16r8`, or
  `s3-waveshare` (Waveshare ESP32-S3-ETH-8DI-8RO).
- **Arduino IDE:** open `arduino/bms_connection_tester/bms_connection_tester.ino`
  (all tabs open automatically; ESP32S3 Dev Module, USB CDC On Boot Enabled,
  921600; N16R8 also Flash 16MB + OPI PSRAM).

## Verify it

```sh
sh run_tests.sh          # full 152: g++ suites + web contract + test_web + test_upload + test_meter + soak (always); HIL when attached
pio test -e native       # 103 Unity tests: checksum, logic, parser, stress, relay, spoof, meter, system, ota, upload
pio run -e esp32-s3-devkitc-1 -e s3-n16r8  # both firmware profiles compile (run inside proot-debian: glibc toolchain)
```

Emulator results for v2.6 (Termux + Debian proot):
- Socket harness (`tools/fw_emu`, real handlers over real HTTP): 62 checks
  PASS (portal, relay flows, spoof/trigger, uploads, backup/restore,
  console, resets, STA/OTA, info, meter heuristic + round dots via new
  `/__bus` bus-state control) + 48 h run (10 checks) PASS.
- Native 152/152 (103 pio + 49 web + contract) + web-contract PASS + 30-day
  soak (2.59 M polls, rollover crossed) — PASS (frozen v1.x untouched).
- Virtual bus (`sh tools/virtual_bus.sh`, in proot-debian for /tmp): 03/04/05 golden, silences,
  resync, red-after-silence — PASS.
- Dashboard + portal pages: `w3m -dump` renders verified; JS `node --check`
  clean; JS↔firmware contract gate green (incl. single-end, no-SIZE_UNKNOWN,
  portal-surface rules).
- Wokwi: diagram DUT pins verified against firmware (relays 5,6,7,8,9,12,13,14 · LEDs 10,11 · RGB 48 · UART 16,17 · button 15 · kill 18 · spoof 21; v2.6 adds no pins);
  headless run needs `WOKWI_CLI_TOKEN` (CI-gated).
- QEMU-S3: parked (Stage-0 flash model clean; guest resets in 2nd-stage bootloader SITE1) — see HANDOFF.

`firmware/bms-tester-8mb.bin` SHA-256: see `firmware/README.md` (refreshed for v2.6; merged single-file image + separate files).
`firmware-n16r8/bms-tester-n16r8.bin` SHA-256: see `firmware-n16r8/README.md`.

## Versions

- **v2.7** — three dashboard variants (FULL default + `s3-classic` +
  `s3-lite`), live SVG bench + meter readout (`mv/ma/msoc`), persistent
  relay names in all variants, OTA stays FULL. 152/152 tests, contract 3/3.
- **v2.6** — software-only daily meter estimate (link gaps ≥ 3 s = new
  meter, no button/GPIO), round link dots, one-file merged flash images,
  `test_meter` + web/emu meter coverage. 152/152 tests (103 pio + 49 web),
  62/62 emu + 10/10 soak.
- **v2.5** — trigger save, portal landing, structured config, on-demand STA,
  whitespace-tolerant JSON, socket harness (64/64) + 30-day soak.
  137/137 tests.
- **v2.4** — Tasmota-grade update path, per-mode relay menu (chase BBM, stop
  dead-band, timing floors), spoof save-only, console, backup/restore,
  custom OTA URL, STA uplink test, info card, mDNS, keep-WiFi/bootcount
  resets. 134/134 tests.
- **v2.3.1** — bench patch: no login wall (per-request admin password),
  sticky saves, UTF-8 pages, chase auto-sweeps, spoof trigger GPIO, WiFi
  kill switch (GPIO18), working OTA check + Install button. 103/103 tests.
- **v2.3** — relay count + chase, 2-stage spoof, per-mode ms holds, industrial
  pack (loop/pause/limit/stagger/direction/labels/counters/autostart),
  persistent logins, manual + auto OTA, coalesced saves. 105/105 tests.
- **v2.2** — captive portal (login page pops on join) + fixed 192.168.4.1.
- **v2.1** — website reliability: 14 host-executed web tests + contract gate,
  `select_reply()` now host-covered, 24 h office-day sim, Wokwi relay modules
  + automation scenario + token-gated CI sim. No behavior change (bench-confirmed
  active-LOW default stands).
- **v2.0** — 8-relay sequencer + always-on AP dashboard + spoof window + admin auth.
  Responder core frozen (66/66 incl. original 32).
- **v1.2** — onboard RGB mirror (GPIO48) + N16R8 16 MB/OPI build + Wokwi NeoPixel.
- **v1.1** — multi-register (03/04/05) + adaptive window + silence-on-unknown + hardened parser.
- **v1.0** — 0x03-only responder, fixed 2 s window. Frozen: `releases/bms-connection-tester-v1.0.zip` + tag.

See [`CHANGELOG.md`](CHANGELOG.md) for full notes.

## Layout

`src/` firmware (protocol core + relay ctrl + web UI + OTA logic) · `docs/` module + protocol ·
`test/` 105 Unity tests · `tools/` meter sim, HIL pytest, QEMU script, soak, report ·
`arduino/` IDE sketch · `firmware/` 8 MB binaries · `firmware-n16r8/` N16R8 binaries ·
`wokwi/` browser sim (relays + buttons) · `local-wokwi/` offline 3D bench
(sim server + Three.js bench + verbatim ESP page + `shots.py` snapshots) ·
`enclosure/` IP65 standalone box + GX16 meter link docs · `captures/` Docklight recordings ·
`scripts/` Linux/Termux setup · `wiki/` wiki sources · `.github/workflows/` CI.
