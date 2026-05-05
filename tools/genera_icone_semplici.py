#!/usr/bin/env python3
"""
Genera icone meteo semplici in BMP grayscale usando solo Pillow
Nessuna dipendenza esterna, nessuna conversione online
"""

import os
from PIL import Image, ImageDraw

# Configurazione
OUTPUT_DIR = "sd_files/icons_bmp"
ICON_SIZE = 140

def create_icon_dir():
    """Crea la directory di output"""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"📁 Creata cartella: {OUTPUT_DIR}")

def draw_sun(draw, cx, cy, radius):
    """Disegna un sole"""
    # Cerchio centrale
    draw.ellipse([cx-radius, cy-radius, cx+radius, cy+radius], 
                 fill=50, outline=30, width=2)
    # Raggi
    for angle in range(0, 360, 45):
        import math
        rad = math.radians(angle)
        x1 = cx + (radius + 10) * math.cos(rad)
        y1 = cy + (radius + 10) * math.sin(rad)
        x2 = cx + (radius + 25) * math.cos(rad)
        y2 = cy + (radius + 25) * math.sin(rad)
        draw.line([x1, y1, x2, y2], fill=50, width=4)

def draw_cloud(draw, cx, cy, width, height):
    """Disegna una nuvola"""
    # 3 cerchi sovrapposti
    r1, r2, r3 = height//2, height//1.5, height//2.2
    draw.ellipse([cx-width//3-r1, cy-r1, cx-width//3+r1, cy+r1], fill=80, outline=60, width=2)
    draw.ellipse([cx-r2, cy-r2+5, cx+r2, cy+r2+5], fill=70, outline=60, width=2)
    draw.ellipse([cx+width//3-r3, cy-r3, cx+width//3+r3, cy+r3], fill=80, outline=60, width=2)

def draw_rain(draw, cx, cy, drops=5):
    """Disegna gocce di pioggia"""
    for i in range(drops):
        x = cx - 30 + i * 15
        y = cy + 20
        draw.line([x, y, x-3, y+15], fill=40, width=2)

def draw_snow(draw, cx, cy, flakes=5):
    """Disegna fiocchi di neve"""
    for i in range(flakes):
        x = cx - 30 + i * 15
        y = cy + 20
        # Croce
        draw.line([x-5, y, x+5, y], fill=200, width=2)
        draw.line([x, y-5, x, y+5], fill=200, width=2)
        # Diagonali
        draw.line([x-3, y-3, x+3, y+3], fill=200, width=1)
        draw.line([x-3, y+3, x+3, y-3], fill=200, width=1)

def draw_lightning(draw, cx, cy):
    """Disegna un fulmine"""
    points = [(cx, cy), (cx-5, cy+15), (cx+2, cy+15), (cx-3, cy+30)]
    draw.line(points, fill=30, width=3)

def create_icon(name, draw_func):
    """Crea un'icona BMP"""
    # Crea immagine grayscale
    img = Image.new('L', (ICON_SIZE, ICON_SIZE), color=255)
    draw = ImageDraw.Draw(img)
    
    # Disegna l'icona
    draw_func(draw, ICON_SIZE//2, ICON_SIZE//2)
    
    # Salva come BMP
    filepath = os.path.join(OUTPUT_DIR, f"{name}.bmp")
    img.save(filepath, 'BMP')
    
    file_size = os.path.getsize(filepath) // 1024
    print(f"  ✓ {name}.bmp ({file_size}KB)")

# Definizione icone
def icon_fog(draw, cx, cy):
    """Nebbia"""
    for i in range(5):
        y = cy - 30 + i * 15
        draw.line([20, y, ICON_SIZE-20, y], fill=100, width=3)

def icon_day_sunny(draw, cx, cy):
    """Sole"""
    draw_sun(draw, cx, cy, 30)

def icon_day_cloudy(draw, cx, cy):
    """Sole con nuvole"""
    draw_sun(draw, cx-20, cy-15, 20)
    draw_cloud(draw, cx+10, cy+10, 50, 30)

def icon_day_thunderstorm(draw, cx, cy):
    """Temporale diurno"""
    draw_sun(draw, cx-25, cy-20, 15)
    draw_cloud(draw, cx, cy, 60, 35)
    draw_lightning(draw, cx, cy+20)
    draw_rain(draw, cx, cy+10, 3)

def icon_rain(draw, cx, cy):
    """Pioggia forte"""
    draw_cloud(draw, cx, cy-10, 60, 35)
    draw_rain(draw, cx, cy+10, 7)

def icon_sprinkle(draw, cx, cy):
    """Pioggia leggera"""
    draw_cloud(draw, cx, cy-10, 55, 30)
    draw_rain(draw, cx, cy+10, 4)

def icon_thunderstorm(draw, cx, cy):
    """Temporale"""
    draw_cloud(draw, cx, cy-10, 65, 40)
    draw_lightning(draw, cx, cy+15)
    draw_rain(draw, cx, cy+5, 5)

def icon_storm_showers(draw, cx, cy):
    """Temporale intenso"""
    draw_cloud(draw, cx, cy-15, 70, 45)
    draw_lightning(draw, cx-10, cy+10)
    draw_lightning(draw, cx+15, cy+15)
    draw_rain(draw, cx, cy, 7)

def icon_rain_mix(draw, cx, cy):
    """Neve e pioggia"""
    draw_cloud(draw, cx, cy-10, 60, 35)
    draw_rain(draw, cx-15, cy+10, 3)
    draw_snow(draw, cx+15, cy+10, 3)

def icon_sleet(draw, cx, cy):
    """Neve leggera e pioggia"""
    draw_cloud(draw, cx, cy-10, 55, 30)
    draw_rain(draw, cx-10, cy+10, 2)
    draw_snow(draw, cx+10, cy+10, 2)

def icon_snow(draw, cx, cy):
    """Neve"""
    draw_cloud(draw, cx, cy-10, 60, 35)
    draw_snow(draw, cx, cy+10, 6)

def icon_snowflake_cold(draw, cx, cy):
    """Neve intensa"""
    draw_cloud(draw, cx, cy-15, 65, 40)
    draw_snow(draw, cx, cy+5, 8)
    # Fiocco grande al centro
    draw.line([cx-15, cy+35, cx+15, cy+35], fill=200, width=3)
    draw.line([cx, cy+20, cx, cy+50], fill=200, width=3)

def main():
    print("=" * 60)
    print("Generazione Icone Meteo BMP")
    print("=" * 60)
    print()
    
    create_icon_dir()
    print()
    
    icons = {
        'wi-fog': icon_fog,
        'wi-day-sunny': icon_day_sunny,
        'wi-day-cloudy': icon_day_cloudy,
        'wi-day-thunderstorm': icon_day_thunderstorm,
        'wi-rain': icon_rain,
        'wi-sprinkle': icon_sprinkle,
        'wi-thunderstorm': icon_thunderstorm,
        'wi-storm-showers': icon_storm_showers,
        'wi-rain-mix': icon_rain_mix,
        'wi-sleet': icon_sleet,
        'wi-snow': icon_snow,
        'wi-snowflake-cold': icon_snowflake_cold
    }
    
    print(f"Generazione {len(icons)} icone...")
    print()
    
    for name, func in icons.items():
        create_icon(name, func)
    
    print()
    print("=" * 60)
    print(f"✓ Completato! Create {len(icons)} icone in {OUTPUT_DIR}")
    print()
    print("📋 Prossimi passi:")
    print(f"  1. Copia '{OUTPUT_DIR}' sulla SD card")
    print("  2. Ricompila il firmware")
    print("=" * 60)

if __name__ == "__main__":
    main()
