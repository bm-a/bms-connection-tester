"""BMS bench-station ENCLOSURE — parametric FreeCAD model.

Parts:
  1. base — 240x190x90 box, 4mm walls; front panel cutouts (E-stop, START,
     SPOOF, WiFi kill, LEDs, USB-C, QR recess); rear M20 gland holes;
     tray bosses; lid-screw flange
  2. lid  — overlapping cover with skirt, gasket groove, TFT bezel cutout

All dimensions in mm. Run: freecadcmd build_enclosure.py
"""
import FreeCAD
import Part
import Mesh
import os

OUT = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- parameters
BOX_L, BOX_W, BOX_H = 240, 190, 90
WALL, BOTTOM = 4, 4
WALL_H = BOX_H - BOTTOM                       # 86

# front panel (y=0 face), (x, z) from bottom-left of outer face, panel 240x90.
# 9 features: 5 switching devices + 2 LEDs + USB-C + QR recess.
ESTOP_D, ESTOP_X, ESTOP_Z = 22.5, 200, 58     # red mushroom E-stop
START_D, START_X, START_Z = 19.2, 100, 32     # green illuminated START
SPOOF_D, SPOOF_X, SPOOF_Z = 16.2, 52, 32      # yellow SPOOF
WIFI_D, WIFI_X, WIFI_Z = 12.5, 140, 32        # WiFi kill toggle (flip guard)
RESET_D, RESET_CB_D, RESET_CB_DP = 8, 16, 3   # recessed reset: hole+counterbore
RESET_X, RESET_Z = 30, 62
LED_D = 5
LED_GRN_X, LED_GRN_Z = 60, 62                 # green LED
LED_RED_X, LED_RED_Z = 80, 62                 # red LED
USBC_L, USBC_H, USBC_X, USBC_Z = 12, 6, 30, 16
QR_S, QR_X, QR_Z = 25, 208, 14                # QR sticker recess

GLAND_D = 20.5                                # rear M20 cable glands
GLAND_XS = [40, 90, 140, 190]
GLAND_Z = 45

# tray mounts: tray 226x176 sits at offset (5,5); holes at tray-local corners
TRAY_OFF = 5
TRAY_HOLES = [(10, 10), (216, 10), (10, 166), (216, 166)]
BOSS_D, BOSS_H, BOSS_HOLE_D = 10, 8, 4.5

# lid screw flange
FLANGE_H = 6
LID_SCREW_D = 3.2
LID_SCREW_XS = [(30, 2), (120, 2), (210, 2), (30, 188), (120, 188), (210, 188)]
LID_CLEAR_D = 3.5

# lid
SKIRT_DROP = 14
GROOVE_W, GROOVE_D = 3, 2.5
BEZ_CUT_L, BEZ_CUT_W = 100, 64                # TFT bezel cutout
BEZ_HOLE_D, BEZ_HOLE_INSET = 3.2, 5


def box_at(l, w, h, x, y, z):
    return Part.makeBox(l, w, h, FreeCAD.Vector(x, y, z))


def hole_y(d, x, z, y0=-1, h=6):
    """Hole through a Y-facing wall (front/rear panels)."""
    return Part.makeCylinder(d / 2, h, FreeCAD.Vector(x, y0, z),
                             FreeCAD.Vector(0, 1, 0))


def hole_z(d, x, y, z0=-1, h=20):
    return Part.makeCylinder(d / 2, h, FreeCAD.Vector(x, y, z0))


