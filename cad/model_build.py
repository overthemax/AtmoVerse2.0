"""AtmoVerse 2.0 - case da scrivania in due pezzi (bozza funzionale).

Linea morbida: sagoma a rettangolo molto arrotondato, spigoli raggiati,
finestra con angoli tondi e supporto cilindrico sul retro (15 gradi).

Assi nel sistema del case: X larghezza, Y profondità (fronte a y=0, retro a
y=DEPTH), Z altezza. Parti:
- "front": cornice con finestra del display, vano e 4 colonnine con inserti M3
- "back":  coperchio con supporto cilindrico, bordo di centraggio e alloggi
           per batteria e moduli; si chiude con 4 viti M3 dal retro
In arancione i componenti acquistati (solo riferimento).
"""
import math
import os
from build123d import (Align, Axis, Box, Cylinder, Plane, Pos, RectangleRounded, Rot,
                       extrude, fillet)
from cad_draft import export_draft

MIN3 = (Align.MIN, Align.MIN, Align.MIN)

# --- Componenti acquistati (misure nominali) ---
PANEL_W, PANEL_H, PANEL_T = 125.0, 99.3, 1.2      # GDEW0583T8, pannello nudo (misurato)
ACTIVE_W, ACTIVE_H = 118.8, 88.2                  # area visibile
ACTIVE_OFF_Z = 2.0                                # area attiva spostata in alto (cavo piatto in basso)
BATT_W, BATT_H, BATT_T = 66.0, 43.0, 13.0         # pacco LiPo (due celle già unite)
MCU_L, MCU_W, MCU_T = 57.5, 25.4, 8.0             # WEMOS LOLIN32 in orizzontale, USB verso destra
MCU_STANDOFF = 4.0                                # sotto la scheda: pin e saldature
SD_W, SD_H = 51.0, 23.0                           # modulo SD
INA_W, INA_H = 20.0, 20.0                         # INA219
RTC_W, RTC_H = 17.0, 16.0                         # DS3231
DRV_W, DRV_H, DRV_T = 45.0, 30.0, 6.0             # scheda driver e-paper: DA MISURARE

# --- Parametri del case ---
WALL = 2.2          # pareti laterali e fronte
CLR = 0.5           # gioco attorno al pannello
BEZEL_SIDE = 8.0    # bordo cornice ai lati e in alto
CHIN = 18.0         # fascia inferiore: piega del cavo piatto
FPC_GAP = 1.5       # spazio dietro al pannello
ELEC_DEPTH = 15.0   # spazio per batteria ed elettronica
BACK_T = 2.5        # coperchio posteriore
TILT_DEG = 15.0
CORNER_R = 16.0     # raggio degli angoli della sagoma
FRONT_EDGE_R = 3.0  # arrotondamento dello spigolo frontale
BACK_EDGE_R = 2.0   # arrotondamento dello spigolo posteriore
WINDOW_R = 3.0      # angoli della finestra del display

POCKET_W, POCKET_H = PANEL_W + 2 * CLR, PANEL_H + 2 * CLR
WIDTH = POCKET_W + 2 * BEZEL_SIDE
HEIGHT = CHIN + POCKET_H + BEZEL_SIDE
DEPTH = WALL + PANEL_T + FPC_GAP + ELEC_DEPTH + BACK_T
FRONT_D = DEPTH - BACK_T
CX, CZ = WIDTH / 2, HEIGHT / 2
pocket_x0 = (WIDTH - POCKET_W) / 2
pocket_z0 = CHIN


def rr_prism(w, h, r, y0, depth, cx=CX, cz=CZ):
    """Prisma a base rettangolare arrotondata nel piano XZ, da y0 a y0+depth."""
    plane = Plane(origin=(cx, y0, cz), x_dir=(1, 0, 0), z_dir=(0, 1, 0))
    return extrude(plane * RectangleRounded(w, h, r), amount=depth)


def y_cyl(x, z, y0, length, d):
    """Cilindro lungo Y da y0 a y0+length."""
    return Pos(x, y0 + length / 2, z) * Rot(90, 0, 0) * Cylinder(d / 2, length)


def union(shapes):
    out = None
    for s in shapes:
        out = s if out is None else out + s
    return out


def edges_at_y(shape, y):
    return shape.edges().filter_by_position(Axis.Y, y - 0.01, y + 0.01)


