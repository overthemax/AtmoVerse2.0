"""AtmoVerse 2.0 - case da scrivania in due pezzi (bozza funzionale).

Linea morbida: sagoma a rettangolo molto arrotondato, spigoli raggiati,
finestra con angoli tondi. Sul retro un supporto "arco": una piastra inclinata
con un sole nascente ritagliato, che si innesta a coda di rondine nel coperchio;
davanti, due piedini sotto la cornice. Il case sta inclinato di 15 gradi.

Assi nel sistema del case: X larghezza, Y profondità (fronte a y=0, retro a
y=DEPTH), Z altezza. Parti:
- "front": cornice con finestra del display, vano, 4 colonnine con inserti M3
           e due piedini anteriori
- "back":  coperchio con bordo di centraggio, alloggi per batteria e moduli e
           la guida a coda di rondine del supporto; si chiude con 4 viti M3 dal retro
- "stand": piastra "arco" (pezzo separato, entra di lato nella guida)
In arancione i componenti acquistati (solo riferimento).
"""
import math
import os
from build123d import (Align, Axis, Box, Circle, Cylinder, JernArc, Line, Plane, Polygon, Pos,
                       Rectangle, RectangleRounded, Rot, Side, Wire, extrude, fillet, make_face,
                       offset)
from cad_draft import export_draft

MIN3 = (Align.MIN, Align.MIN, Align.MIN)

# --- Componenti acquistati (misure nominali) ---
PANEL_W, PANEL_H, PANEL_T = 125.0, 99.3, 1.2      # GDEW0583T8, pannello nudo (misurato)
ACTIVE_W, ACTIVE_H = 118.8, 88.2                  # area visibile
ACTIVE_OFF_Z = 2.0                                # area attiva spostata in alto (cavo piatto in basso)
BATT_W, BATT_H, BATT_T = 66.0, 43.0, 13.0         # pacco LiPo (due celle già unite)
MCU_L, MCU_W, MCU_T = 50.5, 26.3, 8.0             # WEMOS LOLIN32 (misurata) in orizzontale, USB verso destra
MCU_TOTAL_L = 72.8                                # scheda + adattatore micro-USB -> USB-C (misurata)
ADAPTER_L = 23.3                                  # adattatore da solo (misurato): ~1 mm entra nella presa micro-USB
ADAPTER_W, ADAPTER_T = 11.0, 6.0                  # sezione dell'adattatore (misurata)
ADAPTER_OFF_Y, ADAPTER_OFF_Z = 0.0, 0.0           # centro presa USB-C rispetto alla micro-USB (coassiale, verificato)
ADAPTER_CLR = 0.2                                 # gioco per lato dell'adattatore nell'asola della parete
MCU_STANDOFF = 4.0                                # sotto la scheda: pin e saldature
SD_W, SD_H = 51.0, 23.0                           # modulo SD
INA_W, INA_H = 20.0, 20.0                         # INA219
RTC_W, RTC_H = 17.0, 16.0                         # DS3231
DRV_W, DRV_H, DRV_T = 48.5, 22.7, 6.0             # scheda driver e-paper (misurata; spessore stimato)

# --- Parametri del case ---
WALL = 2.2          # pareti laterali
FRONT_T = 3.6       # parete frontale: 1,2 mm di bordo davanti al pannello + incavo
PANEL_RECESS = 2.4  # incavo in cui il pannello si incastona nella cornice
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
DEPTH = FRONT_T + FPC_GAP + ELEC_DEPTH + BACK_T   # il pannello sta dentro l'incavo
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
PANEL_Y = FRONT_T - PANEL_RECESS      # faccia anteriore del pannello (fondo dell'incavo)
BOSS_FRONT_Y = PANEL_Y + PANEL_T + 0.2   # le colonnine premono gli angoli del pannello
# Sulla diagonale di ogni angolo tondo, fuse con la parete (0.5 mm di sovrapposizione)
inner_r = CORNER_R - WALL
boss_off = CORNER_R - (inner_r - BOSS_D / 2 + 0.5) / math.sqrt(2)
boss_pts = [(boss_off, boss_off), (WIDTH - boss_off, boss_off),
            (boss_off, HEIGHT - boss_off), (WIDTH - boss_off, HEIGHT - boss_off)]

