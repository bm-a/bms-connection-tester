#!/usr/bin/env python3
"""Virtual JBD meter: polls the tester like real e-rickshaw meters.

Covers every scenario (v2.0 base): register sweep, cadence sweep, faults.

    python3 tools/virtual_meter.py PORT [--reg 03] [--period 1.0]
        [--jitter 0.0] [--mode normal] [--count 3]

Modes: normal | slowstop (poll then silence) | noise (garbage burst first)
       | write (send a 0x5A write, expect silence) | unknown (reg 0x09, silence)
"""
import argparse
import random
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

try:
    import serial
except ImportError:
    print("need pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(2)


def req_for(reg: str) -> bytes:
    r = int(reg, 16)
    ck = (0x10000 - (r + 0)) & 0xFFFF
    return bytes([0xDD, 0xA5, r, 0x00, (ck >> 8) & 0xFF, ck & 0xFF, 0x77])


def transact(port, tx: bytes, expect_len: int, timeout=0.6) -> bytes:
    with serial.Serial(port, 9600, timeout=timeout) as s:
        s.reset_input_buffer()
        s.write(tx)
        s.flush()
        deadline = time.time() + timeout
        buf = b""
        while time.time() < deadline and len(buf) < expect_len:
            chunk = s.read(expect_len - len(buf))
            if chunk:
                buf += chunk
            elif expect_len == 0:
                break
        return buf


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--reg", default="03")
    ap.add_argument("--period", type=float, default=1.0)
    ap.add_argument("--jitter", type=float, default=0.0)
    ap.add_argument("--mode", default="normal",
                    choices=["normal", "slowstop", "noise", "write", "unknown"])
    ap.add_argument("--count", type=int, default=3)
    args = ap.parse_args()
    ok = True

    if args.mode == "noise":
        with serial.Serial(args.port, 9600, timeout=0.5) as s:
            s.write(bytes(random.getrandbits(8) for _ in range(200)))
            s.flush()
        print("noise burst sent (expect no reply, lamps must stay red)")
        time.sleep(0.3)

    if args.mode == "write":
        tx = bytes([0xDD, 0x5A, 0x10, 0x02, 0xAA, 0x55, 0xFE, 0xFF, 0x77])
        rx = transact(args.port, tx, 1, timeout=0.4)
        print(f"write sent, rx={rx.hex(' ')} (expect silence)")
        ok = ok and (rx == b"")

    if args.mode == "unknown":
        tx = req_for("09")
        rx = transact(args.port, tx, 1, timeout=0.4)
        print(f"unknown-reg sent, rx={rx.hex(' ')} (expect silence)")
        ok = ok and (rx == b"")

    if args.mode in ("normal", "slowstop", "noise"):
        tx = req_for(args.reg)
        want = GOLDEN.get(args.reg)
        for i in range(args.count):
            rx = transact(args.port, tx, len(want))
            match = rx == want
            print(f"poll {i} reg={args.reg}: match={match} rx={rx.hex(' ')}")
            ok = ok and match
            time.sleep(max(0.0, args.period + random.uniform(-args.jitter, args.jitter)))

    if args.mode == "slowstop":
        print("silence for 3 s (lamps must go red by themselves)...")
        time.sleep(3.0)

    print("VIRTUAL-METER " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