# ---------------------------------------------------------------- base
def make_base():
    s = box_at(BOX_L, BOX_W, BOTTOM, 0, 0, 0)                       # bottom
    s = s.fuse(box_at(BOX_L, WALL, WALL_H, 0, 0, BOTTOM))            # front
    s = s.fuse(box_at(BOX_L, WALL, WALL_H, 0, BOX_W - WALL, BOTTOM)) # rear
    s = s.fuse(box_at(WALL, BOX_W - 2 * WALL, WALL_H, 0, WALL, BOTTOM))
    s = s.fuse(box_at(WALL, BOX_W - 2 * WALL, WALL_H,
                      BOX_L - WALL, WALL, BOTTOM))

    # front panel cutouts: 5 switching devices + 2 LEDs + USB-C + QR
    s = s.cut(hole_y(ESTOP_D, ESTOP_X, ESTOP_Z))
    s = s.cut(hole_y(START_D, START_X, START_Z))
    s = s.cut(hole_y(SPOOF_D, SPOOF_X, SPOOF_Z))
    s = s.cut(hole_y(WIFI_D, WIFI_X, WIFI_Z))
    s = s.cut(hole_y(RESET_D, RESET_X, RESET_Z))
    s = s.cut(Part.makeCylinder(RESET_CB_D / 2, RESET_CB_DP + 1,   # recess
                                FreeCAD.Vector(RESET_X, -0.5, RESET_Z),
                                FreeCAD.Vector(0, 1, 0)))
    s = s.cut(hole_y(LED_D, LED_GRN_X, LED_GRN_Z))
    s = s.cut(hole_y(LED_D, LED_RED_X, LED_RED_Z))
    s = s.cut(box_at(USBC_L, 6, USBC_H, USBC_X - USBC_L / 2, -1,
                     USBC_Z - USBC_H / 2))
    s = s.cut(box_at(QR_S, 1.2, QR_S, QR_X - QR_S / 2, -0.5, QR_Z - QR_S / 2))

    # rear gland holes
    for gx in GLAND_XS:
        s = s.cut(hole_y(GLAND_D, gx, GLAND_Z, y0=BOX_W - WALL - 1, h=6))

    # tray mounting bosses
    for hx, hy in TRAY_HOLES:
        bx, by = hx + TRAY_OFF, hy + TRAY_OFF
        s = s.fuse(Part.makeCylinder(BOSS_D / 2, BOSS_H,
                                     FreeCAD.Vector(bx, by, BOTTOM)))
        s = s.cut(hole_z(BOSS_HOLE_D, bx, by, z0=BOTTOM - 2,
                         h=BOSS_H + 4))

    # lid screw flange (frame around the top rim)
    flange = box_at(BOX_L, BOX_W, FLANGE_H, 0, 0, BOX_H)
    flange = flange.cut(box_at(BOX_L - 2 * WALL, BOX_W - 2 * WALL,
                               FLANGE_H + 2, WALL, WALL, BOX_H - 1))
    s = s.fuse(flange)
    for sx, sy in LID_SCREW_XS:
        s = s.cut(hole_z(LID_SCREW_D, sx, sy, z0=BOX_H - 1, h=FLANGE_H + 2))
    return s


# ---------------------------------------------------------------- lid
def make_lid():
    lx, ly = BOX_L + 8, BOX_W + 8          # 248 x 198, overlaps base walls
    s = box_at(lx, ly, 4, 0, 0, 0)        # lid plate (z 0..4)

    # skirt dropping over the base walls
    skirt = box_at(lx, ly, SKIRT_DROP, 0, 0, -SKIRT_DROP)
    skirt = skirt.cut(box_at(BOX_L + 0.5, BOX_W + 0.5, SKIRT_DROP + 2,
                             3.75, 3.75, -SKIRT_DROP - 1))
    s = s.fuse(skirt)

    # gasket groove in the underside (3mm wide ring over the wall tops)
    groove = box_at(BOX_L + 4, BOX_W + 4, GROOVE_D, 2, 2, -GROOVE_D)
    groove = groove.cut(box_at(BOX_L + 4 - 2 * GROOVE_W, BOX_W + 4 - 2 * GROOVE_W,
                               GROOVE_D + 1, 2 + GROOVE_W, 2 + GROOVE_W,
                               -GROOVE_D - 0.5))
    s = s.cut(groove)

    # TFT bezel cutout + mount holes (lid-local; lid extends 4mm past base)
    cx, cy = (lx - BEZ_CUT_L) / 2, (ly - BEZ_CUT_W) / 2
    s = s.cut(box_at(BEZ_CUT_L, BEZ_CUT_W, 6, cx, cy, -1))
    for hx, hy in [(cx + BEZ_HOLE_INSET, cy + BEZ_HOLE_INSET),
                   (cx + BEZ_CUT_L - BEZ_HOLE_INSET, cy + BEZ_HOLE_INSET),
                   (cx + BEZ_HOLE_INSET, cy + BEZ_CUT_W - BEZ_HOLE_INSET),
                   (cx + BEZ_CUT_L - BEZ_HOLE_INSET,
                    cy + BEZ_CUT_W - BEZ_HOLE_INSET)]:
        s = s.cut(hole_z(BEZ_HOLE_D, hx, hy))

    # lid screw clearance holes (base flange holes shifted by +4,+4)
    for sx, sy in LID_SCREW_XS:
        s = s.cut(hole_z(LID_CLEAR_D, sx + 4, sy + 4))
    return s


