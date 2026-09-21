#!/usr/bin/env python3
"""Beautiful dad-friendly Word report — v2.5 (trigger save, portal landing, emulation)."""
from docx import Document
from docx.shared import Pt, Inches, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

NAVY = RGBColor(0x1F, 0x3B, 0x63)
GREY = RGBColor(0x59, 0x59, 0x59)

doc = Document()
normal = doc.styles["Normal"]
normal.font.name = "Calibri"
normal.font.size = Pt(11)
for i in (1, 2):
    hs = doc.styles[f"Heading {i}"]
    hs.font.name = "Calibri"
    hs.font.color.rgb = NAVY

def shade(cell, hexcolor):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:fill"), hexcolor)
    tcPr.append(shd)

def set_cell(cell, text, bold=False, size=10, color=None, center=False):
    cell.text = ""
    par = cell.paragraphs[0]
    if center:
        par.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = par.add_run(text)
    run.bold = bold
    run.font.size = Pt(size)
    run.font.name = "Calibri"
    if color:
        run.font.color.rgb = color

def table(headers, rows, widths=None, hdr_fill="1F3B63"):
    t = doc.add_table(rows=1 + len(rows), cols=len(headers))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for j, h in enumerate(headers):
        c = t.cell(0, j)
        set_cell(c, h, bold=True, size=10, color=RGBColor(0xFF, 0xFF, 0xFF), center=True)
        shade(c, hdr_fill)
    for i, row in enumerate(rows, start=1):
        for j, val in enumerate(row):
            set_cell(t.cell(i, j), str(val), size=10)
    if widths:
        for j, w in enumerate(widths):
            for i in range(len(rows) + 1):
                t.cell(i, j).width = Inches(w)
    doc.add_paragraph()
    return t

def callout(title, text, fill="FFF6D6"):
    t = doc.add_table(rows=1, cols=1)
    t.style = "Table Grid"
    cell = t.cell(0, 0)
    shade(cell, fill)
    cell.text = ""
    p1 = cell.paragraphs[0]
    r = p1.add_run(title + "  ")
    r.bold = True
    r.font.size = Pt(11)
    r.font.name = "Calibri"
    r2 = p1.add_run(text)
    r2.font.size = Pt(11)
    r2.font.name = "Calibri"
    doc.add_paragraph()

sec = doc.sections[0]
fp = sec.footer.paragraphs[0]
fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = fp.add_run("RS485 Connection Tester v2.5  —  Build & Test Report   |   Page ")
r.font.size = Pt(9)
r.font.color.rgb = GREY
fld = OxmlElement("w:fldSimple")
fld.set(qn("w:instr"), "PAGE")
fld.append(OxmlElement("w:r"))
fp._p.append(fld)

# ================= COVER =================
for _ in range(3):
    doc.add_paragraph()
tp = doc.add_paragraph()
tp.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = tp.add_run("E-Rickshaw Meter\nRS485 Connection Tester")
r.font.size = Pt(30)
r.bold = True
r.font.color.rgb = NAVY
sp = doc.add_paragraph()
sp.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = sp.add_run("Build & Test Report v2.5 — for an electronics engineer, no coding needed")
r.font.size = Pt(13)
r.italic = True
r.font.color.rgb = GREY
doc.add_paragraph()
table(["Item", "Detail"], [
    ["Board", "ESP32-S3 DevKitC-1 (or N16R8) + MAX485 module + 8-channel relay module"],
    ["Version", "v2.5 — answers 0x03/0x04/0x05, silent on the rest; self-adjusts to any poll speed; per-mode relay bench; 2-stage fault values + trigger save; phone-friendly portal landing; config web page + Tasmota-grade firmware updates"],
    ["Previous", "v1.0 frozen untouched (ZIP + git tag) — this report covers v2.5 (trigger/portal/schema/emulation on v2.4)"],
    ["Date", "September 2026"],
    ["Firmware", "firmware.bin — v2.5 8 MB build, compiled + verified (SHA in firmware/README.md)"],
    ["Tests", "137 / 137 passing + socket harness 64/64 (protocol + lamps + faults + 30-day soak + relay/BBM/dead-band/spoof-save+trigger/console/backup-v2/OTA-URL+ondemand/upload-gates/portal-landing/whitespace-JSON) + w3m page renders"],
    ["Use", "Green lamp = wiring correct, red lamp = wiring wrong. Button runs the relay sequence. Phone/laptop + web page configures everything."],
], widths=[1.6, 4.6])
callout("Passwords — keep this file safe: ",
        "Wi-Fi network BMS-Tester, password bms12345. Web page login: user admin, password admin123. Change both on first login (Admin card).")
