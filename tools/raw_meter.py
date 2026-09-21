#!/usr/bin/env python3
"""Raw-fd virtual meter for proot PTY tests (no pyserial modem ioctls).
Same GOLDEN vectors + scenarios as tools/virtual_meter.py:
  regs 03/04/05 normal, write-silence, unknown-silence, noise-ignored, slowstop.
Usage: raw_meter.py TTY
Exit 0 on all-PASS, 1 on any FAIL. Prints LED log hint at the end.
"""
import os
import sys
import time

GOLDEN = {
    "03": bytes.fromhex(
        "DD 03 00 1B 14 50 00 00 27 10 27 10 00 01 20 21 00 00 00 00 00 00"
        " 20 64 03 0E 02 0B A5 0B A5 FC DA 77"),
    "04": bytes.fromhex(
        "DD 04 00 1C" + " 0E 82" * 14 + " F8 04 77"),
    "05": bytes.fromhex(
        "DD 05 00 0C 54 45 53 54 2D 31 34 53 31 30 30 41 FC FD 77"),
}

def req_for(reg: str) -> bytes:
    r = int(reg, 16)
    ck = (0x10000 - (r + 0)) & 0xFFFF
    return bytes([0xDD, 0xA5, r, 0x00, (ck >> 8) & 0xFF, ck & 0xFF, 0x77])

def read_exact(fd, n, timeout):
    import select
    buf = b""
    deadline = time.time() + timeout
    while len(buf) < n:
        remaining = deadline - time.time()
        if remaining <= 0:
            break
        r, _, _ = select.select([fd], [], [], remaining)
        if not r:
            break
        try:
            chunk = os.read(fd, n - len(buf))
        except OSError:
            break
        if not chunk:
            break
        buf += chunk
    return buf

def drain(fd):
    import select
    while select.select([fd], [], [], 0.05)[0]:
        try:
            if not os.read(fd, 256):
                break
        except OSError:
            break

def main() -> int:
    dev = sys.argv[1]
    fd = os.open(dev, os.O_RDWR | os.O_NOCTTY)
    ok = True

    for reg, want in GOLDEN.items():
        drain(fd)
        os.write(fd, req_for(reg))
        rx = read_exact(fd, len(want), 1.5)
        match = rx == want
        print(f"poll reg={reg}: match={match} rx={rx.hex(' ')}")
        ok = ok and match

    # write -> silence
    drain(fd)
    os.write(fd, bytes([0xDD, 0x5A, 0x10, 0x02, 0xAA, 0x55, 0xFE, 0xFF, 0x77]))
    rx = read_exact(fd, 1, 0.5)
    print(f"write sent, rx={rx.hex(' ')} (expect silence)")
    ok = ok and (rx == b"")

    # unknown reg -> silence
    drain(fd)
    os.write(fd, req_for("09"))
    rx = read_exact(fd, 1, 0.5)
    print(f"unknown-reg sent, rx={rx.hex(' ')} (expect silence)")
    ok = ok and (rx == b"")

    # noise burst -> no reply, then a good poll still answers (resync proof)
    drain(fd)
    os.write(fd, bytes(range(256)))
    time.sleep(0.3)
    drain(fd)
    os.write(fd, req_for("03"))
    rx = read_exact(fd, len(GOLDEN["03"]), 1.5)
    match = rx == GOLDEN["03"]
    print(f"post-noise poll reg=03: match={match}")
    ok = ok and match

    # slowstop: 3 polls then 3 s silence (DUT must go RED by itself; check dut.log)
    for _ in range(3):
        drain(fd)
        os.write(fd, req_for("03"))
        rx = read_exact(fd, len(GOLDEN["03"]), 1.5)
        ok = ok and (rx == GOLDEN["03"])
        time.sleep(0.3)
    print("silence for 3 s (DUT must go RED by itself, see dut.log)...")
    time.sleep(3.0)

    print("RAW-METER " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
