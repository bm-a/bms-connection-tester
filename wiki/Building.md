# Building (v2.8)

Two ways to get a tester on the bench. The full step-by-step guides live in
the repo docs; this page is the map.

## Option A — DIY (build it yourself)

**Guide: [`docs/BUILD-DIY.md`](../blob/main/docs/BUILD-DIY.md)** — written
from scratch for v2.8. Covers:

- Shopping list (ESP32-S3 DevKitC-1 or N16R8, MAX485 module, 8-channel
  12 V relay module, LEDs + resistors, button, 12 V supply, USB charger,
  twisted pair) with search terms.
- Bench wiring: every wire from the [[Hardware]] pin map, the 10 kΩ DE
  pull-down, 220 Ω LED resistors, separate 12 V coil supply with common
  GND, RS485 A/B twisted pair + 120 Ω termination.
- Power-up order (USB first, then 12 V), smoke-check steps, first-boot LED
  sequence (red at boot, green on first meter poll).
- Flashing the generic FULL image, then the dashboard quick-start
  (join AP → Open Dashboard → run a sequence → read the meter card).

## Option B — Waveshare ESP32-S3-ETH-8DI-8RO (all-in-one)

**Guide: [`docs/BUILD-WAVESHARE.md`](../blob/main/docs/BUILD-WAVESHARE.md)**
— written from scratch for v2.8. Covers:

- What the board already gives you (8 relay outputs, isolated RS485,
  8 digital inputs, RGB) and what you still wire yourself (12 V coil
  supply, meter A/B, USB).
- Terminal map: relay outputs, DI1–DI8 screw terminals, RS485 A/B,
  BOOT button behavior (GPIO0 — don't hold it at power-on unless you want
  download mode).
- Flashing the Waveshare merged image, first-boot check (RGB red → green
  on poll), relay output test from the dashboard tiles.
- DI wiring for spoof trigger (DI1) and WiFi kill (DI2), active-LOW
  opto-isolated inputs.

## Then flash it

Both guides end at the same place: a board with firmware. If you already
have hardware and just need the binaries, skip to [[Flashing]].

After flashing, the bring-up order is: [[Hardware]] (verify wiring) →
dashboard over the AP (`192.168.4.1`) → [[Relays]] (run a sequence) →
[[Dashboard]] (every card explained).