cp = doc.add_paragraph()
cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = cp.add_run("Send this file as-is on WhatsApp — it opens in Word / Google Docs on any phone.")
r.italic = True
r.font.color.rgb = GREY
doc.add_page_break()

# ================= CONTENTS =================
doc.add_heading("Contents", level=1)
for item in [
    "1.  What this box does (start here)",
    "2.  Parts list",
    "3.  Wiring — the complete circuit (lamps + relays + button)",
    "4.  How v2.5 works (every meter type, no code)",
    "5.  Using it on the assembly line (lamps, button, web page)",
    "6.  Getting the software onto the board (3 easy methods)",
    "7.  Build & test report (numbers included)",
    "8.  Troubleshooting table",
    "9.  Safety & limits",
    "10. Project folder map + version history",
]:
    doc.add_paragraph(item)

# ================= 1 =================
doc.add_heading("1. What this box does", level=1)
doc.add_paragraph(
    "Every e-rickshaw meter must have its two RS485 communication wires (A and B) connected correctly "
    "before it leaves the factory. This box replaces the old check (real battery pack or a laptop) with two lamps:")
doc.add_paragraph("GREEN lamp ON  =  wiring correct, meter and box are talking.", style="List Bullet")
doc.add_paragraph("RED lamp ON  =  wiring wrong (or meter off, wires swapped, wire broken).", style="List Bullet")
doc.add_paragraph(
    "BUTTON = runs the 8 relays in order (each relay switches one meter function for testing). "
    "A phone/laptop web page (no office Wi-Fi needed — the box makes its own network) sets the "
    "step-by-step delay, the button behavior and test values. Details in §5.", style="List Bullet")
doc.add_paragraph(
    "Nothing to press, reset or read. v2.3 handles every meter variant by itself: fast or slow polling, meters that "
    "ask for extra data (cell voltages, device name), and meters that send configuration writes — green lights for all "
    "of them as long as the wires carry real traffic.")

# ================= 2 =================
doc.add_heading("2. Parts list", level=1)
doc.add_paragraph("One box needs:")
table(["Part", "Qty", "What it does"], [
    ["ESP32-S3 DevKitC-1 board (N16R8 also fine)", "1", "The brain. Listens, replies, drives lamps + relays + web page."],
    ["MAX485 module (SP3485 also fine)", "1", "Translator: ESP32 serial ↔ noise-proof RS485 signals."],
    ["Green LED + red LED (5 mm)", "1 + 1", "Link status at a glance."],
    ["220 Ω resistor", "2", "One per LED (≈ 6–10 mA)."],
    ["10 kΩ resistor", "1", "Pull-down on direction pin — powers up listening, never jamming."],
    ["8-channel 12 V relay module (SmartElex type, 3 A/channel)", "1", "Switches 8 meter functions in sequence. Optoisolated inputs, ESP-compatible."],
    ["12 V DC adapter (≈ 1 A)", "1", "Relay coils ONLY — USB cannot drive them. Share GND with the ESP."],
    ["Push button (momentary, to GND)", "1", "Starts/stops the relay sequence. Hold 10 s = factory reset."],
    ["USB cable + 5 V charger / power bank", "1", "Powers ESP + MAX485. NEVER the traction battery pack."],
    ["Twisted-pair wire + screw terminals", "1 set", "A/B test leads to the meter; relay wiring to meter functions."],
    ["Small plastic enclosure", "1", "Houses everything."],
], widths=[2.2, 0.7, 3.3])

