#!/usr/bin/env python3
"""Virtual JBD meter: polls the tester like the real e-rickshaw meter.

Usage as virtual RS485 bus endpoint (host emulation, no hardware):
    socat -d -d pty,raw,echo=0,link=$PREFIX/tmp/vbus0 pty,raw,echo=0,link=$PREFIX/tmp/vbus1 &
    python3 tools/virtual_meter.py $PREFIX/tmp/vbus0        # meter side
    # firmware harness / HIL pytest opens $PREFIX/tmp/vbus1  # DUT side

Usage against real hardware:
    python3 tools/virtual_meter.py /dev/ttyUSB0 --bus-uart /dev/ttyACM0
"""
import argparse
import sys
import time

REQUEST = bytes.fromhex("DD A5 03 00 FF FD 77")
GOLDEN = bytes.fromhex(
    "DD 03 00 1B 14 50 00 00 27 10 27 10 00 01 20 21 00 00 00 00 00 00"
    " 20 64 03 0E 02 0B A5 0B A5 FC DA 77"
)

try:
    import serial
except ImportError:
    print("need pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(2)


def poll_once(port: str, timeout: float = 0.5) -> bytes:
    with serial.Serial(port, 9600, timeout=timeout) as s:
        s.reset_input_buffer()
        s.write(REQUEST)
        s.flush()
        deadline = time.time() + timeout
        buf = b""
        while time.time() < deadline and len(buf) < len(GOLDEN):
            chunk = s.read(len(GOLDEN) - len(buf))
            if chunk:
                buf += chunk
        return buf


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="serial port of the tester RS485 side")
    ap.add_argument("--count", type=int, default=3)
    ap.add_argument("--interval", type=float, default=1.0)
    args = ap.parse_args()
    ok = True
    for i in range(args.count):
        rx = poll_once(args.port)
        match = rx == GOLDEN
        print(f"poll {i}: rx={rx.hex(' ')} match={match}")
        ok = ok and match
        if i < args.count - 1:
            time.sleep(args.interval)
    print("VIRTUAL-METER " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
