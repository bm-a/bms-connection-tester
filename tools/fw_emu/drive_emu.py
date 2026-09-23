#!/usr/bin/env python3
"""Exhaustive end-to-end driver for the firmware emulation harness (fw_emu).

Talks REAL HTTP to the REAL web_ui.cpp + relay + spoof logic (POSIX-socket
shim, deterministic virtual time via the control port). Every check below
is one row of the v2.5 scenario matrix; results print as TAP-ish lines and
land in emu_results.json for docs/EMULATION-v2.5.md.

Usage: fw_emu running with FW_EMU_PORT/FW_EMU_CTL, then:
  python3 tools/fw_emu/drive_emu.py [fw_port] [ctl_port]
"""
import http.client
import json
import sys
import time
import urllib.parse

import requests

FW = int(sys.argv[1]) if len(sys.argv) > 1 else 18080
CTL = int(sys.argv[2]) if len(sys.argv) > 2 else 18081
BASE = f"http://127.0.0.1:{FW}"
CURL = f"http://127.0.0.1:{CTL}"
PASS = "admin123"

passed, failed = 0, 0
rows = []


def check(name, cond, evidence=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"ok - {name}")
    else:
        failed += 1
        print(f"FAIL - {name} :: {evidence[:160]}")
    rows.append({"name": name, "ok": bool(cond), "evidence": evidence[:300]})


def post(path, obj):
    return requests.post(BASE + path, json=obj, timeout=10)


def get(path, **kw):
    return requests.get(BASE + path, timeout=10, **kw)


def ctl(path):
    return requests.get(CURL + path, timeout=10).json()


def state():
    return get("/api/state").json()


def step(ms):
    ctl(f"/__time?ms={ms}")


def clock():
    return int(ctl("/__clock")["ms"])


def advance(ms):
    step(clock() + ms)


# ---------- 1. basics + portal ----------
r = get("/")
check("dashboard serves 200 with app", r.status_code == 200 and "BMS Tester" in r.text, r.text[:80])
s = state()
check("state fw 2.7 + defaults", s["fw"] == "2.7" and s["cfg"]["step"] == 250 and s["cfg"]["stag"] == 50, json.dumps(s["cfg"])[:120])
for probe in ["/hotspot-detect.html", "/generate_204", "/gen_204", "/connecttest.txt", "/redirect", "/library/test/success.html"]:
    r = get(probe)
    check(f"probe {probe} -> landing", r.status_code == 200 and "Open Dashboard" in r.text, str(r.status_code))
r = get("/whatever", headers={"User-Agent": "CaptiveNetworkSupport/1.0 wispr"})
check("CNA UA -> landing", r.status_code == 200 and "192.168.4.1" in r.text, str(r.status_code))
r = get("/whatever", allow_redirects=False)
check("person URL -> 302 /", r.status_code == 302 and r.headers.get("Location") == "/", str(r.status_code))

# ---------- 2. relay flows ----------
r = post("/api/config", {"rmode": 0, "nrel": 8, "step": 500, "hseq": 3000})
check("seq config saves", r.json().get("ok") == 1, r.text[:100])
r = post("/api/seq", {"cmd": "start"})
check("START accepted", r.json().get("ok") == 1 and state()["running"] is True, r.text[:80])
advance(3500)
s = state()
check("sequential completes 8 then holds", s["relays"] == [1]*8, str(s["relays"]))
advance(3000)
check("hold expiry auto-OFF", state()["running"] is False, "")
r = post("/api/seq", {"cmd": "start"})
check("dead-band refuses instant restart", r.json().get("ok") == 0 and "settling" in r.json().get("err", ""), r.text[:100])
advance(600)
r = post("/api/seq", {"cmd": "start"})
check("START after dead-band", r.json().get("ok") == 1, r.text[:60])
# mode switch while running must refuse
r = post("/api/config", {"rmode": 2})
check("R10 mode-switch refused mid-run", r.json().get("ok") == 0 and "stop" in r.json().get("err", ""), r.text[:100])
post("/api/seq", {"cmd": "stop"})
advance(600)
# loop + hold 0 refused
r = post("/api/config", {"loop": 1, "hseq": 0, "hall": 0})
check("R9 loop+hold0 refused", r.json().get("ok") == 0 and "finite hold" in r.json().get("err", ""), r.text[:100])
# chase wave: single-lit + BBM gap observed over real time steps
post("/api/config", {"rmode": 2, "nrel": 4, "step": 500, "swp": 0})
r = post("/api/seq", {"cmd": "start"})
check("chase START runs", r.json().get("ok") == 1 and state()["running"] is True, r.text[:80])
weights, gap_seen = [], False
for _ in range(12):
    advance(100)
    w = sum(state()["relays"])
    weights.append(w)
    assert w <= 1, f"two relays lit! {state()['relays']}"
    if w == 0:
        gap_seen = True
