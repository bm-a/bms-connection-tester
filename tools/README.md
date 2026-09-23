# tools/ — test helpers and scripts

![Sequential + spoof demo — what the harnesses prove on the bench](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/demo.gif)

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
- `dut_emu.cpp` + `raw_meter.py` + `virtual_bus.sh` — virtual-bus emulation
  without any ESP32 (Debian/proot or Linux, needs socat + g++): `dut_emu`
  links the real `src/bms_protocol.cpp` and speaks JBD over a PTY;
  `raw_meter.py` runs every scenario over raw-fd I/O (pyserial's modem ioctls
  fail on proot PTYs, so no pyserial here); `sh tools/virtual_bus.sh` runs
  the whole cycle in one shell and asserts RED→GREEN→RED. Exit 0 = PASS.
- `check_web_contract.py` — consistency gate: every dashboard `fetch()` has a
  firmware route, every element id exists (incl. dynamic `lblN` tile ids),
  every state key is emitted, every POSTed key is consumed (incl. `/api/ota`,
  `/api/meter` and the `/update` upload form). Runs in `run_tests.sh` + CI; exit 0 = PASS.
- `make_report.py` — generates the Word report (`RS485-Tester-Report.docx`).
- `fw_emu/` — socket harness: the REAL `web_ui.cpp` + relay + spoof served
  over real HTTP with virtual time. Build: `g++ -DARDUINO -I tools/fw_emu
  -I src src/bms_protocol.cpp src/relay_ctrl.cpp src/web_ui.cpp
  tools/fw_emu/emu_main.cpp -o fw_emu`, then `FW_EMU_PORT=18080 FW_EMU_CTL=18081
  ./fw_emu &`. Control port: `/__time?ms=N` (step clock),
  `/__link?up=` (STA uplink), `/__bus?up=` (RS485 bus LEDs + v2.6 meter
  heuristic), `/__update`, `/__flags`, `/__chip`, `/__nvs` (commit counter).
  Drivers: `drive_emu.py` (62 scenario checks) + `drive_soak.py` (48 virtual
  hours, 10 checks).
