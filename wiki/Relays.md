# Relays + web dashboard (v2.3)

## Wiring the SmartElex-style 12 V 8-channel module
- R1–R8: ESP GPIO **5, 6, 7, 8, 9, 12, 13, 14** → module **IN1–IN8**.
- Module **DC+ / DC−**: separate **12 V supply** (coils ≈ 30 mA each,
  ≈ 240 mA all-on). Tie 12 V GND to ESP GND (common reference for the
  optoisolated inputs). Never power coils from USB/ESP pins.
- Logic default **active-LOW** (LOW = relay ON, boots OFF with no click).
  If the module jumpers select HIGH trigger, flip *Relay logic* on the page.
- Loads: up to 3 A/channel; wire each meter function through NO/COM
  (or NC/COM for fail-safe) per channel.

## Button + spoof inputs
- Button: GPIO15 to GND (internal pull-up). Short press = configured behavior;
  **hold 10 s = factory reset** (wipes AP/admin config, reboots).
- Spoof: GPIO21 to GND (or dashboard FIRE). Stage 1 (100/100/100/100 %, 5 s)
  then stage 2 (88.8 V / 88.8 A / 88.8 °C / 188 %, 10 s) on register `0x03`,
  then auto-revert. Both stages editable.

## Web dashboard (always on, offline OK)
1. Power the ESP → AP **`BMS-Tester`** appears (no office network needed).
2. Join it (default password `bms12345`) — the login page pops up by itself;
   if not, open `192.168.4.1`. Phone clutching mobile data instead? Turn
   mobile data OFF (or tap "stay connected") — the box has no internet.
3. **Relays card:** live 8-tile grid (tap to force ON/OFF; relays past the
   count are greyed out), START / STOP ALL. Header shows cycle + actuation
   counters.
4. **Sequence card:** mode (Sequential 1–N / Chase wave / All ON), relay
   count, step ms, per-mode hold ms (0 = forever), loop + pause + cycle
   limit, ALL-ON stagger, direction, button behavior, logic. Save persists
   to NVS (committed ~1.5 s after the click).
5. **Relay labels card:** name each relay (HORN, LIGHT…) — tiles show names.
6. **Fault spoof card:** stage-1 + stage-2 values + seconds each, pin-enable,
   FIRE now / Cancel.
7. **Firmware card:** auto-check toggle + Check now, STA uplink (hotspot
   SSID/pass for internet), offline firmware-upload page link.
8. **Admin card:** AP SSID/password/channel, admin user/password, boot
   auto-start, Reboot, Factory reset. AP/STA changes apply after reboot.

## Button behaviors (Sequence card → Button)
| Option | Press while idle | Press mid-cycle |
|---|---|---|
| Hold X s, re-press=OFF | starts cycle | everything OFF now |
| Run to end, ignore | starts cycle | ignored until HOLD ends |
| Re-press restarts | starts cycle | restarts from R1 |

## Troubleshooting
- Relay clicks at boot: check logic setting matches module jumpers.
- Relays stay ON: a hold of 0 means "stay on until STOP" — set the mode's
  hold ms to the wanted time.
- 188 % shows 100: the meter caps the display; the box provably sends 188.
  Use stage 1 (100) for the realistic demo.
- Page unreachable: confirm joined to `BMS-Tester` (not office Wi-Fi),
  open `192.168.4.1` (or the gateway IP shown by the OS).
- Locked out: hold the button 10 s → factory reset → `admin`/`admin123`.
- Logged out after reboot: tick "Remember this device" at login (30 days).
- Meter misreads during spoof: expected (stage 1, then the 188 % pattern);
  auto-reverts, `0x04`/`0x05` never change.
