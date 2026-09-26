"""Anteprima delle schermate AtmoVerse (648x480, 1 bit) con i veri font U8g2.

Riproduce posizioni e logica di Screens.cpp decodificando i font dalla
libreria U8g2_for_Adafruit_GFX installata e usando le icone di sd_files/icons.
Serve a vedere la grafica senza caricare il firmware e a generare le immagini
del README.

Uso:
    python tools/anteprima_display.py docs/images
Richiede Pillow; il codice QR richiede anche il modulo qrcode.
"""
import re
import sys
from pathlib import Path
from PIL import Image, ImageDraw

LIB = str(Path.home() / "Documents/Arduino/libraries/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c")
ICONS = str(Path(__file__).resolve().parent.parent / "sd_files/icons")
SRC = open(LIB, encoding="latin-1").read()
W, H = 648, 480
MARGIN = 32


# ---------------------------------------------------------------------------
# Decodifica dei font U8g2
# ---------------------------------------------------------------------------
class U8g2Font:
    def __init__(self, name):
        m = re.search(r"const uint8_t " + name + r"\[\d+\][^=]*=\s*((?:\"(?:[^\"\\]|\\.)*\"\s*)+);", SRC)
        if not m:
            raise ValueError(name)
        literal = "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))
        self.data = literal.encode("latin-1").decode("unicode_escape").encode("latin-1")
        d = self.data
        self.bp0, self.bp1 = d[2], d[3]
        self.bw, self.bh, self.bx, self.by, self.bd = d[4], d[5], d[6], d[7], d[8]
        self.ascent = self.s8(d[13])
        self.descent = self.s8(d[14])
        self.glyphs = {}
        pos = 23
        while d[pos + 1] != 0:
            self.glyphs[d[pos]] = pos
            pos += d[pos + 1]

    @staticmethod
    def s8(v):
        return v - 256 if v > 127 else v

    def glyph(self, code):
        pos = self.glyphs.get(code)
        if pos is None:
            return None
        d, bitpos = self.data, (pos + 2) * 8

        def get(n):
            nonlocal bitpos
            v = 0
            for i in range(n):
                byte = d[bitpos >> 3]
                v |= ((byte >> (bitpos & 7)) & 1) << i
                bitpos += 1
            return v

        def sget(n):
            return get(n) - (1 << (n - 1))

        w, h = get(self.bw), get(self.bh)
        x, y, dx = sget(self.bx), sget(self.by), sget(self.bd)
        px = []
        total = w * h
        while len(px) < total:
            a, b = get(self.bp0), get(self.bp1)
            while True:
                px.extend([0] * a + [1] * b)
                if get(1) == 0:
                    break
        return w, h, x, y, dx, px[:total]

    def width(self, text):
        return sum((self.glyph(ord(c)) or (0, 0, 0, 0, 0, []))[4] for c in text)

    def draw(self, img, x, y, text):
        for c in text:
            g = self.glyph(ord(c))
            if not g:
                continue
            w, h, gx, gy, dx, px = g
            top = y - (h + gy)
            for i, v in enumerate(px):
                if v:
                    xx, yy = x + gx + i % w, top + i // w
                    if 0 <= xx < W and 0 <= yy < H:
                        img.putpixel((xx, yy), 0)
            x += dx

    def line_height(self):
        return self.ascent - self.descent + 4


F = {n: U8g2Font("u8g2_font_" + n) for n in [
    "fur49_tn", "fur35_tf", "fur20_tf", "fur17_tf", "luRS14_tf", "luRS12_tf",
    "luBS14_tf", "luRS10_tf", "luRS08_tf", "luIS19_tf", "luIS14_tf", "luIS12_tf", "luIS10_tf"]}
QUOTE_STYLES = [("luIS19_tf", "luRS12_tf"), ("luIS14_tf", "luRS10_tf"),
                ("luIS12_tf", "luRS10_tf"), ("luIS10_tf", "luRS08_tf")]


