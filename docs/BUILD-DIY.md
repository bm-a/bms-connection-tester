# Build guide: generic ESP32-S3 DIY

Build the tester from discrete modules on a bench: ESP32-S3 board, MAX485
transceiver, 8-channel relay module, LEDs, and a button. Follow every step
in order — a beginner with the parts list below reaches a working tester.

What the finished box does: it impersonates a JBD / Xiaoxiang smart BMS
over RS485, so an e-rickshaw meter or display can be tested without a
battery pack. An 8-relay sequencer switches the meter's own functions in
order, and a web dashboard (always-on Wi-Fi AP `BMS-Tester`) controls
everything.

## 1. Shopping list

| # | Part | Spec | Notes |
|---|---|---|---|
| 1 | ESP32-S3 DevKitC-1 | 8 MB flash | The standard build target. **Or** an ESP32-S3 N16R8 board (16 MB flash + 8 MB OPI PSRAM) — use the `s3-n16r8` firmware env |
| 2 | MAX485 module (or SP3485) | 3.3 V logic | Half-duplex RS485 transceiver with DE/RE pins broken out |
| 3 | 8-channel relay module | 12 V coils, SmartElex-style, optoisolated inputs, ESP 3.3 V compatible | Default active-LOW (LOW = ON); check the module's jumper |
| 4 | Green LED + red LED | 3–5 mm | Link status indicators |
| 5 | 2 × 220 Ω resistors | 1/4 W | LED current limit (~8 mA) |
| 6 | 1 × 10 kΩ resistor | 1/4 W | Pull-down on the RS485 direction pin — **do not skip** |
| 7 | Push button | Momentary, normally open | START/STOP + factory reset |
| 8 | USB-C data cable | | Flashing + powering the ESP |
| 9 | 12 V DC supply | ≥ 1 A | **Relay coils only** — 8 × ~75 mA plus margin |
| 10 | 5 V USB charger (2 A) or power bank | | ESP + MAX485 power |
| 11 | Hookup wire, breadboard or perfboard | | |
| 12 | Twisted-pair wire | | RS485 A/B run to the meter |

Optional: an enclosure. `enclosure/` in this repo has a full IP65
standalone-box design (CAD + STL + front panel + GX16 meter-link
connector docs + 48 V power tree) if you want to box the finished bench.

## 2. Wiring

Power the ESP over USB **only after** all wiring is checked. Keep the
12 V coil supply OFF until step 2.5.

### 2.1 Relay module → ESP32-S3

| Relay module IN | ESP32-S3 GPIO | Notes |
|---|---|---|
| IN1 | GPIO5 | R1 |
| IN2 | GPIO6 | R2 |
| IN3 | GPIO7 | R3 |
| IN4 | GPIO8 | R4 |
| IN5 | GPIO9 | R5 |
| IN6 | GPIO12 | R6 |
| IN7 | GPIO13 | R7 |
| IN8 | GPIO14 | R8 |

- Relay module logic VCC → ESP 3.3 V. JD-VCC (coil power) → **separate
  12 V supply**. Tie the 12 V supply GND to the ESP GND (common ground).
- **Never power the relay coils from the ESP's 5 V pin** — USB cannot
  drive 8 coils; the ESP will brown-out and reset every time relays switch.
- Default logic is active-LOW (GPIO LOW = relay ON). If your module's
  jumper says active-HIGH, flip the **Logic dropdown** in the dashboard's
  Sequence config card instead of rewiring.
- Firmware drives the OFF level *before* setting pin modes, so no relay
  clicks at boot.
- Loads switch up to 3 A/channel (module rating). Use NO/COM/NC per
  channel to the meter functions under test.

### 2.2 MAX485 → ESP32-S3 → meter

| MAX485 pin | Connects to | Notes |
|---|---|---|
| DI | GPIO17 (ESP TX) | ESP transmits replies |
| RO | GPIO16 (ESP RX) | ESP receives meter requests |
| DE + RE (tied together) | GPIO4 (ESP) | HIGH = transmit, LOW = receive |
| VCC | ESP 3.3 V | S3 logic levels |
| GND | ESP GND | Common ground |
| A | Meter RS485 A (D+) | Twisted pair |
| B | Meter RS485 B (D-) | Twisted pair |

