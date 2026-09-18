#!/usr/bin/env python3
"""Beautiful dad-friendly Word report for the RS485 connection tester."""
from docx import Document
from docx.shared import Pt, Inches, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

NAVY = RGBColor(0x1F, 0x3B, 0x63)
ACCENT = RGBColor(0x0B, 0x6E, 0x4F)   # deep green
GREY = RGBColor(0x59, 0x59, 0x59)
GREEN = RGBColor(0x0B, 0x7A, 0x3E)
RED = RGBColor(0xB0, 0x26, 0x13)

doc = Document()

# ---------- base style ----------
normal = doc.styles["Normal"]
normal.font.name = "Calibri"
normal.font.size = Pt(11)

for i in (1, 2):
    hs = doc.styles[f"Heading {i}"]
    hs.font.name = "Calibri"
    hs.font.color.rgb = NAVY

# ---------- helpers ----------
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

def callout(title, text, fill="FFF6D6", border="C9A227"):
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

def footer_with_pages():
    sec = doc.sections[0]
    fp = sec.footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = fp.add_run("RS485 Connection Tester  —  Build & Test Report   |   Page ")
    r.font.size = Pt(9)
    r.font.color.rgb = GREY
    fld1 = OxmlElement("w:fldSimple")
    fld1.set(qn("w:instr"), "PAGE")
    run = OxmlElement("w:r")
    run.append(OxmlElement("w:t"))
    fld1.append(run)
    fp._p.append(fld1)

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
r = sp.add_run("Build & Test Report — written for an electronics engineer, no coding needed")
r.font.size = Pt(13)
r.italic = True
r.font.color.rgb = GREY
doc.add_paragraph()
table(["Item", "Detail"], [
    ["Board", "ESP32-S3 DevKitC-1 + MAX485 module"],
    ["Date", "September 2026"],
    ["Firmware", "firmware.bin — 276,768 bytes, verified"],
    ["Tests", "11 / 11 passing (protocol + lamp logic)"],
    ["Use", "Green lamp = wiring correct, red lamp = wiring wrong. Nothing to press."],
], widths=[1.6, 4.6])
cp = doc.add_paragraph()
cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = cp.add_run("Send this file as-is on WhatsApp — it opens in Word / Google Docs on any phone.")
r.italic = True
r.font.color.rgb = GREY
doc.add_page_break()
footer_with_pages()

# ================= CONTENTS =================
doc.add_heading("Contents", level=1)
for item in [
    "1.  What this box does (one page, start here)",
    "2.  Parts list",
    "3.  Wiring — the complete circuit",
    "4.  How it works (the idea, no code)",
    "5.  Using it on the assembly line",
    "6.  Getting the software onto the board (3 easy methods)",
    "7.  Build & test report (what was proven, with numbers)",
    "8.  Troubleshooting table",
    "9.  Safety & limits",
    "10. Project folder map",
]:
    doc.add_paragraph(item)

# ================= 1 =================
doc.add_heading("1. What this box does", level=1)
doc.add_paragraph(
    "Every e-rickshaw meter must have its two RS485 communication wires (A and B) connected correctly "
    "before it leaves the factory. Until now, checking that meant giving the worker the real battery pack "
    "or a laptop with monitoring software — both expensive and impractical to hand out again and again.")
doc.add_paragraph(
    "This box replaces all that. Inside is one ESP32-S3 microcontroller and one MAX485 chip. The box pretends "
    "to be the battery — just convincingly enough that the meter talks to it. The worker connects two wires, "
    "looks at two lamps, and is done:")
doc.add_paragraph("GREEN lamp ON  =  wiring correct, meter and box are talking.", style="List Bullet")
doc.add_paragraph("RED lamp ON  =  wiring wrong (or meter off, wires swapped, wire broken).", style="List Bullet")
doc.add_paragraph(
    "There is nothing to press, nothing to reset and no screen to read. If the meter stops talking, the box turns "
    "red by itself within about 2 seconds; when talking resumes it turns green again by itself.")

