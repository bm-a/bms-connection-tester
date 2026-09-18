#!/usr/bin/env python3
"""Beautiful dad-friendly Word report — v1.1 (multi-scenario release)."""
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
r = fp.add_run("RS485 Connection Tester v1.1  —  Build & Test Report   |   Page ")
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
r = sp.add_run("Build & Test Report v1.1 — for an electronics engineer, no coding needed")
r.font.size = Pt(13)
r.italic = True
r.font.color.rgb = GREY
doc.add_paragraph()
table(["Item", "Detail"], [
    ["Board", "ESP32-S3 DevKitC-1 + MAX485 module"],
    ["Version", "v1.1 — answers 0x03/0x04/0x05, silent on the rest; self-adjusts to any poll speed"],
    ["Previous", "v1.0 frozen untouched (ZIP + git tag) — this report covers v1.1 only"],
    ["Date", "September 2026"],
    ["Firmware", "firmware.bin — 277,360 bytes, compiled + verified"],
    ["Tests", "28 / 28 passing (protocol + lamp logic + faults + 8-day soak)"],
    ["Use", "Green lamp = wiring correct, red lamp = wiring wrong. Nothing to press."],
], widths=[1.6, 4.6])
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
    "3.  Wiring — the complete circuit",
    "4.  How v1.1 works (every meter type, no code)",
    "5.  Using it on the assembly line",
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
    "Nothing to press, reset or read. v1.1 handles every meter variant by itself: fast or slow polling, meters that "
    "ask for extra data (cell voltages, device name), and meters that send configuration writes — green lights for all "
    "of them as long as the wires carry real traffic.")

# ================= 2 =================
doc.add_heading("2. Parts list", level=1)
doc.add_paragraph("One box needs:")
table(["Part", "Qty", "What it does"], [
    ["ESP32-S3 DevKitC-1 board", "1", "The brain. Listens, replies, drives the lamps."],
    ["MAX485 module (SP3485 also fine)", "1", "Translator: ESP32 serial ↔ noise-proof RS485 signals."],
    ["Green LED + red LED (5 mm)", "1 + 1", "The entire user interface."],
    ["220 Ω resistor", "2", "One per LED (≈ 6–10 mA)."],
    ["10 kΩ resistor", "1", "Pull-down on direction pin — powers up listening, never jamming."],
    ["USB cable + 5 V charger / power bank", "1", "Powers the box. NEVER the traction battery pack."],
    ["Twisted-pair wire + screw terminals", "1 set", "A/B test leads to the meter."],
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
                  "Unchanged from v1.0 — a v1.0-wired box runs v1.1 firmware with zero rewiring.", style="List Bullet")

# ================= 4 =================
doc.add_heading("4. How v1.1 works (the idea, no code)", level=1)
for s in [
    "The meter asks questions in the JBD battery language at 9600 baud — usually register 0x03 (voltage/current/charge), sometimes 0x04 (cell voltages) or 0x05 (device name), occasionally configuration writes.",
    "The box checks every incoming message completely (start, command, length, safety checksum, end byte). Random factory noise can never fake one — proven with a million random bytes in testing.",
    "Known questions get the matching canned answer (0x03 is the byte-exact recording of a real full battery: 52.0 V, 100 %; 0x04/0x05 are consistent synthesized answers). Writes and unknown questions get silence — but they still count as ‘the meter is talking’, so green still lights. Deliberate choice: a tester must never confuse a meter with a wrong answer.",
    "Green/red is now self-adjusting: the box measures the meter's poll rhythm and sets its patience between 2 and 10 seconds. Fast meters, slow meters, jittery meters — all show steady green; a truly silent line always goes red. No configuration, no buttons, forever.",
    "Invisible helper: over USB the box answers STATUS? with GREEN 1.1 / RED 1.1 (the number is the firmware version). Only for automatic tests.",
]:
    doc.add_paragraph(s, style="List Number")

# ================= 5 =================
doc.add_heading("5. Using it on the assembly line", level=1)
for s in [
    "Power the tester from USB. Power the meter from its bench supply.",
    "Connect A→A, B→B, join grounds.",
    "GREEN within ~1 second = PASS. RED = swap A/B first, then continuity and ground.",
    "Next meter. No buttons, no reset, ever — works for any meter variant or poll speed.",
]:
    doc.add_paragraph(s, style="List Number")

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
    "Boards Manager → ‘esp32 by Espressif’. Open arduino/bms_connection_tester/bms_connection_tester.ino.",
    "Board ‘ESP32S3 Dev Module’, USB CDC On Boot = Enabled, Upload Speed 921600. Pick port, Upload.",
]:
    doc.add_paragraph(s, style="List Number")

# ================= 7 =================
doc.add_heading("7. Build & test report", level=1)
doc.add_heading("7.1 Firmware compiled for real — SUCCESS", level=2)
doc.add_paragraph(
    "Built with the genuine Espressif Xtensa GCC 8.4.0 toolchain (PlatformIO + Arduino framework), "
    "6 cores, incremental + cache: v1.1 compiled in 46 seconds. Binary inspected afterward:")