check("chase bitmask weight<=1 always", max(weights) == 1, str(weights))
check("chase BBM all-OFF gap observed", gap_seen, str(weights))
post("/api/seq", {"cmd": "stop"})
advance(600)
# force-in-chase idles the wave, single ON
post("/api/seq", {"cmd": "start"})
advance(700)
r = post("/api/relay", {"i": 3, "on": 1})
s = state()
check("R7 force-in-chase: only tile ON", s["relays"][3] == 1 and sum(s["relays"]) == 1, str(s["relays"]))
# count shrink live safety
post("/api/seq", {"cmd": "stop"})
advance(600)
post("/api/config", {"rmode": 0, "nrel": 8, "step": 200, "hseq": 60000})
post("/api/seq", {"cmd": "start"})
advance(1600)
post("/api/config", {"nrel": 3})
advance(100)
s = state()
check("R22 shrink drops outputs same-tick", s["relays"][3:] == [0]*5, str(s["relays"]))
post("/api/seq", {"cmd": "stop"})
advance(600)
# loop to limit completion
post("/api/config", {"nrel": 2, "step": 200, "hseq": 1000, "loop": 1, "cpause": 1000, "clim": 2})
post("/api/seq", {"cmd": "start"})
for _ in range(120):
    advance(100)
    if not state()["running"]:
        break
s = state()
check("R25 limit=2 stops IDLE all-OFF", s["cycles"] == 2 and s["running"] is False and sum(s["relays"]) == 0, json.dumps({k: s[k] for k in ("cycles", "running")}))
post("/api/config", {"loop": 0, "nrel": 8})

# ---------- 3. spoof + trigger ----------
r = post("/api/spoof", {"cmd": "save", "sv": 520, "ssec": 9})
check("spoof save-only (no fire)", r.json().get("ok") == 1 and state()["spoof"] is False, r.text[:60])
r = post("/api/spoof", {"cmd": "trig", "sena": 1, "spin": 2, "sinv": 1})
s = state()
check("trigger save (pin+enable+polarity, no fire)", s["cfg"]["spin"] == 2 and s["cfg"]["sinv"] == 1 and s["spoof"] is False, json.dumps({k: s["cfg"][k] for k in ("spin", "sinv", "sena")}))
r = post("/api/spoof", {"cmd": "fire", "ssec": 2, "s2sec": 2})
check("FIRE triggers stage 1", state()["stage"] == 1, "")
advance(2100)
check("stage 1 -> stage 2 on schedule", state()["stage"] == 2, "")
advance(2100)
check("auto-revert after stage 2", state()["stage"] == 0, "")
post("/api/spoof", {"cmd": "fire", "ssec": 30, "s2sec": 30})
post("/api/spoof", {"cmd": "cancel"})
check("cancel disarms", state()["spoof"] is False, "")

# ---------- 4. upload over real multipart ----------
def fw_image(n=1500, magic=True, flash_code=0x30):
    import os
    data = bytearray(os.urandom(n))
    if magic:
        data[0], data[3] = 0xE9, flash_code
    return bytes(data)