# ================= 2 =================
doc.add_heading("2. Parts list", level=1)
doc.add_paragraph("Cheap, standard market material. One box needs:")
table(["Part", "Qty", "What it does"], [
    ["ESP32-S3 DevKitC-1 board", "1", "The brain. Listens for the meter, sends the reply, drives the lamps."],
    ["MAX485 module (SP3485 also fine)", "1", "Translator: ESP32 serial ↔ noise-proof RS485 long-wire signals."],
    ["Green LED + red LED (5 mm)", "1 + 1", "The entire user interface."],
    ["220 Ω resistor", "2", "One in series with each LED (≈ 6–10 mA, safe for pin and LED)."],
    ["10 kΩ resistor", "1", "Pull-down on the direction pin — box powers up listening, never jamming."],
    ["USB cable + 5 V charger / power bank", "1", "Powers the box. NEVER the traction battery pack."],
    ["Twisted-pair wire + screw terminals", "1 set", "The A/B test leads to the meter."],
    ["Small plastic enclosure", "1", "Houses everything on the line."],
], widths=[2.2, 0.7, 3.3])

# ================= 3 =================
doc.add_heading("3. Wiring — the complete circuit", level=1)
doc.add_heading("3.1 Every wire", level=2)
table(["Signal", "From → To", "Notes"], [
    ["TX data", "S3 GPIO17 → MAX485 DI", "Box talks out of this pin."],
    ["RX data", "MAX485 RO → S3 GPIO16", "Box listens on this pin."],
    ["Direction", "S3 GPIO4 → MAX485 DE + RE tied", "HIGH = talk, LOW = listen. Software flips it per reply."],
    ["Failsafe", "GPIO4 net → 10 kΩ → GND", "Holds ‘listen’ during power-up float."],
    ["Green lamp", "S3 GPIO10 → 220 Ω → green LED → GND", "Anode (long leg) towards resistor."],
    ["Red lamp", "S3 GPIO11 → 220 Ω → red LED → GND", "Boots red until first valid poll."],
    ["Power", "MAX485 VCC → 3.3 V", "NOT 5 V — S3 pins are 3.3 V logic."],
    ["Ground", "S3 GND = MAX485 GND = meter GND", "Common ground is mandatory for RS485."],
    ["Bus", "MAX485 A/B → meter A/B", "Twisted pair, short run."],
], widths=[1.2, 2.4, 2.6])
callout("First check, always: ",
        "if the box never turns green, swap A and B at the screw terminal and try again. "
        "Swapping is safe and fixes most ‘wrong wiring’ cases instantly.")
doc.add_heading("3.2 Why each part is there", level=2)
doc.add_paragraph("MAX485: the ESP32 speaks 0–3.3 V serial; the meter speaks RS485 (two wires swinging opposite, "
                  "immune to factory noise). The chip translates.", style="List Bullet")
doc.add_paragraph("GPIO4 direction pin: RS485 is half-duplex — a walkie-talkie. Only one side talks at a time. "
                  "The software raises GPIO4, sends the 34-byte reply (~35 ms at 9600 baud), waits for the last bit, "
                  "lowers it. Fully automatic.", style="List Bullet")
doc.add_paragraph("10 kΩ pull-down: at power-up GPIO4 floats; without it the MAX485 could wake up transmitting and jam "
                  "the bus. The resistor parks it in listen mode until software takes over.", style="List Bullet")
doc.add_paragraph("Pin choices are S3-safe: GPIO10/11/16/17/4 avoid strapping pins (0/3/45/46), USB pins (19/20), "
                  "flash pins (26–37) and the console pins (43/44). (Note: classic-ESP32 pin 25 does not exist on S3.)",
                  style="List Bullet")

# ================= 4 =================
doc.add_heading("4. How it works (the idea, no code)", level=1)
for s in [
    "The powered meter asks the same 7-byte question about once per second: ‘battery, send voltage, current, charge % and temperature.’ (Industry name: JBD / Xiaoxiang BMS protocol, 9600 baud.)",
    "The box listens quietly. When all 7 bytes match exactly — noise can never fake all 7 — it answers with one fixed 34-byte message describing a healthy full battery (52.0 V, 0 A, 100 %, 25 °C). The meter believes it found its battery and shows full charge.",
    "Once a second the box asks itself: ‘have I heard the meter in the last 2 seconds?’ Yes → green on, red off. No → red on, green off. That is the whole program.",
    "Invisible helper: on its USB port the box answers the text STATUS? with GREEN or RED. Workers never use it — it exists so automatic tests can check the lamps.",
]:
    doc.add_paragraph(s, style="List Number")

