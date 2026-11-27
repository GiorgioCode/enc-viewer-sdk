#!/usr/bin/env python3
"""Genera el atlas de etiquetas y sus avances; requiere Pillow sólo para regenerar el recurso."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('font', type=Path, help='DejaVuSans.ttf con sus avisos de licencia')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
characters = [*range(32, 768), *range(880, 1328), *range(8192, 8304), 0xFFFD]
font = ImageFont.truetype(str(args.font), 14)
image = Image.new('L', (640, ((len(characters) + 31) // 32) * 24))
draw = ImageDraw.Draw(image)
advances = bytearray()
for index, codepoint in enumerate(characters):
    ch = chr(codepoint)
    draw.text(((index % 32) * 20 + 1, (index // 32) * 24 + 18), ch, font=font, anchor='ls', fill=255)
    advances.append(max(0, min(20, round(font.getlength(ch)))))
# El RLE cuantizado reduce el recurso y evita dependencias de fuentes durante ejecución.
pixels = bytes(round(value / 17) * 17 for value in image.tobytes())
encoded = bytearray(advances)
index = 0
while index < len(pixels):
    count = 1
    while index + count < len(pixels) and pixels[index + count] == pixels[index] and count < 255:
        count += 1
    encoded.extend((count, pixels[index]))
    index += count
(root / 'assets/enc-labels.rle').write_bytes(encoded)
print(f'Atlas: {len(characters)} glifos; {len(encoded)} bytes.')
