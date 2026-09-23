# Hardware (v2.7 pin map — unchanged since v2.0)

The thing under test — a real e-rickshaw meter cluster (dual-dial):

![E-rickshaw meter cluster](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/Actual%20Meter%20Image/IMG_0341.jpeg)

*Speed dial + battery %/V dial + tell-tales. The tester drives exactly this
over RS485 (readouts) + relay-switched lamp lines.*

## Power + signal tree (standalone 48 V box)

![Power and signal tree](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/wiring.png)

*48 V shared bus → 12 V coils + 5 V logic → ESP → MAX485 → A/B → meter;
relays land on J1/J2/AUX. Full enclosure docs: `enclosure/README.md`.*

## MCU
ESP32-S3 DevKitC-1 (8 MB) or N16R8 (16 MB flash + 8 MB octal PSRAM),
dual-core LX7 @ 240 MHz, native USB CDC+JTAG. Arduino core 2.0.x.
Firmware envs: `esp32-s3-devkitc-1` vs `s3-n16r8`
(`board_build.flash_size = 16MB`, `board_build.psram_type = opi`,
`board_build.partitions = default_16MB.csv`).

## Wiring
| Signal | Connection |
|---|---|
| S3 GPIO17 (TX) | → MAX485 DI |
| S3 GPIO16 (RX) | ← MAX485 RO |
| S3 GPIO4 | → MAX485 DE+RE tied (+ 10 kΩ pull-down to GND) |
| S3 GPIO10 | → 220 Ω → green LED → GND |
| S3 GPIO11 | → 220 Ω → red LED → GND |
| S3 GPIO48 | onboard WS2812 RGB (no wiring; mirrors green/red via `neopixelWrite`, brightness 32) |
| S3 GPIO5/6/7/8/9/12/13/14 | → relay module IN1–IN8 (v2.0; 12 V coils, own supply, common GND) |
| S3 GPIO15 | → button to GND (v2.0; internal pull-up) |
| S3 GPIO18 | → WiFi-kill to GND (v2.3.1; ground = AP off; internal pull-up) |
| S3 GPIO21 | → spoof trigger to GND (v2.0; internal pull-up) |
| MAX485 VCC / GND | 3.3 V (NOT 5 V) / common GND with meter |
| MAX485 A/B | → meter A/B, twisted pair; 120 Ω across A–B on long runs |

Avoided pins: strapping 0/3/45/46, USB-JTAG 19/20, flash/PSRAM 26–37,
console 43/44. UART2 @ 9600 8N1 via GPIO matrix.

## LEDs
`apply_leds()` drives discretes as strict opposites and mirrors the same
state on the RGB (green = talking, red = silent). Boot red. RGB needs no
library and runs from the 250 ms eval, never the hot RX loop, so 9600-baud
timing is unaffected.

## Power
USB 5 V for the ESP + MAX485 — never the traction pack. v2.0: Wi-Fi AP is
always on (radio active, ~1 W-class — use a real charger) and relay coils
run on their own 12 V supply (common GND with the ESP).
Full module background: `docs/MODULES.md`. Relay/web details: [[Relays]].