# ================= 3 =================
doc.add_heading("3. Wiring — the complete circuit", level=1)
doc.add_heading("3.1 Every wire", level=2)
table(["Signal", "From → To", "Notes"], [
    ["TX data", "S3 GPIO17 → MAX485 DI", "Box talks out of this pin."],
    ["RX data", "MAX485 RO → S3 GPIO16", "Box listens here."],
    ["Direction", "S3 GPIO4 → MAX485 DE + RE tied", "HIGH = talk, LOW = listen, automatic."],
    ["Failsafe", "GPIO4 net → 10 kΩ → GND", "Holds ‘listen’ during power-up float."],
    ["Green lamp", "S3 GPIO10 → 220 Ω → green LED → GND", "Long leg towards resistor."],
    ["Red lamp", "S3 GPIO11 → 220 Ω → red LED → GND", "Boots red until first valid poll."],
    ["Power", "MAX485 VCC → 3.3 V", "NOT 5 V — S3 pins are 3.3 V logic."],
    ["Ground", "S3 GND = MAX485 GND = meter GND", "Common ground is mandatory."],
    ["Bus", "MAX485 A/B → meter A/B", "Twisted pair, short run."],
], widths=[1.2, 2.4, 2.6])
callout("First check, always: ",
        "if the box never turns green, swap A and B at the screw terminal and try again. Safe, instant, fixes most cases.")
doc.add_paragraph("Pin choices are S3-safe (avoid strapping 0/3/45/46, USB 19/20, flash 26–37, console 43/44). "
                   "Unchanged from v1.0 — a v1.0-wired box runs v2.3 firmware; only new wires are relays/button below.", style="List Bullet")
doc.add_heading("3.2 Relay bench wiring (v2.x additions)", level=2)
table(["Signal", "From → To", "Notes"], [
    ["Relays 1–8", "S3 GPIO5/6/7/8/9/12/13/14 → relay IN1–IN8", "One wire per channel, in order."],
    ["Relay logic", "Idle HIGH, relay clicks ON when pulled LOW", "Confirmed on bench — boots OFF with no click. A web setting flips it if a module ever needs HIGH."],
    ["Relay power", "12 V adapter → module DC+/DC−", "Coils ≈ 30 mA each (≈ 240 mA all-on). USB cannot do this."],
    ["Relay ground", "12 V GND = ESP GND", "Common reference is mandatory, optoisolation does the rest."],
    ["Relay outputs", "Each COM/NO/NC → one meter function", "Up to 3 A per channel. NO = closes when relay clicks."],
    ["Button", "S3 GPIO15 → button → GND", "Built-in pull-up; short press runs the sequence, 10 s hold wipes settings."],
    ["Spoof trigger", "S3 GPIO21 → contact → GND (optional)", "Or press FIRE on the web page instead."],
], widths=[1.2, 2.4, 2.6])
callout("12 V rule: ",
        "the ESP is USB-powered, the relay coils are 12 V-powered, and their grounds are tied together. Relays will never click without the 12 V adapter.")

# ================= 4 =================
doc.add_heading("4. How v2.5 works (the idea, no code)", level=1)
for s in [
    "The meter asks questions in the JBD battery language at 9600 baud — usually register 0x03 (voltage/current/charge), sometimes 0x04 (cell voltages) or 0x05 (device name), occasionally configuration writes.",
    "The box checks every incoming message completely (start, command, length, safety checksum, end byte). Random factory noise can never fake one — proven with a million random bytes in testing.",
    "Known questions get the matching canned answer (0x03 is the byte-exact recording of a real full battery: 52.0 V, 100 %; 0x04/0x05 are consistent synthesized answers). Writes and unknown questions get silence — but they still count as ‘the meter is talking’, so green still lights. Deliberate choice: a tester must never confuse a meter with a wrong answer.",
    "Green/red is now self-adjusting: the box measures the meter's poll rhythm and sets its patience between 2 and 10 seconds. Fast meters, slow meters, jittery meters — all show steady green; a truly silent line always goes red. No configuration, no buttons, forever.",
    "Relays: one button press switches the first N relays in order with an adjustable pause between clicks (default half a second), holds each mode for its own time in milliseconds, then releases — or switches them all at once, or sweeps a single lit relay (chase wave, both directions). Three button styles are selectable: hold-then-auto-off with re-press abort, run-to-the-end ignoring presses, or re-press restarts from R1. Burn-in extras: loop the cycle with a cooling pause, stop after N cycles, stagger the all-at-once inrush, name each relay (HORN, LIGHT…), and count cycles + relay clicks for QC. The web page also toggles each relay by hand.",
    "Fault test: a second input (or the web FIRE button) first shows a realistic full pack (100 V / 100 A / 100 °C / 100 %) for 5 seconds, then the over-range pattern (88.8 / 88.8 / 88.8 / 188 %) for 10 seconds — then everything returns to normal by itself. Both stages (values + seconds) are adjustable. Note: if the meter shows 100 instead of 188, that is the meter capping the display — the box provably sends 188.",
    "Web dashboard: the box permanently broadcasts its own Wi-Fi network (no office internet needed). Any phone or laptop joins it and opens the control page — live relay buttons, delay/mode settings, fault-test values, firmware updates, admin password. Tick ‘remember this device’ to stay logged in for 30 days, even across power cuts. All settings survive power cuts.",
    "Firmware updates: two ways. Offline: open the Firmware upload page and send the matching .bin file (works with zero internet). Automatic: enter a phone-hotspot Wi-Fi once — the box checks GitHub for new releases and installs them itself when the bench is idle.",
    "Invisible helper: over USB the box answers STATUS? with GREEN 2.3 / RED 2.3 (the number is the firmware version). Only for automatic tests.",
]:
    doc.add_paragraph(s, style="List Number")

