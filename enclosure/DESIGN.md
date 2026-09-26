# BMS Connection Tester — Bench Station Enclosure Design

**Target:** `bm-a/bms-connection-tester` v2.7 (ESP32-S3 + MAX485 + SmartElex 8-ch relay + dual 48 V bucks)
**Concept:** turn the tester from a bench prototype into a proper **assembly-line QC station**:
one box, one GX16 plug, one START button, big PASS/FAIL on top. No laptop, no battery pack.
**Box baseline:** off-shelf IP65 polycarbonate ~240 × 190 × 90 mm (per `enclosure/README.md`; `DAD:` confirm).

> Convention: every electrical value the electronics owner must confirm is marked `DAD:` —
> software never guesses amps (repo convention, kept).

---

## 1. Use potential — what this box becomes

1. **Assembly-line QC station (primary).** Worker plugs the meter's GX16, presses START,
   watches the top display: link → 8 relay tell-tales sweep → PASS/FAIL + today's count.
   No laptop, no traction battery, ~30 s per meter.
2. **Field-service diagnostic.** Portable 48 V-powered box; technician fires SPOOF to force
   known voltage/current/SOC frames and verifies the meter reads correctly on-site.
3. **Burn-in / soak rig.** Loop + chase-wave modes cycle all 8 relays unattended; display
   shows cycle count and elapsed time.
4. **Training rig.** New line workers learn meter behavior against the two spoof stages
   (100 % → 88.8 V / 88.8 A / 88.8 % / 188 °C-clamped) without touching real packs.
5. **R&D regression.** New meter firmware is validated against frozen golden frames
   (0x03/0x04/0x05, checksums verified) before it ships.

---

## 2. Display on top

### Selection — 2.8" TFT (ILI9341, 320×240, SPI)

| Option | Pins | Verdict |
|---|---|---|
| **2.8" ILI9341 TFT, SPI (recommended)** | 5 GPIO | Big PASS/FAIL, 8-relay matrix, progress bar, readable at 1 m on a bench |
| 1.3" SH1106 OLED, I2C | 2 GPIO | Fallback: cheaper, text-only, fine if box must stay small |

A bench QC tool lives or dies on glanceability. The TFT wins.

### What it shows (screens)

- **BOOT:** logo, FW version, self-test (relay coils click once, bus silent → RED).
- **IDLE:** LINK status big (GREEN/RED), meter count today (pass/fail), AP IP + QR, relay names.
- **RUNNING:** progress bar R1→R8, current step highlighted, elapsed time, big STOP hint.
- **SPOOF:** stage 1/2 indicator with live V/A/SOC values being spoofed.
- **RESULT:** PASS / FAIL full-screen (green/red), latched until next START.
- **OTA/SETTINGS:** check result, version strings (rarely needed; web UI stays primary).

### Mounting

- Recessed window in the **lid**: 3D-printed bezel + foam gasket, display module on
  M2.5 standoffs behind a ~46 × 78 mm cutout (verify against actual module).
- Tilt: flat is fine for bench use; a 10–15° wedge riser under the box helps standing operators.
- Keep the SPI ribbon **short (<15 cm)** and routed away from relay coils (EMI).

### Pins (all currently free, non-strapping, no PSRAM conflict)

| Signal | GPIO |
|---|---|
| SCK | 38 |
| MOSI | 39 |
| CS | 40 |
| DC | 41 |
| RST | 42 |

Spare: GPIO1, 2, 47. (Used today: 4,5–9,10,11,12–14,15,16,17,18,21,48; avoided: 0,3,45,46,19,20,26–37,43,44.)

### Firmware work needed

New `src/display_ui.cpp`: SPI init, screen state machine driven off the existing
`RelaySequencer` state + link tracker. **Must be non-blocking** — update on state change
plus a 250 ms tick (same cadence as the LED eval); never `delay()` in the RS485 hot path.
Estimate: small, self-contained module + tests in the host harness style.

---

## 3. Buttons — full schedule

