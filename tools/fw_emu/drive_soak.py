#!/usr/bin/env python3
"""48-virtual-hour end-to-end run on the emulation harness.

A looped sequential program runs 2 virtual days (hourly time jumps; the
sequencer catches up exactly per tick). Asserts the QC counters, NVS
write-coalescing over real HTTP, a spoof fire + revert, and clean stop.
Complements tools/soak_sim.cpp (30 days, pure logic) at the HTTP layer.

Usage: fw_emu running, then: python3 tools/fw_emu/drive_soak.py [fw] [ctl]
"""
import sys

import requests

FW = int(sys.argv[1]) if len(sys.argv) > 1 else 18080
CTL = int(sys.argv[2]) if len(sys.argv) > 2 else 18081
BASE = f"http://127.0.0.1:{FW}"
CURL = f"http://127.0.0.1:{CTL}"
PASS = "admin123"

passed, failed = 0, 0


def check(name, cond, evidence=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"ok - {name}")
    else:
        failed += 1
        print(f"FAIL - {name} :: {evidence[:160]}")


def post(path, obj):
    return requests.post(BASE + path, json=obj, timeout=15)


def get(path, **kw):
    return requests.get(BASE + path, timeout=15, **kw)


def ctl(path):
    return requests.get(CURL + path, timeout=15).json()


def state():
    return get("/api/state").json()


def clock():
    return int(ctl("/__clock")["ms"])


def advance(ms, grain=100):
    ctl(f"/__time?ms={clock() + ms}&tick_ms={grain}")


# Loop program: 8 relays, 500 ms grid, 2 s hold, 1 s pause, forever.
advance(1000)  # settle past any previous run's stop dead-band
post("/api/seq", {"cmd": "stop"})
advance(1000)
r = post("/api/config", {"rmode": 0, "nrel": 8, "step": 500, "hseq": 2000,
                          "loop": 1, "cpause": 1000, "clim": 0})
assert r.json().get("ok") == 1, r.text[:100]
r = post("/api/seq", {"cmd": "start"})
assert r.json().get("ok") == 1, r.text[:100]
s0 = state()
cyc0, act0 = s0["cycles"], s0["acts"]
advance(3600000)  # settle first hour (flushes the setup save, warms up)
nvs0 = ctl("/__nvs")["commits"]
for _ in range(47):
    advance(3600000)  # one virtual hour per step, 100 ms grains
s = state()
check("48h loop still running", s["running"] is True, "")
# Exact: complete cycles × 8 edges, plus the one in-flight partial cycle
# (its relays are lit but its hold hasn't expired into cyclesDone yet).
# Deltas: counters persist across runs by design (QC), so compare within run.
dc, da = s["cycles"] - cyc0, s["acts"] - act0
extra = da - dc * 8
check("actuations == 8/cycle + in-flight", 0 <= extra <= 8 and dc > 25000,
      f"dcyc={dc} dact={da}")
check("two days of cycles", s["cycles"] > 25000, f"cyc={s['cycles']}")
check("no NVS writes while running untouched", ctl("/__nvs")["commits"] == nvs0, "")
# 3 rapid saves coalesce into one flash commit, end to end.
post("/api/config", {"step": 501})
post("/api/config", {"step": 502})
post("/api/spoof", {"cmd": "save", "sv": 510})
advance(500)
check("saves coalescing (no flush yet)", ctl("/__nvs")["commits"] == nvs0, "")
advance(2000)
check("one coalesced commit", ctl("/__nvs")["commits"] == nvs0 + 1, "")
# Spoof fire + full revert inside the long run.
post("/api/spoof", {"cmd": "fire", "ssec": 5, "s2sec": 5})
advance(1000)
check("spoof stage 1 mid-run", state()["stage"] == 1, "")
advance(5000)
check("spoof stage 2 mid-run", state()["stage"] == 2, "")
advance(6000)
check("spoof reverted mid-run", state()["stage"] == 0, "")
# Clean stop: everything OFF, counters preserved for QC.
post("/api/seq", {"cmd": "stop"})
advance(100)
s = state()
check("stop ends clean", s["running"] is False and sum(s["relays"]) == 0 and s["cycles"] > 25000,
      f"running={s['running']} cyc={s['cycles']}")

print(f"\n{passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