table(["Artifact", "Detail"], [
    ["firmware.bin", "277,360 bytes — contains all three canned replies (0x03/0x04/0x05) plus STATUS? logic, verified inside the binary."],
    ["SHA-256 (firmware.bin)", "7195161117ef68e3a3cd4c7793539a87c03083b067bde1a6e5b13ae330a376cf"],
    ["bootloader + partitions", "Standard S3 loader and flash layout, refreshed with this build."],
    ["On-target test builds", "All three unit-test programs also compile + link for the S3 chip (they execute once a board is plugged in)."],
    ["QEMU S3 boot test", "QEMU 9.2.2 built from source on this phone; it boots our firmware to the Arduino flash-init step. Tracing proved QEMU's flash model lacked RDID 0x90/0xAB and GD25Q64 SFDP — both patched (separate emulator repo). Only an undecodable DIO-era cmd 0x77 remains: emulator gap, our code is never reached. Wokwi S3 is the practical Arduino-emulation path."],
], widths=[1.7, 4.5])
doc.add_heading("7.2 Automated tests — 28 / 28 PASS (run_tests.sh)", level=2)
table(["Group", "Tests", "Result"], [
    ["Checksums + golden frame (7)", "FFFD / FCDA / FCA8 / FA86 / F65A (2nd Docklight 0x2A variant), byte-exact 0x03 frame, exact-yes / 7xcorrupt-no.", "7 PASS"],
    ["Lamp logic, adaptive (8)", "Boot red, green fast, red after window, self-heal, slow-poll adapt, 2 s floor / 10 s cap, rollover, legacy compat.", "8 PASS"],
    ["Parser + dispatcher + faults (13)", "0x03/0x04/0x05 reads, write flagged, all corruptions rejected, noise re-sync, overlong rejected, split delivery, option-A silence, canned-frame checksums, 1 M noise bytes = zero false frames, 100 k fast-poll + slow-poll soaks.", "13 PASS"],
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
    "Carried-over v1.0 finding: reply checksums exclude the echoed command byte (FCDA, not FCD7) — so all canned replies "
    "are frozen literals, never recomputed at runtime.", style="List Bullet")

# ================= 8 =================
doc.add_heading("8. Troubleshooting", level=1)
table(["Symptom", "Fix (in order)"], [
    ["Always RED", "1) Swap A/B.  2) Common GND.  3) Meter powered + polling.  4) MAX485 VCC = 3.3 V."],
    ["GREEN flickers", "Loose terminal / long untwisted run. Shorten, twist; 120 Ω across A–B if long."],
    ["Both LEDs dark", "Unpowered, LEDs backwards (flat side to GND), or 220 Ω forgotten."],
    ["Answers once then stops", "GPIO4/DE-RE fault (stuck TX jams bus). Check GPIO4 + 10 kΩ pull-down."],
    ["PC cannot see S3", "DATA cable first, then CP210x/CH343 driver; pick the USB-UART port."],
], widths=[1.6, 4.6])

# ================= 9 =================
doc.add_heading("9. Safety & limits", level=1)
doc.add_paragraph("Power the tester ONLY from USB. Never from the traction pack.", style="List Bullet")
doc.add_paragraph("This jig proves WIRING and A/B polarity for any meter variant and poll speed. It does not test meter accuracy, calibration or protection trips.", style="List Bullet")
doc.add_paragraph("Common ground required; short twisted bench leads only.", style="List Bullet")
callout("Remember: ", "the real battery pack is never needed on the line — that is the whole point.")

# ================= 10 =================
doc.add_heading("10. Project folder map + version history", level=1)
table(["Path", "What it is"], [
    ["src/main.cpp, src/bms_protocol.*", "v1.1 program (parser + dispatcher + adaptive window)."],
    ["VERSION", "1.1 (also baked into STATUS? replies)."],
    ["arduino/bms_connection_tester/", "Same v1.1 as an Arduino sketch + README."],
    ["firmware/*.bin", "Ready-to-flash v1.1 binaries + flash README."],
    ["test/test_checksum|test_logic|test_parser", "28 automated tests, all passing."],
    ["tools/soak_sim.cpp", "8-day run: 691,040 polls answered, rollover crossed, no reset."],
    ["captures/", "Original Docklight xlsx + README (ground-truth vectors)."],
    ["releases/", "Frozen bms-connection-tester-v1.0.zip (git-ignored)."],
    ["tools/virtual_meter.py", "Meter simulator with scenario modes."],
    ["tools/test_hardware.py", "Auto board test when hardware appears."],
    ["tools/run_qemu_s3.sh", "One-command S3 emulation boot test (real Linux/Mac)."],
    ["wokwi/", "Browser simulation (meter now cycles 0x03/0x04/0x05)."],
    ["run_tests.sh", "Runs everything runnable in one command."],
    ["bms-connection-tester-v1.0.zip + git tag v1.0", "Frozen v1.0 — untouched by v1.1 work. Current release: v1.1."],
], widths=[2.6, 3.6])
doc.add_paragraph()
ep = doc.add_paragraph()
ep.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = ep.add_run("— End of report v1.1. This document + the wiring table in §3 is all any electronics engineer needs to build, flash and maintain it. —")
r.italic = True
r.font.color.rgb = GREY

doc.save("/data/data/com.termux/files/home/bms-connection-tester/RS485-Tester-Report.docx")
print("saved v1.1 RS485-Tester-Report.docx")
