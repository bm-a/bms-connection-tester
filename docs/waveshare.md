# Waveshare ESP32-S3-POE-ETH-8DI-8DO build (`s3-waveshare`)

Board-variant firmware for the **Waveshare ESP32-S3-POE-ETH-8DI-8DO**
(ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB OPI PSRAM; the `s3-waveshare` env
and the `BOARD_WAVESHARE_8DI8RO` flag keep their historic 8DI8RO names — the
EXIO/GPIO interface is identical on the 8DO). Same BMS-impersonation
protocol, same output-test logic, same web dashboard as the generic build —
only the hardware abstraction and pin mapping change.

**Wiring:** see [waveshare-wiring.md](waveshare-wiring.md) for the complete
connection guide — every screw terminal, RS485 A/B + 120R jumper, DO
(NO/COM/NC) wiring, DI wiring, what each LED means, and step-by-step bench
bring-up.

## Pin map (verified against the official Waveshare 8DO wiki + 8DO demo code)

| Function | Waveshare | Notes |
|---|---|---|
| Outputs DO1–DO8 | TCA9554PWR **EXIO1–EXIO8** @ I²C `0x20` (SDA **GPIO42**, SCL **GPIO41**) | Output-register bit *i* = channel *i+1*; **HIGH bit = ON** (Darlington sink outputs, 500 mA); parked **OFF** at boot (deliberate safe-boot divergence from the demo's all-ON) |
| RS485 | TX **GPIO17**, RX **GPIO18** | Onboard isolated SP3485; direction on **GPIO21** via ESP32 UART **RS485 half-duplex mode** (`setPins(-1,-1,-1,21)` + `setMode()`, exactly like the official 8DO demo `WS_RS485.cpp`); manual DE drive is only a `setMode()` fallback |
| START/STOP button | **GPIO0** (BOOT) | Press = LOW; 10 s hold = factory reset (unchanged behavior) |
| Spoof trigger | **GPIO4** (DI1 terminal) | Active = LOW; web-changeable to DI1–DI8 (GPIO4–11) |
| Wi-Fi kill | **GPIO5** (DI2 terminal) | Active = LOW = AP off |
| Status lamp | **GPIO38** (onboard WS2812) | Green = link, red = silent (no discrete LEDs on this board); RGB byte order (R/G swapped on the wire, per the demo). Flashes red while the TCA9554 output expander is unreachable |
| Free inputs | DI3–DI8 (**GPIO6–GPIO11**) | Opto-isolated, active LOW |
| Ethernet (W5500) | **GPIO12–16** (INT12/MOSI13/MISO14/SCLK15/CS16, RST **GPIO39**) | DHCP client, hostname `bms-tester`; dashboard answers on the Ethernet IP too (`eth_ip` in /api/state). Best-effort: failure never blocks boot or Wi-Fi |
| Buzzer | **GPIO46** (LEDC 1 kHz / 8-bit / duty ≤ 200) | Link up/down beep, sequencer start/stop beeps, output-driver failure alarm |
| Reserved | GPIO40 (RTC int) | Untouched by firmware |

DI1–DI8 are opto-isolated: wire the trigger/kill as a dry contact or driven
signal between the DIx terminal and COM per the Waveshare DI wiring (active
pulls the GPIO LOW). The existing `button_invert` / `spoof_invert` toggles
still flip the sense if your wiring differs.

Two deliberate differences from the generic build:

- **Output polarity is fixed.** The TCA9554 output stage drives HIGH-bit =
  ON in hardware, so the web **Logic (active-low) dropdown is disabled** on
  this board (greyed out, tooltip explains it's fixed Active-HIGH).
  Dashboard output labels always match the hardware.
- **Ethernet is integrated (best-effort).** The W5500 (SPI SCK15/MISO14/MOSI13,
  CS16/IRQ12/RST39) gets a DHCP address via ESP-IDF `esp_eth` and the
  dashboard answers on it (`eth_ip` in /api/state, `ETH ip` in the
  Information card). The always-on `BMS-Tester` Wi-Fi AP stays exactly as
  before; an Ethernet failure never blocks boot or touches Wi-Fi.

One deliberate difference from the official Waveshare 8DO demo:

- **Boot parks all outputs OFF (`0x00`).** The demo's `Dout_Init()` writes
  `0xFF` (all channels ON at boot); this firmware writes `0x00` instead —
  the safe boot state for an automated test bench. The register *order*
  (OUTPUT register before CONFIG register) matches the demo exactly.

## Wi-Fi

The board brings up a Wi-Fi access point on boot (same as the generic
build):

| Setting | Default | Notes |
|---|---|---|
| AP SSID | `BMS-Tester` | (`WEB_AP_SSID_DEFAULT` in `src/web_ui.h`) |
| AP password | `bms12345` | (`WEB_AP_PASS_DEFAULT`; min 8 chars) |
| Admin password | `admin123` | Gates reboot, factory reset, update upload, and saves in the web UI |

All three are changeable from the web UI **admin tab** (leave a password
field blank to keep the current value; settings persist in NVS across
reboots). The defaults above apply to a fresh flash or after a factory
reset (hold BOOT 10 s).

- **Wi-Fi kill switch:** DI2 terminal (**GPIO5**), active LOW = AP off.
  Ground DI2 to drop the AP, release to bring it back.
- **STA client mode** is also available from the web UI admin tab: the box
  can join your own network instead of (or as well as) hosting the AP.

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

`sh run_tests.sh` includes `test_waveshare` (16 tests): pin map, TCA9554
output-byte mapping + relay-count clipping, DI-only spoof allowlist, `-wsN`
version/compare/URL/tag rules, the variant asset + filename gates, and the
official-8DO-demo alignment checks (RS485/TXD1EN pins, TCA register map,
init order, output polarity). **171 native tests, 0 failures**, plus the
30-day soak sim (2,591,400 polls/replies, PASS).

## Emulation verification (QEMU, ESP32-S3)

The compiled firmware was exercised in QEMU (Espressif fork) against a
virtual JBD meter. Results:

| Feature | Result | Notes |
|---|---|---|
| RS485 0x03/0x04/0x05 | PASS | Valid checksums; silence on writes/unknown/garbage |
| Link/GREEN | PASS | GREEN with sustained traffic |
| 8 outputs (TCA logic) | PASS | Sequencer bytes 0x01→0xFF observed; busfail-park-safe on I²C error |
| START/STOP (BOOT) | PASS | Press starts sequencer; 10 s hold factory-resets |
| SPOOF (DI1) | PASS | Stage 1 (100.0 V) + stage 2 (88.8 V) frames live via DI1 trigger |
| Wi-Fi kill (DI2) | PASS | DI2 edge → AP-off handler fires |
| Physical TCA9554/DI/UART2 | UNTESTED | QEMU has no I²C controller, GPIO stub, or UART2 chardev |
| Ethernet | N/A | Not implemented |

Two firmware fixes came out of emulation and are in this release:
1. **TCA init order**: output register `0x00` is now written *before*
   configuring pins as outputs (prevents output glitch at boot) — the same
   OUTPUT-before-CONFIG order the official 8DO demo uses.
2. **OTA scan**: the Waveshare build now scans all `-wsN` releases and picks
   the *newest by version* (not just the first valid tag).
