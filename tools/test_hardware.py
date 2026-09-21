"""Hardware-in-loop test: real ESP32-S3 + USB-RS485 adapter loop.

Wiring: adapter A/B <-> tester MAX485 A/B. Adapter USB on this PC,
tester USB (CDC console) on this PC.

- Sends the fixed JBD poll, asserts byte-exact golden SOC100% reply.
- Sends STATUS? on the CDC console, asserts GREEN within 1s of polling
  and RED after ~2.5s of silence.
Skipped automatically when ports are absent (see run_tests.sh).
"""
import os
import time

import pytest
import serial

REQUEST = bytes.fromhex("DD A5 03 00 FF FD 77")
GOLDEN = bytes.fromhex(
    "DD 03 00 1B 14 50 00 00 27 10 27 10 00 01 20 21 00 00 00 00 00 00"
    " 20 64 03 0E 02 0B A5 0B A5 FC DA 77"
)

BUS_PORT = os.environ.get("HIL_BUS_PORT", "")
CDC_PORT = os.environ.get("HIL_CDC_PORT", "")

need_hw = pytest.mark.skipif(
    not BUS_PORT or not CDC_PORT, reason="no HIL ports (set HIL_BUS_PORT/HIL_CDC_PORT)"
)


def poll(bus, timeout=0.6):
    bus.reset_input_buffer()
    bus.write(REQUEST)
    bus.flush()
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline and len(buf) < len(GOLDEN):
        chunk = bus.read(len(GOLDEN) - len(buf))
        if chunk:
            buf += chunk
    return buf


def status(cdc):
    cdc.reset_input_buffer()
    cdc.write(b"STATUS?\n")
    cdc.flush()
    line = cdc.readline().decode(errors="replace").strip()
    return line.split()[0] if line else line  # "GREEN 2.2" -> "GREEN"


@need_hw
def test_golden_reply():
    with serial.Serial(BUS_PORT, 9600, timeout=0.6) as bus:
        assert poll(bus) == GOLDEN


@need_hw
def test_led_green_then_red():
    with serial.Serial(BUS_PORT, 9600, timeout=0.6) as bus, serial.Serial(
        CDC_PORT, 115200, timeout=1.0
    ) as cdc:
        time.sleep(0.2)
        assert poll(bus) == GOLDEN
        assert status(cdc) == "GREEN"
        time.sleep(2.6)  # silence -> must revert to red by itself
        assert status(cdc) == "RED"
