# Relays + sequencer (v2.8)

![Relay tiles](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/tiles.png)

*Tiles glow green when ON; tap = force ON/OFF (IDLE only); names persist.
Watch a full cycle:*

![Sequential run demo](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/demo.gif)

The sequencer is a pure `millis()` state machine — no `delay()` anywhere, so
RS485 keeps priority while relays run. Every field lives on the dashboard's
Sequence config card ([[Dashboard]]); this page is the deep dive.

## Wiring the 12 V 8-channel module (DIY)
- R1–R8: ESP GPIO **5, 6, 7, 8, 9, 12, 13, 14** → module **IN1–IN8**.
- Module **DC+ / DC−**: separate **12 V supply** (coils ≈ 30 mA each,
  ≈ 240 mA all-on). Tie 12 V GND to ESP GND (common reference for the
  optoisolated inputs). Never power coils from USB/ESP pins.
- Logic default **active-LOW** (LOW = relay ON, boots OFF with no click:
  the firmware drives the OFF level *before* `pinMode OUTPUT`).
  If the module jumpers select HIGH trigger, flip *Relay logic* on the page.
- Switched side: one common **48 V rail** into the relay COMs, each NO to
  its **own** component (meter, indicator, …) — parallel loads, so
  simultaneous-ON is fine (currents just add). **Warning:** never land two
  relay outputs on two *different* voltage points (e.g. cell taps) —
  paralleling potentials shorts them. That topology needs chase (one at a
  time) or isolated wiring.

## Waveshare board
Relays R1–R8 go through the TCA9554PWR expander (I2C `0x20`, SDA42/SCL41);
EXIO1–8 = output bits 0–7, **HIGH bit = ON**. The dashboard logic toggle is
a no-op here. All outputs park OFF at boot. Only the first *Count* relays
participate — the rest are forced OFF, same rule as the DIY path.

## Modes (Sequence card → Mode; only the active mode's fields show)
| Mode | What runs | Fields that matter |
|---|---|---|
| Sequential 1–N | R1→Rn cumulative, one new relay per Step, then Hold | Step, Hold-seq, Direction |
| Chase wave | ONE lit relay sweeping R1→Rn→R1…, 20 ms all-OFF gap between steps (break-before-make: release is slower than pull-in) | Step (= wave speed), Sweeps, Direction |
| All ON at once | All N together, or ramped with Stagger | Hold-all, Stagger |
| Shared (always) | START / STOP ALL / tiles, Count, Button, Logic, Loop + Pause + Limit, Labels, Auto-start | — |

Timing picture, Sequential (defaults: step 250 ms, hold 30 s, R1→R8):
`t=0` R1 ON → `t=250` R2 ON → … → `t=1750` R8 ON → hold until `t=31750` →
all OFF = one cycle. Direction reversed = R8→R1.

Chase (step 250 ms, 3 sweeps): R1 ON → 250 ms → all OFF 20 ms → R2 ON →
… → R8 ON → 20 ms gap → R7 ON … (ping-pong); after 3 sweeps the wave stops.

All ON (stagger 50 ms): R1 ON → 50 ms → R2 ON → … → R8 ON → hold → all OFF.
Stagger 0 = all slam at once (allowed, warned — 8× inrush on the 12 V rail).

- **Hold 0 = stay ON until STOP** in every mode (there is no silent timeout).
- **Loop + hold 0 is refused** — a hold that never expires could never
  complete a cycle, so the save is rejected with a named error.
- **Mode switch needs STOP first** — switching mid-run would strand the old
  mode's relays, so it is rejected until IDLE.
- **START is refused for 0.5 s after STOP** (relay settle dead-band;
  armatures are still releasing). Same dead-band covers loop restarts
  implicitly (the pause floor is ≥ 500 ms all-OFF).
- **Count shrink acts immediately**: relays past the new count drop the
  same tick, their manual forces are cleared, and a later regrow never
  resurrects a stale ON without a fresh tap.
- **Manual tiles vs sequence**: a forced tile wins for that relay. During
  CHASE, forcing a tile stops the wave first (two ON at once would break
  the one-at-a-time promise on a shared harness). STOP ALL clears forces;
  RESTART (button mode) keeps them.
- **Loop pause is coil cooling**: all relays OFF for the pause, then the
  next cycle starts fresh. Pause floor 500 ms, default 2000 ms.
- **Boot-parked OFF**: every boot drives all relays OFF before anything
  else (DIY: OFF level before `pinMode OUTPUT` — no click; Waveshare:
  expander init `0x00`).

## Counters
- **Cycles** (`cyc`): completed full cycles. **Actuations** (`act`):
  relay turn-ON edges. Both in the Relays card header, in `STATUS`, and in
  `/api/state` (`cycles`, `acts`).
- STOP ALL keeps the counters (QC tallies); only a factory reset clears them.

## Timing floors (why the clamps are what they are)
- **Step ≥ 100 ms** (default 250): mechanical bounce (5–15 ms) + BMS ADC
  settle (~50–100 ms) — faster risks switching or sampling mid-transition.
