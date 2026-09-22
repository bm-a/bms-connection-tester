# Config schema v2 (firmware v2.5+; meters added v2.6)

One shared validation table, enforced identically by `/api/config`,
`/api/spoof`, `/api/restore`, and the emulation drivers. NVS stays flat
key-value (namespace `bms2`); structure lives in the JSON export layer.
Secrets (`ap_pass`, `admin_pass`, `sta_pass`) are in **no** section —
never exported, never restored, re-entered by hand.

Backup shape: `{"config":2,"fw":"2.6","relays":{...},"spoof":{...},
"trigger":{...},"network":{...},"ota":{...},"meta":{...}}`.
Restore accepts v2 sectioned **and** v1 flat (`{"backup":1,...}`, migrated
through the same table); anything else is rejected with a named error.

## relays.* (Sequence card → Save; NVS as shown)

| JSON key | NVS | Range | Default | Notes |
|---|---|---|---|---|
| mode | rmode | 0–2 (seq/chase/all) | 0 | change needs IDLE (R10) |
| count | nrel | 1–8 | 8 | shrink acts same-tick, clears stale forces (R8) |
| step_ms | step | 100–60000 | 250 | seq + chase grid (R4) |
| hold_seq_ms | hseq | 0–3600000, 0 = forever | 30000 | sequential soak |
| chase_sweeps | swp | 0–100, 0 = forever | 3 | auto-hold = sweeps × count × step |
| hold_all_ms | hall | 0–3600000, 0 = forever | 300000 | all-on soak |
| stagger_ms | stag | 0–1000 (0 = slam, warned) | 50 | all-on ramp (R5) |
| direction | dir | 0–1 | 0 | activation order only |
| button | bmode | 0–2 | 0 | abort / lock / restart (restart keeps forces) |
| active_low | alow | bool | true | match module jumpers |
| loop | loop | bool | false | needs finite hold (R9) |
| pause_ms | cpause | 500–60000 | 2000 | all-OFF coil cooling (R6) |
| limit | clim | 0–60000, 0 = forever | 0 | exact N cycles then IDLE |
| autostart | auto | bool | false | starts configured mode at boot |
| labels[8] | rlbl0–7 | 1–11 printable, no quotes | R1–R8 | display only |

## spoof.* (Fault card → FIRE / Save only)

Stage 1 then stage 2 on register `0x03`, then auto-revert. Units ×0.1,
seconds 1–120. `0x04`/`0x05` never change.

| JSON key | NVS | Range | Default |
|---|---|---|---|
| stage1 {v,a,c,soc,secs} | sv,sa,sc,ssoc,ssec | 0–9999, 0–255, 1–120 | 1000,1000,1000,100,5 |
| stage2 {v,a,c,soc,secs} | s2v,s2a,s2c,s2soc,s2sec | same | 888,888,888,188,10 |

## trigger.* (Trigger group → Save trigger; physical pin path)

| JSON key | NVS | Range | Default |
|---|---|---|---|
| enabled | sena | bool | true |
| gpio | spin | allowlist 1,2,21,38–44,47 else 21 | 21 |
| active_low | sinv | bool (false = pull LOW to fire) | false |

## network.* (Admin card → Save / Save + reboot)

| JSON key | NVS | Notes |
|---|---|---|
| ap_ssid | ap_ssid | 1–31 chars, applies after reboot |
| ap_ch | ap_ch | 1–13 |
| sta_en | sta_en | hotspot uplink for OTA only |
| sta_ssid | sta_ssid | applies after reboot |

## ota.* (Firmware card)

| JSON key | NVS | Notes |
|---|---|---|
| auto | ota_auto | daily GitHub check (needs STA) |
| every_h | ota_int | hours, 0 = manual only |
| url | ota_url | blank = GitHub releases; else http(s) .bin |

## meta.*

| JSON key | NVS | Notes |
|---|---|---|
| boot | bootn | boot counter (Reset-99 zeroes, no reboot) |

## meters.* (v2.6 daily meter-test counting — NOT config, NOT in backups)

Daily QC tallies, approximate by design (no meter ID exists on the wire).
Reported read-only in `/api/state` under `cfg` as `m_met/m_att/m_ps/m_fl`.
Fed automatically every loop from the live link state (`web_tick`): a RED
gap ≥ 3 s (`LINK_GAP_NEW_METER_MS`) closed by GREEN = reseat = new meter;
steady GREEN across RESTARTs/retries = same meter; sub-3 s flickers stay;
a gap that elapses mid-cycle defers its close until IDLE. The only operator
control is `/api/meter {"cmd":"reset"}` (manual New-day, no RTC) plus the
console `DAYRESET`. Never through `/api/config` or restore. Deliberately
excluded from backup/restore: a restored backup must not resurrect
yesterday's tallies. NVS keys `m_met/m_att/m_ps/m_fl` (flat, `putUInt`),
flushed on close/reset only (1 write per meter — decades of endurance;
attempts ride along in the same batch, so a power cut loses at most the
current meter's attempts), restored at boot, never cleared at boot.
Verdict rule: every accepted `start()` opens an attempt; a completed cycle
latches pass; closing a meter records pass if a cycle completed since the
last close, else fail ("closed out with no completed cycle" — the only
honest fail signal). A close with no open attempt just opens meter #1.
