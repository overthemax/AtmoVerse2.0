#!/usr/bin/env python3
"""
Converts every BMP icon from 24-bit to 1-bit (black and white)
for the e-ink display.
"""

from PIL import Image
from pathlib import Path

# Folders with the original and the converted icons
INPUT_DIR = Path("../icons_bmp")
OUTPUT_DIR = Path("../icons_bmp_1bit")

def convert_to_1bit(input_path, output_path):
    """Converts a BMP image to 1-bit"""
    try:
        img = Image.open(input_path)
        img_gray = img.convert('L')
        # Pure black and white with dithering
        img_1bit = img_gray.convert('1', dither=Image.FLOYDSTEINBERG)
        img_1bit.save(output_path, 'BMP')
        return True
    except Exception as e:
        print(f"Error converting {input_path.name}: {e}")
        return False

def main():
    OUTPUT_DIR.mkdir(exist_ok=True)

    bmp_files = list(INPUT_DIR.glob("*.bmp"))

    print(f"Found {len(bmp_files)} BMP files to convert")
    print(f"Input:  {INPUT_DIR.absolute()}")
    print(f"Output: {OUTPUT_DIR.absolute()}")
    print()

    converted = 0
    failed = 0

    for bmp_file in bmp_files:
        output_path = OUTPUT_DIR / bmp_file.name
        print(f"Converting {bmp_file.name}...", end=" ")

        if convert_to_1bit(bmp_file, output_path):
            original_size = bmp_file.stat().st_size
            new_size = output_path.stat().st_size
            reduction = (1 - new_size / original_size) * 100
            print(f"ok ({original_size/1024:.1f} KB -> {new_size/1024:.1f} KB, -{reduction:.0f}%)")
            converted += 1
        else:
            print("FAILED")
            failed += 1

    print()
    print(f"Done: {converted} converted, {failed} failed")
    print(f"Now copy the icons from '{OUTPUT_DIR.absolute()}' to sd_files/icons/")

if __name__ == "__main__":
    main()
