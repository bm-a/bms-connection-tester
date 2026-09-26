# Hardware (v2.8 pin maps)

The thing under test — a real e-rickshaw meter cluster (dual-dial):

![E-rickshaw meter cluster](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/Actual%20Meter%20Image/IMG_0341.jpeg)

*Speed dial + battery %/V dial + tell-tales. The tester drives exactly this
over RS485 (readouts) + relay-switched lamp lines.*

Two supported hardware options. Pick one — the firmware auto-selects the
board at compile time (`s3-waveshare` env sets `-DBOARD_WAVESHARE_8DI8RO=1`).

## Option A — DIY (generic ESP32-S3)

![Power and signal tree](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/wiring.png)

*48 V shared bus → 12 V coils + 5 V logic → ESP → MAX485 → A/B → meter;
relays land on J1/J2/AUX. Full enclosure docs: `enclosure/README.md`.*

### MCU
ESP32-S3 DevKitC-1 (8 MB) or N16R8 (16 MB flash + 8 MB octal PSRAM),
dual-core LX7 @ 240 MHz, native USB CDC+JTAG. Arduino core 2.0.x.
Firmware envs: `esp32-s3-devkitc-1` (8 MB) vs `s3-n16r8`
(`board_build.flash_size = 16MB`, `board_build.psram_type = opi`,
`board_build.partitions = default_16MB.csv`).

### Pin map (unchanged since v2.0)
| Signal | Connection |
|---|---|
| S3 GPIO17 (TX) | → MAX485 DI |
| S3 GPIO16 (RX) | ← MAX485 RO |
| S3 GPIO4 | → MAX485 DE+RE tied (+ 10 kΩ pull-down to GND) |
| S3 GPIO10 | → 220 Ω → green LED → GND |
| S3 GPIO11 | → 220 Ω → red LED → GND |
| S3 GPIO48 | onboard WS2812 RGB (no wiring; mirrors green/red via `neopixelWrite`, brightness 32) |
| S3 GPIO5/6/7/8/9/12/13/14 | → relay module IN1–IN8 (12 V coils, own supply, common GND) |
| S3 GPIO15 | → button to GND (internal pull-up) — START/STOP + 10 s factory reset |
| S3 GPIO18 | → WiFi-kill to GND (internal pull-up) — ground = AP off |
| S3 GPIO21 | → spoof trigger to GND (internal pull-up; default, web-changeable) |
| MAX485 VCC / GND | 3.3 V (NOT 5 V) / common GND with meter |
| MAX485 A/B | → meter A/B, twisted pair; 120 Ω across A–B on long runs |

Avoided pins: strapping 0/3/45/46, USB-JTAG 19/20, flash/PSRAM 26–37,
console 43/44. UART2 @ 9600 8N1 via GPIO matrix.

### Spoof-trigger safe pins (DIY)
Only these GPIOs are accepted for the trigger pin; anything else falls back
to 21: **1, 2, 21, 38, 39, 40, 41, 42, 43, 44, 47**.

### LEDs
`apply_leds()` drives discretes as strict opposites and mirrors the same
state on the RGB (green = talking, red = silent). Boot red. RGB needs no
library and runs from the 250 ms eval, never the hot RX loop, so 9600-baud
timing is unaffected.

## Option B — Waveshare ESP32-S3-ETH-8DI-8RO

All-in-one board (ESP32-S3-WROOM-1-N16R8: 16 MB flash + 8 MB OPI PSRAM):
8 relay outputs via expander, isolated RS485, 8 digital inputs, onboard
RGB — no external relay module, MAX485, or LEDs needed.

### Pin map
| Signal | Connection |
|---|---|
| Relays R1–R8 | TCA9554PWR @ I2C `0x20` (SDA GPIO42 / SCL GPIO41); EXIO1–8 = output-register bits 0–7; **HIGH bit = ON**. The dashboard *active-low* toggle is a no-op on this board — labels always match hardware. Relays park OFF at boot (`TCA9554PWR_Init(0x00)`). |
| RS485 | TX GPIO17 / RX GPIO18, **hardware automatic direction** (no DE/RE pin) |
| BOOT button | GPIO0 — START/STOP (holding it at power-on enters download mode; normal) |
| DI1 (GPIO4) | spoof trigger (default, web-changeable to any DI1–DI8) |
| DI2 (GPIO5) | WiFi kill (default) — ground = AP off |
| DI1–DI8 | GPIO4–GPIO11, opto-isolated, active = LOW (INPUT_PULLUP) |
| RGB | GPIO38 onboard WS2812 (no discrete green/red LEDs on this board) |

### Reserved / unused (do not touch)
- GPIO12–16: W5500 Ethernet (INT/MOSI/MISO/SCLK/CS) — **reserved, Ethernet
  is NOT implemented** in v2.8 (Wi-Fi AP stays the management path).
- GPIO40: RTC interrupt; GPIO41/42 shared with RTC @ 0x51.
- GPIO46: buzzer (unused by this firmware).

### Spoof-trigger safe pins (Waveshare)
Only the DI screw terminals are user-drivable: **DI1–DI8 (GPIO4–11,
active LOW)**. Anything else falls back to DI1 (GPIO4).

## Power (both boards)
- **ESP + logic: USB 5 V** (use a real charger — the Wi-Fi AP radio is
  always on, ~1 W-class). Never power the ESP from the traction pack.
- **Relay coils: separate 12 V supply** (DIY: ≈ 30 mA/coil, ≈ 240 mA
  all-on). Tie coil-supply GND to ESP GND (common reference for the
  optoisolated inputs). Never power coils from USB/ESP pins.
- MAX485 VCC = 3.3 V, common GND with the meter.

## RS485 bus wiring
- A/B twisted pair from the transceiver to the meter; 120 Ω termination
  across A–B on long runs.
- The Waveshare board's RS485 is **isolated** (SP3485) with automatic
  direction control — no DE/RE wiring.

## Enclosure option
`enclosure/` documents an IP65 standalone box + GX16 meter-link sockets
for the DIY build (48 V shared bus → bucks → ESP/relays). The Waveshare
board is already DIN-rail friendly as-is.

Full module background: `docs/MODULES.md`. Build guides: [[Building]].
Relay/web details: [[Relays]], [[Dashboard]].
