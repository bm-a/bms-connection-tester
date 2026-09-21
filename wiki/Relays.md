# Relays + web dashboard (v2.0)

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
- Spoof: GPIO21 to GND (or dashboard FIRE). 10 s default of
  88.8 V / 88.8 A / 88.8 °C / 188 % on register `0x03`, then auto-revert.

## Web dashboard (always on, offline OK)
1. Power the ESP → AP **`BMS-Tester`** appears (no office network needed).
2. Join it (default password `bms12345`), open `192.168.4.1`, log in
   (`admin` / `admin123` — change immediately in Admin card).
3. **Relays card:** live 8-tile grid (tap to force ON/OFF), START / STOP ALL.
4. **Sequence card:** mode (Sequential 1–8 / All ON), step ms, hold s
   (0 = forever), button behavior (hold+abort / locked / restart), logic.
   Save persists to NVS (survives reboot).
5. **Fault spoof card:** values + duration + pin-enable, FIRE now / Cancel.
6. **Admin card:** AP SSID/password/channel, admin user/password,
   Reboot, Factory reset. AP changes apply after reboot.

## Button behaviors (Sequence card → Button)
| Option | Press while idle | Press mid-cycle |
|---|---|---|
| Hold X s, re-press=OFF | starts cycle | everything OFF now |
| Run to end, ignore | starts cycle | ignored until HOLD ends |
| Re-press restarts | starts cycle | restarts from R1 |

## Troubleshooting
- Relay clicks at boot: check logic setting matches module jumpers.
- Page unreachable: confirm joined to `BMS-Tester` (not office Wi-Fi),
  open `192.168.4.1` (or the gateway IP shown by the OS).
- Locked out: hold the button 10 s → factory reset → `admin`/`admin123`.
- Meter misreads during spoof: expected for 10 s (188 % is an intentional
  out-of-range pattern); auto-reverts, `0x04`/`0x05` never change.
