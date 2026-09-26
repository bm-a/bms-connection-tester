# Waveshare ESP32-S3-ETH-8DI-8RO build (`s3-waveshare`)

Board-variant firmware for the **Waveshare ESP32-S3-ETH-8DI-8RO**
(ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB OPI PSRAM). Same BMS-impersonation
protocol, same relay-test logic, same web dashboard as the generic build —
only the hardware abstraction and pin mapping change.

## Pin map (verified against the official Waveshare wiki + vendor demo code)

| Function | Waveshare | Notes |
|---|---|---|
| Relays R1–R8 | TCA9554PWR **EXIO1–EXIO8** @ I²C `0x20` (SDA **GPIO42**, SCL **GPIO41**) | Output-register bit *i* = relay *i+1*; **HIGH bit = ON** (fixed in hardware) |
| RS485 | TX **GPIO17**, RX **GPIO18** | Onboard isolated SP3485 with **hardware auto direction** — no DE/RE pin exists |
| START/STOP button | **GPIO0** (BOOT) | Press = LOW; 10 s hold = factory reset (unchanged behavior) |
| Spoof trigger | **GPIO4** (DI1 terminal) | Active = LOW; web-changeable to DI1–DI8 (GPIO4–11) |
| Wi-Fi kill | **GPIO5** (DI2 terminal) | Active = LOW = AP off |
| Status lamp | **GPIO38** (onboard WS2812) | Green = link, red = silent (no discrete LEDs on this board) |
| Free inputs | DI3–DI8 (**GPIO6–GPIO11**) | Opto-isolated, active LOW |
| Reserved | GPIO12–16 (W5500 Ethernet), GPIO40 (RTC int), GPIO46 (buzzer) | Untouched by firmware |

DI1–DI8 are opto-isolated: wire the trigger/kill as a dry contact or driven
signal between the DIx terminal and COM per the Waveshare DI wiring (active
pulls the GPIO LOW). The existing `button_invert` / `spoof_invert` toggles
still flip the sense if your wiring differs.

Two deliberate differences from the generic build:

- **Relay polarity is fixed.** The TCA9554 output stage drives HIGH-bit = ON
  in hardware, so the web **Logic (active-low) dropdown is disabled** on this
  board (greyed out, tooltip explains it's fixed Active-HIGH). Dashboard relay
  labels always match the coils.
- **Ethernet is reserved, not integrated.** The W5500 pins are left alone;
  the box keeps the always-on Wi-Fi AP (`BMS-Tester`) like the generic build.

## Build

```sh
pio run -e s3-waveshare          # .pio/build/s3-waveshare/firmware.bin
```

## First flash (new board)

The release ships a **merged one-file image** `bms-tester-waveshare.bin`
(bootloader + partitions + app). Flash at offset `0x0`:

```sh
esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0x0 bms-tester-waveshare.bin
```

(If the upload doesn't start: hold **BOOT**, press/release **RESET**, then flash.)

## OTA / updates

This board runs its **own release line**: prerelease tags `v2.7-wsN`, asset
`waveshare-firmware.bin`, firmware version `2.7-wsN`.

- Prereleases never become GitHub `/releases/latest`, so generic boards'
  auto-OTA is unaffected — and the Waveshare box scans `/releases` for its
  own `-wsN` tags instead of `/releases/latest`.
- Version compare treats the two lines as incomparable (`2.7` == `2.7-ws1`):
  a generic release can never auto-install onto this board and vice versa.
- Manual upload (`/update`) and custom-URL OTA still enforce the
  exact-variant filename gate: only `waveshare-firmware.bin` is accepted.

## Tests

`sh run_tests.sh` includes `test_waveshare` (12 tests): pin map, TCA9554
output-byte mapping + relay-count clipping, DI-only spoof allowlist, `-wsN`
version/compare/URL/tag rules, and the variant asset + filename gates. The
generic suite is unchanged (same 152 tests, no flag). **164 tests, 0 failures.**

## Emulation verification (QEMU, ESP32-S3)

The compiled firmware was exercised in QEMU (Espressif fork) against a
virtual JBD meter. Results:

| Feature | Result | Notes |
|---|---|---|
| RS485 0x03/0x04/0x05 | PASS | Valid checksums; silence on writes/unknown/garbage |
| Link/GREEN | PASS | GREEN with sustained traffic |
| 8 relays (TCA logic) | PASS | Sequencer bytes 0x01→0xFF observed; busfail-park-safe on I²C error |
| START/STOP (BOOT) | PASS | Press starts sequencer; 10 s hold factory-resets |
| SPOOF (DI1) | PASS | Stage 1 (100.0 V) + stage 2 (88.8 V) frames live via DI1 trigger |
| Wi-Fi kill (DI2) | PASS | DI2 edge → AP-off handler fires |
| Physical TCA9554/DI/UART2 | UNTESTED | QEMU has no I²C controller, GPIO stub, or UART2 chardev |
| Ethernet | N/A | Not implemented |

Two firmware fixes came out of emulation and are in this release:
1. **TCA init order**: output register `0x00` is now written *before*
   configuring pins as outputs (prevents relay glitch at boot).
2. **OTA scan**: the Waveshare build now scans all `-wsN` releases and picks
   the *newest by version* (not just the first valid tag).
