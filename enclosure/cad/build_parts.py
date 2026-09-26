"""BMS bench-station printable parts — parametric FreeCAD model.

Parts:
  1. tray   — internal mounting plate, 3 zones (logic / relay / power) + barrier wall

(The enclosure shell is built by build_enclosure.py: base + lid2 one-shot cover.
The old separate display bezel was superseded by lid2's integrated TFT mount.)

All dimensions in mm. Values marked VERIFY are typical module dims —
measure your actual parts and tweak here, then re-run.

Run:  freecadcmd build_parts.py        (outputs .FCStd + .stl into ./cad/)
"""
import FreeCAD
import Part
import Mesh
import os

OUT = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- parameters
TRAY_L, TRAY_W, TRAY_T = 226, 176, 3          # fits 240x190x90 IP65 box
BOX_HOLE_D, BOX_HOLE_INSET = 4.5, 10         # tray -> box mounting

ESP_L, ESP_W = 52.5, 28.5                    # VERIFY ESP32-S3-DevKitC-1
MAX485_L, MAX485_W = 44, 14                  # VERIFY MAX485 module
RELAY_L, RELAY_W = 56, 138                   # VERIFY SmartElex 8ch (rotated 90 deg)
RELAY_HOLE_INSET = 4
RELAY_BOSS_D, RELAY_BOSS_H, RELAY_HOLE_D = 8, 10, 3.2
RELAY_MOD_H = 18                             # VERIFY relay module height
BUCK_L, BUCK_W = 45, 23                      # VERIFY buck modules
BUCK_BOSS_D, BUCK_BOSS_H, BUCK_HOLE_D = 7, 8, 3.2
BUCK_HOLE_INSET = 3
BUCK_MOD_H = 15                              # VERIFY buck height
FUSE_L, FUSE_W, FUSE_H = 22, 14, 12          # VERIFY fuse holder
BARRIER_T, BARRIER_H = 3, 25                 # 48V zone barrier wall


def box_at(l, w, h, x, y, z):
    return Part.makeBox(l, w, h, FreeCAD.Vector(x, y, z))


def hole_at(d, x, y, z0=-1, h=20):
    return Part.makeCylinder(d / 2, h, FreeCAD.Vector(x, y, z0))


# ---------------------------------------------------------------- tray
def make_tray():
    s = box_at(TRAY_L, TRAY_W, TRAY_T, 0, 0, 0)
    # box mounting holes
    for x, y in [(BOX_HOLE_INSET, BOX_HOLE_INSET),
                 (TRAY_L - BOX_HOLE_INSET, BOX_HOLE_INSET),
                 (BOX_HOLE_INSET, TRAY_W - BOX_HOLE_INSET),
                 (TRAY_L - BOX_HOLE_INSET, TRAY_W - BOX_HOLE_INSET)]:
        s = s.cut(hole_at(BOX_HOLE_D, x, y))

    keepouts = []
    # ---- logic zone (X 8-64): ESP32 cradle + MAX485
    ex, ey = 10, 20
    keepouts.append(("ESP32-S3", box_at(ESP_L, ESP_W, 12, ex, ey, TRAY_T)))
    for cx, cy in [(8, 18), (56, 18), (8, 42), (56, 42)]:      # cradle clips
        s = s.fuse(box_at(8, 8, 10, cx, cy, TRAY_T))
    keepouts.append(("MAX485", box_at(MAX485_L, MAX485_W, 10, 10, 60, TRAY_T)))

    # ---- relay zone (X 68-128): module on standoffs
    rx, ry = 70, 12
    for hx, hy in [(rx + RELAY_HOLE_INSET, ry + RELAY_HOLE_INSET),
                   (rx + RELAY_L - RELAY_HOLE_INSET, ry + RELAY_HOLE_INSET),
                   (rx + RELAY_HOLE_INSET, ry + RELAY_W - RELAY_HOLE_INSET),
                   (rx + RELAY_L - RELAY_HOLE_INSET, ry + RELAY_W - RELAY_HOLE_INSET)]:
        s = s.fuse(Part.makeCylinder(RELAY_BOSS_D / 2, RELAY_BOSS_H,
                                     FreeCAD.Vector(hx, hy, TRAY_T)))
        s = s.cut(hole_at(RELAY_HOLE_D, hx, hy, z0=0,
                          h=TRAY_T + RELAY_BOSS_H + 2))
    keepouts.append(("Relay8ch", box_at(RELAY_L, RELAY_W, RELAY_MOD_H,
                                       rx, ry, TRAY_T + RELAY_BOSS_H)))

    # ---- power zone (X 132-218): bucks + fuse, behind barrier wall
    for bx, by in [(136, 12), (136, 44)]:
        for hx, hy in [(bx + BUCK_HOLE_INSET, by + BUCK_HOLE_INSET),
                       (bx + BUCK_L - BUCK_HOLE_INSET, by + BUCK_HOLE_INSET),
                       (bx + BUCK_HOLE_INSET, by + BUCK_W - BUCK_HOLE_INSET),
                       (bx + BUCK_L - BUCK_HOLE_INSET, by + BUCK_W - BUCK_HOLE_INSET)]:
            s = s.fuse(Part.makeCylinder(BUCK_BOSS_D / 2, BUCK_BOSS_H,
                                         FreeCAD.Vector(hx, hy, TRAY_T)))
            s = s.cut(hole_at(BUCK_HOLE_D, hx, hy, z0=0,
                              h=TRAY_T + BUCK_BOSS_H + 2))
        keepouts.append(("Buck", box_at(BUCK_L, BUCK_W, BUCK_MOD_H,
                                        bx, by, TRAY_T + BUCK_BOSS_H)))
    keepouts.append(("Fuse", box_at(FUSE_L, FUSE_W, FUSE_H, 136, 80, TRAY_T)))

    # barrier wall with wire pass-through notch
    s = s.fuse(box_at(BARRIER_T, 152, BARRIER_H, 128, 8, TRAY_T))
    s = s.cut(box_at(BARRIER_T + 1, 24, 12, 127.5, 100, TRAY_T + 13))
    return s, keepouts


def build(name, maker):
    doc = FreeCAD.newDocument(name)
    shape, keepouts = maker()
    main = doc.addObject("Part::Feature", name)
    main.Shape = shape
    for kname, kshape in keepouts:
        k = doc.addObject("Part::Feature", "KO_" + kname)
        k.Shape = kshape
        if k.ViewObject is not None:  # headless freecadcmd has no ViewObject
            k.ViewObject.Transparency = 75
            k.ViewObject.ShapeColor = (1.0, 0.6, 0.0)
    doc.recompute()
    fcstd = os.path.join(OUT, name + ".FCStd")
    stl = os.path.join(OUT, name + ".stl")
    doc.saveAs(fcstd)
    Mesh.export([main], stl)
    print("wrote", fcstd, "and", stl)
    FreeCAD.closeDocument(name)


build("tray", make_tray)
print("DONE")
