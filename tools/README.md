# tools/ — test helpers and scripts

- `virtual_meter.py` — PC program that pretends to be the meter. Scenario modes for
  bench testing without hardware:
  `python3 tools/virtual_meter.py PORT [--reg 03|04|05] [--period S] [--jitter S]`
  `[--mode normal|slowstop|noise|write|unknown] [--count N]`
- `test_hardware.py` — pytest hardware-in-loop: exact golden reply + GREEN→RED timing
  via `STATUS?`. Needs `HIL_BUS_PORT` (RS485 adapter) + `HIL_CDC_PORT` (S3 USB).
  Skips itself when no hardware is present.
- `run_qemu_s3.sh` — boots `firmware/firmware.bin` on an emulated ESP32-S3
  (Espressif QEMU fork) and checks `STATUS?`. Needs a **real Linux/Mac** — QEMU's JIT
  cannot start inside phone proot sandboxes (documented abort in static glib init).
- `make_report.py` — generates the Word report (`RS485-Tester-Report.docx`).