# --- Viti: M3 dal retro in inserti a caldo M3 (diam. 4.0, lung. 5.7) ---
BOSS_D = 8.0
INSERT_D, INSERT_L = 4.0, 6.5
SCREW_CLEAR_D = 3.4
BOSS_FRONT_Y = WALL + PANEL_T + 0.2   # le colonnine premono gli angoli del pannello
# Sulla diagonale di ogni angolo tondo, fuse con la parete (0.5 mm di sovrapposizione)
inner_r = CORNER_R - WALL
boss_off = CORNER_R - (inner_r - BOSS_D / 2 + 0.5) / math.sqrt(2)
boss_pts = [(boss_off, boss_off), (WIDTH - boss_off, boss_off),
            (boss_off, HEIGHT - boss_off), (WIDTH - boss_off, HEIGHT - boss_off)]

# --- Cornice frontale ---
outer = rr_prism(WIDTH, HEIGHT, CORNER_R, 0, FRONT_D)
outer = fillet(edges_at_y(outer, 0), FRONT_EDGE_R)
outer = fillet(edges_at_y(outer, FRONT_D), 0.8)
cavity = rr_prism(WIDTH - 2 * WALL, HEIGHT - 2 * WALL, inner_r, WALL, FRONT_D)
win_w, win_h = ACTIVE_W + 1.0, ACTIVE_H + 1.0
win_cz = pocket_z0 + CLR + (PANEL_H - ACTIVE_H) / 2 + ACTIVE_OFF_Z - 0.5 + win_h / 2
window = rr_prism(win_w, win_h, WINDOW_R, -0.1, WALL + 0.2, cz=win_cz)
# Pannello tenuto in sede da 3 alette sul bordo (oltre alle colonnine negli angoli)
tab_y = WALL + PANEL_T + 0.2
tabs = union([
    Pos(CX - 10, tab_y, HEIGHT - BEZEL_SIDE - 2) * Box(20, 2.0, BEZEL_SIDE - WALL + 2.1, align=MIN3),
    Pos(WALL - 0.1, tab_y, CZ - 10) * Box(BEZEL_SIDE - WALL + 2.1, 2.0, 20, align=MIN3),
    Pos(WIDTH - BEZEL_SIDE - 2, tab_y, CZ - 10) * Box(BEZEL_SIDE - WALL + 2.1, 2.0, 20, align=MIN3),
])
# LOLIN32 a destra, con la presa USB a 0.5 mm dalla parete destra
mcu_x0 = WIDTH - WALL - 0.5 - MCU_L
MCU_Z0 = 40.0
mcu_pcb_y = FRONT_D - MCU_STANDOFF - 1.6     # faccia componenti della scheda
usb_y = mcu_pcb_y - 1.5                      # centro della presa micro-USB
usb_slot = Pos(WIDTH - WALL / 2, usb_y, MCU_Z0 + MCU_W / 2) * Rot(0, 90, 0) * \
    Box(12.0, 8.0, WALL + 2.0)  # attraversa la parete destra
bosses = union([y_cyl(x, z, BOSS_FRONT_Y, FRONT_D - BOSS_FRONT_Y, BOSS_D) for x, z in boss_pts])
inserts = union([y_cyl(x, z, FRONT_D - INSERT_L, INSERT_L + 0.1, INSERT_D) for x, z in boss_pts])
front = outer - cavity + bosses + tabs - window - usb_slot - inserts

# --- Coperchio posteriore ---
back = rr_prism(WIDTH, HEIGHT, CORNER_R, FRONT_D, BACK_T)
back = fillet(edges_at_y(back, DEPTH), BACK_EDGE_R)
back = fillet(edges_at_y(back, FRONT_D), 0.8)
# Bordo di centraggio che entra nel vano della cornice (gioco 0.3 per lato)
LIP_W, LIP_H, LIP_CLR = 1.6, 3.0, 0.3
lip_o = WALL + LIP_CLR
lip = rr_prism(WIDTH - 2 * lip_o, HEIGHT - 2 * lip_o, CORNER_R - lip_o, FRONT_D - LIP_H, LIP_H + 0.1) - \
    rr_prism(WIDTH - 2 * (lip_o + LIP_W), HEIGHT - 2 * (lip_o + LIP_W), CORNER_R - lip_o - LIP_W,
             FRONT_D - LIP_H - 0.1, LIP_H + 0.3) - \
    union([y_cyl(x, z, FRONT_D - LIP_H - 0.1, LIP_H + 0.3, BOSS_D + 1.0) for x, z in boss_pts])
back = back + lip

