# Emulators — test everything without the battery (v2.7 results)

![Suite results](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/tests.png)

*152/152 suites + 3-variant contract + 30-day soak — what Layer 1 proves.*

## Layer 1 — native tests + soak + contract (Termux, always)
`sh run_tests.sh` (g++ + local Unity fallback) and `pio test -e native` (103):
checksum (7), logic (8), parser (13), stress (4), relay (36), spoof (11),
meter (10), ota (6), upload (5), system (3) = **103/103 pio**; plus `test_web`
(49, g++ host stubs) and the `check_web_contract.py` gate = **152/152 total**,
plus `tools/soak_sim.cpp` 30-day run (`2591400 polls / 2591400 replies`,
millis-wrap crossed, 30 NVS commits, green-on-resume) — PASS. Protocol core
untouched since v1.x; dashboard JS passes `node --check`.

## Layer 2 — virtual bus (Debian proot + socat, no ESP32)
`socat` PTY pair + a host DUT harness linking the real `src/bms_protocol.cpp`
(same loop as `main.cpp`: feed → `note_poll` → `reply_for` → 250 ms LED eval)
against `tools/virtual_meter.py` scenarios:
03/04/05 golden byte-exact, silence on write/unknown, noise-burst resync,
3 s silence → RED by itself — **PASS**.
Note: pyserial's modem ioctls fail on proot PTYs, so the harness uses raw-fd
I/O; the shipped `virtual_meter.py` is unchanged and used on real serial ports.

## Layer 3 — socket harness (real firmware over real HTTP, v2.5+)

`tools/fw_emu/emu_main.cpp` compiles the REAL `web_ui.cpp` + relay + spoof
against a POSIX-socket shim and serves it on 127.0.0.1; virtual time and
state ride a side control port (`/__time`, `/__link` for STA, `/__bus` for
the RS485 LEDs + meter heuristic, `/__update`, `/__flags`, `/__chip`,
`/__nvs`). `drive_emu.py` (62 checks: portal, relay flows, spoof/trigger,
uploads, backup/restore, console, resets, STA/OTA, info, meter gaps + round
dots) + `drive_soak.py` (48 virtual hours, 10 checks) — **72/72 PASS**.
`w3m -dump` verifies the rendered pages.

## Layer 4b — offline 3D bench (no internet, phone-hosted)

`local-wokwi/` (also standalone: https://github.com/bm-a/bms-tester-sim):
one ESP32-S3 + MAX485 + photo-textured meter + 8 relays + GX16 sockets,
electron-flow viz sized by live rail current, pressable buttons, relay board
with persistent names, and the verbatim ESP dashboard embedded below:

![FULL dashboard snapshot](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/dash-full.png)

![Sequential + spoof demo](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/demo.gif)

*Honest limit: no WebGL screenshots can be captured in this container, so the
3D canvas itself has no stills — the snapshots above are the dashboard and
relay states it drives. Open the bench to see the 3D live.*

## Layer 4 — Wokwi browser + headless sim (functional proof)
`wokwi/diagram.json`: DUT S3 + discretes (10/11) + **NeoPixel on GPIO48**
(DIN/VDD/VSS) + **8 relay modules (npn = energize-on-LOW, NO-contact indicator
LEDs)** + button (15) + spoof button (21, `1.l`/`2.l`) + meter S3.
Pin names verified against docs.wokwi.com (this caught real bugs: VCC/GND and
`1`/`2` were wrong). `wokwi/meter.ino` cycles 03/04/05 @ 1 s and prints replies.
- Browser: open in wokwi.com with the PIO `firmware.bin`/`firmware.elf`.
- Headless/CI: `wokwi-cli wokwi --timeout 90000 --scenario wokwi/sim.yaml`
  (needs `WOKWI_CLI_TOKEN`; CI `wokwi-sim` job runs it when the secret exists):
  meter replies → button press → relay pins LOW in sequence → spoof press →
  `22 B0` on the meter console.
- Honest limit: ESP32 Wi-Fi AP mode is outside the verified sim surface;
  dashboard HTTP has no emulator — it is covered by `test_web` + contract gate.

## Layer 5 — QEMU-S3 (boot only, known gap)
Built per `bm-a/esp32s3-qemu-arm64` (source build in proot, sanitized PATH,
RDID/SFDP patches). Not re-run for v2.3 (QEMU cannot start in the phone
sandbox); last re-confirm on the v2.1 image reproduces the documented
Arduino-guest gap — `M25P80: Unknown cmd 0x10` (~22×) +
`Invalid read at addr 0x10200C` (~84×), flash-init assert loop.
Verdict unchanged: **emulator gap, not firmware** — our code is never reached;
QEMU also models neither RMT/WS2812 nor discrete LEDs. Functional proof = Wokwi.
`tools/run_qemu_s3.sh` remains the one-command boot check for real Linux/Mac
(QEMU JIT cannot run inside proot).

## Layer 6 — hardware-in-loop (real board, when available)
`tools/test_hardware.py` (pytest, `HIL_BUS_PORT` + `HIL_CDC_PORT`):
golden exact + `GREEN ≤ 1 s` → `RED` after ~2.6 s silence via `STATUS?`.
Auto-skips without hardware. Bench expectation for v2.6: meter shows
≈ 52 V / 100 %, discretes + RGB agree; relay sequence + chase + 2-stage spoof
(100 first, then 88.8 / 88.8 / 88.8 / 188 %) verified on the meter; meter card
counts reseats, one-file image flashes to 0x0. See [[Relays]].