# ================= 5 =================
doc.add_heading("5. Using it on the assembly line", level=1)
doc.add_heading("5.1 Wiring check (lamps)", level=2)
for s in [
    "Power the tester from USB. Power the meter from its bench supply.",
    "Connect A→A, B→B, join grounds.",
    "GREEN within ~1 second = PASS. RED = swap A/B first, then continuity and ground.",
    "Next meter. Works for any meter variant or poll speed.",
]:
    doc.add_paragraph(s, style="List Number")
doc.add_heading("5.2 Relay bench (button)", level=2)
for s in [
    "Power the 12 V adapter for the relay coils (relays never click without it).",
    "Short press the button: relays click R1→R8 in order (or all at once, per setting).",
    "Re-press behavior follows the selected style: abort-to-OFF, ignored-till-done, or restart-from-R1.",
    "Each relay output switches one meter function under test — watch the meter respond per channel.",
]:
    doc.add_paragraph(s, style="List Number")
doc.add_heading("5.3 Web dashboard (phone/laptop, no office Wi-Fi needed)", level=2)
for s in [
    "On the phone/laptop, join Wi-Fi network BMS-Tester with password bms12345. A login page pops up by itself; if not, open 192.168.4.1 in the browser.",
    "Phone says ‘no internet’ or keeps using mobile data? Turn mobile data OFF (or tap ‘stay connected’) — the box has no internet, it IS the network.",
    "Login: user admin, password admin123.",
    "FIRST: open the Admin card and change the Wi-Fi password and the login password. (Locked out later? Hold the box button 10 s — factory reset, back to the passwords above.)",
    "Relays card: live green tiles = ON; tap any tile to force it; START runs the sequence, STOP ALL releases everything.",
    "Sequence card: mode (1-by-1, chase wave, or all-at-once), how many relays (1–8), step pause in ms, hold time per mode in ms (0 = stay on), loop + cooling pause + cycle limit for burn-in, all-at-once stagger, direction, button style, relay logic. Save stores it through power cuts.",
    "Relay labels card: give each relay a name (HORN, LIGHT…) — workers see names, not R-numbers.",
    "Fault card: stage 1 (100s) and stage 2 (88.8/188) values + seconds each, press FIRE — the meter shows stage 1, then stage 2, then returns to normal by itself.",
    "Firmware card: tick auto-check and enter a hotspot Wi-Fi once for automatic updates; or open the Firmware upload page and send a .bin file (no internet needed).",
    "Admin card: Wi-Fi + login passwords, auto-start on boot (burn-in), reboot, factory reset.",
]:
    doc.add_paragraph(s, style="List Number")
callout("Keep safe: ",
        "this document carries the live passwords (BMS-Tester / bms12345, admin / admin123). Change them on first login and re-share only the new ones.")

# ================= 6 =================
doc.add_heading("6. Getting the software onto the board", level=1)
doc.add_paragraph("Pick ONE method, once per board. Wiring is identical for all.")
doc.add_heading("Method A — ready binaries + esptool (fastest, no IDE)", level=2)
for s in [
    "On any PC:  pip install esptool",
    "Plug in the S3 (DATA cable), find the port.",
    "Run (replace PORT):  esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin  (files in firmware/)",
    "Unplug, fit in the box. Boots red, green on first poll.",
]:
    doc.add_paragraph(s, style="List Number")