# ---------------------------------------------------------------- lid v2:
# one-shot 3D-printable cover.
# Integrated TFT mount (no separate bezel), vent slots over the logic zone,
# skirt + gasket groove + lid screw holes.
# PRINT ORIENTATION: upside-down, flat outer face on the bed, skirt up.
# Everything is vertical walls / upward-facing pockets -> no supports needed.
def make_lid_v2():
    lx, ly = BOX_L + 8, BOX_W + 8          # 248 x 198, overlaps base walls
    s = box_at(lx, ly, 4, 0, 0, 0)        # lid plate (z 0..4)

    # skirt dropping over the base walls
    skirt = box_at(lx, ly, SKIRT_DROP, 0, 0, -SKIRT_DROP)
    skirt = skirt.cut(box_at(BOX_L + 0.5, BOX_W + 0.5, SKIRT_DROP + 2,
                             3.75, 3.75, -SKIRT_DROP - 1))
    s = s.fuse(skirt)

    # gasket groove in the underside (3mm wide ring over the wall tops)
    groove = box_at(BOX_L + 4, BOX_W + 4, GROOVE_D, 2, 2, -GROOVE_D)
    groove = groove.cut(box_at(BOX_L + 4 - 2 * GROOVE_W, BOX_W + 4 - 2 * GROOVE_W,
                               GROOVE_D + 1, 2 + GROOVE_W, 2 + GROOVE_W,
                               -GROOVE_D - 0.5))
    s = s.cut(groove)

    # --- integrated TFT mount: 58x44 window through the plate,
    #     4x M2.5 screw posts on the underside for the 86x50 module
    win_l, win_w = 58, 44
    wx, wy = (lx - win_l) / 2, (ly - win_w) / 2
    s = s.cut(box_at(win_l, win_w, 6, wx, wy, -1))
    POST_D, POST_H, POST_PILOT = 6, 8, 2.2
    for px, py in [(85, 78), (163, 78), (85, 120), (163, 120)]:
        s = s.fuse(Part.makeCylinder(POST_D / 2, POST_H,
                                     FreeCAD.Vector(px, py, -POST_H)))
        s = s.cut(hole_z(POST_PILOT, px, py, z0=-POST_H - 1, h=POST_H + 2))

    # --- vent slots over the logic zone (low-voltage side only)
    for vx in [45, 62, 79, 96, 113]:
        s = s.cut(box_at(3, 6, 30, vx - 1.5, -1, 45))

    # lid screw clearance holes (base flange holes shifted by +4,+4)
    for sx, sy in LID_SCREW_XS:
        s = s.cut(hole_z(LID_CLEAR_D, sx + 4, sy + 4))
    return s


def build(name, maker):
    doc = FreeCAD.newDocument(name)
    main = doc.addObject("Part::Feature", name)
    main.Shape = maker()
    doc.recompute()
    fcstd = os.path.join(OUT, name + ".FCStd")
    stl = os.path.join(OUT, name + ".stl")
    doc.saveAs(fcstd)
    Mesh.export([main], stl)
    print("wrote", fcstd, "and", stl)
    FreeCAD.closeDocument(name)


build("base", make_base)
build("lid", make_lid)
build("lid2", make_lid_v2)
print("DONE")
