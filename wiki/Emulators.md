# Emulators — test everything without the battery (v2.1 results)

## Layer 1 — native tests + soak + contract (Termux, always)
`sh run_tests.sh` (g++ + local Unity fallback) and `pio test -e native` (52):
checksum (7), logic (8), parser (13), stress (4), relay (12), spoof (6),
system (2) = **52/52 pio**; plus `test_web` (14, g++ host stubs) and the
`check_web_contract.py` gate = **66/66 total**, plus `tools/soak_sim.cpp`
8-day run (`691040 polls / 691040 replies`, millis-wrap crossed,
green-on-resume) — PASS. Protocol core untouched since v1.x; dashboard JS
passes `node --check`.

## Layer 2 — virtual bus (Debian proot + socat, no ESP32)
`socat` PTY pair + a host DUT harness linking the real `src/bms_protocol.cpp`
(same loop as `main.cpp`: feed → `note_poll` → `reply_for` → 250 ms LED eval)
against `tools/virtual_meter.py` scenarios:
03/04/05 golden byte-exact, silence on write/unknown, noise-burst resync,
3 s silence → RED by itself — **PASS**.
Note: pyserial's modem ioctls fail on proot PTYs, so the harness uses raw-fd
I/O; the shipped `virtual_meter.py` is unchanged and used on real serial ports.

## Layer 3 — Wokwi browser + headless sim (functional proof)
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

## Layer 4 — QEMU-S3 (boot only, known gap)
Built per `bm-a/esp32s3-qemu-arm64` (source build in proot, sanitized PATH,
RDID/SFDP patches). v2.0 8 MB image merged with `--fill-flash-size 8MB`:
boot reproduces the documented Arduino-guest gap — `M25P80: Unknown cmd 0x10`
(~22×) + `Invalid read at addr 0x10200C` (~84×), flash-init assert loop.
Verdict unchanged: **emulator gap, not firmware** — our code is never reached;
QEMU also models neither RMT/WS2812 nor discrete LEDs. Functional proof = Wokwi.
`tools/run_qemu_s3.sh` remains the one-command boot check for real Linux/Mac
(QEMU JIT cannot run inside proot).

## Layer 5 — hardware-in-loop (real board, when available)
`tools/test_hardware.py` (pytest, `HIL_BUS_PORT` + `HIL_CDC_PORT`):
golden exact + `GREEN ≤ 1 s` → `RED` after ~2.6 s silence via `STATUS?`.
Auto-skips without hardware. Bench expectation for v2.0: meter shows
≈ 52 V / 100 %, discretes + RGB agree; relay sequence + 10 s spoof
(88.8 / 88.8 / 88.8 / 188 %) verified on the meter. See [[Relays]].
