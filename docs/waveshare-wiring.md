# Waveshare ESP32-S3-ETH-8DI-8RO — Complete Wiring & Connection Guide

Step-by-step wiring for the BMS connection tester firmware (`s3-waveshare`
build). Covers every screw terminal, what each LED means, and how to
troubleshoot the RS485 link. Board reference:
https://www.waveshare.com/wiki/ESP32-S3-ETH-8DI-8RO

---

## 1. Power — pick ONE

| Option | Connector | Notes |
|---|---|---|
| USB-C | Type-C port | 5 V / 1 A. Powers the board, flashes firmware, and gives you the serial console. **Use this for bench setup.** |
| Screw terminal | `VIN` / `GND` (7–36 V) | Wide-range DC input for DIN-rail installs. Do NOT feed more than 36 V. |

Both feed the same rail — never connect both at once.

**PWR LED** (red, near the USB port) lights as soon as the board has power.
If it's dark, check your supply before anything else.

---

## 2. RS485 to the JBD meter — the important one

### Terminals

The board's RS485 screw terminal block is labeled (left to right):

| Board terminal | Goes to | SP3485 pin |
|---|---|---|
| **A** | Meter's **A** | Pin 6 (non-inverting) |
| **B** | Meter's **B** | Pin 7 (inverting) |
| **GND** | Meter's GND (if it has one) | — |

Wire: **A→A, B→B**. Twisted pair, keep it short on the bench (< 3 m needs
no special treatment).

### JBD meter side

On JBD/Jiabaida smart BMS units the RS485 port is usually:

- **J5** (HY2.0-4P): pin 1 = **B**, pin 2 = **A**, or
- **J1** (to RS485 adapter): green wire = **A**, yellow wire = **B**.

Match **A→A** and **B→B**. The firmware polls at **9600 baud, 8N1** —
the meter must be set to 9600 baud (JBD default).

### The 120 Ω termination jumper

Next to the RS485 terminal there is a jumper labeled **120R**. The wiki FAQ
says it explicitly: *"Please move the jumper cap to 120R and try again.
Some RS485 devices require a 120R resistor to be connected in series."*

- **Bench (short cable, 2 devices):** try **without** the jumper first.
- **No comms:** put the jumper **ON** (enables the 120 Ω terminator).
- Only one terminator per bus end — with just board + meter, one jumper on
  is correct.

### Are A and B reversed? How to tell

This is the single most common RS485 wiring fault, and the industry's
A/B labeling is genuinely confusing (even the RS-485 standard's own
naming trips up manufacturers). Symptoms of **reversed A/B**:

- **TXD LED blinks** (the board IS transmitting polls — hardware LED,
  always blinks on TX).
- **RXD LED stays dark** (the meter hears garbage and never answers).
- Web dashboard shows **no meter data**, RGB status lamp stays **red**.
- No damage — RS485 is differential and tolerant; swapping A/B cannot
  hurt either side.

**Fix:** power down, swap the two wires (A↔B) at ONE end only, power up.
If the RXD LED starts flickering and the dashboard shows voltage, you had
them reversed. There is no universal "correct" — some JBD batches and
some USB-RS485 dongles label them opposite; trust the LEDs, not the
silkscreen.

### RS485 indicator LEDs

| LED | Color | Meaning | Driven by |
|---|---|---|---|
| **TXD** | (activity) | Blinks when the board transmits on the RS485 bus | **Hardware** — tied to the UART TX line, blinks automatically on every poll the firmware sends. No firmware code needed. |
| **RXD** | (activity) | Blinks when anything arrives on the RS485 bus | **Hardware** — tied to the UART RX line, blinks on every byte received, valid or not. |

These are **pure hardware** indicators. The firmware does not (and cannot)
drive them via GPIO — they reflect raw electrical activity on the bus.

**"The RS485 lights aren't working" — checklist:**

1. **TXD never blinks:** the firmware isn't transmitting. Check the board
   booted (PWR on, RGB lamp red), and that a test is actually running
   (START pressed). At 9600 baud each poll is ~4 ms — look closely, or
   watch in a dark room.
2. **TXD blinks, RXD dark:** the meter isn't answering. Check A/B
   (swap them), 120R jumper, meter powered, meter baud = 9600.
3. **Both blink but no data:** electrical link is fine; the meter is
   answering but frames fail checksum. Check for a second device on the
   bus, or severe noise (route the pair away from relay/mains wiring).
4. **Neither ever blinks, board is alive:** hardware fault on the
   isolated RS485 section — inspect the terminal screws and the 120R
   jumper seating.

> Note: the firmware's direction control is **hardware automatic** — the
> onboard circuit switches the SP3485 between TX and RX by itself. There
> is no DE/RE GPIO to configure (unlike the generic MAX485 build, which
> uses GPIO4).

---

## 3. Relay outputs R1–R8

Eight independent changeover relays, driven by the **TCA9554PWR** I²C
expander (address `0x20`; firmware handles it — no user config).

| Per channel | Terminals |
|---|---|
| R1 … R8 | **COM** (common), **NO** (normally open), **NC** (normally closed) |

- Contact rating: **≤ 10 A @ 250 V AC** or **≤ 10 A @ 30 V DC** per channel.
- The tester firmware sequences them R1→R8 during a test run
  (firmware bytes `0x01`→`0xFF` on the TCA output register).