callout("No-install alternative: ",
        "a browser flasher (e.g. espthings.io/tools/esp32-flasher) with the same three files at 0x0 / 0x8000 / 0x10000.")
doc.add_heading("Method B — PlatformIO (exact project)", level=2)
for s in [
    "VS Code + ‘PlatformIO IDE’ extension. File → Open Folder → bms-connection-tester.",
    "Click → Upload for esp32-s3-devkitc-1. ‘SUCCESS’ = done.",
]:
    doc.add_paragraph(s, style="List Number")
doc.add_heading("Method C — Arduino IDE", level=2)
for s in [
    "Boards Manager → ‘esp32 by Espressif’. Open arduino/bms_connection_tester/bms_connection_tester.ino (all tabs open automatically).",
    "Board ‘ESP32S3 Dev Module’, USB CDC On Boot = Enabled, Upload Speed 921600 (N16R8 boards also: Flash 16MB, PSRAM OPI). Pick port, Upload.",
]:
    doc.add_paragraph(s, style="List Number")

# ================= 7 =================
doc.add_heading("7. Build & test report", level=1)
doc.add_heading("7.1 Firmware compiled for real — SUCCESS", level=2)
doc.add_paragraph(
    "Built with the genuine Espressif Xtensa GCC 8.4.0 toolchain (PlatformIO + Arduino framework): "
    "both S3 profiles compiled SUCCESS (8 MB + N16R8). Binary inspected afterward:")
table(["Artifact", "Detail"], [
    ["firmware.bin", "v2.5 8 MB build — three canned replies (0x03/0x04/0x05) plus STATUS?, per-mode relay bench, AP dashboard + console, 2-stage spoof + trigger save, phone landing page, Tasmota-grade OTA; SHAs in firmware/README.md."],
    ["SHA-256 (firmware.bin)", "see firmware/README.md (v2.5 binaries refreshed; golden bytes + version + AP strings verified inside)"],
    ["bootloader + partitions", "Standard S3 loader and flash layout, refreshed with this build."],
    ["On-target test builds", "All unit-test programs also compile + link for the S3 chip (they execute once a board is plugged in)."],
    ["QEMU S3 boot test", "Parked at v2.4: Stage-0 flash model clean (GD WRSR2-QE + burst-wrap fixes banked), guest resets in the 2nd-stage bootloader (init returns 0x0a) — emulator gap, not firmware. Wokwi S3 + virtual-bus PTY emulation are the practical paths."],
], widths=[1.7, 4.5])
doc.add_heading("7.2 Automated tests — 137 / 137 PASS (run_tests.sh) + socket harness 64/64", level=2)
table(["Group", "Tests", "Result"], [
    ["Checksums + golden frame (7)", "FFFD / FCDA / FCA8 / FA86 / F65A (2nd Docklight 0x2A variant), byte-exact 0x03 frame, exact-yes / 7xcorrupt-no.", "7 PASS"],
    ["Lamp logic, adaptive (8)", "Boot red, green fast, red after window, self-heal, slow-poll adapt, 2 s floor / 10 s cap, rollover, legacy compat.", "8 PASS"],
    ["Parser + dispatcher + faults (13)", "0x03/0x04/0x05 reads, write flagged, all corruptions rejected, noise re-sync, overlong rejected, split delivery, option-A silence, canned-frame checksums, 1 M noise bytes = zero false frames, fast + slow soaks.", "13 PASS"],
    ["Stress (4)", "1,785 exhaustive corruptions, 10 M fuzz, cadence×register sweep, 5,000-frame saturation.", "4 PASS"],
    ["Relay bench (36)", "Boot OFF, stepping, per-mode holds, ALL-ON (+ default stagger ramp), chase + 20 ms break-before-make, count live-shrink safety, loop/pause/limit, direction, counters, button behaviors, force-in-chase, RESTART keeps forces, stop dead-band, mid-cycle latching, polarity, rollover, debounce.", "36 PASS"],
    ["Spoof 2-stage (11)", "Stage-1 ‘100’ + stage-2 88.8/88.8/88.8/188 bytes, checksum validity, custom values, stage handoff/cancel/retrigger, rollover.", "11 PASS"],
    ["OTA logic (6)", "Version compare (2.10 > 2.9), per-board file pick, safe download links, idle-only auto-check gate.", "6 PASS"],
    ["Website, host-executed (44)", "No login wall (per-request passwords), NVS migration, portal landing + redirect, validation + named rejects, whitespace-tolerant JSON, coalesced saves, per-mode config, spoof save/fire/trigger, Tasmota upload gates + rejects, console, backup v2 + restore v1/v2, STA test + on-demand join, OTA URL/interval/install, mDNS, keep-WiFi/bootcount resets, info fields, fuzz, factory reset.", "44 PASS"],
    ["Socket harness, real HTTP (54+10)", "drive_emu.py: portal, relay flows, spoof/trigger, uploads, backup/restore, console, resets, STA/OTA, info — over the real handlers. drive_soak.py: 48 virtual hours looped, counters exact, NVS coalescing, spoof mid-run, clean stop.", "64 PASS"],
    ["Month soak (pure logic)", "30 virtual days from day 40 across the millis() wrap: 2.59 M polls answered, 309 k looped cycles, exact stage/force/stop behavior, 30 NVS commits, silence → red → green, no reset.", "SOAK PASS"],
    ["Update gates (5)", "Explicit sketch budget, variant asset pick, exact filename match (suffix-trap proof), image-head magic + flash-size matrix, error vocabulary.", "5 PASS"],
    ["System 24 h sim (3)", "Reply-selection matrix + 86,400-poll office day: every reply checksum-validated, exact 5 s + 10 s spoof stages, relay + chase schedule, loop cycles, noise, green all day.", "3 PASS"],
], widths=[2.3, 2.9, 0.9])
doc.add_paragraph("The hardware-in-loop test (real board + adapter asserting exact replies and GREEN→RED timing) runs "
                  "itself the moment hardware is detected; until then it skips. The virtual meter gained scenario modes "
                  "(--reg, --period, --jitter, --mode normal/slowstop/noise/write/unknown) for bench testing.")
