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
- `STATUS?` over USB serial replies `GREEN 2.2` / `RED 2.2` (automation hook).
- v2.x: 8-relay sequencer + always-on AP dashboard + fault spoof — see [[Relays]].
- v2.1: website reliability (host-executed web tests, contract gate, 24 h sim).

Start here: [[Flashing]] to load it, [[Hardware]] to wire it,
[[Protocol]] for the byte format, [[Relays]] for the test bench,
[[Emulators]] to test without hardware, [[Versions]] for per-release changes.
Companion electronics handbook: https://github.com/bm-a/jbd-bms-rs485-handbook
