#!/usr/bin/env python3
"""
Script SEMPLIFICATO per Windows - converte SVG in BMP usando solo Pillow
Requisiti: pip install pillow
"""

import os
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont, ImageEnhance
except ImportError:
    print("❌ Pillow mancante!")
    print("\nInstalla con:")
    print("  python -m pip install pillow")
    sys.exit(1)

# Configurazione
INPUT_DIR = "sd_files/icons_png"  # Cambiato da icons a icons_png
OUTPUT_DIR = "sd_files/icons_bmp"
ICON_SIZE = 140

print("=" * 60)
print("NOTA: Questo script converte icone ESISTENTI PNG/JPG in BMP")
print("Se hai solo SVG, usa un convertitore online prima:")
print("  https://cloudconvert.com/svg-to-png")
print("=" * 60)
print()

def convert_image_to_grayscale_bmp(input_path, output_path, size=140):
    """Converte un'immagine in BMP scala di grigi"""
    try:
        print(f"  Caricamento immagine...")
        img = Image.open(input_path)
        
        # Se ha alpha, composita su sfondo bianco
        if img.mode in ('RGBA', 'LA') or (img.mode == 'P' and 'transparency' in img.info):
            print(f"  Rimozione trasparenza...")
            background = Image.new('RGB', img.size, (255, 255, 255))
            if img.mode == 'P':
                img = img.convert('RGBA')
            background.paste(img, mask=img.split()[-1] if img.mode == 'RGBA' else None)
            img = background
        
        # Ridimensiona con anti-aliasing
        print(f"  Ridimensionamento → {size}x{size}...")
        img = img.resize((size, size), Image.Resampling.LANCZOS)
        
        # Converti in scala di grigi
        print(f"  Conversione → scala di grigi...")
        img_gray = img.convert('L')
        
        # Aumenta contrasto per e-ink
        enhancer = ImageEnhance.Contrast(img_gray)
        img_gray = enhancer.enhance(1.3)
        
        # Salva come BMP 8-bit
        img_gray.save(output_path, 'BMP')
        
        file_size = os.path.getsize(output_path)
        print(f"  ✓ Salvato: {output_path} ({file_size//1024}KB)")
        return True
        
    except Exception as e:
        print(f"  ❌ Errore: {e}")
        return False

def main():
    input_path = Path(INPUT_DIR)
    if not input_path.exists():
        print(f"❌ Cartella non trovata: {INPUT_DIR}")
        return
    
    output_path = Path(OUTPUT_DIR)
    output_path.mkdir(exist_ok=True)
    
    print(f"📁 Input:  {INPUT_DIR}")
    print(f"📁 Output: {OUTPUT_DIR}")
    print(f"📐 Dimensione: {ICON_SIZE}x{ICON_SIZE} px")
    print()
    
    # Cerca file immagine (PNG, JPG, SVG)
    image_files = list(input_path.glob("*.png")) + \
                  list(input_path.glob("*.jpg")) + \
                  list(input_path.glob("*.jpeg")) + \
                  list(input_path.glob("*.svg"))
    
    if not image_files:
        print(f"❌ Nessun file immagine trovato in {INPUT_DIR}")
        print()
        print("💡 Suggerimento:")
        print("   1. Vai su https://cloudconvert.com/svg-to-png")
        print("   2. Carica tutti i file .svg")
        print("   3. Scarica i PNG")
        print("   4. Copia i PNG in sd_files/icons/")
        print("   5. Riesegui questo script")
        return
    
    print(f"Trovati {len(image_files)} file")
    print()
    
    converted = 0
    failed = 0
    
    for img_file in image_files:
        bmp_name = img_file.stem + ".bmp"
        bmp_path = output_path / bmp_name
        
        print(f"🔄 {img_file.name}")
        
        if convert_image_to_grayscale_bmp(img_file, str(bmp_path), ICON_SIZE):
            converted += 1
        else:
            failed += 1
        
        print()
    
    print("=" * 60)
    print(f"✓ Convertiti: {converted}/{len(image_files)}")
    if failed > 0:
        print(f"❌ Falliti: {failed}")
    print()
    print("📋 Prossimi passi:")
    print(f"  1. Copia '{OUTPUT_DIR}' sulla SD card")
    print(f"  2. Ricompila il firmware")
    print("=" * 60)

if __name__ == "__main__":
    main()