Firmware functions today: GPIO15 = START/STOP button (web-selectable: abort / run-lock / restart;
**10 s hold = factory reset**), GPIO21 = SPOOF trigger, GPIO18 = WiFi kill (maintained).

| # | Label | Function (GPIO) | Kind | Size | Placement | Notes |
|---|---|---|---|---|---|---|
| 1 | **START** | Seq start/stop (GPIO15) | Metal **illuminated** momentary, green ring, IP65 | 19 mm | Front panel, center — dominant | Ring lit while running. 10 s hold = factory reset → engrave "HOLD 10s = RESET" under it |
| 2 | **SPOOF** | Fire 2-stage spoof (GPIO21) | Metal momentary, yellow cap, IP65 | 16 mm | Front panel, left of START, ≥25 mm edge gap | Deliberate action; no guard needed, separation prevents mis-press with gloves |
| 3 | **WiFi KILL** | AP+portal off (GPIO18) | **Maintained** metal toggle + flip guard cover | 12 mm | Front panel, far right, isolated zone | Guard prevents accidental kill mid-test. Amber "AP OFF" jewel lamp beside it |
| 4 | **E-STOP** *(new, hardware-only)* | Cuts 48 V inlet (NC contact in series after fuse) | Red mushroom, latching, twist-release, 22 mm | 22 mm | Front panel top-right corner, unmissable | No firmware needed. `DAD:` contact current rating vs 48 V bus load |
| 5 | **RESET** | ESP32 EN (recessed tactile) | Recessed push, behind 3 mm hole | — | Side wall or front-panel corner | Paperclip access; recessed = no accidental press |
| 6 | *ENCODER (optional v2)* | Display menu nav | EC11 rotary + push | — | Below display on lid | Needs firmware menu system — defer |

**Why these kinds:** metal panel-mount buttons (not PCB tactiles) survive a line environment;
illuminated START gives state at a glance; maintained + guarded for the kill function is
standard industrial practice; E-STOP is non-negotiable once a shared 48 V bus is in a worker's hands.

**Placement rationale:** frequency ordering left→right (SPOOF → START → KILL);
destructive/rare actions (KILL, E-STOP) physically separated from the high-frequency START;
all within one hand's reach at bench height; engraved traffolyte labels, not stickers.

---

## 4. Panel layout

### Front panel (operator face, 240 mm wide)

```
┌──────────────────────────────────────────────────────────┐
│ [E-STOP]                              [KILL+guard] (AP OFF)│
│                                                          │
│ (SPOOF)    [      START      ]      (G)(R) [RGB]          │
│  yellow     green illuminated      link LEDs  window      │
│                                                          │
│ [USB-C service]            [QR → 192.168.4.1]             │
└──────────────────────────────────────────────────────────┘
```

### Lid (top)

```
┌──────────────────────────────────────────────────────────┐
│  ┌────────────────────┐                                   │
│  │                    │      [ QR plate ]                   │
│  │   2.8" TFT window  │      "BMS TESTER v2.7"              │
│  │   (bezel+gasket)   │                                   │
│  └────────────────────┘                                   │
└──────────────────────────────────────────────────────────┘
```

### Rear

- PG9 gland: 48 V inlet (fused, fuse holder accessible without opening lid)
- PG7 gland: RS485 A/B twisted pair to meter
- PG13.5 gland: relay tails / GX16 J1+J2+AUX
- Vent slots on side walls (never top — drip protection)

---

## 5. Enclosure construction & internal zones

```
┌────────────────── 240 ──────────────────┐
│ LEFT: logic tray        MID: relay       │ RIGHT: power
│ S3 devkit on M3         8-ch module on   │ corner: 2 bucks,
│ standoffs + MAX485 +    standoffs        │ fuse, TVS, reverse-
│ 10k pulldown + LED Rs                    │ polarity diode
│ ── low-voltage zone ── │ ── switched ── │ ── 48 V zone ──
└──────────────────────────────────────────┘
```

- **Stack-up:** base tray (3D-printed or polycarb sub-plate) holds all three zones;
  48 V zone gets a physical barrier wall (3 mm) from logic — separation by construction.
