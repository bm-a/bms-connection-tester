# src/ — tester firmware

- `bms_protocol.h` / `bms_protocol.cpp` — hardware-independent core (FROZEN
  since v1.x): JBD checksum, streaming parser, reply dispatcher (option A),
  adaptive tracker, canned frames, `FW_VERSION` (`"2.3"`).
- `relay_ctrl.h` / `relay_ctrl.cpp` — hardware-independent bench add-on:
  8-relay sequencer (sequential/all-ON/chase, relay count, 3 button behaviors,
  per-mode ms holds, loop/pause/limit, stagger, direction, QC counters),
  debounced inputs, 2-stage spoof plan + frame builder, NVS-backed config
  struct. Host-tested (`test_relay`, `test_spoof`).
- `ota.h` / `ota.cpp` — hardware-independent OTA decision logic: version
  compare, per-variant asset pick, download URL, auto-check gate.
  Host-tested (`test_ota`); network I/O lives in `main.cpp` (ESP-only).
- `web_ui.h` / `web_ui.cpp` — ESP-only: always-on AP + captive portal, login
  (RAM session + persistent NVS slots), dashboard
  (relays/sequence/labels/spoof/OTA/admin), NVS v3 load/save + v2 migration,
  optional STA uplink, manual `/update` upload. Host-tested on stubs
  (`test_web`).
- `main.cpp` — Arduino sketch: frozen RS485 RX → parse → reply path, 250 ms
  LED eval, `STATUS?` (`GREEN 2.3` / `RED 2.3`), plus relay/web/spoof/OTA
  handling (all non-blocking, RS485 keeps priority).

Pins: TX=17, RX=16, DE=4, green LED=10, red LED=11, onboard RGB=48 (WS2812,
`neopixelWrite`, no extra library), relays R1–R8 = 5/6/7/8/9/12/13/14,
button=15, spoof=21. UART2 @ 9600 8N1.
Compiles with `pio run -e esp32-s3-devkitc-1` (8 MB, Wokwi) or
`pio run -e s3-n16r8` (16 MB flash + OPI PSRAM).
