# Dashboard field reference (v2.8)

![FULL dashboard](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-full.png)

*FULL variant: bench strip, tiles, meters-today. CLASSIC is pixel-identical
to v2.6; LITE keeps only tiles + names + LINK:*

![LITE dashboard](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/dash-lite.png)

> **Three variants, one per build** (`WEB_UI_VARIANT` in `platformio.ini`):
> CLASSIC (`s3-classic`) = v2.6 page verbatim; **FULL** (default envs) =
> everything below + live SVG bench card; LITE (`s3-lite`) = Relays card +
> Relay labels card + LINK pill only. Relay names are NVS-persistent in all
> three. OTA pulls FULL builds, so updating never strands a classic/lite box.
> v2.8 ships FULL for generic boards and FULL for the Waveshare board.

Open: join AP **`BMS-Tester`** (`bms12345`) → phones get a landing page with
a big **Open Dashboard** button + Safari/Chrome steps (the mini-browser
can't hold the live page — open `192.168.4.1` in Safari/Chrome instead;
laptops use `bmstester.local`). Desktops typing any URL land on the
dashboard directly. No login — the page is open;
reboot/reset/upload/saves ask the admin password per request
(default `admin123`). The 1 s tick refreshes **status only**; your edits are
never clobbered. Every Save commits to NVS ~1.5 s after the click
(RAM reacts instantly).

## Bench card (FULL only)

- Inline SVG, zero CDN: 48V BUS → 12V/5V bucks → ESP32-S3 → MAX485 (amber
  DE dot = transmitting) → animated A/B flow → meter box with live
  `52.0V 0.0A 100%` readout (`mv`/`ma`/`msoc` in `/api/state`, read-only).
  `mv`/`ma`/`msoc` mirror the last `0x03` reply: golden 520/0/100, or the
  active spoof stage's values while spoofing. Read-only — never saved,
  never restored.
- 8 relay blocks glow green with the coils; flow speed follows load.
  Purely visual — every control lives in the cards below as before.

## Relays card
- **START / STOP ALL**: start restarts from R1 (refused 0.5 s after a stop:
  "relays settling"); STOP ALL = everything OFF + manual forces cleared +
  loop cancelled (counters kept for QC).
- **Tiles**: live R1–R8 (tap = force ON/OFF). Past the Count = greyed out.
  Tile taps need IDLE (LITE note). Forcing a tile during CHASE stops the
  wave first (two ON at once would break the one-at-a-time promise).
  Header: `seq RUNNING/IDLE · cyc N · act M` (completed cycles, turn-ON edges),
  plus two round link dots (green/red mirrors of the LINK pill) at the top.

## Meters today card (approx)
- Live `meters N · attempts A · pass P · fail F`, estimated from link gaps
  (the JBD protocol carries no meter ID — this is a software estimate,
  never per-unit exact):
  - First GREEN sighting since boot opens meter #1 (no verdict).
  - A RED gap ≥ 3 s **arms** a reseat candidate; the candidate **commits**
    as a new meter only after GREEN holds 5 s (fumble guard — a scratchy
    reseat counts one unit, not two).
  - Retries with the unit plugged in (steady GREEN) never open a meter.
    Sub-3 s flickers (slow poll, noise) stay on the same meter.
  - Every accepted START opens an **attempt**; a completed cycle latches
    **pass**; a meter closed with no completed cycle counts **fail**.
  - The closing meter's verdict is snapshotted when the candidate arms, so
    an eager START during the 5 s settle attributes to the new meter.
  - A yank mid-cycle invalidates the test: the previous meter closes as fail.
- **New day (reset)**: clear all four (asks first). No RTC — you declare
  the day boundary; reboots never clear; a seated unit re-opens as #1.
- NVS keys `m_met`/`m_att`/`m_ps`/`m_fl`, flushed on close/reset only
  (1 write per meter max, never per actuation/tick). Counters are NOT in
  backups — a restore must not resurrect yesterday's tallies.
- Console: `DAYRESET` (admin password required); `STATUS` prints the batch.

## Sequence config card (only the active mode's rows show)
| Field | Units | Range | Default | Applies to | Notes |
|---|---|---|---|---|---|
| Mode | — | Sequential / Chase / All ON | Sequential | all | Save; **needs STOP first** (mode switch needs IDLE) |
| Relays (Count) | relays | 1–8 | 8 | all | Save; shrink acts immediately — relays past the new count drop the same tick, their manual forces are cleared, regrow never resurrects a stale ON |
| Step ms | ms | 100–60000 | 250 | Sequential, Chase | Save |
| Hold sequential ms | ms, 0 = stay ON | 0–3600000 | 30000 | Sequential | Save |
| Chase sweeps | sweeps, 0 = forever | 0–100 | 3 | Chase | Save; auto-hold = sweeps × relays × step |
| Hold all-on ms | ms, 0 = stay ON | 0–3600000 | 300000 | All ON | Save |
| All-ON stagger ms | ms, 0 = slam all at once | 0–1000 | 50 | All ON | Save |
| Direction | R1→Rn / Rn→R1 | — | R1→Rn | Sequential, Chase | Save |
| Button | 3 behaviors | — | Hold-abort | all | Save (see [[Relays]] for the behaviors) |
| Logic | Active-LOW / HIGH | — | LOW | all | Save (match module jumpers!); **no-op on the Waveshare board** (TCA9554 is fixed HIGH = ON) |
| Loop cycles | on/off | — | off | all | Save; **needs a finite hold** (hold 0 = forever is refused with a named error) |
| Pause ms | ms (all OFF, coil cooling) | 500–60000 | 2000 | loop | Save |
| Cycle limit | cycles, 0 = forever | 0–60000 | 0 | loop | Save |
| Boot auto-start | on/off | — | off | all | Save (burn-in racks) |

