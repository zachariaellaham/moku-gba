#!/usr/bin/env python3
"""Tile PNG screenshots into one contact sheet (3x scale) for visual review.
usage: screenshot_grid.py OUT.png IN1.png IN2.png ... [--cols N] [--scale S]"""
import sys
from PIL import Image, ImageDraw

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opts = {a.split('=')[0]: a.split('=')[1] for a in sys.argv[1:] if a.startswith('--') and '=' in a}
    cols = int(opts.get('--cols', 3)); scale = int(opts.get('--scale', 2))
    out, ins = args[0], args[1:]
    imgs = [(p, Image.open(p).convert('RGB')) for p in ins]
    w, h = 240 * scale, 160 * scale
    rows = (len(imgs) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (w + 8) + 8, rows * (h + 20) + 8), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    for i, (p, im) in enumerate(imgs):
        x = 8 + (i % cols) * (w + 8); y = 8 + (i // cols) * (h + 20)
        sheet.paste(im.resize((w, h), Image.NEAREST), (x, y + 12))
        d.text((x, y), p.split('/')[-1], fill=(230, 230, 230))
    sheet.save(out)
    print(out, sheet.size)

if __name__ == '__main__':
    main()