- **10 kΩ resistor from the DE/RE net (GPIO4) to GND.** The pin floats at
  reset; without the pull-down the MAX485 can power up transmitting and
  jam the bus. This is the single most common DIY wiring mistake.
- Firmware sequence per reply: DE HIGH → write → flush → 1.5 ms guard →
  DE LOW → drain stale RX.
- Common ground across ESP, MAX485, and meter. 120 Ω termination across
  A–B at the far end if the cable run is long.
- Protocol: 9600 8N1. The tester answers `0x03` (basic info: 52.0 V,
  0 A, 100 %), `0x04` (14 cell voltages), `0x05` (device name) and stays
  **silent on writes and unknown registers**.

### 2.3 Status LEDs

| LED | ESP32-S3 | Wiring |
|---|---|---|
| Green (link) | GPIO10 | GPIO10 → 220 Ω → LED anode, LED cathode → GND |
| Red (silent) | GPIO11 | GPIO11 → 220 Ω → LED anode, LED cathode → GND |

Driven as strict opposites in firmware — both-on / both-off (other than
unpowered) is impossible. Green = valid BMS traffic seen, red = bus
silent. On N16R8 boards the onboard WS2812 RGB (GPIO48) mirrors the same
state automatically.

### 2.4 Button and control inputs

| Input | ESP32-S3 | Wiring | Behavior |
|---|---|---|---|
| START/STOP button | GPIO15 | Button between GPIO15 and GND (internal pull-up; press = LOW) | Web-selectable behavior: hold-to-run, run-to-completion, or re-press restart. **Hold 10 s = factory reset** |
| Wi-Fi kill | GPIO18 | Jumper/switch between GPIO18 and GND | Grounded = AP off; release = AP back |
| Spoof trigger | GPIO21 (default) | Button/switch between GPIO21 and GND (internal pull-up) | Fires the 2-stage fault spoof. Pin is web-changeable: safe pins are 1, 2, 21, 38–44, 47 |

30 ms debounce on the button in firmware. All three inputs are
web-invertible if your wiring is active-HIGH.

### 2.5 Power-up order

1. 12 V coil supply OFF, USB unplugged. Double-check every row above,
   especially the 10 kΩ pull-down on GPIO4 and that relay JD-VCC goes
   to 12 V, not to the ESP.
2. Plug USB into the ESP (use a real charger, not a weak laptop port —
   the always-on AP draws ~1 W-class).
3. Switch the 12 V coil supply ON.
4. Green/red LEDs: red at boot, green ≤ 1 s after the meter starts
   polling. No relay clicks at boot.

## 3. Build environment choice

`platformio.ini` ships these environments:

| Env | Target | Dashboard | Use when |
|---|---|---|---|
| `esp32-s3-devkitc-1` | 8 MB DevKitC-1 | **FULL** (default) | Standard DIY build — this is the v2.8 release image |
| `s3-n16r8` | 16 MB + OPI PSRAM | FULL | N16R8 boards (correct flash/PSRAM map) |
| `s3-classic` | 8 MB | CLASSIC | You want the v2.6 page byte-verbatim |
| `s3-lite` | 8 MB | LITE | Smallest page: relay tiles + names + LINK pill only (~4.9 KB) |

FULL = everything (live SVG bench card, relay tiles + names,
meters-today, sequence config, spoof, OTA, console). OTA assets always
stay FULL so updating never strands a box.

## 4. Flashing

### 4.1 Fast path: release image (recommended)

1. Install esptool: `pip install esptool`.
2. Download from the **v2.8** GitHub release:
   - 8 MB DevKitC-1 → `bms-tester-8mb.bin` (from `firmware/`)
   - N16R8 → `bms-tester-n16r8.bin` (from `firmware-n16r8/`)
   
   Both are merged one-file images (bootloader + partitions + app).
3. Plug the ESP32-S3 in over USB-C. Note the serial port.
4. Flash at offset `0x0`:

```sh
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 write_flash 0x0 bms-tester-8mb.bin
```

### 4.2 From source: PlatformIO

```sh
pio run -e esp32-s3-devkitc-1 -t upload   # 8 MB board
pio run -e s3-n16r8 -t upload             # N16R8 board
```

### 4.3 Arduino IDE

1. Open `arduino/bms_connection_tester/bms_connection_tester.ino`
   (all tabs open automatically).
2. Board: **ESP32S3 Dev Module**, **USB CDC On Boot: Enabled**, upload
   speed 921600.
3. N16R8 also needs: Flash Size 16 MB + PSRAM OPI.

### 4.4 Verify the flash

Run the host test suite (no hardware needed):

```sh
sh run_tests.sh
```

All suites green means the firmware logic is sound; wiring is then the
only remaining variable.

## 5. First boot and dashboard tour

1. On your phone or laptop, join the Wi-Fi network **`BMS-Tester`**
   (open network, always broadcasting). A captive-portal popup may
   appear — open the dashboard in the real browser instead:
   **`http://192.168.4.1`** (laptops can also use `bmstester.local`).
   "No internet" is normal. Turn mobile data off if the page will not load.
2. The dashboard has no login wall. Reboot, factory reset, firmware
   upload, and all saves ask for the admin password instead
   (default `admin123`).

Walk every card top to bottom:

1. **Relays** — 8 tiles showing live relay state. Click a tile to force
   that relay on/off manually. START/STOP buttons run the configured
   sequence. Cycle and actuation counters sit here.
2. **Bench (live)** — FULL-dashboard SVG strip: 48 V → bucks → ESP →
   MAX485 with TX/RX activity dots → A/B flow → live meter readout
   (`mv`/`ma`/`msoc` mirroring the last `0x03` reply, golden values or
   the active spoof stage).
3. **Meters today (approx)** — software estimate, no button needed:
   a link gap ≥ 3 s (reseat) counts a new meter once the link holds 5 s
   (fumble guard); retries with the meter plugged in do not count.
   Shows meters / attempts / pass / fail. Pass = a full cycle completed
   before the swap. **New day (reset)** zeroes the counters (no RTC on
   the box; a reboot never zeroes them).
4. **Sequence config** — mode (Sequential / Chase / All-ON), step delay
   (≥ 100 ms, default 250), hold time, direction fwd/rev, how many of
   the 8 relays participate, loop + inter-cycle pause (500–60000 ms,
   default 2000) + cycle limit (burn-in racks), All-ON stagger (inrush
   ramp, default 50 ms), boot auto-start, **Logic** (active-LOW default;
   flip if your relay module jumpers say HIGH). Chase has a fixed 20 ms
   break-before-make. Mode switch needs IDLE; START is refused 0.5 s
   after STOP (relay settle); count-shrink acts immediately.
5. **Relay labels** — 8 editable names (e.g. "Headlamp", "Horn"),
   stored in NVS, shown on the tiles.
6. **Fault spoof (0x03 test values, stage 1 then 2)** — two fully
   editable stages: stage 1 shows realistic values first, stage 2 shows
   the fault pattern (default 88.8 V / 88.8 A / 88.8 °C / 188 %).
   Duration 1–120 s (default 10 s), then auto-reverts to golden.
   Trigger group: enable + GPIO + polarity, with a dedicated Save
   (stores without firing) — or press FIRE now. `0x04`/`0x05` never change.
7. **Firmware update** — offline `/update` file upload (Tasmota-grade
   gates: exact variant filename `firmware.bin`, `0xE9` magic +
   flash-size-vs-chip check on the first bytes, explicit sketch budget,
   one finalize — a wrong file changes nothing), OTA check cadence,
   Check now / Install, custom firmware URL (Upgrade-from-URL).
8. **Admin & Wi-Fi AP** — change the AP SSID/password/channel, change
   the **admin password** (do this first — see step 3 below), STA uplink
   section (enter office Wi-Fi credentials, **Test** joins ≤ 30 s,
   reports RSSI/IP, then drops back to AP-only; used for OTA),
   **Backup configuration** (downloads JSON; passwords never included —
   save before upgrading), **Restore** (upload a backup + admin
   password), reboot, factory reset, keep-WiFi reset, boot-counter reset.