# Alloggio batteria: quattro angolari alti 8 mm (la batteria si ferma con biadesivo)
BATT_X0, BATT_Z0 = WALL + 6, 46.0
cradle_h, cradle_t, cradle_l = 8.0, 1.6, 10.0
bx0, bz0 = BATT_X0 - 0.5 - cradle_t, BATT_Z0 - 0.5 - cradle_t
bx1, bz1 = BATT_X0 + BATT_W + 0.5, BATT_Z0 + BATT_H + 0.5
corners = []
for cx, cz, sx, sz in [(bx0, bz0, 1, 1), (bx1, bz0, -1, 1), (bx0, bz1, 1, -1), (bx1, bz1, -1, -1)]:
    ax = cx if sx > 0 else cx + cradle_t - cradle_l
    az = cz if sz > 0 else cz + cradle_t - cradle_l
    corners.append(Pos(ax, FRONT_D - cradle_h, cz) * Box(cradle_l, cradle_h + 0.1, cradle_t, align=MIN3))
    corners.append(Pos(cx, FRONT_D - cradle_h, az) * Box(cradle_t, cradle_h + 0.1, cradle_l, align=MIN3))
back = back + union(corners)

# Moduli: cornicette basse (si fissano con biadesivo, i fori non servono)
FRAME_H, FRAME_T, FRAME_CLR = 3.0, 1.2, 0.4


def frame(x0, z0, w, h, height=FRAME_H):
    ow, oh = w + 2 * (FRAME_CLR + FRAME_T), h + 2 * (FRAME_CLR + FRAME_T)
    o = Pos(x0 - FRAME_CLR - FRAME_T, FRONT_D - height, z0 - FRAME_CLR - FRAME_T) * \
        Box(ow, height + 0.1, oh, align=MIN3)
    i = Pos(x0 - FRAME_CLR, FRONT_D - height - 0.1, z0 - FRAME_CLR) * \
        Box(w + 2 * FRAME_CLR, height + 0.3, h + 2 * FRAME_CLR, align=MIN3)
    return o - i


# LOLIN32: quattro appoggi alti MCU_STANDOFF sotto i bordi e due sponde
mcu_supports = union([
    Pos(x, FRONT_D - MCU_STANDOFF, z) * Box(6.0, MCU_STANDOFF + 0.1, 3.0, align=MIN3)
    for x in (mcu_x0 + 4, mcu_x0 + MCU_L - 10) for z in (MCU_Z0, MCU_Z0 + MCU_W - 3)
]) + union([
    Pos(mcu_x0 + 10, FRONT_D - MCU_STANDOFF - 2.5, z) * Box(30.0, MCU_STANDOFF + 2.6, FRAME_T, align=MIN3)
    for z in (MCU_Z0 - FRAME_CLR - FRAME_T, MCU_Z0 + MCU_W + FRAME_CLR)
])
# Colonna destra dal basso: LOLIN32, SD, INA219 + RTC. La scheda driver sta in
# basso al centro: il cavo piatto del pannello esce lì ed è corto.
right_x0 = mcu_x0 + 2
SD_Z0 = MCU_Z0 + MCU_W + 4
INA_Z0 = SD_Z0 + SD_H + 4
RTC_X0 = right_x0 + INA_W + 6
DRV_X0, DRV_Z0 = CX - DRV_W / 2, 7.0
modules = union([
    frame(right_x0, SD_Z0, SD_W, SD_H),
    frame(right_x0, INA_Z0, INA_W, INA_H),
    frame(RTC_X0, INA_Z0, RTC_W, RTC_H),
    frame(DRV_X0, DRV_Z0, DRV_W, DRV_H),
])
back = back + mcu_supports + modules

# Fori passanti per le viti, feritoie di aerazione arrotondate
back = back - union([y_cyl(x, z, FRONT_D - LIP_H - 0.2, BACK_T + LIP_H + 0.4, SCREW_CLEAR_D) for x, z in boss_pts])

# Tasto a sfioramento: la vite in alto a destra (guardando il retro, quindi
# x piccola) fa da elettrodo. Un capocorda ad anello M3 si infila sotto la
# testa della colonnina, in un incavo sul lato interno del coperchio, e un filo
# lo collega al GPIO2 (ingresso touch T2) della LOLIN32.
TOUCH_X, TOUCH_Z = boss_pts[2]            # (boss_off, HEIGHT - boss_off)
LUG_D, LUG_DEPTH, LUG_TAIL_W, LUG_TAIL_L = 7.5, 0.8, 3.5, 12.0
lug_seat = y_cyl(TOUCH_X, TOUCH_Z, FRONT_D - 0.1, LUG_DEPTH + 0.1, LUG_D)
# Scanalatura per la linguetta del capocorda, lungo la diagonale verso l'interno
lug_tail = Pos(TOUCH_X, FRONT_D - 0.1, TOUCH_Z) * Rot(0, 45, 0) * \
    Pos(LUG_TAIL_L / 2, (LUG_DEPTH + 0.1) / 2, 0) * Box(LUG_TAIL_L, LUG_DEPTH + 0.1, LUG_TAIL_W)