- **Relay polarity is fixed in hardware** (HIGH bit = ON). The web UI's
  "Logic" dropdown is disabled on this board — what you see is what the
  coil does.
- There are **no per-relay status LEDs** on this board — watch the web
  dashboard's relay indicators, or listen for the clicks.

**Safety:** relay contacts are isolated from the logic side (optocouplers
+ power isolation), but the screw terminals expose **mains-capable**
contacts. Wire and touch them only with power off, fuse the load side,
and never exceed the 10 A rating. For the meter-tester use case the
relays switch low-voltage test loads — keep it that way.

---

## 4. Digital inputs DI1–DI8

Opto-isolated inputs, **active LOW** (pull the DIx terminal to COM/GND to
trigger). The firmware reads them with internal pull-ups.

| Terminal | GPIO | Firmware function (default) |
|---|---|---|
| **DI1** | GPIO4 | **Spoof trigger** — two-stage: 100.0 V, then 88.8 V |
| **DI2** | GPIO5 | **Wi-Fi kill** — ground = AP off, release = AP back |
| DI3–DI8 | GPIO6–GPIO11 | Free (reserved for future use) |
| **COM** | — | Common/ground reference for the DI terminals |

Wire a dry contact (pushbutton, relay contact) or an open-collector
signal **between DIx and COM**. Active = contact closed = GPIO reads LOW.

- The spoof input is web-changeable to any of DI1–DI8 (admin tab).
- `button_invert` / `spoof_invert` toggles flip the sense if your wiring
  is active-HIGH instead.

---

## 5. Every indicator on the board

| Indicator | Type | Meaning |
|---|---|---|
| **PWR** | Red LED, hardware | On = board powered. Dark = check supply. |
| **TXD** | Activity LED, hardware | Blinks on RS485 transmit (see §2). |
| **RXD** | Activity LED, hardware | Blinks on RS485 receive (see §2). |
| **WS2812 RGB** | Addressable LED, **GPIO38**, firmware-driven | **Red** = silent (no valid meter traffic). **Green** = link (talking to meter). This is the firmware's main status lamp. |
| BOOT / RESET | Tactile buttons | BOOT (GPIO0): press = START/STOP test; **hold 10 s = factory reset**. RESET: hardware reset. |

The **RGB lamp is the one the firmware controls**. Its truth table:

| State | Color | When |
|---|---|---|
| Boot / idle / no meter | **Red** | From the first line of `setup()`, before anything else |
| Valid JBD traffic | **Green** | After the first accepted meter frame |
| OTA update | (blue-ish progress) | During firmware upload |

> Hardware note: this board's RGB element expects **RGB byte order**
> (not the WS2812-standard GRB). The firmware compensates (swaps R/G in
> the `neopixelWrite` call) — the same workaround as Waveshare's own
> demo (`RGB_Light(r,g,b)` → `neopixelWrite(pin, g, r, b)`).

There are **no** GPIO-driven TXD/RXD LEDs and **no** per-relay LEDs —
don't go looking for GPIOs for them; they don't exist.

---

## 6. Other connectors (reference)

| Connector | Use | Firmware touches it? |
|---|---|---|
| RJ45 Ethernet (W5500) | 10/100 Mbps network | **No** — reserved, not implemented. The box uses Wi-Fi. |
| RTC battery header | PCF85063 backup cell | No |
| TF card slot | Storage | No |
| Pin header (GPIO0/1/2/3/21/43/44/45/47/48 …) | Expansion | No — free for your own hardware |
| Buzzer (GPIO46) | Audible alerts | No — unused by this firmware |

---

## 7. Bench bring-up — step by step

1. **Power** via USB-C. PWR LED on. RGB lamp **red** within a second.
2. **Wi-Fi:** join AP `BMS-Tester` (password `bms12345`), open
   `http://192.168.4.1` (admin password `admin123`).
3. **RS485:** wire A→A, B→B to the meter (GND if available). Leave the
   120R jumper **off** for the first try.
4. **Press BOOT** (or START in the web UI). TXD should flicker as polls
   go out.
5. **Watch RXD.** Flickering + dashboard voltage = link up, RGB turns
   **green**. Dark RXD → swap A/B at one end (§2), then try the 120R
   jumper.
6. **Relays:** the test sequence clicks R1→R8; confirm on the dashboard.
7. **Spoof:** short DI1 to COM → dashboard shows 100.0 V, then 88.8 V.
8. **Wi-Fi kill:** short DI2 to COM → AP drops; release → AP returns.

---

## 8. Quick troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| PWR dark | No power | Check USB-C / 7–36 V terminal |
| RGB never lights | Firmware didn't boot | Reflash; check serial console |
| RGB stuck red, TXD blinks, RXD dark | Meter not answering | Swap A/B; 120R jumper; meter power/baud |
| RGB stuck red, TXD dark | No polls sent | Start a test (BOOT / web START) |
| RXD blinks, no data on dashboard | Checksum failures | Noise on the pair; shorten cable; check termination |
| Relays don't click | TCA9554 not found | I²C bus issue — reflash; check 3V3 rail |
| AP missing | DI2 grounded | Release DI2; or factory reset (BOOT 10 s) |
| Board not detected for flashing | Not in download mode | Hold BOOT, tap RESET, release BOOT, then flash |