# --- Cornice frontale ---
outer = rr_prism(WIDTH, HEIGHT, CORNER_R, 0, FRONT_D)
outer = fillet(edges_at_y(outer, 0), FRONT_EDGE_R)
outer = fillet(edges_at_y(outer, FRONT_D), 0.8)
cavity = rr_prism(WIDTH - 2 * WALL, HEIGHT - 2 * WALL, inner_r, FRONT_T, FRONT_D)
# Incavo del pannello: profondo PANEL_RECESS dalla faccia interna della parete frontale
recess = Pos(pocket_x0, PANEL_Y, pocket_z0) * Box(POCKET_W, PANEL_RECESS + 0.1, POCKET_H, align=MIN3)
win_w, win_h = ACTIVE_W + 1.0, ACTIVE_H + 1.0
win_cz = pocket_z0 + CLR + (PANEL_H - ACTIVE_H) / 2 + ACTIVE_OFF_Z - 0.5 + win_h / 2
window = rr_prism(win_w, win_h, WINDOW_R, -0.1, PANEL_Y + 0.2, cz=win_cz)
# Pannello tenuto in sede da 3 alette sul bordo (oltre alle colonnine negli angoli)
tab_y = PANEL_Y + PANEL_T + 0.2
tabs = union([
    Pos(CX - 10, tab_y, HEIGHT - BEZEL_SIDE - 2) * Box(20, 2.0, BEZEL_SIDE - WALL + 2.1, align=MIN3),
    Pos(WALL - 0.1, tab_y, CZ - 10) * Box(BEZEL_SIDE - WALL + 2.1, 2.0, 20, align=MIN3),
    Pos(WIDTH - BEZEL_SIDE - 2, tab_y, CZ - 10) * Box(BEZEL_SIDE - WALL + 2.1, 2.0, 20, align=MIN3),
])
# LOLIN32 a destra: l'adattatore USB-C attraversa la parete destra e la sua
# faccia è a filo con l'esterno; il cavo si innesta direttamente da fuori.
mcu_x0 = WIDTH - MCU_TOTAL_L
adapter_x0 = mcu_x0 + MCU_L                  # fine scheda = inizio adattatore
MCU_Z0 = 55.0   # sopra l'alloggio batteria (la scheda ora arriva fin sopra la batteria)
mcu_pcb_y = FRONT_D - MCU_STANDOFF - 1.6     # faccia componenti della scheda
usb_y = mcu_pcb_y - 1.5                      # centro della presa micro-USB
usb_z = MCU_Z0 + MCU_W / 2
usbc_y, usbc_z = usb_y + ADAPTER_OFF_Y, usb_z + ADAPTER_OFF_Z   # centro della presa USB-C
# Asola nella parete destra con la sezione dell'adattatore più il gioco:
# ADAPTER_W lungo Z (larghezza della scheda), ADAPTER_T lungo Y (spessore)
usbc_w, usbc_t = ADAPTER_W + 2 * ADAPTER_CLR, ADAPTER_T + 2 * ADAPTER_CLR
usb_slot = extrude(Plane(origin=(WIDTH - WALL - 1.0, usbc_y, usbc_z), x_dir=(0, 0, 1), z_dir=(1, 0, 0)) *
                   RectangleRounded(usbc_w, usbc_t, ADAPTER_CLR), amount=WALL + 2.0)
bosses = union([y_cyl(x, z, BOSS_FRONT_Y, FRONT_D - BOSS_FRONT_Y, BOSS_D) for x, z in boss_pts])
inserts = union([y_cyl(x, z, FRONT_D - INSERT_L, INSERT_L + 0.1, INSERT_D) for x, z in boss_pts])
front = outer - cavity - recess + bosses + tabs - window - usb_slot - inserts