def left(img, f, x, y, t): F[f].draw(img, x, y, t)
def right(img, f, x, y, t): F[f].draw(img, x - F[f].width(t), y, t)
def center(img, f, cx, y, t): F[f].draw(img, cx - F[f].width(t) // 2, y, t)


def wrap(f, text, maxw):
    lines, line = [], ""
    for word in text.split():
        cand = (line + " " + word) if line else word
        if line and F[f].width(cand) > maxw:
            lines.append(line)
            line = word
        else:
            line = cand
    if line:
        lines.append(line)
    return lines


def hline(d, y):
    d.line([(MARGIN, y), (W - MARGIN - 1, y)], fill=0)


def draw_icon(img, name, x, y, scale=2):
    icon = Image.open(f"{ICONS}/{name}.bmp").convert("1")
    icon = icon.resize((icon.width * scale, icon.height * scale), Image.NEAREST)
    img.paste(icon, (x, y))


def draw_battery(img, d, xr, baseline, pct, charging=False):
    bw, bh = 22, 11
    bx, by = xr - bw - 2, baseline - bh + 1
    d.rectangle([bx, by, bx + bw - 1, by + bh - 1], outline=0)
    d.rectangle([bx + bw, by + 3, bx + bw + 1, by + bh - 4], fill=0)
    fill = (bw - 4) * pct // 100
    if fill:
        d.rectangle([bx + 2, by + 2, bx + 1 + fill, by + bh - 3], fill=0)
    label = f"{pct}%" if not charging else f"In carica \xb7 {pct}%"
    right(img, "luRS08_tf", bx - 6, baseline, label)


def draw_quote(img, text, author):
    top, bottom = 346, 452
    width, height = W - 2 * MARGIN, bottom - top
    text = "\xab" + text + "\xbb"
    for tf, af in QUOTE_STYLES:
        lines = wrap(tf, text, width)
        alines = wrap(af, author, width)[:2] if author else []
        total = len(lines) * F[tf].line_height() + (4 + len(alines) * F[af].line_height() if alines else 0)
        if len(lines) <= 8 and total <= height:
            break
    y = top + (height - total) // 2
    for l in lines:
        y += F[tf].line_height()
        center(img, tf, W // 2, y - 4 + F[tf].descent, l)
    if alines:
        y += 4
        for l in alines:
            y += F[af].line_height()
            center(img, af, W // 2, y - 4 + F[af].descent, l)
    return tf


def main_screen(quote, author, out):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    left(img, "fur17_tf", MARGIN, 44, "Venerd\xec 26 settembre")
    right(img, "luRS14_tf", W - MARGIN, 44, "Roma")
    hline(d, 60)
    left(img, "fur49_tn", MARGIN - 2, 136, "10:42")
    left(img, "fur35_tf", MARGIN, 204, "18\xb0")
    left(img, "luRS14_tf", MARGIN, 238, "Nubi sparse")
    draw_icon(img, "wi-day-cloudy", W - MARGIN - 200, 66)
    colw = (W - 2 * MARGIN) // 4
    for i, (lab, val) in enumerate([("Percepita", "17\xb0"), ("Umidit\xe0", "62%"),
                                    ("Vento", "12 km/h"), ("Pressione", "1016 hPa")]):
        left(img, "luRS10_tf", MARGIN + i * colw, 292, lab)
        left(img, "luBS14_tf", MARGIN + i * colw, 318, val)
    hline(d, 334)
    style = draw_quote(img, quote, author)
    left(img, "luRS08_tf", MARGIN, H - 12, "Aggiornato alle 10:30")
    center(img, "luRS08_tf", W // 2, H - 12, "192.168.1.23")
    draw_battery(img, d, W - MARGIN, H - 12, 62)
    img.save(out)
    print(out, "stile citazione:", style)


def setup_screen(out):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    left(img, "fur20_tf", MARGIN, 52, "Configurazione")
    left(img, "luRS12_tf", MARGIN, 80, "AtmoVerse non \xe8 collegato a una rete WiFi.")
    hline(d, 96)
    qr_area, qr_left = 216, W - MARGIN - 216
    try:
        import qrcode
        q = qrcode.QRCode(border=0, error_correction=qrcode.constants.ERROR_CORRECT_M)
        q.add_data("WIFI:T:WPA;S:AtmoVerse_AP_A1B2;P:atmoverse;;")
        q.make()
        m = q.get_matrix()
        mod = qr_area // len(m)
        size = mod * len(m)
        x0 = qr_left + (qr_area - size) // 2
        for r, row in enumerate(m):
            for c, v in enumerate(row):
                if v:
                    d.rectangle([x0 + c * mod, 116 + r * mod, x0 + c * mod + mod - 1, 116 + r * mod + mod - 1], fill=0)
    except ImportError:
        size = qr_area
        d.rectangle([qr_left, 116, qr_left + size, 116 + size], outline=0)
        center(img, "luRS10_tf", qr_left + size // 2, 116 + size // 2, "[codice QR]")
    center(img, "luRS10_tf", qr_left + qr_area // 2, 116 + size + 24, "Inquadra per collegarti")
    textw = qr_left - MARGIN - 32
    steps = [
        "Inquadra il codice QR con la fotocamera del telefono: il telefono si collega alla rete di AtmoVerse.",
        "Si apre da sola la pagina di configurazione. Se non compare, apri il browser su http://192.168.4.1",
        "Scegli la rete WiFi di casa, inserisci citt\xe0 e API key di OpenWeatherMap e tocca Salva.",
    ]
    y = 132
    for i, s in enumerate(steps):
        left(img, "luBS14_tf", MARGIN, y, str(i + 1))
        for l in wrap("luRS12_tf", s, textw)[:4]:
            left(img, "luRS12_tf", MARGIN + 26, y, l)
            y += F["luRS12_tf"].line_height()
        y += 14
    y += 6
    left(img, "luRS10_tf", MARGIN, y, "Collegamento manuale")
    y += 24
    left(img, "luRS12_tf", MARGIN, y, "Rete  AtmoVerse_AP_A1B2")
    y += F["luRS12_tf"].line_height()
    left(img, "luRS12_tf", MARGIN, y, "Password  atmoverse")
    left(img, "luRS08_tf", MARGIN, H - 12, "Quando la rete di casa torna disponibile, AtmoVerse si ricollega da solo.")
    draw_battery(img, d, W - MARGIN, H - 12, 62)
    img.save(out)
    print(out)


if __name__ == "__main__":
    outdir = sys.argv[1]
    Path(outdir).mkdir(parents=True, exist_ok=True)
    main_screen("Non importa quanto cammino tu abbia gi\xe0 fatto, importa sempre quanto cammino devi ancora fare.",
                "Proverbio giapponese", f"{outdir}/display-principale.png")
    main_screen("As midnight was striking bronze blows upon the dusky air, Dorian Gray, dressed commonly, "
                "and with a muffler wrapped round his throat, crept quietly out of his house.",
                "Oscar Wilde, The Picture of Dorian Gray", f"{outdir}/display-citazione-lunga.png")
    setup_screen(f"{outdir}/display-configurazione.png")