- **Stagger default 50 ms** (0 = explicit all-at-once slam, allowed but
  warned): 8 contactors closing together stacks back-EMF into the shared
  12 V rail plus up to 8× load inrush.
- **Chase gap 20 ms, fixed** (not a field): covers release-vs-pull-in skew.
- There is no contact feedback, so these timing floors *are* the settle
  verification — no code can confirm a click without added hardware.

## Industrial loop usage (burn-in racks)
Loop ON + finite hold + pause (coil cooling) + cycle limit (0 = forever) +
boot auto-start: the box powers up, runs the soak unattended, and the
Meters-today card tallies units. Coalesced NVS saves mean the loop never
stalls on flash writes.

## Button + spoof + WiFi-kill inputs
- Button: GPIO15 to GND (DIY; internal pull-up) / BOOT GPIO0 (Waveshare).
  Short press = configured behavior (Hold-abort / Run-lock / Restart —
  Restart keeps manual tile forces); **hold 10 s = factory reset** (wipes
  AP/admin config, reboots).
- Spoof: trigger GPIO to GND (DIY default 21; Waveshare default DI1/GPIO4)
  or dashboard FIRE. Safe-pin allowlist enforced per board (see
  [[Dashboard]]). Stage 1 (100/100/100/100 %, 5 s) then stage 2
  (88.8 V / 88.8 A / 88.8 °C / 188 %, 10 s) on register `0x03`, then
  auto-revert. Both stages editable; **Save only** stages without firing,
  FIRE saves + fires, Cancel disarms. `0x04`/`0x05` never change.
- WiFi kill: GPIO18 (DIY) / DI2 GPIO5 (Waveshare) to GND drops the AP +
  portal + server at once (mDNS too); release to bring everything back.

## Daily meter counting (v2.8, approximate — no button, no new GPIO)

The JBD protocol carries no meter ID — the box only sees link state and
relay actuations — so the count is a software estimate from link gaps,
never inferred per-unit exact. Rules:
- First GREEN sighting opens meter #1.
- A RED gap ≥ 3 s **arms** a reseat candidate; the candidate **commits** as
  a new meter only after GREEN holds 5 s (fumble guard: seat 4 s, pull,
  seat properly = one unit, not two).
- Steady GREEN across RESTARTs/retries = same meter, always. Sub-3 s
  flickers (slow poll, noise) never open a meter.
- Every accepted START opens an **attempt** (retries included); a completed
  cycle latches **pass**; a meter closed with no completed cycle counts
  **fail** ("closed out with no completed cycle" — the only honest signal).
- The closing meter's verdict is snapshotted at arm time, so an eager START
  during the 5 s settle attributes to the new meter. A RED before the
  settle merges the window's activity back (same meter all along).
- A yank mid-cycle invalidates the test: the previous meter closes as fail.
- Closing is deferred while the sequencer runs; the pending close lands on
  the next IDLE eval.
- Day boundary is manual: **New day (reset)** on the dashboard (no RTC;
  a reboot never clears — a power flicker must not eat QC data; a seated
  unit re-opens as #1 at once).
- 1 NVS write per meter (never per actuation/tick); counters are NOT in
  backups (a restore must not resurrect yesterday's tallies).
- Console: `DAYRESET` (admin-gated); `STATUS` shows the batch.

## Button behaviors (Sequence card → Button)
| Option | Press while idle | Press mid-cycle |
|---|---|---|
| Hold X s, re-press=OFF | starts cycle | everything OFF now |
| Run to end, ignore | starts cycle | ignored until HOLD ends |
| Re-press restarts | starts cycle | restarts from R1 (tiles kept) |

## Troubleshooting
- Relay clicks at boot: check logic setting matches module jumpers (DIY);
  on Waveshare the logic toggle is a no-op by design.
- Relays stay ON: a hold of 0 means "stay on until STOP" — set the mode's
  hold to the wanted time (and turn loop off first if it complains).
- "stop the sequence first": switch modes only from IDLE (STOP ALL, then set).
- "relays settling": START inside 0.5 s of STOP — wait a beat, tap again.
- "loop needs a finite hold": loop is on with a hold of 0 — set a hold time.
- 188 % shows 100: the meter caps the display; the box provably sends 188.
  Use stage 1 (100) for the realistic demo.
- Page unreachable: confirm joined to `BMS-Tester` (not office Wi-Fi),
  open `192.168.4.1` (or `bmstester.local` from a laptop). `192.168.4.1`
  always works; `.local` needs mDNS (no Android).
- Locked out: hold the button 10 s → factory reset → AP `BMS-Tester`/`bms12345`, admin `admin123`.
- No WiFi at all: GPIO18 (DIY) / DI2 (Waveshare) may be grounded (kill switch) — release it.
- Meter misreads during spoof: expected (stage 1, then the 188 % pattern);
  auto-reverts, `0x04`/`0x05` never change.
