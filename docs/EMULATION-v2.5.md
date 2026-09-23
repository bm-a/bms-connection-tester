# Emulation report v2.5 — every feature, actually executed

![Suite results](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/tests.png)

*(Historical v2.5 report — method unchanged in v2.7; current counts above.)*

Harness: `tools/fw_emu/` — the REAL `web_ui.cpp` + relay + spoof + OTA
decision code compiled natively behind a POSIX-socket HTTP shim, driven over
real HTTP with deterministic virtual time. No test doubles in the request
path: same handlers, real HTTP parsing, real multipart framing.
Drivers: `drive_emu.py` (54 checks) + `drive_soak.py` (10 checks, 48 virtual
hours). Run: boot `fw_emu`, then both drivers. Result at write time:
**64/64 PASS** (fresh emulator).

Text-browser proof: `w3m -dump` renders of `/` (full dashboard: every card,
field, button) and the portal landing (button + 3 steps) are captured during
verification runs.

## Per-feature verdicts (all PASS)

- Dashboard serves 200 with app; state defaults (fw 2.4, step 250, stagger 50).
- Portal: 6 OS probe paths + CNA UA → landing page; person URL → 302 `/`.
- Sequential completes 8 then holds; hold expiry auto-OFF; dead-band refuses
  instant restart with a named error; START accepted after 600 ms.
- R10 mode-switch refused mid-run (mode untouched); R9 loop+hold0 refused
  (config untouched); accepted with finite hold.
- Chase: START runs; bitmask weight ≤ 1 on every 100 ms observation across
  1200 ms; all-OFF BBM gaps observed; wraps correctly.
- R7 force-in-chase idles the wave (exactly the forced tile ON, no phantom
  cycle); R22 count-shrink drops outputs same tick.
- R25 limit=2 stops IDLE with all OFF, exactly 2 cycles counted.
- Spoof save-only stages without firing; trigger save stores pin+enable+
  polarity without firing; FIRE → stage 1 → stage 2 on schedule → revert;
  Cancel disarms mid-plan.
- Upload (real multipart, both field orders): valid file → UPDATE OK, exactly
  one `Update.end`, byte-identical flash image; no password → 403, nothing
  flashed; wrong variant / garbage / oversize-flash → 400 with the named
  cause each. Progress endpoint reports bytes.
- Backup v2 sections with zero secrets in the export; v2 restore
  round-trips; v1 flat backups still migrate; garbage rejected by name.
- Console: HELP/STATUS/VERSION free with correct text; privileged verbs need
  the password; unknown verbs rejected by name.
- keep-WiFi reset reboots with identity kept; bootcount resets to 0.
- STA test starts, links, reports RSSI/IP, drops to AP-only; 30 s timeout
  path reports the named failure; OTA URL validation; on-demand join fires
  the install path with saved creds and a dead boot link.
- Info card: all 13 fields present and shaped.
- 48 h loop: still running, actuations == 8×cycles + in-flight partial only,
  >25 k cycles, zero NVS writes while untouched, 3 rapid saves → exactly 1
  commit, spoof fire/revert mid-run, clean stop with counters preserved.

## 30-day soak (`tools/soak_sim.cpp`, pure logic, 0.5 s)

2,591,400 polls → 2,591,400 replies (zero lost); green/red windows correct;
millis wrap crossed mid-month with correct behavior after; 309,625 looped
cycles + 2,577,808 actuations; live count-shrink, chase switch-over,
force-in-chase single-ON, and stops all exact; weekly spoof fires exact;
**30 NVS commits for 30 days of save bursts** (coalescing projection:
decades of flash endurance at this rate); silence → red, resume → green,
no reset needed.

## Bugs the harness caught (all fixed, all now covered)

1. Multipart `server.arg("pass")` agreement — never satisfiable on real
   servers (args stay empty for multipart); every real upload 403'd. Fix:
   streamed field is the sole password source. (The host stub had masked it
   by injecting args.)
2. Minimal JSON parser rejected `"key": value` whitespace (python requests
   style) — same bug class as the v2.3.1 OTA tag-space parse. Fix:
   whitespace-tolerant `jval` core for all three helpers.
3. On-demand STA gap: the one-shot test drops its link, so STA-gated
   check/install/URL could never follow it. Fix: actions join with saved
   creds themselves (15 s, Tasmota-style blocking).
4. `emu_main` never called `seq.begin()` — every start refused; console
   masked it by always replying ok. Fix: begin on boot + console reports
   refusal as ok:0.
5. Upload filename suffix-match would cross-flash (`firmware.bin` ⊂
   `n16r8-firmware.bin`) — caught by `test_upload`; exact basename compare.

## Honest boundaries (what emulation does NOT cover)

- QEMU-S3 cannot run this Arduino guest (proven twice, parked in HANDOFF
  §9) — the socket harness IS the emulation.
- Nothing is flashed to silicon here (no USB device attached); flash proof
  is PIO builds of both envs + byte verification of the shipped bins.
- Physical pins (button/kill/spoof edges), LEDs, mDNS multicast, and real
  RF/AP behavior run on the bench only — see the HANDOFF §11 checklist.
- Wokwi headless needs `WOKWI_CLI_TOKEN`; diagram pins verified 1:1 instead.
- Virtual time is driver-stepped; wall-clock races (WiFi reconnect storms,
  concurrent portal + upload load) are bench items.