- **Lid:** display bezel + desiccant sachet + laminated wiring label inside.
- **Standoffs:** M3 brass heat-set inserts; S3 devkit ~52 × 28 mm, relay module ~138 × 56 mm
  (verify SmartElex dims), bucks ~43 × 21 mm class (verify).
- **Thermal:** bucks warm-not-hot (`DAD:` thermal call); side vents sized after soak test.

---

## 6. Internal wiring plan + safety maths

- **Power tree:** 48 V IN (fused) → buck 48→12 V (relay JD-VCC, coils ~8×75 mA + margin) →
  buck 48→5 V (logic: ESP + MAX485). Star GND at power corner: 48 V GND = ESP GND = relay GND = meter GND.
- **Gauges (defaults, `DAD:` confirm):** 48 V feed 18 AWG · relay loads 18 AWG · logic 22 AWG ·
  RS485 A/B twisted pair 22 AWG.
- **Fusing:** inlet fuse on 48 V (`DAD:` rating); per-channel ≤3 A if meter lines are driven
  rather than dry-contact (`DAD:` confirm dry-contact vs driven).
- **Clearance:** ≥3 mm between 48 V terminals and logic (IPC-2221 is lenient at 48 V, but the
  barrier wall makes it moot); keep mains nowhere near this box — 48 V DC only.
- **Strain relief:** every external cable lands on a gland + internal cable-tie anchor;
  relay tails get a service loop so the lid opens without tension.
- **Display SPI:** route away from relay coils; if flicker appears, twist SCK/MOSI with GND.
- **E-STOP wiring:** NC contact in series with 48 V *after* the fuse, before the bucks —
  kill is absolute, fusing stays intact.
- **Bench power-on order:** bucks first (no ESP) → verify 12.0/5.0 V (`DAD:` tolerances) →
  flash → RED boot → GX16 J1 in → GREEN ≤1 s → web START → R1..R8 sweep → kill tests →
  1 h soak.

---

## 7. Indicative BOM

| Part | Spec | Qty |
|---|---|---|
| IP65 polycarbonate box | ~240×190×90 | 1 |
| 2.8" ILI9341 TFT, SPI | 320×240 | 1 |
| Metal illuminated pushbutton, green | 19 mm, momentary, IP65 | 1 |
| Metal pushbutton, yellow | 16 mm, momentary, IP65 | 1 |
| Metal toggle + guard cover | 12 mm, maintained | 1 |
| E-stop mushroom, NC | 22 mm, latching twist-release | 1 |
| Amber jewel indicator | 12 V, panel mount | 1 |
| Green/red panel LEDs (bezels) | 12 mm | 2 |
| Cable glands | PG9 ×1, PG7 ×1, PG13.5 ×1 | 3 |
| Fuse holder + fuse | panel accessible, `DAD:` rating | 1 |
| TVS diode + reverse-polarity diode | 48 V bus, `DAD:` part nos | 1+1 |
| Buck 48→12 V / 48→5 V | `DAD:` part nos + ratings | 1+1 |
| GX16-5 chassis sockets | meter link J1/J2/AUX | 3 |
| USB-C pigtail + dust cap | service port | 1 |
| Wire 18/22 AWG, twisted pair | silicone insulation | as needed |
| M3 standoffs, heat-set inserts | — | set |
| Engraved labels / traffolyte tags | START/SPOOF/KILL/E-STOP/QR | set |

---

## 8. What stays as-is

- Firmware v2.7 logic untouched except the new display module; all GPIOs/pins unchanged.
- Web dashboard remains the admin surface (config, OTA, console) — display is *status only*.
- GX16 pin table: dad overwrites with the meter's real 10-pin table; sim re-maps in 5 min.

## 9. Next step

Parametric CAD (OpenSCAD): base tray with zones, lid with display cutout + bezel,
front-panel drilling template with exact hole positions, then STLs + rendered previews.
Needs from you: confirm display choice (2.8" TFT?) and measure your actual relay/buck modules.
