#!/usr/bin/env python3
"""
Script per convertire le icone SVG in BMP monocromatiche per display e-ink
Requisiti: pip install pillow cairosvg
"""

import os
import sys
from pathlib import Path

try:
    from PIL import Image
    import cairosvg
except ImportError:
    print("❌ Librerie mancanti!")
    print("\nInstalla con:")
    print("  pip install pillow cairosvg")
    sys.exit(1)

# Configurazione
INPUT_DIR = "sd_files/icons"       # Cartella con SVG
OUTPUT_DIR = "sd_files/icons_bmp"  # Cartella output BMP
ICON_SIZE = 400                     # Dimensione icone in pixel (400x400)

def convert_svg_to_bmp(svg_path, bmp_path, size=140):
    """Converte un file SVG in BMP scala di grigi (8-bit)"""
    try:
        # Converti SVG in PNG temporaneo (ad alta risoluzione per anti-aliasing)
        temp_png = bmp_path.replace('.bmp', '_temp.png')
        
        print(f"  Rendering SVG → PNG temporaneo...")
        cairosvg.svg2png(
            url=str(svg_path),
            write_to=temp_png,
            output_width=size * 2,  # 2x per anti-aliasing migliore
            output_height=size * 2
        )
        
        # Apri PNG e converti in scala di grigi
        print(f"  Conversione → BMP scala di grigi...")
        img = Image.open(temp_png)
        
        # Ridimensiona alla dimensione finale con anti-aliasing
        img_resized = img.resize((size, size), Image.Resampling.LANCZOS)
        
        # Converti in scala di grigi (8-bit, 256 livelli)
        img_gray = img_resized.convert('L')
        
        # Opzionale: aumenta il contrasto per e-ink
        from PIL import ImageEnhance
        enhancer = ImageEnhance.Contrast(img_gray)
        img_gray = enhancer.enhance(1.3)  # Aumenta contrasto del 30%
        
        # Salva come BMP 8-bit
        img_gray.save(bmp_path, 'BMP')
        
        # Rimuovi file temporaneo
        os.remove(temp_png)
        
        # Mostra info file
        file_size = os.path.getsize(bmp_path)
        print(f"  ✓ Salvato: {bmp_path} ({file_size//1024}KB)")
        return True
        
    except Exception as e:
        print(f"  ❌ Errore: {e}")
        return False

def main():
    print("=" * 60)
    print("Conversione Icone SVG → BMP per AtmoVerse")
    print("=" * 60)
    print()
    
    # Verifica cartella input
    input_path = Path(INPUT_DIR)
    if not input_path.exists():
        print(f"❌ Cartella non trovata: {INPUT_DIR}")
        print(f"   Esegui lo script dalla cartella AtmoVerse_2.0")
        return
    
    # Crea cartella output
    output_path = Path(OUTPUT_DIR)
    output_path.mkdir(exist_ok=True)
    print(f"📁 Input:  {INPUT_DIR}")
    print(f"📁 Output: {OUTPUT_DIR}")
    print(f"📐 Dimensione: {ICON_SIZE}x{ICON_SIZE} px")
    print()
    
    # Trova tutti i file SVG
    svg_files = list(input_path.glob("*.svg"))
    
    if not svg_files:
        print(f"❌ Nessun file SVG trovato in {INPUT_DIR}")
        return
    
    print(f"Trovati {len(svg_files)} file SVG")
    print()
    
    # Converti ogni SVG
    converted = 0
    failed = 0
    
    for svg_file in svg_files:
        # Nome file output
        bmp_name = svg_file.stem + ".bmp"
        bmp_path = output_path / bmp_name
        
        print(f"🔄 {svg_file.name}")
        
        if convert_svg_to_bmp(svg_file, str(bmp_path), ICON_SIZE):
            converted += 1
        else:
            failed += 1
        
        print()
    
    # Riepilogo
    print("=" * 60)
    print(f"✓ Convertiti: {converted}/{len(svg_files)}")
    if failed > 0:
        print(f"❌ Falliti: {failed}")
    print()
    print("📋 Prossimi passi:")
    print(f"  1. Copia la cartella '{OUTPUT_DIR}' sulla SD card")
    print(f"  2. Ricompila il firmware con supporto BMP")
    print("=" * 60)

if __name__ == "__main__":
    main()