# Piedini anteriori: due cunei sotto il fondo, vicino al bordo frontale, con la
# faccia inferiore sul piano del tavolo (inclinato di TILT_DEG nel sistema del
# case). Spostano in avanti l'appoggio: premere la vite-touch sul retro non fa
# più ribaltare il case in avanti. Stampati con la cornice (fronte sul piatto)
# non hanno sporgenze: la faccia inclinata rientra salendo.
FOOT_W, FOOT_L = 16.0, 12.0   # larghezza e lunghezza (in profondità) di ogni piedino
FOOT_INSET = 22.0             # distanza dai fianchi (nel tratto diritto del fondo)
FOOT_TOP = 2.0                # si fondono nel fondo (spesso WALL) senza entrare nel vano
TAN_TILT = math.tan(math.radians(TILT_DEG))


def table_z(y):
    """Quota del piano del tavolo nel sistema del case (passa per lo spigolo inferiore posteriore)."""
    return (y - DEPTH) * TAN_TILT


def yz_prism(pts, x0, width):
    """Prisma da un poligono nel piano YZ (punti (y, z)), da x0 a x0+width."""
    plane = Plane(origin=(x0, 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))
    return extrude(plane * Polygon(*pts, align=None), amount=width, dir=(1, 0, 0))


foot_pts = [(0, FOOT_TOP), (FOOT_L, FOOT_TOP), (FOOT_L, table_z(FOOT_L)), (0, table_z(0))]
feet = union([yz_prism(foot_pts, x0, FOOT_W) for x0 in (FOOT_INSET, WIDTH - FOOT_INSET - FOOT_W)])
front = front + feet

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
BATT_X0, BATT_Z0 = 14.5, 7.0   # più in basso possibile: baricentro basso (sopra il bordo di centraggio)
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
# Adattatore USB-C: lo tiene l'asola nella parete; in più un appoggio basso a
# metà del tratto interno, con gioco (non lo forza contro la presa micro-USB)
ADAPTER_PAD_GAP = 0.6
adapter_back_y = usbc_y + ADAPTER_T / 2 + ADAPTER_PAD_GAP
adapter_pad = Pos(adapter_x0 + (WIDTH - WALL - adapter_x0) / 2, (adapter_back_y + FRONT_D + 0.1) / 2, usbc_z) * \
    Box(8.0, FRONT_D + 0.1 - adapter_back_y, 8.0)
