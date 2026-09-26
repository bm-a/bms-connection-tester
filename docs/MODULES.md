# Hardware modules

The device under test — e-rickshaw meter cluster:

![E-rickshaw meter cluster](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/Actual%20Meter%20Image/IMG_0341.jpeg)

![Power and signal tree](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/wiring.png)

## 1. ESP32-S3 DevKitC-1 / N16R8 (application MCU)

- Espressif ESP32-S3: dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM,
  native USB (CDC + JTAG).
  - DevKitC-1 N8: 8 MB quad-SPI flash.
  - **N16R8 (this build): 16 MB flash + 8 MB octal PSRAM** — use PIO env
    `s3-n16r8` (`flash 16MB, psram opi, default_16MB.csv`).
- Arduino core 2.0.x provides `Serial` (USB/UART0 console) and `Serial2`,
  which the GPIO matrix can route to any pins — here RX = GPIO16, TX = GPIO17.
- GPIOs used: **4** (RS485 direction), **10/11** (LEDs), **16/17** (UART2),
  **48** (onboard WS2812 RGB, output-only), **5/6/7/8/9/12/13/14** (relays R1–R8),
  **15** (button), **18** (WiFi kill), **21** (spoof trigger).
  All avoid strapping (0/3/45/46), USB-JTAG (19/20), flash/PSRAM (26–37)
  and the UART0 console (43/44). (Classic-ESP32 GPIO25 does not exist on S3.)
  v2.6 adds NO new GPIO: the meter counter is pure software (link gaps).

## 2. MAX485 (RS485 transceiver)

- Converts the ESP32's 0–3.3 V UART into differential A/B signalling that
  survives factory-floor noise over twisted pair.
- `DE` + `RE` tied together to GPIO4: HIGH transmits, LOW receives
  (half-duplex — one talker at a time, like a walkie-talkie).
- Firmware sequence per reply: DE HIGH → `write()` → `flush(true)` →
  1.5 ms guard → DE LOW → drain stale RX.
- **10 kΩ pull-down** on the DE net: the pin floats at reset, so without it
  the MAX485 can power up transmitting and jam the bus.
- Power at **3.3 V** (S3 logic levels), **common ground** across MCU, MAX485
  and meter. A/B to the meter over short twisted pair; 120 Ω termination
  across A–B if the run is long.

## 3. Status LEDs

- Green (GPIO10) / red (GPIO11), each via 220 Ω to GND (~8 mA — bright,
  well under pin and LED limits). Driven as strict opposites in one function,
  so both-on / both-off (other than unpowered) is impossible by construction.
- Onboard WS2812 RGB (GPIO48, `neopixelWrite`, brightness 32/255) mirrors
  the discretes: green = talking, red = silent. No extra library needed.
  Boot red on both; green ≤ 1 s after first valid frame; red ≤ window after silence.
- The dashboard header mirrors the same state as two round dots
  (`#dotG`/`#dotR`, green/red) beside the LINK pill — same signal, no new logic.

## 4. Relay outputs (v2.0)

- R1–R8 → GPIO **5, 6, 7, 8, 9, 12, 13, 14** → relay module IN1–IN8.
- Module: SmartElex 12 V 8-channel (3 A/channel), optoisolated inputs,
  ESP 3.3 V compatible. **Coils need a separate 12 V supply** (USB cannot
  drive them); tie the 12 V supply GND to the ESP GND. Default active-LOW
  (LOW = ON); flip in the web UI if the module jumpers say HIGH.
- Firmware drives the OFF level *before* `pinMode`, so no relay clicks at boot.
- Loads switch up to 3 A/channel (module rating); use NO/COM/NC per channel
  to the meter functions under test.

## 5. Button + spoof inputs (v2.0)

- Button → GPIO15 to GND (internal pull-up; press = LOW, web-invertible),
  30 ms debounce. Short press runs the configured relay behavior; **hold
  10 s = factory reset** (wipes Wi-Fi/admin config, reboots).
- Spoof trigger → GPIO21 to GND (internal pull-up, web-invertible) or the
  web FIRE button: meter sees 88.8 V / 88.8 A / 88.8 °C / 188 % on `0x03`
  for the configured window (default 10 s), then auto-reverts to golden.
- Daily meter counting (v2.6) needs NO input: it watches link gaps in
  software (RED ≥ 3 s then GREEN = reseat = new meter). See
  `docs/CONFIG-SCHEMA.md` meters.* for the rules.

## 6. Power

- USB 5 V (charger or power bank) for the ESP + MAX485. The whole point of the
  device is that the traction/battery pack is never needed.
- v2.0 keeps Wi-Fi AP always on: idle draw rises (radio active, roughly
  1 W-class depending on clients) — still USB-powered, but use a real
  charger, not a weak laptop port. Relay coils are on their own 12 V supply.
