# Build guide: Waveshare ESP32-S3-ETH-8DI-8RO

One board, no soldering of logic parts. This guide takes you from the box
to a working BMS connection tester: flashing, wiring the meter, first
sequence, first spoof, and reading the daily counters.

What the finished box does: it impersonates a JBD / Xiaoxiang smart BMS
over RS485, so an e-rickshaw meter or display can be tested without a
battery pack. An 8-relay sequencer switches the meter's own functions in
order, and a web dashboard (always-on Wi-Fi AP `BMS-Tester`) controls
everything.

## 1. Shopping list

| # | Part | Notes |
|---|---|---|
| 1 | Waveshare **ESP32-S3-ETH-8DI-8RO** | The exact board name. ESP32-S3-WROOM-1-N16R8 module: 16 MB flash, 8 MB OPI PSRAM. Has 8 relay outputs, 8 opto-isolated digital inputs, isolated RS485, and Ethernet hardware on board |
| 2 | USB-C data cable | For flashing and bench power. Must be a data cable, not charge-only |
| 3 | 5 V USB charger (2 A) or power bank | Bench power for the board |
| 4 | Twisted-pair wire for RS485 A/B | Short run to the meter; shielded twisted pair if the factory floor is noisy |
| 5 | Wire for relay outputs | 8 channels, NO/COM/NC per channel, to the meter functions under test |
| 6 | Small push buttons / dry contacts (optional) | Only if you want physical spoof-trigger / Wi-Fi-kill switches instead of the web UI |

No separate relay module, no MAX485 module, no LEDs, no pull-down
resistors: the board integrates all of them. For permanent installs the
board also accepts DC 7–36 V on its VIN terminal (Waveshare spec); USB-C
5 V is enough for bench use.

## 2. Board tour (what the firmware uses)

| Function | Board resource | Firmware mapping |
|---|---|---|
| Relays R1–R8 | TCA9554PWR I/O expander @ I2C `0x20` | SDA GPIO42, SCL GPIO41. EXIO1–EXIO8 = R1–R8. Output-register bit HIGH = relay ON (fixed in hardware) |
| RS485 | Onboard isolated SP3485 | TX GPIO17, RX GPIO18. **Hardware auto-direction** — there is no DE/RE pin |
| START/STOP | BOOT button | GPIO0. Press = LOW. 10 s hold = factory reset |
| Spoof trigger | DI1 terminal | GPIO4, active LOW. Web-changeable to DI1–DI8 (GPIO4–11) |
| Wi-Fi kill | DI2 terminal | GPIO5, active LOW = AP off |
| Status lamp | Onboard WS2812 RGB | GPIO38. Green = link up, red = bus silent |
| Spare inputs | DI3–DI8 terminals | GPIO6–GPIO11, opto-isolated, active LOW, currently unused by firmware |
| Reserved, untouched | W5500 Ethernet (GPIO12–16), GPIO40 (RTC int), GPIO46 (buzzer) | Ethernet is **not implemented** in firmware; the box stays on the Wi-Fi AP |

DI1–DI8 are opto-isolated: wire a trigger as a dry contact (or driven
signal) between the DIx terminal and COM so the GPIO is pulled LOW when
active. The dashboard's invert toggles flip the sense if your wiring
differs.

Two deliberate differences from the generic DIY build:

- **Relay polarity is fixed.** The TCA9554 stage drives HIGH-bit = ON in
  hardware, so the dashboard **Logic dropdown is disabled** (greyed out)
  on this board. Relay labels always match the coils.
- **Ethernet is reserved, not integrated.** The W5500 pins are left alone;
  the box keeps the always-on `BMS-Tester` AP like the generic build.

## 3. Wiring

### 3.1 RS485 to the meter

| Board terminal | Meter side |
|---|---|
| RS485 **A** | Meter RS485 A (D+) |
| RS485 **B** | Meter RS485 B (D-) |

- The transceiver is isolated, so no shared GND is strictly required.
  Keep A/B as a twisted pair.
- If the link LED stays red after wiring, **swap A and B first** — swapped
  polarity is the most common cause. Then confirm the meter is actually
  polling (the tester only answers; it never initiates).

### 3.2 Relay outputs to the meter functions

Each of the 8 relay channels exposes COM / NO / NC terminals.

| Channel | Board terminal | Wire to |
|---|---|---|
| R1 | Relay 1 COM/NO/NC | Meter function 1 |
| R2 | Relay 2 COM/NO/NC | Meter function 2 |
| R3–R8 | … | … |

- Use NO (normally open) when the function should be off at rest and
  switch on during the test; NC for the inverse.
- At boot the firmware parks all relays OFF before the expander is fully
  configured (break-before-make on mode changes), so nothing glitches.
- Relay loads: check the Waveshare spec for per-channel current rating
  and do not exceed it.

### 3.3 Digital inputs (optional physical switches)

| Function | Terminal | Wiring | Behavior |
|---|---|---|---|
| Spoof trigger | DI1 (default) | Dry contact DI1–COM, close = active | Fires the 2-stage fault spoof. Web-changeable to DI1–DI8 |
| Wi-Fi kill | DI2 | Dry contact DI2–COM, close = active | AP off while held; release to restore |

Leave DI3–DI8 unwired if unused. Everything these switches do is also
available as dashboard buttons.

## 4. Flashing

### 4.1 Fast path: release image (recommended)

1. Install esptool: `pip install esptool`.
2. Download `bms-tester-waveshare.bin` from the **v2.8** GitHub release
   (merged one-file image: bootloader + partitions + app).
3. Plug the board in over USB-C. Note the serial port (`/dev/ttyACM0` on
   Linux, `COMx` on Windows).
4. Flash at offset `0x0`:

```sh
esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0x0 bms-tester-waveshare.bin
```

5. If the upload does not start: hold **BOOT**, press and release
   **RESET**, then run the command again.

### 4.2 From source: PlatformIO

```sh
pio run -e s3-waveshare -t upload
```

This builds the `s3-waveshare` environment (16 MB flash map, OPI PSRAM,
`-DBOARD_WAVESHARE_8DI8RO=1`, FULL dashboard) and uploads it.

### 4.3 What you should see at boot

- RGB LED comes up **red** (bus silent, no meter frames yet).
- Within ~1 s of the meter polling, RGB turns **green** and the
  dashboard LINK pill goes green.
- All 8 relays stay OFF through boot — no clicks.

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
   (≥ 100 ms), hold time, direction fwd/rev, how many of the 8 relays
   participate, loop + inter-cycle pause + cycle limit (burn-in racks),
   All-ON stagger (inrush ramp), boot auto-start. The Logic dropdown is
   greyed out on this board (polarity fixed Active-HIGH). Mode switch
   needs IDLE; START is refused 0.5 s after STOP (relay settle).
5. **Relay labels** — 8 editable names (e.g. "Headlamp", "Horn"),
   stored in NVS, shown on the tiles.
6. **Fault spoof (0x03 test values, stage 1 then 2)** — two fully
   editable stages: stage 1 shows realistic values first, stage 2 shows
   the fault pattern (default 88.8 V / 88.8 A / 88.8 °C / 188 %).
   Duration 1–120 s (default 10 s), then auto-reverts to golden.
   Trigger group: enable + DI pin + polarity, with a dedicated Save
   (stores without firing) — or press FIRE now. `0x04`/`0x05` never change.
7. **Firmware update** — offline `/update` file upload (Tasmota-grade
   gates: exact variant filename `waveshare-firmware.bin`, image-head
   check, explicit sketch budget — a wrong file changes nothing), OTA
   check cadence, Check now / Install, custom firmware URL.
8. **Admin & Wi-Fi AP** — change the AP SSID/password/channel, change
   the **admin password** (do this first — see step 3 below), STA uplink
   section (enter office Wi-Fi credentials, **Test** joins ≤ 30 s,
   reports RSSI/IP, then drops back to AP-only; used for OTA),
   **Backup configuration** (downloads JSON; passwords never included —
   save before upgrading), **Restore** (upload a backup + admin
   password), reboot, factory reset, keep-WiFi reset, boot-counter reset.
9. **Information** — firmware version, board variant (`waveshare`), flash
   size, heap, uptime, boot count, reset reason, RSSI, and the pin map
   for this board.
10. **Console** — type commands: `START STOP FIRE CANCEL STATUS UPTIME
    VERSION REBOOT RESET HELP`. `STATUS` replies
    `LINK GREEN seq=IDLE cyc=… act=… spoof=… met=… att=… ps=… fl=…`.
    (Over USB-serial, `STATUS?` replies `GREEN 2.8` / `RED 2.8` — test-jig
    only, not a JBD command.)

3. **Change the admin password now** (Admin & Wi-Fi AP card). The
   dashboard is open to anyone on the AP; the password gates reboot,
   reset, uploads, and saves.
4. **Run your first sequence:** Sequence config → set relay count and
   step/hold → START (or press BOOT on the board). Watch the tiles and
   listen for the 8 relays clicking in order. STOP halts.
5. **Fire your first spoof:** Fault spoof card → FIRE (or close the DI1
   contact). The meter should show the stage-1 values, then the stage-2
   fault pattern, then revert to golden when the window expires.
   CANCEL aborts early.
6. **Read meters-today:** run a sequence, then unplug the meter's RS485
   for > 3 s and plug it back in. After the link holds 5 s, the meters
   counter increments and the completed cycle records a pass.

## 6. OTA / updates

v2.8 unifies the release line: one **v2.8** release carries both the
generic and the Waveshare assets.

- The Waveshare build checks GitHub `releases/latest` over the STA
  uplink and offers the update when a newer tag exists.
- It downloads only the exact-variant asset **`waveshare-firmware.bin`**;
  a generic `firmware.bin` is refused by the filename gate.
- Manual upload (`/update`) and custom-URL OTA enforce the same gates.
- Before any upgrade, download a **Backup configuration** JSON from the
  Admin card.

## 7. Troubleshooting

| Symptom | Check |
|---|---|
| RGB stays red, LINK pill red | Meter not polling, or A/B swapped. Swap A and B at the board terminals first. Confirm the meter speaks 9600 8N1 JBD and is powered. The tester never initiates — it only answers |
| Relays do not click | You flashed the generic build by mistake — reflash `bms-tester-waveshare.bin` (the TCA9554 expander only exists in the waveshare build). Check relay output wiring COM/NO/NC and that the load is within the channel rating |
| `BMS-Tester` AP not visible | DI2 contact closed (Wi-Fi kill active)? Open it. Weak USB power — use a real charger. Reboot the board |
| Dashboard will not load at 192.168.4.1 | Phone routing around the AP — turn mobile data off. Stay joined to `BMS-Tester` ("no internet" is normal) |
| Upload/flash does not start | Hold BOOT, press/release RESET, then flash. Try another USB-C data cable (charge-only cables fail silently) |
| OTA says check failed | STA uplink not joined or no internet — run the STA Test in the Admin card and read its message line |
| Counters look wrong | Meters-today is an estimate from link gaps: a fumbly reseat needs the 5 s settle; retries without unplugging never count. Use New day (reset) at shift start |
| Everything misbehaves after a bad config | 10 s hold on BOOT = factory reset (wipes Wi-Fi/admin config, reboots). Or console `RESET` with the admin password |