9. **Information** — firmware version, build variant, flash size, heap,
   uptime, boot count, reset reason, RSSI, and the pin map for this build.
10. **Console** — type commands: `START STOP FIRE CANCEL DAYRESET STATUS
    UPTIME VERSION REBOOT RESET HELP`. `STATUS` replies
    `LINK GREEN seq=IDLE cyc=… act=… spoof=… met=… att=… ps=… fl=…`.
    (Over USB-serial, `STATUS?` replies `GREEN 2.8` / `RED 2.8` — test-jig
    only, not a JBD command.)

3. **Change the admin password now** (Admin & Wi-Fi AP card). The
   dashboard is open to anyone on the AP; the password gates reboot,
   reset, uploads, and saves.
4. **Run your first sequence:** Sequence config → relay count 8, step
   250 ms, hold 500 ms → START (or press the GPIO15 button). Watch the
   tiles light in order and listen for the 8 relays clicking. STOP halts.
5. **Fire your first spoof:** Fault spoof card → FIRE (or ground GPIO21).
   The meter should show the stage-1 values, then the stage-2 fault
   pattern, then revert to golden when the window expires. CANCEL aborts
   early.
6. **Read meters-today:** run a sequence, then unplug the meter's RS485
   for > 3 s and plug it back in. After the link holds 5 s, the meters
   counter increments and the completed cycle records a pass.
7. **Back up your config:** Admin card → Backup configuration, save the
   JSON somewhere safe. Restore it after any firmware upgrade.

## 6. OTA / updates

v2.8 is a single unified release line.

- The box checks GitHub `releases/latest` over the STA uplink and
  offers the update when a newer tag exists.
- It downloads only the exact-variant asset (**`firmware.bin`** for the
  8 MB build); an N16R8 or Waveshare file is refused by the filename gate.
- Manual upload (`/update`) and custom-URL OTA enforce the same gates.
- Before any upgrade, download a **Backup configuration** JSON from the
  Admin card.

## 7. Troubleshooting

| Symptom | Check |
|---|---|
| Red LED stays on, LINK pill red | Meter not polling, or A/B swapped — swap A and B first. Confirm the meter speaks 9600 8N1 JBD and is powered. The tester never initiates — it only answers |
| Red LED at boot even with meter wired | 10 kΩ pull-down on GPIO4 missing or wrong net — without it the MAX485 can power up transmitting and jam the bus |
| Relays chatter / ESP resets when relays switch | Coils powered from the ESP 5 V pin — move JD-VCC to the separate 12 V supply and common the GNDs. This is the #1 DIY failure |
| Relays work backwards | Logic dropdown in Sequence config (active-LOW vs HIGH), or the relay module's jumper |
| No relay clicks at all | IN1–8 → GPIO 5,6,7,8,9,12,13,14 in order; relay module logic VCC = 3.3 V; 12 V coil supply on and GND commoned |
| `BMS-Tester` AP not visible | GPIO18 grounded (Wi-Fi kill active)? Remove the jumper. Weak USB power — use a real charger |
| Dashboard will not load at 192.168.4.1 | Phone routing around the AP — turn mobile data off. Stay joined to `BMS-Tester` |
| No USB serial / boot loops after Arduino flash | Tools → **USB CDC On Boot: Enabled**. Re-flash with the release image to rule out a bad build |
| Upload does not start (esptool) | Hold BOOT, press/release RESET (or EN), then flash. Try another USB-C data cable |
| OTA says check failed | STA uplink not joined or no internet — run the STA Test in the Admin card and read its message line |
| Counters look wrong | Meters-today is an estimate from link gaps: a fumbly reseat needs the 5 s settle; retries without unplugging never count. Use New day (reset) at shift start |
| Everything misbehaves after a bad config | 10 s hold on the GPIO15 button = factory reset (wipes Wi-Fi/admin config, reboots). Or console `RESET` with the admin password |
