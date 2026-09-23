# Standalone tester enclosure — IP65 box + 48 V dual-buck + GX16 meter link
v2.6 · software builder + dad (electronics) split. Every electrical value dad
owns is marked `DAD:` — software never guesses amps.

![Power and signal tree](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/wiring.png)

The device this box tests — e-rickshaw meter cluster:

![E-rickshaw meter cluster](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/Actual%20Meter%20Image/IMG_0341.jpeg)

## Power tree (48 V shared with meter)
```
48V IN (shared bus, fused) ─┬─ buck 48→12V (coils only) ── relay JD-VCC
                            └─ buck 48→5V (logic) ── ESP 5V + MAX485 side
                            star GND: 48V GND = ESP GND = relay GND = meter GND
```
- `DAD:` 48 V inlet fuse rating + wire gauge (default print: logic 22 AWG, relay loads 18 AWG, 48 V feed 18 AWG — confirm).
- `DAD:` buck module part numbers + current ratings (coils ~8×75 mA + margin; logic ~1 W + WiFi radio margin — confirm).
- Reverse-polarity diode + TVS on 48 V inlet. Fuse holder accessible without opening lid.

## Box + panel (off-shelf IP65 polycarbonate ~240×190×90 mm — `DAD:` confirm size)
- Zones: left logic tray (S3 + MAX485 + DE 10 kΩ pulldown + 220 Ω LED resistors),
  middle 8-ch relay on standoffs, right power corner (2 bucks + fuse + TVS), lid desiccant + wiring label.
- Front panel (etched acrylic, charcoal + orange): green/red link bezels
  (GPIO10/11), RGB window (GPIO48), illuminated green START (GPIO15→GND, momentary),
  yellow SPOOF (GPIO21→GND, momentary), **maintained metal WiFi-kill toggle with
  guard cover (GPIO18→GND)** + amber "AP OFF" jewel, USB-C service pigtail with
  dust cap, QR → `192.168.4.1` + `bmstester.local`.
- Glands: PG9 48 V, PG7 RS485 A/B twisted pair, PG13.5 relay/GX16 tails.

## GX16-5 meter link (DEFAULT — dad replaces with real 10-pin table)
| Conn | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| J1 BUS | 48V | GND | A | B | R1 line |
| J2 LOADS | R2 | R3 | R4 | R5 | R6 |
| AUX pigtail | R7 | R8 | — | — | — |
`DAD:` overwrite this table with the meter's real pinout. Sim + 3D read
`sim_server.py GX16` dict — a 5-minute re-map, no rework. Relay COM feed:
tester pulls lines LOW, meter side sources its own lamp returns (`DAD:` confirm
dry-contact vs driven; if driven, add per-channel fuse ≤3 A default).

## Assembly + power-on test (dad's bench sheet)
1. Bucks first, no ESP: 48 V in → verify 12.0 V + 5.0 V at terminals (`DAD:` tolerances).
2. Logic: USB flash, boot RED → meter plugged via J1 → GREEN ≤1 s, serial polls 03/04/05.
3. Loads: web START → R1..R8 click in order → meter tell-tales follow (see sim).
4. Kill tests: J1 unplug → RED ~2 s; GPIO18 toggle → AP drops, USB STATUS? alive.
5. Soak: 1 h run, bucks warm-not-hot (`DAD:` thermal call), glands strain-relieved.

## Sim mapping (software side)
`local-wokwi/` mirrors all of the above: 48 V rail + both bucks + fuse lamp in 3D,
GX16 sockets with pin numbers, photo-textured meter, WiFi toggle (GPIO18),
`/esp/` exact firmware page in scrollable row. `/api/gx16` serves the live table.