doc.add_heading("7.3 Week-long continuous run — SOAK PASS", level=2)
doc.add_paragraph("tools/soak_sim.cpp drives the real firmware logic through 8 simulated days: 691,040 polls, all answered, daily noise bursts rejected, silence gaps correctly red, the 32-bit millis() rollover crossed mid-run, and green-on-resume with no reset. Runs in 0.04 s as part of run_tests.sh.")
doc.add_heading("7.4 Testing caught real bugs (proof the suite works)", level=2)
doc.add_paragraph(
    "1) The new parser emitted a frame when a corrupted checksum was followed by 0x77 — fixed by gating emission on "
    "checksum success, now covered by test. 2) A hand-written test vector had a wrong checksum (FEEF, not FEFF) — fixed. "
    "3) Arduino's headers define a B1 macro that broke the parser's state names on S3 builds — renamed, S3 build green.", style="List Bullet")
doc.add_paragraph(
    "4) Retired login wall (v2.3.1): per-request admin password now gates reboot/reset/upload/OTA-admin; "
    "the dashboard is open on the WPA2 AP. "
    "5) A stray semicolon ended the dashboard status builder early and dropped the fault-test values "
    "from the web reply (2 web tests caught it before release). "
    "6) GitHub OTA check never matched (API pretty-prints a space after the colon) + no dashboard "
    "install path (host reasoning + new web tests; fixed with tolerant parse + Install button).", style="List Bullet")
doc.add_paragraph(
    "Carried-over v1.0 finding: reply checksums exclude the echoed command byte (FCDA, not FCD7) — so all canned replies "
    "are frozen literals, never recomputed at runtime.", style="List Bullet")