Validation rules (all enforced with named errors, nothing silently clamped):
- Mode switch needs IDLE: "stop the sequence first, then switch mode".
- Loop needs a finite hold: "loop needs a finite hold (hold 0 = forever)".
- START refused 0.5 s after STOP: "relays settling — wait a beat, then START"
  (relay settle dead-band; armatures still releasing). Loop restarts ride the
  same rule via the ≥ 500 ms pause floor.

## Relay labels card
- R1–R8 names (HORN, LIGHT…): letters/numbers, shown on tiles. Save persists
  to NVS (`lbl0`–`lbl7`; display-only, length-capped, quotes/backslashes and
  control chars rejected).

## Fault spoof card
- **Enable** + **Trigger GPIO** + **Trigger polarity** (pull LOW = default
  pull-up wiring, or pull HIGH). Safe-pin allowlist enforced:
  - DIY: 1, 2, 21, 38–44, 47 (anything else → 21).
  - Waveshare: DI1–DI8 (GPIO4–11, active LOW; anything else → DI1).
- **Save trigger** stores the enable+pin+polarity triple **without firing** —
  for physical-switch users (previously the only save path was FIRE).
- **Stage 1**: V/A/°C in ×0.1 units (0–9999), SOC % (0–255), seconds (1–120).
  Defaults: 100.0 / 100.0 / 100.0 / 100 %, 5 s (realistic full pack).
- **Stage 2**: same fields. Defaults: 88.8 / 88.8 / 88.8 / 188 %, 10 s
  (deliberately out-of-range pattern; the meter may cap 188 % at 100 — the
  box provably sends 188).
- **FIRE now** = save + trigger the two-stage plan (stage 1 → stage 2 →
  auto-revert to golden). **Save only** = persist without firing.
  **Cancel** = disarm (saves nothing).
- Spoofing only changes register `0x03`; `0x04`/`0x05` never change.

## Firmware update card
- Status line: OTA state (`never checked` / `up to date` / `update
  available: vX.Y`) + latest tag.
- **Auto-check + Every-h** (0 = manual only) + **Check now** (needs STA).
  Both boards check `/releases/latest` since v2.8 (unified release line).
- **Install update** (appears when pending; needs STA; reboots on success).
  Generic boxes download `firmware.bin`; Waveshare boxes download
  `waveshare-firmware.bin` (exact variant-asset match — no cross-flash).
- **Firmware upload** link: offline `/update` page with progress bar;
  Tasmota-grade gates *before* writing — `0xE9` image magic, flash-size
  vs chip, explicit sketch budget, variant match. A wrong file or password
  changes nothing — the box keeps running.
- **Custom firmware URL** (Tasmota OtaUrl style): blank = GitHub releases;
  must be `http(s)….bin`. **Upgrade from URL** installs it (needs STA).
- **STA uplink**: hotspot SSID/pass (blank = keep), enable flag.
  **Test uplink** joins for ≤ 30 s *without rebooting*, reports
  `joined, RSSI -58 dBm, IP …` or a named failure, drops back to AP-only.
  Check/Install/URL-join with the saved creds themselves when boot STA is
  off (on-demand, Tasmota-style) — so test-then-install just works.
  Saved STA applies at boot (Save + reboot).
- **Backup configuration**: downloads all settings as structured JSON
  (`docs/CONFIG-SCHEMA.md`): `{"config":2,"relays":{…},"spoof":{…},
  "trigger":{…},"network":{…},"ota":{…},"meta":{…}}`. **Passwords never
  leave the box** (in no section, by design). **Restore**: v2 sectioned or
  v1 flat backups accepted; every value passes the same clamps; passwords
  re-entered by hand. Meter counters are excluded from backups.

## Admin & Wi-Fi AP card
- AP SSID / Pass (8+, blank = keep) / Channel 1–13; New admin pass (4+).
- Auto-start sequence on boot (burn-in).
- **Save** / **Save + reboot** (AP/STA apply at reboot) / **Reboot** /
  **Factory reset** (wipes everything incl. passwords) /
  **Reset settings (keep Wi-Fi)** (wipes bench settings, keeps AP/STA/admin) /
  **Reset boot counter**.

## Information card
- Firmware + variant (`8mb` / `n16r8` / `waveshare`), flash KB, free sketch
  bytes, heap/PSRAM bytes, uptime, boot counter, last reset reason, STA
  RSSI/IP/MAC, and the fixed pin map for the build (relays, UART, button,
  kill, spoof, safe spares) — the Waveshare build reports its TCA9554/DI map.

## Console card
One-line commands (case-insensitive):
- Need the admin password: `START` `STOP` `FIRE` `CANCEL` `DAYRESET`
  `REBOOT` `RESET`.
- Free: `STATUS` `UPTIME` `VERSION` `HELP`.
- `STATUS` prints e.g.
  `LINK GREEN seq=IDLE cyc=3 act=24 spoof=0 met=5 att=5 ps=4 fl=1`.
- `VERSION` prints e.g. `bms-tester 2.8 (8mb)`.
- `DAYRESET` = the Meters-today New-day reset. `RESET` = factory reset.
- Unknown verbs answer `unknown command (try HELP)`.

## Network behavior
- Always-on AP `BMS-Tester` from every boot; fixed `192.168.4.1`.
- Captive portal: joining phones get the landing page (big Open-Dashboard
  button + Safari/Chrome steps); any other URL 302s to `/`. Turn mobile
  data off if the phone routes around the "no internet" network.
- mDNS: `bmstester.local` (laptops; no Android mDNS).
- STA uplink is optional and off by default; the box is fully usable on the
  offline AP.
