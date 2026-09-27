# v2.8.1 Waveshare Emulation Tests

## QEMU Status (2026-09-27)

**QEMU's esp32s3 machine CANNOT execute Arduino-ESP32 firmware.** Proven:
- Espressif QEMU 9.2.2 (esp_develop_9.2.2_20260417) installed and verified
- Both the real v2.8.1 firmware AND a minimal Arduino "hello world" crash
  during Arduino core init (before `setup()`), rebooting via RTC_SW_SYS_RST
- This is a QEMU peripheral-emulation limitation, NOT a firmware bug
- The Arduino core touches hardware registers QEMU doesn't implement

## Host-Side Logic Emulation

Since QEMU cannot run the firmware, the **actual v2.8.1 source files** are
compiled on host (x86_64) with Arduino stubs:

- `src/bms_protocol.cpp` — JBD parser, checksums, PollTracker (UNMODIFIED)
- `src/relay_ctrl.cpp` — Relay sequencer, spoof frames (UNMODIFIED)
- `src/waveshare_pins.h` — GPIO assignments (UNMODIFIED)
- `apply_leds()` — EXACT v2.8.1 code from src/main.cpp:152-173 (with byte-order fix)

Stubs: `neopixelWrite()` logs args, `tca_write_reg()` logs I2C writes,
GPIO reads return simulated values.

## Results: 32/32 PASS

See `v281_emulation_results.txt` for full output.

| Test | Result |
|------|--------|
| Boot LED = RED, wire [32,0,0] | PASS |
| Link LED = GREEN, wire [0,32,0] | PASS |
| JBD 0x03 request parses, reply checksum valid | PASS |
| JBD 0x04 (35B) and 0x05 (19B) replies | PASS |
| Silence on writes (0x5A) | PASS |
| Silence on unknown registers | PASS |
| Garbage (bad checksum) rejected | PASS |
| 10,000 random bytes → 0 false link triggers | PASS |
| TCA9554 init (OUTPUT=0x00, CONFIG=0x00) | PASS |
| TCA9554 relay sequence 0x01-0xFF (257 writes) | PASS |
| DI1=GPIO4, DI2=GPIO5, BOOT=GPIO0 | PASS |
| RGB=GPIO38, RS485 TX=GPIO17/RX=GPIO18 | PASS |
| I2C SDA=GPIO42, SCL=GPIO41 | PASS |
| Spoof stage 1 = 100.0V, stage 2 = 88.8V | PASS |
| Spoof checksums valid | PASS |

## Run

```sh
cd test/qemu/host
g++ -std=c++17 -I. test_v281.cpp bms_protocol.cpp relay_ctrl.cpp -o test_v281
./test_v281
```

## Limitations

- RMT/WS2812 timing: not tested (no hardware)
- WiFi RF: not tested (no hardware)  
- I2C bus electrical: not tested (QEMU has no I2C) — TCA LOGIC verified
- GPIO electrical: assignments verified, behavior not tested on silicon
