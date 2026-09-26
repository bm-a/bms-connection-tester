# bms-connection-tester wiki — Home

ESP32-S3 firmware + hardware that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485, so compatible meters, displays, or hosts can be exercised
**without the real battery pack**.

What it actually tests: **the meter/display is the device under test** — the
box pretends to be a healthy BMS, answers the meter's JBD requests with
canned/configurable values, and drives the meter's own functions through an
8-relay sequencer while the operator watches the meter's screen. It does not
interrogate or test a physical BMS.

![FULL dashboard with live bench](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-full.png)

*v2.8 FULL dashboard: LINK pill, live bench strip, glowing relay tiles —
[open the interactive 3D bench](https://github.com/bm-a/bms-tester-sim) to
click it yourself.*

![Sequential run + spoof demo](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/demo.gif)

*R1→R8 sequential run, then a spoof FIRE driving the meter readout off golden.*

- **Green = valid BMS traffic seen, red = bus silent.** Boots red, green ≤ 1 s
  after the first valid frame, red again after silence. No buttons or screens.
- Answers `0x03` (basic info: 52.0 V, 0 A, 100 %, 14S), `0x04` (14-cell
  voltages), `0x05` (device name `TEST-14S100A`); silent on writes/unknown
  while still counting them as live traffic — see [[Protocol]].
- Two hardware options: **DIY** (generic ESP32-S3 + MAX485 + relay module,
  build it yourself — see [[Building]]) or the **Waveshare
  ESP32-S3-ETH-8DI-8RO** all-in-one board (see [[Hardware]]).
- v2.8 is a **unified release**: one tag, variant firmware assets (generic
  FULL + Waveshare). The old Waveshare `-wsN` prerelease line is retired;
  both boards OTA-check `/releases/latest` and pull their own asset.
- `STATUS?` over USB serial replies `GREEN 2.8` / `RED 2.8` (HIL/automation hook).
- v2.x: 8-relay sequencer + always-on AP dashboard + fault spoof — see [[Relays]].
- v2.2: captive portal (dashboard pops on join) + fixed 192.168.4.1.
- v2.3: relay count + chase, 2-stage spoof, per-mode ms holds, industrial pack
  (loop/labels/counters/autostart), manual + auto OTA.
- v2.3.1: no login wall, sticky saves, chase auto-sweeps, spoof GPIO,
  WiFi kill switch, working OTA check + Install.
- v2.4: Tasmota-grade update path, per-mode relay menu (chase BBM + stop
  dead-band), spoof save-only, console, config backup/restore, custom OTA
  URL, STA uplink test, info card, mDNS — see [[Dashboard]].
- v2.5: trigger save (pin + enable + polarity, no firing), captive-portal
  landing (phones get an Open-Dashboard page for Safari/Chrome), structured
  config schema (v1+v2 backups), on-demand STA join, whitespace-tolerant
  JSON, socket emulation harness + report — see [[Dashboard]] and
  `docs/EMULATION-v2.5.md`.
- v2.6: software-only daily meter estimate (link gaps = new meter, no
  button/GPIO), round link dots on the dashboard, one-file merged flash
  images per board — see [[Dashboard]] and [[Flashing]].
- v2.7: three dashboard variants (FULL default + CLASSIC + LITE), live SVG
  bench card + meter readout, persistent relay names — see [[Dashboard]].
- v2.8: 5 s settle fumble guard on the meter counter (verdict snapshot,
  mid-cycle yank = fail), unified Waveshare release line, FULL + Waveshare
  builds, 166/166 tests — see [[Versions]].

Start here: [[Building]] to build the hardware, [[Flashing]] to load it,
[[Hardware]] for the pin maps, [[Protocol]] for the byte format, [[Relays]]
for the test bench, [[Dashboard]] for every page field, [[Emulators]] to
test without hardware, [[Versions]] for per-release changes.

Companion repos: [jbd-bms-rs485-handbook](https://github.com/bm-a/jbd-bms-rs485-handbook)
(RS485 + JBD protocol handbook) · [bms-tester-sim](https://github.com/bm-a/bms-tester-sim)
(offline 3D bench simulator).