mcu_supports = mcu_supports + adapter_pad
# Colonna destra dal basso: LOLIN32 (con adattatore), SD. INA219 + RTC in alto a
# sinistra, sopra la batteria (con l'adattatore la colonna destra non basta più).
# La scheda driver sta in basso al centro: il cavo piatto del pannello esce lì ed è corto.
right_x0 = WIDTH - WALL - 4.0 - max(SD_W, INA_W + 6 + RTC_W) - FRAME_CLR - FRAME_T  # colonna allineata alla parete destra
SD_Z0 = MCU_Z0 + MCU_W + 4
INA_X0, INA_Z0 = BATT_X0, BATT_Z0 + BATT_H + 10.0   # sopra l'alloggio batteria, sotto le feritoie
RTC_X0 = INA_X0 + INA_W + 6
DRV_X0, DRV_Z0 = BATT_X0 + BATT_W + 0.5 + cradle_t + 3.0, 15.0   # in basso a destra, accanto alla batteria e sopra la colonnina
modules = union([
    frame(right_x0, SD_Z0, SD_W, SD_H),
    frame(INA_X0, INA_Z0, INA_W, INA_H),
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
back = back - union([rr_prism(4.0, 14.0, 1.9, FRONT_D - 0.1, BACK_T + 0.2, cx=25 + i * 9, cz=HEIGHT - 26)
                     for i in range(6)])

# --- Supporto "arco": piastra inclinata con un sole nascente ritagliato ---
# Pezzo separato: la testa della piastra ha una coda di rondine (svasata solo sul
# lato inferiore) che entra di lato nella guida sul retro del coperchio e si
# ferma contro la sponda di fine corsa. Il peso del case spinge la piastra
# dentro la guida; la svasatura impedisce che si sfili all'indietro.
# Visto di fianco il supporto è un arco: esce dritto dalla guida e poi curva
# all'indietro (raggio STAND_ARC_R) fino a posarsi tangente al tavolo, come il
# pattino di una sedia a dondolo, e dopo il punto d'appoggio si rialza un poco.
# Stampa: piastra sul fianco (profilo sul piatto, larghezza in verticale);
# coperchio in piedi sul bordo inferiore. Con STAND_ANGLE >= 45 gradi il cielo della guida
# sta entro i 45 gradi di sporgenza; pavimento (verso l'alto), fondo (il retro,
# verticale) e sponda di fine corsa (verticale) sono autoportanti.
# Profilo nel piano YZ: J è dove il piano medio della piastra incontra il retro.
STAND_W = 96.0          # larghezza della piastra (centrata; non arriva alle viti né alla presa USB-C)
STAND_T = 4.0           # spessore della piastra
STAND_Z = 62.0          # quota di J sul retro del coperchio (sotto le feritoie)
STAND_ANGLE = 50.0      # inclinazione della piastra rispetto all'orizzontale del case (65 sul tavolo)
STAND_ARC_R = 45.0      # raggio della curva (piano medio), convessa verso il tavolo
STAND_TAIL = 12.0       # gradi di curva oltre il punto d'appoggio (la coda si rialza)
SUN_R = 22.0            # sole nascente sull'orizzonte (il punto d'appoggio), visto da dietro
SUN_RAY_GAP, SUN_RAY_W = 4.5, 3.0   # raggio concentrico: distanza dal sole e larghezza
SUN_RAY_BASE = 7.0     # il raggio si ferma sopra il piede: la fascia interna resta attaccata
DT_L = 12.0             # profondità della coda di rondine lungo la piastra
DT_FLARE = 12.0         # svasatura (gradi) sul lato inferiore
DT_CLR = 0.25           # gioco per lato nella guida (PLA)
DT_WALL = 3.0           # parete della guida attorno alla coda
DT_STOP = 3.0           # sponda di fine corsa (lato x minore); la piastra si infila dal lato x maggiore
_b = math.radians(STAND_ANGLE)
_ax = (math.cos(_b), -math.sin(_b))     # lungo la piastra, dal retro verso il piede
_nu = (math.sin(_b), math.cos(_b))      # normale della faccia superiore (a vista)
stand_x0 = CX - STAND_W / 2


def _pt(s_, w):
    """Punto a distanza s_ lungo la piastra e w dal piano medio (sistema YZ del case)."""
    return (DEPTH + s_ * _ax[0] + w * _nu[0], STAND_Z + s_ * _ax[1] + w * _nu[1])


def _plate_profile(clr=0.0, far=90.0):
    """Sezione della piastra con la coda svasata (clr > 0: sagoma della guida)."""
    h = STAND_T / 2 + clr
    rear = -15.0                          # oltre il retro: si taglia dopo
    fl = math.tan(math.radians(DT_FLARE))
    return [_pt(far, h), _pt(rear, h), _pt(rear, -h - (DT_L - rear) * fl), _pt(DT_L, -h), _pt(far, -h)]


# Dal punto J la piastra va dritta per _s_arc, poi curva a sinistra (verso la
# faccia a vista) di 90 - TILT_DEG + STAND_ANGLE gradi fino a diventare
# parallela al tavolo, con la faccia inferiore tangente al piano del tavolo.
_nt = (-math.sin(math.radians(TILT_DEG)), math.cos(math.radians(TILT_DEG)))   # "su" del tavolo, nel case
_bt = (_nt[1], -_nt[0])                                                     # "indietro" del tavolo, nel case
_turn = STAND_ANGLE + TILT_DEG                                              # da -STAND_ANGLE a +TILT_DEG
_dot = lambda u, v: u[0] * v[0] + u[1] * v[1]
_s_arc = (STAND_ARC_R + STAND_T / 2 - _dot(_nt, (0, STAND_Z)) - STAND_ARC_R * _dot(_nt, _nu)) / _dot(_nt, _ax)
assert _s_arc > DT_L + 3, "curva troppo ampia per STAND_Z: alzare STAND_Z o ridurre STAND_ARC_R"
_path = Wire([Line(_pt(-15, 0), _pt(_s_arc, 0)),
              JernArc(start=_pt(_s_arc, 0), tangent=_ax, radius=STAND_ARC_R, arc_size=_turn + STAND_TAIL)])
_arc_c = (_pt(_s_arc, 0)[0] + STAND_ARC_R * _nu[0], _pt(_s_arc, 0)[1] + STAND_ARC_R * _nu[1])
stand_contact = (_arc_c[0] - (STAND_ARC_R + STAND_T / 2) * _nt[0],
                 _arc_c[1] - (STAND_ARC_R + STAND_T / 2) * _nt[1])            # appoggio sul tavolo (YZ del case)
_fl = math.tan(math.radians(DT_FLARE))
_flare = [_pt(-15, -STAND_T / 2 + 0.2), _pt(-15, -STAND_T / 2 - (DT_L + 15) * _fl), _pt(DT_L, -STAND_T / 2),
          _pt(DT_L + 1, -STAND_T / 2 + 0.2)]
_plate_face = make_face(offset(_path, amount=STAND_T / 2, side=Side.BOTH))
plate = extrude(Plane(origin=(stand_x0, 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0)) * _plate_face,
                amount=STAND_W, dir=(1, 0, 0)) + yz_prism(_flare, stand_x0, STAND_W)
plate = plate - Pos(-10, DEPTH - 50 + DT_CLR, 0) * Box(WIDTH + 20, 50, HEIGHT, align=MIN3)
# Sole: ritagliato in direzione orizzontale (visto da dietro è un semicerchio
# esatto) con il centro sul punto d'appoggio + raggio concentrico interrotto
_sun_pl = Plane(origin=(CX, stand_contact[0], stand_contact[1]), x_dir=(-1, 0, 0), z_dir=(0, _bt[0], _bt[1]))
sun = extrude(_sun_pl * (Circle(SUN_R) + Pos(0, -SUN_R / 2) * Rectangle(2 * SUN_R, SUN_R)),
              amount=120, both=True)
ray = extrude(_sun_pl * ((Circle(SUN_R + SUN_RAY_GAP + SUN_RAY_W) - Circle(SUN_R + SUN_RAY_GAP)) -
                         Pos(0, SUN_RAY_BASE - 30) * Rectangle(120, 60)), amount=120, both=True)
stand = plate - sun - ray

# Guida sul coperchio: blocco con la sagoma della coda (più il gioco) scavata.
# Stampata col coperchio in piedi: faccia posteriore verticale all'imbocco,
# sopra una rampa a 55 gradi (verso l'alto), sotto una faccia a STAND_ANGLE
# gradi dall'orizzontale (sporgenza entro i 45 gradi). Pareti >= DT_WALL.
def _z_on(p, q, y):
    """Quota della retta p-q alla coordinata y."""
    return p[1] + (q[1] - p[1]) * (y - p[0]) / (q[0] - p[0])


_h = STAND_T / 2 + DT_CLR
_fl_deg = STAND_ANGLE - DT_FLARE                                 # pendenza del pavimento svasato
_y_end = _pt(DT_L, _h)[0]                                        # imbocco (labbro del cielo)
_z_ceil = _z_on(_pt(90, _h), _pt(-15, _h), _y_end)
_z_floor = _z_on(_pt(DT_L, -_h), _pt(-15, -_h - (DT_L + 15) * math.tan(math.radians(DT_FLARE))), DEPTH)
_top = (_y_end, _z_ceil + DT_WALL / math.cos(_b))
_low_c = (DEPTH - 0.5, _z_floor - DT_WALL / math.cos(math.radians(_fl_deg)))
guide_pts = [(DEPTH - 0.5, _top[1] + (_y_end - DEPTH + 0.5) * math.tan(math.radians(55))), _top,
             (_y_end, _low_c[1] - (_y_end - DEPTH + 0.5) * math.tan(_b)), _low_c]
dt_guide = yz_prism(guide_pts, stand_x0 - DT_CLR - DT_STOP, STAND_W + DT_CLR + DT_STOP) - \
    yz_prism(_plate_profile(DT_CLR), stand_x0 - DT_CLR, STAND_W + 10)
back = back + dt_guide

# --- Riferimenti dei componenti ---
panel_ref = Pos(pocket_x0 + CLR, PANEL_Y, pocket_z0 + CLR) * Box(PANEL_W, PANEL_T, PANEL_H, align=MIN3)
battery = Pos(BATT_X0, FRONT_D - BATT_T, BATT_Z0) * Box(BATT_W, BATT_T, BATT_H, align=MIN3)
mcu = Pos(mcu_x0, FRONT_D - MCU_STANDOFF - MCU_T, MCU_Z0) * Box(MCU_L, MCU_T, MCU_W, align=MIN3)
sd = Pos(right_x0, FRONT_D - 4.0, SD_Z0) * Box(SD_W, 4.0, SD_H, align=MIN3)
ina = Pos(INA_X0, FRONT_D - 4.0, INA_Z0) * Box(INA_W, 4.0, INA_H, align=MIN3)
rtc = Pos(RTC_X0, FRONT_D - 4.0, INA_Z0) * Box(RTC_W, 4.0, RTC_H, align=MIN3)
drv = Pos(DRV_X0, FRONT_D - DRV_T, DRV_Z0) * Box(DRV_W, DRV_T, DRV_H, align=MIN3)
adapter = Pos(adapter_x0, usbc_y - ADAPTER_T / 2, usbc_z - ADAPTER_W / 2) * \
    Box(MCU_TOTAL_L - MCU_L, ADAPTER_T, ADAPTER_W, align=MIN3)
refs = {"panel": panel_ref, "battery": battery, "lolin32": mcu, "usb-c-adapter": adapter,
        "epd-driver": drv, "sd": sd, "ina219": ina, "rtc": rtc}


def on_desk(shape):
    """Posa sul tavolo: rotazione attorno allo spigolo inferiore posteriore."""
    return Pos(0, DEPTH, 0) * Rot(-TILT_DEG, 0, 0) * Pos(0, -DEPTH, 0) * shape


if os.environ.get("ATMO_EXPORT"):
    # File per la stampa, già orientati sul piatto (z = 0):
    # - cornice a faccia in giù (il fronte appoggia sul piatto);
    # - coperchio in piedi sul bordo inferiore: supporti solo sotto le sponde interne;
    # - piastra "arco" sul fianco (profilo sul piatto): nessun supporto.
    from build123d import export_step, export_stl
    out = os.environ["ATMO_EXPORT"]
    os.makedirs(out, exist_ok=True)
    front_print = Rot(90, 0, 0) * front
    back_print = back
    stand_print = Rot(0, -90, 0) * stand   # fianco sul piatto, larghezza in verticale
    for name, shape in (("atmoverse-cornice", front_print), ("atmoverse-coperchio", back_print),
                        ("atmoverse-supporto", stand_print)):
        bb = shape.bounding_box()
        shape = Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * shape
        export_stl(shape, os.path.join(out, name + ".stl"), tolerance=0.02, angular_tolerance=0.1)
        export_step(shape, os.path.join(out, name + ".step"))
        bb = shape.bounding_box()
        print(f"{name}: {bb.size.X:.1f} x {bb.size.Y:.1f} x {bb.size.Z:.1f} mm, "
              f"volume {shape.volume / 1000:.1f} cm3, valido {shape.is_valid}")
    raise SystemExit(0)

if os.environ.get("ATMO_EXPLODED"):
    # Vista esplosa per controllare l'interno: cornice girata di 180 gradi
    # (lato interno verso l'osservatore), coperchio affiancato
    def inside(shape):
        return Pos(CX, DEPTH / 2, 0) * Rot(0, 0, 180) * Pos(-CX, -DEPTH / 2, 0) * shape
    shift = Pos(WIDTH + 30, 0, 0)
    export_draft({"front": inside(front), "back": shift * back, "stand": shift * stand},
                 references={k: (inside(v) if k == "panel" else shift * v) for k, v in refs.items()})
    raise SystemExit(0)

export_draft(
    {"front": on_desk(front), "back": on_desk(back), "stand": on_desk(stand)},
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
        "front-feet": {"owner": "front", "role": "solid"},
        "dovetail-guide": {"owner": "back", "role": "solid"},
        "arc-stand": {"owner": "stand", "role": "separate"},
        "sun-cutout": {"owner": "stand", "role": "cutter"},
    },
)
