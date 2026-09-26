# JBD Smart BMS RS485 Responder + Relay Test Bench — ESP32-S3

A firmware + hardware design that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485 so you can test the **meter/display** — the meter is the device
under test, not a real BMS. The box pretends to be a healthy 14S pack,
answers the meter's JBD polls with golden or fault-injected values, and an
**8-relay sequencer** switches the meter's own functions in order, driven by
a button or the built-in web dashboard. Status LEDs report link state live:
**green = valid BMS traffic seen, red = bus silent**. No screens needed.

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
| Releases | **v2.8** current — FULL dashboard + Waveshare in one release · `v2.7` 3 dashboard variants · `v2.6` meter estimate · `v2.5` portal/schema/harness · `v2.4` Tasmota update · `v2.3.1` bench patch · `v2.3` relay bench · `v2.2` captive portal · `v2.1` web reliability · `v2.0` relay bench · `v1.2` RGB+N16R8 · `v1.0` frozen (ZIP + tag) |
| Tests | **166 passing** via `sh run_tests.sh` (native Unity + web + Waveshare board-backend) + 30-day soak + 3-variant web contract |
| Firmware | `firmware/` (8 MB) + `firmware-n16r8/` (16 MB) one-file images, SHAs in their READMEs |
| Web UI | Always-on AP `BMS-Tester` → dashboard in 3 variants (FULL default, `s3-classic`, `s3-lite`) — no office Wi-Fi needed |
| Build guides | [DIY build-it-yourself](docs/BUILD-DIY.md) · [Waveshare build](docs/BUILD-WAVESHARE.md) |

## Dashboard (v2.8)

Three builds, one per `WEB_UI_VARIANT` — relay names are NVS-persistent in
all three, and OTA always pulls FULL so updating never strands a box.

| Variant | Build | What you get |
|---|---|---|
| **FULL** (default) | `esp32-s3-devkitc-1`, `s3-n16r8`, `s3-waveshare` | Everything: live SVG bench card, relay tiles + names, meters-today, sequence config, spoof, OTA, console |
| CLASSIC | `s3-classic` | v2.6 page byte-verbatim |
| LITE | `s3-lite` | Relay tiles + names + LINK pill only (~4.9 KB page). Tiles tap = force ON/OFF (needs IDLE) |

![FULL dashboard snapshot](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-full.png)

*FULL variant: LINK pill + cycle counters, live bench strip (48V → ESP → MAX485 → meter readout), glowing relay tiles, meters-today — rendered from live sim state.*

![Sequential run + spoof demo](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/demo.gif)

*R1→R8 sequential run, then a spoof FIRE driving the meter readout off golden — the same flow the 3D bench shows live.*

![LITE dashboard snapshot](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-lite.png)

*LITE variant: relays + names + link, nothing else — smallest flash footprint.*

### Dashboard cards (FULL)

