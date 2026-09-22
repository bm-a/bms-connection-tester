# Relays + sequencer (v2.4)

## Wiring the 12 V 8-channel module
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

## Modes (Sequence card → Mode; only the active mode's fields show)
| Mode | What runs | Fields that matter |
|---|---|---|
| Sequential 1–N | R1→Rn cumulative, one new relay per Step, then Hold | Step, Hold-seq, Direction |
| Chase wave | ONE lit relay sweeping R1→Rn→R1…, 20 ms all-OFF gap between steps (break-before-make: release is slower than pull-in) | Step (= wave speed), Sweeps, Direction |
| All ON at once | All N together, or ramped with Stagger | Hold-all, Stagger |
| Shared (always) | START / STOP ALL / tiles, Count, Button, Logic, Loop + Pause + Limit, Labels, Auto-start | — |

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

## Timing floors (why the clamps are what they are)
- **Step ≥ 100 ms** (default 250): mechanical bounce (5–15 ms) + BMS ADC
  settle (~50–100 ms) — faster risks switching or sampling mid-transition.
- **Stagger default 50 ms** (0 = explicit all-at-once slam, allowed but
  warned): 8 contactors closing together stacks back-EMF into the shared
  12 V rail plus up to 8× load inrush.
- **Chase gap 20 ms, fixed** (not a field): covers release-vs-pull-in skew.
- There is no contact feedback, so these timing floors *are* the settle
  verification — no code can confirm a click without added hardware.

## Button + spoof + WiFi-kill inputs
- Button: GPIO15 to GND (internal pull-up). Short press = configured behavior
  (Hold-abort / Run-lock / Restart — Restart keeps manual tile forces);
  **hold 10 s = factory reset** (wipes AP/admin config, reboots).
- Spoof: trigger GPIO (default 21) to GND (or dashboard FIRE). Safe pins:
  1, 2, 21, 38–44, 47 (anything else → 21). Stage 1 (100/100/100/100 %, 5 s)
  then stage 2 (88.8 V / 88.8 A / 88.8 °C / 188 %, 10 s) on register `0x03`,
  then auto-revert. Both stages editable; **Save only** stages without
  firing, FIRE saves + fires, Cancel disarms. `0x04`/`0x05` never change.
- WiFi kill: GPIO18 to GND drops the AP + portal + server at once (mDNS
  too); release to bring everything back. Default on at boot.

## Daily meter counting (v2.6, approximate — no button, no new GPIO)

The JBD protocol carries no meter ID — the box only sees link state and
relay actuations — so the count is a software estimate from link gaps,
never inferred per-unit exact. Rules:
- First GREEN sighting opens meter #1.
- A RED gap ≥ 3 s closed by GREEN = reseat = new meter (closes the previous
  one, opens the next: `meters/attempts/pass/fail`).
- Steady GREEN across RESTARTs/retries = same meter, always. Sub-3 s
  flickers (slow poll, noise) never open a meter.
- Every accepted START opens an **attempt** (retries included); a completed
  cycle latches **pass**; a meter closed with no completed cycle counts
  **fail** ("closed out with no completed cycle" — the only honest signal).
- A gap that elapses mid-cycle defers its close until IDLE, so the
  in-flight cycle's verdict lands on the right meter.
- Day boundary is manual: **New day (reset)** on the dashboard (no RTC;
  a reboot never clears — a power flicker must not eat QC data; a seated
  unit re-opens as #1 at once).
- 1 NVS write per meter (never per actuation/tick); counters are NOT in
  backups (a restore must not resurrect yesterday's tallies).
- Console: `DAYRESET` (admin-gated); `STATUS` shows the batch.

## Web dashboard: see [[Dashboard]] for every card and field.

## Button behaviors (Sequence card → Button)
| Option | Press while idle | Press mid-cycle |
|---|---|---|
| Hold X s, re-press=OFF | starts cycle | everything OFF now |
| Run to end, ignore | starts cycle | ignored until HOLD ends |
| Re-press restarts | starts cycle | restarts from R1 (tiles kept) |

## Troubleshooting
- Relay clicks at boot: check logic setting matches module jumpers.
- Relays stay ON: a hold of 0 means "stay on until STOP" — set the mode's
  hold to the wanted time (and turn loop off first if it complains).
- "stop the sequence first": switch modes only from IDLE (STOP ALL, then set).
- "relays settling": START inside 0.5 s of STOP — wait a beat, tap again.
- 188 % shows 100: the meter caps the display; the box provably sends 188.
  Use stage 1 (100) for the realistic demo.
- Page unreachable: confirm joined to `BMS-Tester` (not office Wi-Fi),
  open `192.168.4.1` (or `bmstester.local` from a laptop). `192.168.4.1`
  always works; `.local` needs mDNS (no Android).
- Locked out: hold the button 10 s → factory reset → AP `BMS-Tester`/`bms12345`, admin `admin123`.
- No WiFi at all: GPIO18 may be grounded (kill switch) — release it.
- Meter misreads during spoof: expected (stage 1, then the 188 % pattern);
  auto-reverts, `0x04`/`0x05` never change.
