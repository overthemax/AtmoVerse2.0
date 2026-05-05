#!/usr/bin/env python3
"""
Genera BMP con antialiasing per numeri in corsivo elegante (64x96 pixel)
Ottimizzato per display e-ink con scala di grigi.

Richiede: pip install pillow

Uso: python generate_italic_digits.py
Output: cartella 'fonts' con i file BMP
"""

from PIL import Image, ImageDraw, ImageFont
import os

# Configurazione
WIDTH = 64
HEIGHT = 96
SUPERSAMPLE = 4  # Fattore di supersampling per antialiasing
OUTPUT_DIR = "../sd_files/fonts"

# Caratteri da generare
CHARS = {
    '0': '0', '1': '1', '2': '2', '3': '3', '4': '4',
    '5': '5', '6': '6', '7': '7', '8': '8', '9': '9',
    'dot': '.', 'colon': ':', 'degree': '°'
}

def find_italic_font():
    """Cerca un font corsivo disponibile nel sistema"""
    # Font corsivi comuni su Windows (preferenza per font eleganti)
    font_paths = [
        # Font eleganti corsivi
        "C:/Windows/Fonts/georgiai.ttf",   # Georgia Italic (elegante)
        "C:/Windows/Fonts/timesi.ttf",     # Times New Roman Italic
        "C:/Windows/Fonts/palai.ttf",      # Palatino Italic
        "C:/Windows/Fonts/bkant.ttf",      # Book Antiqua
        "C:/Windows/Fonts/garait.ttf",     # Garamond Italic
        # Fallback
        "C:/Windows/Fonts/calibrii.ttf",   # Calibri Italic
        "C:/Windows/Fonts/ariali.ttf",     # Arial Italic
        "C:/Windows/Fonts/georgia.ttf",    # Georgia
        "C:/Windows/Fonts/times.ttf",      # Times New Roman
    ]
    
    for path in font_paths:
        if os.path.exists(path):
            return path
    
    return None

def create_digit_bmp_antialiased(char, filename, font_path):
    """Crea un BMP con antialiasing per un singolo carattere"""
    
    # Dimensioni supersampled (rendering ad alta risoluzione)
    ss_width = WIDTH * SUPERSAMPLE
    ss_height = HEIGHT * SUPERSAMPLE
    
    # Crea immagine ad alta risoluzione in scala di grigi
    img_hires = Image.new('L', (ss_width, ss_height), 255)  # 255 = bianco
    draw = ImageDraw.Draw(img_hires)
    
    # Carica font (dimensione proporzionale all'altezza supersampled)
    try:
        font_size = int(ss_height * 0.80)
        font = ImageFont.truetype(font_path, font_size)
    except:
        font = ImageFont.load_default()
    
    # Calcola posizione centrata
    bbox = draw.textbbox((0, 0), char, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    
    x = (ss_width - text_width) // 2 - bbox[0]
    y = (ss_height - text_height) // 2 - bbox[1]
    
    # Disegna carattere in nero
    draw.text((x, y), char, font=font, fill=0)  # 0 = nero
    
    # Ridimensiona con antialiasing (LANCZOS per qualità migliore)
    img_final = img_hires.resize((WIDTH, HEIGHT), Image.LANCZOS)
    
    # Quantizza a 4 livelli di grigio (ottimale per e-ink)
    # 0 = nero, 85 = grigio scuro, 170 = grigio chiaro, 255 = bianco
    def quantize_4_levels(pixel):
        if pixel < 43:
            return 0        # Nero
        elif pixel < 128:
            return 85       # Grigio scuro
        elif pixel < 213:
            return 170      # Grigio chiaro
        else:
            return 255      # Bianco
    
    # Applica quantizzazione
    pixels = img_final.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            pixels[x, y] = quantize_4_levels(pixels[x, y])
    
    # Salva come BMP 8-bit grayscale
    img_final.save(filename, 'BMP')
    print(f"  Creato: {filename} ({WIDTH}x{HEIGHT}, 4 livelli grigio)")

def main():
    # Crea cartella output
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, OUTPUT_DIR)
    os.makedirs(output_path, exist_ok=True)
    
    print("=" * 50)
    print(f"Generazione font corsivo {WIDTH}x{HEIGHT} pixel")
    print(f"Antialiasing: {SUPERSAMPLE}x supersampling + quantizzazione 4 livelli")
    print(f"Output: {output_path}")
    print("=" * 50)
    print()
    
    # Trova font corsivo
    font_path = find_italic_font()
    if font_path:
        print(f"Font trovato: {font_path}")
    else:
        print("ATTENZIONE: Nessun font corsivo trovato, uso font di default")
    
    print()
    print("Generazione caratteri:")
    
    # Genera ogni carattere
    for name, char in CHARS.items():
        filename = os.path.join(output_path, f"{name}.bmp")
        create_digit_bmp_antialiased(char, filename, font_path)
    
    print()
    print("=" * 50)
    print("Fatto! Copia la cartella 'fonts' sulla SD card.")
    print("=" * 50)
    print()
    print("I BMP sono ottimizzati per display e-ink con 4 livelli di grigio:")
    print("  - Nero (0)")
    print("  - Grigio scuro (85)")
    print("  - Grigio chiaro (170)")  
    print("  - Bianco (255)")
    print()
    print("Per un font più elegante, scarica e installa:")
    print("  - Playfair Display Italic (Google Fonts)")
    print("  - Cormorant Garamond Italic (Google Fonts)")
    print("  - Libre Baskerville Italic (Google Fonts)")

if __name__ == "__main__":
    main()