| Card | What it does |
|---|---|
| Bench (live) | Inline SVG bench card: 48 V → bucks → ESP → MAX485 with TX/RX dots → A/B flow → meter readout (`mv`/`ma`/`msoc`), 8 relay blocks glowing green. Zero CDN — works on the offline AP |
| Relays | START / STOP ALL buttons + 8 relay tiles. Tapping a tile forces it ON/OFF (`/api/relay`); forcing during CHASE idles the wave first so two relays never light at once |
| Meters today (approx) | `meters` / `attempts` / `pass` / `fail` + **New day (reset)** button. Software estimate, honestly labeled — see [Meter counter](#meter-counter) |
| Sequence config | Mode, relay count, step/hold, chase sweeps, stagger, direction, button behavior, logic, loop/pause/limit, Save. Only the active mode's fields are shown |
| Relay labels | 8 editable names, NVS-persistent (display-only; quotes/backslashes rejected) |
| Fault spoof (0x03 test values) | Pin-trigger enable + GPIO + polarity + Save trigger; stage 1 and stage 2 V/A/°C/SOC%/secs; FIRE now / Save only / Cancel |
| Firmware update | OTA status + latest tag; auto-check toggle + cadence (hours, 0 = manual); Check now / Install update; custom firmware URL + Upgrade from URL; **STA uplink** (enable + SSID/pass + Test uplink); **Backup configuration** (JSON download, passwords never included) + Restore from file |
| Admin & Wi-Fi AP | AP SSID / pass (8+, blank = keep) / channel (1–13); new admin pass (4+, blank = keep); auto-start sequence on boot (burn-in); Save / Save+reboot / Reboot / Factory reset / Reset settings (keep Wi-Fi) / Reset boot counter |
| Information | Firmware + variant, flash size, free sketch space, heap, uptime, boot count, reset reason, STA RSSI/IP, pin map for the board variant |
| Console | One-line command box: `START STOP FIRE CANCEL DAYRESET STATUS UPTIME VERSION REBOOT RESET HELP` — hardware verbs ask for the admin password |

The dashboard is open on the WPA2 AP — no login wall. Reboot, factory
reset, `/update` upload, OTA-admin and AP/admin saves ask for the admin
password **per request** (default `admin123`). Passwords never appear in
`/api/state`.

## JBD responder

The box is a virtual JBD BMS. It answers the meter's polls and nothing else:

| Register | Reply |
|---|---|
| `0x03` basic info | 52.0 V (`0x1450`), 0 A, 100 % SOC (`0x64`), 14 cells — byte-exact golden frame captured from a real pack |
| `0x04` cell voltages | 14 × 3714 mV = 52.0 V, checksum-verified synthesis |
| `0x05` device name | `TEST-14S100A` |
| writes / unknown registers | **silent** — no reply, still counted live |

- **Streaming parser + checksum**: every incoming frame is validated
  completely — line noise can never fake a link. Proven by a **10 M-byte
  structured fuzz** (pure random + valid-prefix mutations), zero emits.
- **Adaptive link window**: the green/red decision uses
  `clamp(2×EMA + 500 ms, 2 s, 10 s)` — self-adjusts to any meter poll speed.
  Green LED = well-formed frame seen inside the window; red = bus silent.
- **USB `STATUS?`**: send `STATUS?` over USB-serial (test-jig only, not a JBD
  command) → `GREEN 2.8` / `RED 2.8`. First token is stable for HIL fixtures.
- Protocol byte layout and checksum rule: [`docs/PROTOCOL.md`](docs/PROTOCOL.md).

## 8-relay sequencer

Switches the meter's own functions in order. Runs on `millis()` — no
`delay()` anywhere, RS485 keeps priority. Relays park **OFF** at boot;
all state resets clean.

| Mode | Behavior |
|---|---|
| Sequential 1-N | R1→Rn (or Rn→R1), each ON for the step delay, then the sequential hold |
| Chase wave | Single lit relay sweeping R1→Rn, wraps; **20 ms break-before-make** — each advance parks all-OFF first (release is slower than pull-in) |
| All ON at once | All N relays ON with optional stagger (0 = contactor slam, 20–1000 ms = inrush ramp) |

Config ranges (validate-then-commit: a rejected save changes **nothing**):

| Key | Range | Default |
|---|---|---|
| Relay count | 1–8 (beyond-N forced OFF; shrinking drops stale forces immediately) | 8 |
| Step ms | 100–60000 | 250 |
| Hold sequential ms | 0–3600000 (0 = stay ON) | 30000 |
| Chase sweeps | 0–100 (0 = forever; auto-hold = sweeps × relays × step) | 3 |
| Hold all-on ms | 0–3600000 (0 = stay ON) | 300000 |
| All-ON stagger ms | 0–1000 (0 = all at once) | 50 |
| Direction | R1→Rn / Rn→R1 | R1→Rn |
| Button | Hold X ms, re-press=OFF / Run to end, ignore presses / Re-press restarts | Hold X ms |
| Logic | Active-LOW (SmartElex) / Active-HIGH (no-op on Waveshare — TCA9554 bit HIGH = ON) | Active-LOW |
| Loop cycles | on/off + pause 500–60000 ms + cycle limit 0–60000 (0 = ∞) | off / 2000 / 0 |
| Auto-start on boot | burn-in racks | off |

Safety rules, enforced in firmware and host-tested:

- **0.5 s post-STOP dead-band** — START inside it is refused ("relays
  settling"); a refused start opens no attempt.
- **Mode switch needs IDLE** — switching mode with relays live is rejected.
- **Loop needs a finite hold** — hold 0 never expires, so loop + hold 0 is refused.
- **RESTART keeps tile forces**; count-shrink clears stale forces, never resurrects them.
- **Actuations counter** counts turn-ON edges only; **cyclesDone** counts
  completed cycles. Both surface on the dashboard and in `STATUS`.

## Meter counter

Software-only **estimate** of units tested per day — no button, no extra
GPIO, no RTC. The JBD protocol carries no meter ID, so the box watches link
gaps and applies one documented workflow assumption: a failed/retried test
does **not** unplug the meter (link stays GREEN); a new meter means a
physical reseat (link drops to RED for seconds).

- Link gap **≥ 3 s** arms a reseat **candidate**; the new meter **commits
  only after GREEN holds 5 s** (fumble guard — seat 4 s, pull, seat properly
  = one unit, not two).
- Sub-3 s flickers (slow poll, noise) stay on the same meter.
- Steady GREEN across RESTARTs/retries = same meter, however many attempts.
- The closing meter's verdict is snapshotted at arm time, so an eager START
  during the settle window attributes to the **new** meter.
- A **mid-cycle yank invalidates the test**: the previous meter closes as
  fail (no completed cycle while validly under test).
- Dashboard shows **meters / attempts / pass / fail** (pass = a full cycle
  completed before the swap). **New day (reset)** is manual — no RTC; boot
  persists (a power flicker never eats QC data); backups exclude counters.
- NVS is coalesced: **≤ 1 write per meter** — a power cut loses at most the
  current meter's attempts, stated on the card, not hidden.

## Fault spoof

Makes the meter display abnormal `0x03` data — tests how the **meter
renders** bad values. It does not trip real BMS protections (there is no
real BMS here). `0x04`/`0x05` never change.

Two stages, each fully editable (V/A/°C in ×0.1 units, SOC %, seconds):

| | V | A | °C | SOC | Secs | Range |
|---|---|---|---|---|---|---|
| Stage 1 (realistic full pack) | 100.0 | 100.0 | 100.0 | 100 % | 5 | values 0–999.9, secs 1–120 |
| Stage 2 (over-range pattern) | 88.8 | 88.8 | 88.8 | 188 % | 10 | values 0–999.9, secs 1–120 |

- **FIRE now** = save + fire; **Save only** stages values + pin without
  firing; **Cancel** aborts a running plan (auto-reverts after stage 2).
- **Trigger group**: enable + GPIO + polarity (pull-LOW to fire with
  pull-up / pull-HIGH to fire). Save trigger stores the triple without
  firing — for physical-switch users.
- Safe trigger pins, anything else falls back: generic boards
  `1, 2, 21, 38–44, 47` (fallback **21**); Waveshare `DI1–DI8`
  (`GPIO4–11`, active LOW, fallback **DI1**).

## Admin, network & OTA

- **Always-on AP** `BMS-Tester` (WPA2), fixed gateway **192.168.4.1**,
  captive portal with landing page (Safari/Chrome steps), mDNS
  **`bmstester.local`** for laptops/desktops. Turn mobile data off if the
  phone routes around the portal.
- **STA uplink** (optional, for GitHub OTA only — the AP stays on): enable +
  SSID + pass, one-shot **Test uplink** (joins ≤ 30 s without rebooting,
  reports RSSI/IP, drops back to AP-only).
- **OTA**: checks `https://api.github.com/repos/bm-a/bms-connection-tester/releases/latest`
  — one unified v2.8 line, Waveshare included. Exact variant-asset match
  (`firmware.bin` / `n16r8-firmware.bin` / `waveshare-firmware.bin`);
  Tasmota-grade gates: `0xE9` magic + flash-size-vs-chip gate on the first
  bytes, explicit sketch budget (never `SIZE_UNKNOWN`), exact variant match
  (no 8 MB ↔ N16R8 cross-flash). Auto-check cadence editable (0–720 h,
  0 = manual) + Check now + Install update. A wrong file or password changes
  nothing — the box keeps running.
- **Custom firmware URL** + **Upgrade from URL** (Tasmota OtaUrl style).
- **Manual `/update` upload**: works fully offline, progress bar, same gates.
- **Backup / Restore**: whole config as sectioned JSON
  (`relays`/`spoof`/`trigger`/`network`/`ota`/`meta`, `{"config":2,...}` —
  see [`docs/CONFIG-SCHEMA.md`](docs/CONFIG-SCHEMA.md)). **Passwords never
  leave the box** (they are in no section, by design). v1 flat backups still
  migrate.
- **Reboot** (flushes pending saves first), **Factory reset** (wipes NVS +
  reboots; also: hold the button 10 s), **Reset settings (keep Wi-Fi)** —
  wipes bench settings but keeps AP/STA/admin identity, **Reset boot
  counter** (no reboot needed).
- **Coalesced NVS saves**: config writes batch, never stall the loop.
- **Console verbs**: `START STOP FIRE CANCEL DAYRESET STATUS UPTIME VERSION
  REBOOT RESET HELP` — hardware verbs (`START STOP FIRE CANCEL DAYRESET
  REBOOT RESET`) require the admin password; `STATUS` prints link state,
  run state, cycles, actuations, spoof stage, and the meter counters.

## Hardware — generic DIY build

Full step-by-step: [docs/BUILD-DIY.md](docs/BUILD-DIY.md). Pin map:

| Module | Role | Pins |
|---|---|---|
| ESP32-S3 DevKitC-1 / N16R8 | Application MCU (240 MHz LX7, native USB) | UART2 TX = GPIO17, RX = GPIO16 |
| MAX485 (or SP3485) | Half-duplex RS485 transceiver | DI ← TX17, RO → RX16, DE+RE ← GPIO4 (**10 kΩ pull-down** — boots in listen mode, never jams the bus) |
| Green / red LEDs | Link indicator (mutually exclusive) | GPIO10 / GPIO11 via 220 Ω; GPIO48 RGB mirrors |
| 8ch relay module | Switches meter functions in sequence | IN1–8 ← GPIO5/6/7/8/9/12/13/14; **separate 12 V supply, common GND** |
| Button | Starts/stops sequences | GPIO15 to GND (pull-up); 10 s hold = factory reset |
| WiFi kill | Drops the AP | GPIO18 to GND (pull-up); ground = AP off, release = restore |
| Spoof trigger | Fires the 2-stage plan | GPIO21 to GND (configurable, safe pins only) |

Power: USB for the ESP + 12 V for the relay coils. Full wiring, relay
supply, and troubleshooting in [`docs/MODULES.md`](docs/MODULES.md).

## Hardware — Waveshare ESP32-S3-ETH-8DI-8RO

Full step-by-step: [docs/BUILD-WAVESHARE.md](docs/BUILD-WAVESHARE.md).
Details in [`docs/waveshare.md`](docs/waveshare.md).

| Function | Mapping |
|---|---|
| Relays R1–R8 | TCA9554PWR @ I2C `0x20` (SDA42/SCL41); EXIO1–8 = bits 0–7, **bit HIGH = ON**; parked OFF at boot. The web "active-low" toggle is a **no-op** on this board |
| RS485 | Isolated transceiver, TX17/RX18, **hardware auto direction** — no DE pin |
| BOOT button | GPIO0 = START/STOP (holding at power-on enters download mode — normal) |
| DI1 (GPIO4) | Spoof trigger (default, web-changeable; DI1–DI8 = GPIO4–11, opto-isolated, active LOW) |
| DI2 (GPIO5) | WiFi kill (default; ground = AP off) |
| RGB status | GPIO38 |
| W5500 Ethernet | Pins reserved, **not implemented** — Wi-Fi kept |

## Build environments

`platformio.ini` (only the selected `WEB_UI_VARIANT` page compiles in —
no flash bloat):

| Env | Board | What |
|---|---|---|
| `esp32-s3-devkitc-1` | ESP32-S3 8 MB | FULL dashboard (default) |
| `s3-n16r8` | 16 MB + OPI PSRAM | FULL dashboard, N16R8 flash/PSRAM map |
| `s3-classic` | 8 MB | CLASSIC dashboard (v2.6 page) |
| `s3-lite` | 8 MB | LITE dashboard |
| `s3-waveshare` | Waveshare ESP32-S3-ETH-8DI-8RO | FULL dashboard, TCA9554 relays, isolated RS485 |
| `s3_tests` / `native` | host | on-target / host unit tests |

**v2.8 ships FULL (generic) + Waveshare.** OTA assets stay FULL on every
line so updating never strands a box.

## Flash it

- **Ready binaries (esptool, any PC):** `pip install esptool`, then one
  command per image — merged bootloader+partitions+app, structure-verified:
  `esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 <image>`
  - Generic 8 MB: `firmware/bms-tester-8mb.bin`
  - N16R8: `firmware-n16r8/bms-tester-n16r8.bin`
  - Waveshare: `bms-tester-waveshare.bin` (v2.8 release asset) at `0x0`
- **PlatformIO:** upload `esp32-s3-devkitc-1`, `s3-n16r8`, or `s3-waveshare`.
- **Arduino IDE:** open `arduino/bms_connection_tester/bms_connection_tester.ino`
  (all tabs open automatically; ESP32S3 Dev Module, USB CDC On Boot Enabled,
  921600; N16R8 also Flash 16MB + OPI PSRAM).

## Verify it

```sh
sh run_tests.sh          # 166 Unity tests: checksum, logic, parser, stress,
                        # relay, spoof, meter, ota, upload, web, system,
                        # waveshare board-backend + web contract + 30-day soak
pio test -e native       # host Unity suite (same asserts as run_tests.sh)
pio run -e esp32-s3-devkitc-1 -e s3-n16r8 -e s3-waveshare   # firmware compiles
```

- Socket harness (`tools/fw_emu`, real handlers over real HTTP): portal,
  relay flows, spoof/trigger, uploads, backup/restore, console, resets,
  STA/OTA, info, meter heuristic — PASS.
- 30-day soak (2.59 M polls, rollover crossed): PASS.
- Virtual bus (`sh tools/virtual_bus.sh`): 03/04/05 golden, silences,
  resync, red-after-silence — PASS.
- Dashboard + portal pages: `w3m -dump` renders verified; JS `node --check`
  clean; JS↔firmware contract gate green.
- HIL pytest (`tools/`) when a real ESP32-S3 + RS485 adapter is attached.
- QEMU-S3: parked (Stage-0 flash model clean; guest resets in 2nd-stage
  bootloader) — see HANDOFF.md.

## Versions

| Version | Highlights |
|---|---|
| **v2.8** | Meter counter hardened: 3 s gap arms a reseat candidate, new meter commits after GREEN holds **5 s** (fumble guard); verdict snapshotted at arm time (eager START attributes to the new meter); mid-cycle yank = fail. **Unified release line — Waveshare included**, OTA via `releases/latest` for all variants. Ships FULL (generic) + Waveshare. 166/166 tests |
| v2.7 | Three dashboard variants (FULL default + `s3-classic` + `s3-lite`), live SVG bench + meter readout (`mv`/`ma`/`msoc`), persistent relay names everywhere, OTA stays FULL. Waveshare `v2.7-ws1` prerelease line |
| v2.6 | Software-only daily meter estimate (link gaps = new meter, no button/GPIO), round link dots, one-file merged flash images, `test_meter` + web/emu meter coverage |
| v2.5 | Trigger save, portal landing, structured config (`docs/CONFIG-SCHEMA.md`), on-demand STA, whitespace-tolerant JSON, socket harness + 30-day soak |
| v2.4 | Tasmota-grade update path, per-mode relay menu (chase BBM, stop dead-band, timing floors), spoof save-only, console, backup/restore, custom OTA URL, STA uplink test, info card, mDNS, keep-WiFi/bootcount resets |
| v2.3.1 | No login wall (per-request admin password), sticky saves, UTF-8 pages, chase auto-sweeps, spoof trigger GPIO, WiFi kill switch (GPIO18), working OTA check + Install button |
| v2.3 | Relay count + chase, 2-stage spoof, per-mode ms holds, industrial pack (loop/pause/limit/stagger/direction/labels/counters/autostart), persistent logins, manual + auto OTA, coalesced saves |
| v2.2 | Captive portal (login page pops on join) + fixed 192.168.4.1 |
| v2.1 | Website reliability: 14 host-executed web tests + contract gate, 24 h office-day sim |
| v2.0 | 8-relay sequencer + always-on AP dashboard + spoof window + admin auth. Responder core frozen |
| v1.2 | Onboard RGB mirror (GPIO48) + N16R8 16 MB/OPI build + Wokwi NeoPixel |
| v1.1 | Multi-register (03/04/05) + adaptive window + silence-on-unknown + hardened parser |
| v1.0 | 0x03-only responder, fixed 2 s window. Frozen: `releases/bms-connection-tester-v1.0.zip` + tag |

See [`CHANGELOG.md`](CHANGELOG.md) for full notes.

## Layout

`src/` firmware (protocol core + relay ctrl + web UI + OTA logic) ·
`docs/` module + protocol docs + the two build guides ·
`test/` 166 Unity tests · `tools/` socket harness, virtual bus, soak, HIL
pytest, QEMU script · `arduino/` IDE sketch · `firmware/` 8 MB one-file
image · `firmware-n16r8/` N16R8 one-file image · `wokwi/` browser sim ·
`local-wokwi/` offline 3D bench (sim server + Three.js bench + verbatim ESP
page) · `enclosure/` IP65 standalone box + GX16 meter link docs ·
`captures/` Docklight recordings · `scripts/` Linux/Termux setup ·
`wiki/` wiki sources (mirrors the [GitHub wiki](../../wiki)) ·
`.github/workflows/` CI.

Docs: [`docs/MODULES.md`](docs/MODULES.md) (wiring, relay supply, power,
troubleshooting) · [`docs/PROTOCOL.md`](docs/PROTOCOL.md) (byte layout,
checksum) · [`docs/CONFIG-SCHEMA.md`](docs/CONFIG-SCHEMA.md) (backup shape,
validation, migration) · [`docs/waveshare.md`](docs/waveshare.md) (board
variant) · [`docs/EMULATION-v2.5.md`](docs/EMULATION-v2.5.md) (harness
methodology). Wiki: [Home](../../wiki/Home) · [Hardware](../../wiki/Hardware)
· [Relays](../../wiki/Relays) · [Dashboard](../../wiki/Dashboard) ·
[Protocol](../../wiki/Protocol) · [Flashing](../../wiki/Flashing) ·
[Emulators](../../wiki/Emulators) · [Versions](../../wiki/Versions).
