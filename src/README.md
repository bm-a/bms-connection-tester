# src/ — tester firmware

- `bms_protocol.h` / `bms_protocol.cpp` — hardware-independent core (FROZEN
  since v1.x): JBD checksum, streaming parser, reply dispatcher (option A),
  adaptive tracker, canned frames, `FW_VERSION` (`"2.0"`).
- `relay_ctrl.h` / `relay_ctrl.cpp` — v2.0 hardware-independent add-on:
  8-relay sequencer (sequential/all-ON, 3 button behaviors, hold timer),
  debounced inputs, spoof window + spoof-frame builder, NVS-backed config
  struct. Host-tested (`test_relay`, `test_spoof`).
- `web_ui.h` / `web_ui.cpp` — v2.0 ESP-only: always-on AP, login session,
  dashboard (relays/sequence/spoof/admin), NVS load/save. Never host-built.
- `main.cpp` — Arduino sketch: frozen RS485 RX → parse → reply path, 250 ms
  LED eval, `STATUS?` (`GREEN 2.0` / `RED 2.0`), plus v2.0 relay/web/spoof
  handling (all non-blocking, RS485 keeps priority).

Pins: TX=17, RX=16, DE=4, green LED=10, red LED=11, onboard RGB=48 (WS2812,
`neopixelWrite`, no extra library), relays R1–R8 = 5/6/7/8/9/12/13/14,
button=15, spoof=21. UART2 @ 9600 8N1.
Compiles with `pio run -e esp32-s3-devkitc-1` (8 MB, Wokwi) or
`pio run -e s3-n16r8` (16 MB flash + OPI PSRAM).