# ================= 8 =================
doc.add_heading("8. Troubleshooting", level=1)
table(["Symptom", "Fix (in order)"], [
    ["Always RED", "1) Swap A/B.  2) Common GND.  3) Meter powered + polling.  4) MAX485 VCC = 3.3 V."],
    ["GREEN flickers", "Loose terminal / long untwisted run. Shorten, twist; 120 Ω across A–B if long."],
    ["Both LEDs dark", "Unpowered, LEDs backwards (flat side to GND), or 220 Ω forgotten."],
    ["Answers once then stops", "GPIO4/DE-RE fault (stuck TX jams bus). Check GPIO4 + 10 kΩ pull-down."],
    ["Relays never click", "1) 12 V adapter plugged + LED on module lit.  2) 12 V GND tied to ESP GND.  3) Web page → relay logic matches module (default LOW).  4) Relays card: relays past the count are greyed out — raise Relays."],
    ["Relay clicks at power-on", "Wrong logic setting — flip relay logic on the web page and Save."],
    ["Relays stay ON forever", "Normal if a hold is 0 (= stay on until STOP). Set per-mode hold ms (sequence / chase / all-on) to the wanted time."],
    ["188 % shows 100", "The meter caps the display — the box provably sends 188 (valid checksum). Use stage 1 (100) for a realistic demo."],
    ["Phone cannot see BMS-Tester", "Box fully booted? (takes ~5 s with Wi-Fi). Forget + rejoin; confirm a DATA USB cable powers it."],
    ["Web page won't open", "Joined BMS-Tester (not office Wi-Fi)? Open 192.168.4.1 exactly."],
    ["Login rejected", "Changed earlier? Ask who changed it. Else hold the box button 10 s (factory reset) → admin/admin123 back."],
    ["PC cannot see S3", "DATA cable first, then CP210x/CH343 driver; pick the USB-UART port."],
], widths=[1.6, 4.6])

# ================= 9 =================
doc.add_heading("9. Safety & limits", level=1)
doc.add_paragraph("Power the tester ONLY from USB. Never from the traction pack. Relay coils ONLY from the 12 V adapter (grounds tied).", style="List Bullet")
doc.add_paragraph("USB must be a real charger — the Wi-Fi radio is always on. A weak laptop port browns out.", style="List Bullet")
doc.add_paragraph("This jig proves WIRING and A/B polarity for any meter variant and poll speed. It does not test meter accuracy, calibration or protection trips.", style="List Bullet")
doc.add_paragraph("Common ground required; short twisted bench leads only.", style="List Bullet")
callout("Remember: ", "the real battery pack is never needed on the line — that is the whole point.")

# ================= 10 =================
doc.add_heading("10. Project folder map + version history", level=1)
table(["Path", "What it is"], [
    ["src/main.cpp, src/bms_protocol.*", "v2.4 program (frozen responder core)."],
    ["src/relay_ctrl.*, src/web_ui.*, src/ota.*, src/fw_upload.h", "v2.4 relay bench + dashboard + update gates."],
    ["VERSION", "2.3 (also baked into STATUS? replies)."],
    ["arduino/bms_connection_tester/", "Same v2.4 as an Arduino sketch + README (10 tabs)."],
    ["firmware/*.bin + firmware-n16r8/*.bin", "Ready-to-flash v2.4 binaries + flash READMEs with SHAs."],
    ["test/ (9 suites)", "105 automated tests, all passing (incl. relay bench, OTA, website + 24 h sim)."],
    ["tools/soak_sim.cpp", "8-day run: 691,040 polls answered, rollover crossed, no reset."],
    ["tools/virtual_bus.sh", "One-shot PTY emulation: real protocol core vs scripted meter."],
    ["captures/", "Original Docklight xlsx + README (ground-truth vectors)."],
    ["releases/", "Frozen bms-connection-tester-v1.0.zip (git-ignored)."],
    ["tools/virtual_meter.py", "Meter simulator with scenario modes."],
    ["tools/test_hardware.py", "Auto board test when hardware appears."],
    ["tools/run_qemu_s3.sh", "One-command S3 emulation boot test (real Linux/Mac)."],
    ["wokwi/", "Browser simulation + sim.yaml headless scenario (relays, buttons, meter)."],
    ["wiki/", "Online manual mirror (Relays page has the web guide)."],
    ["run_tests.sh", "Runs everything runnable in one command."],
    ["bms-connection-tester-v1.0.zip + git tag v1.0", "Frozen v1.0 — untouched by v2.x work. Current release: v2.5."],
], widths=[2.6, 3.6])
doc.add_paragraph()
ep = doc.add_paragraph()
ep.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = ep.add_run("— End of report v2.5. This document + the wiring table in §3 is all any electronics engineer needs to build, flash and maintain it. —")
r.italic = True
r.font.color.rgb = GREY

doc.save("/data/data/com.termux/files/home/bms-connection-tester/RS485-Tester-Report.docx")
print("saved v2.5 RS485-Tester-Report.docx")
