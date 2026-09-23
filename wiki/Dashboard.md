# Dashboard field reference (v2.7)

![FULL dashboard](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/dash-full.png)

*FULL variant: bench strip, tiles, meters-today. CLASSIC is pixel-identical
to v2.6; LITE keeps only tiles + names + LINK:*

![LITE dashboard](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/dash-lite.png)

> **Three variants, one per build** (`WEB_UI_VARIANT` in `platformio.ini`):
> CLASSIC (`s3-classic`) = v2.6 page verbatim; **FULL** (default envs) =
> everything below + live SVG bench card; LITE (`s3-lite`) = Relays card +
> Relay labels card + LINK pill only. Relay names are NVS-persistent in all
> three. OTA pulls FULL builds, so updating never strands a classic/lite box.

Open: join AP **`BMS-Tester`** (`bms12345`) → phones get a landing page with
a big **Open Dashboard** button + Safari/Chrome steps (the mini-browser
can't hold the live page — open `192.168.4.1` in Safari/Chrome instead;
laptops use `bmstester.local`). Desktops typing any URL land on the
dashboard directly. No login — the page is open;
reboot/reset/upload/saves ask the admin password per request
(default `admin123`). The 1 s tick refreshes **status only**; your edits are
never clobbered. Every Save commits to NVS ~1.5 s after the click
(RAM reacts instantly).

## Bench card (v2.7 FULL only)

- Inline SVG, zero CDN: 48V BUS → 12V/5V bucks → ESP32-S3 → MAX485 (amber
  DE dot = transmitting) → animated A/B flow → meter box with live
  `52.0V 0.0A 100%` readout (`mv`/`ma`/`msoc` in `/api/state`, read-only).
- 8 relay blocks glow green with the coils; flow speed follows load.
  Purely visual — every control lives in the cards below as before.

## Relays card
- **START / STOP ALL**: start restarts from R1 (refused 0.5 s after a stop:
  "relays settling"); STOP ALL = everything OFF + manual forces cleared +
  loop cancelled (counters kept for QC).
- **Tiles**: live R1–R8 (tap = force ON/OFF). Past the Count = greyed out.
  Header: `seq RUNNING/IDLE · cyc N · act M` (completed cycles, turn-ON edges),
  plus two round link dots (green/red mirrors of the LINK pill) at the top.

## Meters today card (v2.6, approx)
- Live `meters N · attempts A · pass P · fail F`, estimated from link gaps:
  a RED gap ≥ 3 s (reseat) counts a new meter; retries with the unit plugged
  in never do; brief flickers stay. Pass = a full cycle completed before the
  swap. No button — nothing to press.
- **New day (reset)**: clear all four (asks first). No RTC — you declare
  the day boundary; reboots never clear; a seated unit re-opens as #1.

## Sequence config card (only the active mode's rows show)
| Field | Units | Range | Default | Applies to | Save owner |
|---|---|---|---|---|---|
| Mode | — | Sequential / Chase / All ON | Sequential | all | Save (needs STOP to change) |
| Relays (Count) | relays | 1–8 | 8 | all | Save |
| Step ms | ms | 100–60000 | 250 | Sequential, Chase | Save |
| Hold sequential ms | ms, 0 = stay ON | 0–3600000 | 30000 | Sequential | Save |
| Chase sweeps | sweeps, 0 = forever | 0–100 | 3 | Chase | Save (auto-hold = sweeps × relays × step) |
| Hold all-on ms | ms, 0 = stay ON | 0–3600000 | 300000 | All ON | Save |
| All-ON stagger ms | ms, 0 = slam all at once | 0–1000 | 50 | All ON | Save |
| Direction | R1→Rn / Rn→R1 | — | R1→Rn | Sequential, Chase | Save |
| Button | 3 behaviors | — | Hold-abort | all | Save |
| Logic | Active-LOW / HIGH | — | LOW | all | Save (match module jumpers!) |
| Loop cycles | on/off | — | off | all (needs finite hold) | Save |
| Pause ms | ms (all OFF, coil cooling) | 500–60000 | 2000 | loop | Save |
| Cycle limit | cycles, 0 = forever | 0–60000 | 0 | loop | Save |

## Relay labels card
- R1–R8 names (HORN, LIGHT…): letters/numbers, shown on tiles. Save persists.

## Fault spoof card
- **Enable + Trigger GPIO** (safe pins 1, 2, 21, 38–44, 47; else 21) +
  **Trigger polarity** (pull LOW = default pull-up wiring, or pull HIGH).
  **Save trigger** stores the triple without firing — for physical-switch
  users (previously the only save path was FIRE).
- **Stage 1 + Stage 2**: V/A/°C (×0.1 units), SOC %, seconds each (1–120).
- **FIRE now** = save + trigger the two-stage plan. **Save only** = persist
  without firing. **Cancel** = disarm (saves nothing).

## Firmware update card
- Status line: OTA state (`never checked` / `up to date` / `update
  available: vX.Y`) + latest tag.
- **Auto-check + Every-h** (0 = manual only) + **Check now** (needs STA).
- **Install update** (appears when pending; needs STA; reboots on success).
- **Firmware upload** link: offline page with progress bar; the box checks
  variant + image + size *before* writing; wrong file/password changes nothing.
- **Custom firmware URL** (Tasmota OtaUrl style): blank = GitHub releases;
  must be `http(s)…​.bin`. **Upgrade from URL** installs it (needs STA).
- **STA uplink**: hotspot SSID/pass (blank = keep), enable flag.
  **Test uplink** joins for ≤ 30 s *without rebooting*, reports
  `joined, RSSI -58 dBm, IP …` or a named failure, drops back to AP-only.
  Check/Install/URL-join with the saved creds themselves when boot STA is
  off (on-demand, Tasmota-style) — so test-then-install just works.
  Saved STA applies at boot (Save + reboot).
- **Backup configuration**: downloads all settings as structured JSON
  (`docs/CONFIG-SCHEMA.md`: relays/spoof/trigger/network/ota/meta sections,
  passwords never included). **Restore**: v2 sectioned or v1 flat backups;
  values pass the same clamps; passwords re-entered by hand.

## Admin & Wi-Fi AP card
- AP SSID / Pass (8+, blank = keep) / Channel 1–13; New admin pass (4+).
- Auto-start sequence on boot (burn-in).
- **Save** / **Save + reboot** (AP/STA apply at reboot) / **Reboot** /
  **Factory reset** (wipes everything incl. passwords) /
  **Reset settings (keep Wi-Fi)** (wipes bench settings, keeps AP/STA/admin) /
  **Reset boot counter**.

## Information card
- Firmware + variant (`8mb`/`n16r8`), flash KB, free sketch bytes,
  heap/PSRAM bytes, uptime, boot counter, last reset reason, STA
  RSSI/IP/MAC, and the fixed pin map (relays, UART, button, kill, spoof,
  safe spares).

## Console card
- One-line commands: `START STOP FIRE CANCEL STATUS UPTIME VERSION REBOOT
  RESET HELP`. Hardware verbs ask for the admin password; the rest are free.