back = back - lug_seat - lug_tail
back = back - union([rr_prism(4.0, 14.0, 1.9, FRONT_D - 0.1, BACK_T + 0.2, cx=CX - 25 + i * 10, cz=HEIGHT - 26)
                     for i in range(6)])

# --- Supporto cilindrico sul retro, stampato insieme al coperchio ---
# Il case poggia sullo spigolo inferiore posteriore e sul cilindro, inclinato
# di TILT_DEG. Centro del cilindro: tocca il tavolo nella posa inclinata.
STAND_R, STAND_L, STAND_OUT = 20.0, 90.0, 12.0
t = math.radians(TILT_DEG)
stand_y = DEPTH + STAND_OUT
stand_z = (STAND_R + STAND_OUT * math.sin(t)) / math.cos(t)
stand = Pos(CX, stand_y, stand_z) * Rot(0, 90, 0) * Cylinder(STAND_R, STAND_L)
stand = fillet(stand.edges(), 3.0)
# Solo la parte dietro al coperchio (non deve entrare nel vano)
stand = stand - Pos(CX - STAND_L, 0, 0) * Box(2 * STAND_L, DEPTH - 0.6, HEIGHT, align=MIN3)
back = back + stand

# --- Riferimenti dei componenti ---
panel_ref = Pos(pocket_x0 + CLR, WALL, pocket_z0 + CLR) * Box(PANEL_W, PANEL_T, PANEL_H, align=MIN3)
battery = Pos(BATT_X0, FRONT_D - BATT_T, BATT_Z0) * Box(BATT_W, BATT_T, BATT_H, align=MIN3)
mcu = Pos(mcu_x0, FRONT_D - MCU_STANDOFF - MCU_T, MCU_Z0) * Box(MCU_L, MCU_T, MCU_W, align=MIN3)
sd = Pos(right_x0, FRONT_D - 4.0, SD_Z0) * Box(SD_W, 4.0, SD_H, align=MIN3)
ina = Pos(right_x0, FRONT_D - 4.0, INA_Z0) * Box(INA_W, 4.0, INA_H, align=MIN3)
rtc = Pos(RTC_X0, FRONT_D - 4.0, INA_Z0) * Box(RTC_W, 4.0, RTC_H, align=MIN3)
drv = Pos(DRV_X0, FRONT_D - DRV_T, DRV_Z0) * Box(DRV_W, DRV_T, DRV_H, align=MIN3)
refs = {"panel": panel_ref, "battery": battery, "lolin32": mcu, "epd-driver": drv,
        "sd": sd, "ina219": ina, "rtc": rtc}


def on_desk(shape):
    """Posa sul tavolo: rotazione attorno allo spigolo inferiore posteriore."""
    return Pos(0, DEPTH, 0) * Rot(-TILT_DEG, 0, 0) * Pos(0, -DEPTH, 0) * shape


if os.environ.get("ATMO_EXPLODED"):
    # Vista esplosa per controllare l'interno: cornice girata di 180 gradi
    # (lato interno verso l'osservatore), coperchio affiancato
    def inside(shape):
        return Pos(CX, DEPTH / 2, 0) * Rot(0, 0, 180) * Pos(-CX, -DEPTH / 2, 0) * shape
    shift = Pos(WIDTH + 30, 0, 0)
    export_draft({"front": inside(front), "back": shift * back},
                 references={k: (inside(v) if k == "panel" else shift * v) for k, v in refs.items()})
    raise SystemExit(0)

export_draft(
    {"front": on_desk(front), "back": on_desk(back)},
    references={k: on_desk(v) for k, v in refs.items()},
    construction_features={
        "front-body": {"owner": "front", "role": "solid"},
        "electronics-cavity": {"owner": "front", "role": "cutter"},
        "viewing-window": {"owner": "front", "role": "cutter"},
        "panel-tabs": {"owner": "front", "role": "solid"},
        "usb-access": {"owner": "front", "role": "cutter"},
        "insert-bosses": {"owner": "front", "role": "solid"},
        "insert-holes": {"owner": "front", "role": "cutter"},
        "back-cover": {"owner": "back", "role": "solid"},
        "locating-lip": {"owner": "back", "role": "solid"},
        "battery-cradle": {"owner": "back", "role": "solid"},
        "mcu-supports": {"owner": "back", "role": "solid"},
        "module-frames": {"owner": "back", "role": "solid"},
        "screw-holes": {"owner": "back", "role": "cutter"},
        "back-vents": {"owner": "back", "role": "cutter"},
        "cylinder-stand": {"owner": "back", "role": "solid"},
    },
)
