# Enclosure CAD — 3D-printable bench-station shell

Parametric FreeCAD models for the BMS connection tester enclosure.
Regenerate everything with:

```
freecadcmd build_enclosure.py   # base + lid2
freecadcmd build_parts.py       # tray
```

## Print set (3 parts)

| Part | File | Size (mm) | Notes |
|------|------|-----------|-------|
| base | `base.stl` | 240 × 190 × 96 | Front panel: E-stop Ø22.5, START Ø19.2, SPOOF Ø16.2, WiFi kill Ø12.5, recessed reset, 2× LED Ø5, USB-C slot, QR recess. Rear: 4× M20 gland holes. Needs ≥250 mm bed |
| lid2 | `lid2.stl` | 248 × 198 × 18 | One-shot cover: integrated TFT mount (no separate bezel), vent slots, skirt, gasket groove. Print upside-down, no supports |
| tray | `tray.stl` | 226 × 176 × 28 | Internal mount: ESP32-S3 cradle, MAX485, 8-relay standoffs, buck mounts, fuse, 48 V barrier wall |

All module dimensions marked `VERIFY` in the scripts are typical values —
measure your actual hardware with calipers before printing.