def raw_multipart(fields, file_field="firmware", filename="firmware.bin", data=b"", file_first=False):
    boundary = "emuBOUNDARY1234"
    parts = []
    items = []
    if file_first:
        items.append(("file", None))
        for k, v in fields.items():
            items.append(("field", (k, v)))
    else:
        for k, v in fields.items():
            items.append(("field", (k, v)))
        items.append(("file", None))
    for kind, payload in items:
        if kind == "field":
            k, v = payload
            parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="{k}"\r\n\r\n{v}\r\n'.encode())
        else:
            parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="{file_field}"; filename="{filename}"\r\nContent-Type: application/octet-stream\r\n\r\n'.encode() + data + b'\r\n')
    parts.append(f'--{boundary}--\r\n'.encode())
    body = b"".join(parts)
    c = http.client.HTTPConnection("127.0.0.1", FW, timeout=30)
    c.request("POST", "/update", body, {"Content-Type": f"multipart/form-data; boundary={boundary}", "Content-Length": str(len(body))})
    resp = c.getresponse()
    return resp.status, resp.read().decode(errors="replace")

img = fw_image()
st, body = raw_multipart({"pass": PASS}, data=img)
up = ctl("/__update")
check("upload valid -> UPDATE OK, single end", st == 200 and "UPDATE OK" in body and up["ends"] == 1 and up["bytes"] == len(img), f"{st} {body[:60]} ends={up['ends']}")
st, body = raw_multipart({"pass": PASS}, data=img, file_first=True)
check("pass-last order also flashes", st == 200 and "UPDATE OK" in body, f"{st} {body[:60]}")
st, body = raw_multipart({}, data=img)
check("no password -> 403, nothing flashed", st == 403, f"{st} {body[:60]}")
st, body = raw_multipart({"pass": PASS}, filename="n16r8-firmware.bin", data=img)
check("wrong variant -> 400 named", st == 400 and "wrong file" in body, f"{st} {body[:80]}")
st, body = raw_multipart({"pass": PASS}, data=b'{"not":"firmware"}' * 40)
check("garbage -> 400 bad magic", st == 400 and "magic" in body, f"{st} {body[:80]}")
big16 = bytearray(img)
big16[3] = 0x40
st, body = raw_multipart({"pass": PASS}, data=bytes(big16))
check("16MB image on 8MB chip -> 400", st == 400 and "more flash" in body, f"{st} {body[:80]}")
r = get("/api/uprog")
check("uprog progress endpoint", r.status_code == 200 and "written" in r.text, r.text[:80])

# ---------- 5. backup/restore/console/resets ----------
r = get("/api/backup")
bk = r.json()
check("backup v2 sections, no secrets", bk.get("config") == 2 and "relays" in bk and "trigger" in bk and "bms12345" not in r.text and "admin123" not in r.text, str(sorted(bk.keys())))
post("/api/config", {"nrel": 5, "step": 777})
bk["pass"] = PASS
r = post("/api/restore", bk)
check("v2 restore round-trips", r.json().get("ok") == 1 and state()["cfg"]["nrel"] == 8, r.text[:100])
r = post("/api/restore", {"backup": 1, "pass": PASS, "nrel": 6})
check("v1 flat backup still accepted", r.json().get("ok") == 1 and state()["cfg"]["nrel"] == 6, r.text[:80])
post("/api/config", {"nrel": 8})
r = post("/api/cmd", {"cmd": "HELP"})
check("console HELP free", "START STOP" in r.json().get("out", ""), r.text[:80])
r = post("/api/cmd", {"cmd": "start"})
check("console START needs password", r.json().get("ok") == 0, r.text[:60])
r = post("/api/cmd", {"cmd": "start", "pass": PASS})
check("console START works", r.json().get("ok") == 1 and r.json().get("out") == "started", r.text[:60])
r = post("/api/cmd", {"cmd": "bogus"})
check("console rejects unknown", r.json().get("ok") == 0, r.text[:60])
post("/api/cmd", {"cmd": "stop", "pass": PASS})
advance(700)
r = post("/api/admin", {"cmd": "reset_keepwifi", "pass": PASS})
f = ctl("/__flags")
check("keepwifi reset + reboot flag", r.json().get("ok") == 1 and f["restart"] == 1, r.text[:60])
r = post("/api/admin", {"cmd": "bootcount_reset", "pass": PASS})
check("bootcount reset", state()["cfg"]["boot"] == 0, "")

