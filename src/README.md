# src/ — tester firmware

- `bms_protocol.h` / `bms_protocol.cpp` — hardware-independent core: JBD checksum,
  streaming request parser (`JbdParser`), reply dispatcher (`reply_for`, option A:
  silence on writes/unknown registers), adaptive poll tracker (`PollTracker`),
  canned frames (`BMS_RESPONSE` 0x03 golden, `BMS_RESPONSE_CELLS` 0x04,
  `BMS_RESPONSE_NAME` 0x05), `FW_VERSION`.
- `main.cpp` — Arduino sketch for ESP32-S3: RS485 RX → parse → reply,
  DE/RE direction control, 250 ms LED evaluation, `STATUS?` debug command
  (`GREEN 1.1` / `RED 1.1`).

Pins: TX=17, RX=16, DE=4, green LED=10, red LED=11. UART2 @ 9600 8N1.
Compiles with `pio run -e esp32-s3-devkitc-1`.
