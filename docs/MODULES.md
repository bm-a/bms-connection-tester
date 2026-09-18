# Hardware modules

## 1. ESP32-S3 DevKitC-1 (application MCU)

- Espressif ESP32-S3R?/N8: dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM,
  8 MB quad-SPI flash, native USB (CDC + JTAG).
- Arduino core 2.0.x provides `Serial` (USB/UART0 console) and `Serial2`,
  which the GPIO matrix can route to any pins — here RX = GPIO16, TX = GPIO17.
- GPIOs used: **4** (RS485 direction), **10/11** (LEDs), **16/17** (UART2).
  All avoid strapping (0/3/45/46), USB-JTAG (19/20), flash/PSRAM (26–37)
  and the UART0 console (43/44). (Classic-ESP32 GPIO25 does not exist on S3.)

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
- Boot red; green ≤ 1 s after first valid frame; red ≤ window after silence.

## 4. Power

- USB 5 V (charger or power bank). The whole point of the device is that the
  traction/battery pack is never needed. Idle draw ≈ 0.3–0.5 W (radio off).