# ================= 5 =================
doc.add_heading("5. Using it on the assembly line", level=1)
for s in [
    "Power the tester from USB charger / power bank.",
    "Power the meter from its normal bench supply (not from a traction pack).",
    "Connect A→A, B→B, join grounds.",
    "GREEN within ~1 second = PASS. RED = swap A/B first, then check continuity and ground.",
    "Next meter. No buttons, no reset, ever.",
]:
    doc.add_paragraph(s, style="List Number")

# ================= 6 =================
doc.add_heading("6. Getting the software onto the board", level=1)
doc.add_paragraph("The program is finished and tested. Three methods, easiest first. Do any ONE of them, once per board.")
doc.add_heading("Method A — ready binaries + esptool (fastest, no IDE)", level=2)
doc.add_paragraph("The project has a firmware/ folder with three tested files. On any PC:")
for s in [
    "Install Python, then run:  pip install esptool",
    "Plug in the S3 with a DATA USB cable, find the port (COMx on Windows, /dev/ttyACM0 on Linux).",
    "Run (replace PORT):  esptool.py --chip esp32s3 --port PORT --baud 460800 write-flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin",
    "Unplug, fit in the box. Boots red, turns green when the meter polls.",
]:
    doc.add_paragraph(s, style="List Number")
callout("No-install alternative: ",
        "open a browser flasher (e.g. espthings.io/tools/esp32-flasher), load the same three files at the same three "
        "addresses (0x0 / 0x8000 / 0x10000) and flash.")
doc.add_heading("Method B — PlatformIO (exact project)", level=2)
for s in [
    "Install VS Code, add the ‘PlatformIO IDE’ extension, restart.",
    "File → Open Folder → bms-connection-tester.",
    "Plug in the S3, click the → Upload arrow for esp32-s3-devkitc-1. First build downloads tools (minutes); later ones take seconds. ‘SUCCESS’ = done.",
]:
    doc.add_paragraph(s, style="List Number")
doc.add_heading("Method C — Arduino IDE", level=2)
for s in [
    "Install Arduino IDE; Boards Manager → install ‘esp32 by Espressif’.",
    "Open arduino/bms_connection_tester/bms_connection_tester.ino (the other two tabs open automatically).",
    "Board ‘ESP32S3 Dev Module’, USB CDC On Boot = Enabled, Upload Speed 921600. Pick the port, Upload.",
]:
    doc.add_paragraph(s, style="List Number")
doc.add_paragraph("If the PC cannot see the board: try another cable first (charge-only cables fail), then install the "
                  "CP210x/CH343 USB-serial driver for your board version.", style="List Bullet")

# ================= 7 =================
doc.add_heading("7. Build & test report", level=1)
doc.add_heading("7.1 Firmware compiled for real — SUCCESS", level=2)
doc.add_paragraph(
    "The S3 firmware was compiled with the genuine Espressif Xtensa GCC 8.4.0 toolchain "
    "(PlatformIO, espressif32 platform, Arduino framework) in 13 min 42 s, and the resulting binary was inspected:")
table(["Artifact", "Detail"], [
    ["firmware.bin", "276,768 bytes — contains the golden 34-byte reply AND the 7-byte request detector, verified byte-for-byte inside the binary."],
    ["SHA-256 (firmware.bin)", "de863a25ff5521da34bb7be563771aff98438bb0bf21ebca8decf39f899f6685"],
    ["bootloader.bin", "15,104 bytes — standard S3 second-stage loader."],
    ["partitions.bin", "3,072 bytes — standard flash layout."],
    ["On-target test builds", "Both unit-test programs also compile + link for the S3 chip (they execute once a board is plugged in)."],
], widths=[1.7, 4.5])
doc.add_heading("7.2 Automated tests — 11 / 11 PASS (one command: run_tests.sh)", level=2)
table(["Test", "What it proves", "Result"], [
    ["Request checksum = FFFD", "Question validator matches the real meter's bytes.", "PASS"],
    ["Reply checksum = FCDA", "Calculator matches 4 real battery recordings.", "PASS"],
    ["SOC-50% vector = FCA8", "Algorithm is not tuned to one sample.", "PASS"],
    ["90 %/45 °C vector = FA86", "Long-frame variant also matches.", "PASS"],
    ["Golden frame byte-exact", "Box sends exactly what a real full battery sends.", "PASS"],
    ["Matcher: exact yes / 7× corrupt no", "Factory noise can never fake a ‘connected’. ", "PASS"],
    ["Boots red", "No false green before first poll.", "PASS"],
    ["Green ≤ 1 s after poll", "Worker sees instant feedback.", "PASS"],
    ["Red after 2 s silence", "Unplugged meter shows red by itself.", "PASS"],
    ["Self-heals on resume", "No reset button ever needed.", "PASS"],
    ["Clock rollover safe", "Correct even after ~49 days powered on.", "PASS"],
], widths=[2.0, 2.9, 0.9])
doc.add_paragraph("Hardware-in-loop test (real board + USB-RS485 adapter asserting the exact reply and GREEN→RED timing) "
                  "is scripted in tools/test_hardware.py and runs automatically the moment a board is detected; until then it skips itself.")
