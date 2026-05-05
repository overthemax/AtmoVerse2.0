#!/usr/bin/env python3
"""
Converte tutte le icone BMP da 24-bit a 1-bit (bianco/nero)
per compatibilità con display e-ink
"""

from PIL import Image
import os
from pathlib import Path

# Directory con le icone originali e di output
INPUT_DIR = Path("../icons_bmp")
OUTPUT_DIR = Path("../icons_bmp_1bit")

def convert_to_1bit(input_path, output_path):
    """Converte un'immagine BMP a 1-bit"""
    try:
        # Apri l'immagine
        img = Image.open(input_path)
        
        # Converti in scala di grigi
        img_gray = img.convert('L')
        
        # Converti in 1-bit (bianco/nero puro) con dithering
        img_1bit = img_gray.convert('1', dither=Image.FLOYDSTEINBERG)
        
        # Salva come BMP 1-bit
        img_1bit.save(output_path, 'BMP')
        
        return True
    except Exception as e:
        print(f"Errore durante conversione di {input_path.name}: {e}")
        return False

def main():
    # Crea la directory di output se non esiste
    OUTPUT_DIR.mkdir(exist_ok=True)
    
    # Trova tutti i file BMP
    bmp_files = list(INPUT_DIR.glob("*.bmp"))
    
    print(f"Trovati {len(bmp_files)} file BMP da convertire")
    print(f"Input:  {INPUT_DIR.absolute()}")
    print(f"Output: {OUTPUT_DIR.absolute()}")
    print()
    
    converted = 0
    failed = 0
    
    for bmp_file in bmp_files:
        output_path = OUTPUT_DIR / bmp_file.name
        print(f"Conversione: {bmp_file.name}...", end=" ")
        
        if convert_to_1bit(bmp_file, output_path):
            # Verifica la dimensione del file convertito
            original_size = bmp_file.stat().st_size
            new_size = output_path.stat().st_size
            reduction = (1 - new_size / original_size) * 100
            
            print(f"✓ ({original_size/1024:.1f}KB → {new_size/1024:.1f}KB, -{reduction:.0f}%)")
            converted += 1
        else:
            print("✗ ERRORE")
            failed += 1
    
    print()
    print(f"Conversione completata:")
    print(f"  ✓ Convertiti: {converted}")
    print(f"  ✗ Falliti: {failed}")
    print()
    print(f"Copia ora le icone da '{OUTPUT_DIR.absolute()}' alla SD card nella cartella '/icons/'")

if __name__ == "__main__":
    main()