# ---------- 6. STA + OTA ----------
r = post("/api/sta", {"cmd": "test", "pass": PASS, "ssid": "Hot", "sta_pass": "pw"})
check("STA test starts", r.json().get("ok") == 1, r.text[:60])
ctl("/__link?up=1")
advance(500)
s = state()
check("STA test reports RSSI/IP", s["cfg"]["sta_test"] == 2 and "RSSI" in s["cfg"]["sta_test_msg"], s["cfg"]["sta_test_msg"])
r = post("/api/ota", {"ota_url": "ftp://x/y.txt", "pass": PASS})
check("bad OTA URL refused", r.json().get("ok") == 0, r.text[:60])
r = post("/api/ota", {"ota_url": "http://192.168.1.9:8000/n16r8-firmware.bin", "ota_int_h": 6, "pass": PASS})
check("OTA URL + cadence save", r.json().get("ok") == 1, r.text[:60])
ctl("/__link?up=0")
r = post("/api/ota", {"cmd": "url_upgrade", "pass": PASS})
check("url_upgrade needs creds", r.json().get("ok") == 0 and "STA offline" in r.text, r.text[:80])
# v2.5 on-demand: saved creds + available-but-unjoined link installs itself.
post("/api/admin", {"sta_ssid": "Hot", "sta_pass": "pw", "pass": PASS})
ctl("/__link?up=1")
r = post("/api/ota", {"cmd": "url_upgrade", "pass": PASS})
f = ctl("/__flags")
check("url_upgrade on-demand fires install", r.json().get("ok") == 1 and f["ota_install"] == 1, r.text[:80])
r = post("/api/ota", {"cmd": "check", "pass": PASS})
f = ctl("/__flags")
check("OTA check path", r.json().get("ok") == 1 and f["ota_check"] == 1, r.text[:80])

# ---------- 7. info card ----------
s = state()["cfg"]
check("info fields complete", all(k in s for k in ("variant", "flash_kb", "sketch_free", "heap", "psram", "uptime_s", "boot", "reset", "rssi", "sta_ip", "sta_mac", "ota_url", "ota_int_h")), str(sorted(s.keys())[:8]))

# ---------- 8. v2.6 daily meters (link-gap heuristic) + round LEDs ----------
r = get("/")
check("round link dots served", 'id=dotG' in r.text and 'id=dotR' in r.text and 'id=meters' in r.text, r.text[:80])
check("no NEXT button served", "NEXT METER" not in r.text, r.text[:80])
ctl("/__bus?up=1")
advance(500)
s = state()
check("first GREEN opens meter #1", s["cfg"]["m_met"] == 1 and s["cfg"]["m_att"] == 0, json.dumps({k: s["cfg"][k] for k in ("m_met", "m_att", "m_ps", "m_fl")}))
r = post("/api/seq", {"cmd": "start"})
advance(4000)
post("/api/seq", {"cmd": "stop"})
advance(1000)
ctl("/__bus?up=0")
advance(5000)
ctl("/__bus?up=1")
advance(500)
s = state()
check("reseat gap closes meter #2", s["cfg"]["m_met"] == 2, json.dumps(s["cfg"])[:80])
check("abort-close verdicts fail", s["cfg"]["m_fl"] == 1 and s["cfg"]["m_att"] == 1, json.dumps({k: s["cfg"][k] for k in ("m_met", "m_att", "m_ps", "m_fl")}))
ctl("/__bus?up=0")
advance(500)
ctl("/__bus?up=1")
advance(500)
s = state()
check("sub-3s flicker stays same meter", s["cfg"]["m_met"] == 2, json.dumps(s["cfg"])[:80])
r = post("/api/cmd", {"cmd": "dayreset", "pass": PASS})
check("console dayreset clears", r.json().get("ok") == 1, r.text[:60])
s = state()
check("reset seats unit as #1", s["cfg"]["m_met"] == 1 and s["cfg"]["m_ps"] == 0 and s["cfg"]["m_fl"] == 0, json.dumps(s["cfg"])[:80])

print(f"\n{passed} passed, {failed} failed")
with open("emu_results.json", "w") as fp:
    json.dump({"passed": passed, "failed": failed, "rows": rows}, fp, indent=1)
sys.exit(1 if failed else 0)