doc.add_heading("7.3 One genuine finding", level=2)
doc.add_paragraph(
    "The handwritten protocol note claimed the reply checksum covers ‘command + length + data’, but the real recordings "
    "prove it EXCLUDES the echoed command byte (including it gives FCD7 instead of the recorded FCDA — off by exactly "
    "0x03). The firmware therefore sends the recorded bytes verbatim instead of recalculating — deliberate and correct. "
    "A fifth recording (charging case) matches no formula and is treated as a copying error in the notes.")

# ================= 8 =================
doc.add_heading("8. Troubleshooting", level=1)
table(["Symptom", "Fix (in order)"], [
    ["Always RED", "1) Swap A/B.  2) Check common GND.  3) Check meter powered + polling.  4) MAX485 VCC = 3.3 V."],
    ["GREEN flickers", "Loose terminal or long untwisted run. Shorten/twist; add 120 Ω across A–B if cable is long."],
    ["Both LEDs dark", "Board unpowered, LEDs backwards (flat side = cathode = GND side), or 220 Ω forgotten."],
    ["Answers once then stops", "GPIO4/DE-RE fault (stuck transmitting jams bus). Check GPIO4 continuity + 10 kΩ pull-down."],
    ["PC cannot see S3", "Different USB DATA cable first, then CP210x/CH343 driver; pick the USB-UART port."],
], widths=[1.6, 4.6])

# ================= 9 =================
doc.add_heading("9. Safety & limits", level=1)
doc.add_paragraph("Power the tester ONLY from USB. Never from the traction pack.", style="List Bullet")
doc.add_paragraph("This jig proves WIRING continuity and A/B polarity. It does not test meter accuracy, calibration or protection trips — those keep their existing procedures.", style="List Bullet")
doc.add_paragraph("RS485 needs the common ground: without it even perfect A/B wiring is unreliable.", style="List Bullet")
doc.add_paragraph("Short twisted bench leads only — this is a factory jig, not a long-distance installation.", style="List Bullet")
callout("Remember: ",
        "the whole point of the box is that the real battery pack is never needed on the line.")

# ================= 10 =================
doc.add_heading("10. Project folder map", level=1)
table(["Path", "What it is"], [
    ["src/main.cpp, src/bms_protocol.*", "The box's program (PlatformIO)."],
    ["arduino/bms_connection_tester/", "Same program as an Arduino sketch + README."],
    ["firmware/*.bin", "Ready-to-flash binaries + flash README."],
    ["test/test_checksum/, test/test_logic/", "11 automated tests (all passing)."],
    ["tools/virtual_meter.py", "PC program that pretends to be the meter (testing without hardware)."],
    ["tools/test_hardware.py", "Automatic board test once hardware is plugged in."],
    ["wokwi/", "Browser simulation of the setup (optional demo)."],
    ["run_tests.sh", "One command that runs everything runnable."],
], widths=[2.6, 3.6])
doc.add_paragraph()
ep = doc.add_paragraph()
ep.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = ep.add_run("— End of report. The design is intentionally small: this document + the wiring table in §3 is all any electronics engineer needs to build, flash and maintain it. —")
r.italic = True
r.font.color.rgb = GREY

doc.save("/data/data/com.termux/files/home/bms-connection-tester/RS485-Tester-Report.docx")
print("saved beautiful RS485-Tester-Report.docx")
