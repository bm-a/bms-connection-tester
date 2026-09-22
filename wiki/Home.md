# bms-connection-tester wiki — Home

ESP32-S3 firmware + hardware that impersonates a **JBD / Xiaoxiang Smart BMS**
over RS485, so compatible meters, displays, or hosts can be exercised
**without the real battery pack**.

- **Green = valid BMS traffic seen, red = bus silent.** Boots red, green ≤ 1 s
  after the first valid frame, red again after silence. No buttons or screens.
- Answers `0x03` (basic info, byte-exact real capture: 52.0 V, 100 %),
  `0x04` (14-cell voltages), `0x05` (device name); silent on writes/unknown
  (option A) while still counting them as live traffic.
- Targets: ESP32-S3 DevKitC-1 (8 MB) and N16R8 (16 MB + OPI PSRAM), MAX485,
  external LEDs (GPIO10/11) plus onboard WS2812 RGB (GPIO48) mirroring both.
- `STATUS?` over USB serial replies `GREEN 2.4` / `RED 2.4` (automation hook).
- v2.x: 8-relay sequencer + always-on AP dashboard + fault spoof — see [[Relays]].
- v2.2: captive portal (dashboard pops on join) + fixed 192.168.4.1.
- v2.3: relay count + chase, 2-stage spoof, per-mode ms holds, industrial pack
  (loop/labels/counters/autostart), manual + auto OTA.
- v2.3.1: no login wall, sticky saves, chase auto-sweeps, spoof GPIO,
  WiFi kill switch, working OTA check + Install.
- v2.4: Tasmota-grade update path, per-mode relay menu (chase BBM + stop
  dead-band), spoof save-only, console, config backup/restore, custom OTA
  URL, STA uplink test, info card, mDNS — see [[Dashboard]].
- v2.6: software-only daily meter estimate (link gaps ≥ 3 s = new meter, no
  button/GPIO), round link dots on the dashboard, one-file merged flash
  images per board — see [[Dashboard]] and [[Flashing]].
- v2.5: trigger save (pin + enable + polarity, no firing), captive-portal
  landing (phones get an Open-Dashboard page for Safari/Chrome), structured
  config schema (v1+v2 backups), on-demand STA join, whitespace-tolerant
  JSON, socket emulation harness + 64/64 report — see [[Dashboard]] and
  `docs/EMULATION-v2.5.md`.

Start here: [[Flashing]] to load it, [[Hardware]] to wire it,
[[Protocol]] for the byte format, [[Relays]] for the test bench,
[[Dashboard]] for every page field, [[Emulators]] to test without hardware,
[[Versions]] for per-release changes.
Companion electronics handbook: https://github.com/bm-a/jbd-bms-rs485-handbook
