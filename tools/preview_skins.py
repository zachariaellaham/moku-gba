#!/usr/bin/env python3
"""preview_skins.py - render the generated board art to PNGs so it can be inspected by eye.

It deliberately parses the *generated* src/game/render/skin_data.cpp (not the python objects in
gen_art.py) and reads back the *generated* 4bpp BMP sprite sheets with PIL, so the previews prove
what the ROM will actually get. The board is composed exactly the way board_layer must compose it:

  1. fill the whole 144x160 board area with `margin_index`, then tile `board_texture` (32x32,
     opaque) over the *whole* area, phase-aligned to the board area origin;
  2. tile `margin_texture` (16x16, index 0 transparent) over the margin only, i.e. outside the
     grid rectangle, with the same phase (its rows 0..7 land on the top band, rows 8..15 on the
     bottom band);
  3. draw the grid lines in `line_index` at cell/2 + k*cell and the hoshi in `hoshi_index`;
  4. blit cell x cell stone/marker sprites at (k*cell, j*cell), index 0 transparent.

Usage: python3 tools/preview_skins.py [--root DIR] [--out DIR] [--scale N]
"""
import argparse
import os
import re
import sys

from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

SKIN_NAMES = ['classic', 'pup', 'dino']
CELLS = [16, 11, 8]
SET_FIELDS = ['black', 'white', 'black_atari', 'white_atari', 'ghost_black', 'ghost_white',
              'last_marker', 'dead_marker', 'terr_black', 'terr_white', 'mission_marker']
BOARD_W, BOARD_H = 144, 160          # the board area of the 240x160 screen
HOSHI = {7: [(3, 3)],
         9: [(2, 2), (6, 2), (4, 4), (2, 6), (6, 6)],
         13: [(3, 3), (9, 3), (6, 6), (3, 9), (9, 9)],
         19: [(3, 3), (9, 3), (15, 3), (3, 9), (9, 9), (15, 9), (3, 15), (9, 15), (15, 15)]}


# ------------------------------------------------------------------------------------------------
# Parse the generated C++
# ------------------------------------------------------------------------------------------------

class Skin:
    def __init__(self, name):
        self.name = name
        self.palette = []            # list of (r, g, b)
        self.bg = self.line = self.hoshi = self.margin = 0
        self.arrays = {}             # array name -> flat list
        self.dims = {}               # array name -> (w, h)


def bgr555_to_rgb(v):
    r = (v & 31) << 3
    g = ((v >> 5) & 31) << 3
    b = ((v >> 10) & 31) << 3
    return (r | (r >> 5), g | (g >> 5), b | (b >> 5))


def parse_cpp(path):
    src = open(path).read()
    src = re.sub(r'//[^\n]*', '', src)                      # strip the ascii-art comments
    skins = {n: Skin(n) for n in SKIN_NAMES}

    for m in re.finditer(r'constexpr uint16_t (\w+)_palette\[(\d+)\]\s*=\s*\{([^}]*)\}', src):
        name, _, body = m.group(1), m.group(2), m.group(3)
        vals = [int(v, 0) for v in body.replace('\n', '').split(',') if v.strip()]
        skins[name].palette = [bgr555_to_rgb(v) for v in vals]

    arrays = {}
    for m in re.finditer(r'constexpr uint8_t (\w+)\[(\d+)\]\s*=\s*\{([^}]*)\}', src):
        name, n, body = m.group(1), int(m.group(2)), m.group(3)
        vals = [int(v) for v in body.replace('\n', '').split(',') if v.strip()]
        if len(vals) != n:
            raise ValueError('%s: declared %d values, found %d' % (name, n, len(vals)))
        arrays[name] = vals

    # the k_skins table carries the sizes and the four index fields
    table = src[src.index('constexpr SkinData k_skins'):]
    for name in SKIN_NAMES:
        sk = skins[name]
        i = table.index(name + '_palette')
        chunk = table[i:table.index('},\n    {', i) if '},\n    {' in table[i:] else len(table)]
        chunk = table[i:i + 4000]
        nums = re.match(r'\w+,\s*\d+,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),', chunk)
        sk.bg, sk.line, sk.hoshi, sk.margin = (int(g) for g in nums.groups())
        for m in re.finditer(r'\{(\d+),\s*(\d+),\s*(' + name + r'_\w+)\}', chunk):
            w, h, arr = int(m.group(1)), int(m.group(2)), m.group(3)
            if arr not in arrays:
                raise ValueError('k_skins references unknown array ' + arr)
            if w * h != len(arrays[arr]):
                raise ValueError('%s: {%d,%d} but %d values' % (arr, w, h, len(arrays[arr])))
            sk.arrays[arr] = arrays[arr]
            sk.dims[arr] = (w, h)
    return [skins[n] for n in SKIN_NAMES]


# ------------------------------------------------------------------------------------------------
# Software board renderer (mirrors what board_layer must do)
# ------------------------------------------------------------------------------------------------

class Canvas:
    """RGB canvas; indices are resolved through a skin palette, index 0 = transparent."""

    def __init__(self, w, h, rgb=(0, 0, 0)):
        self.w, self.h = w, h
        self.px = [rgb] * (w * h)

    def put(self, x, y, rgb):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = rgb

    def get(self, x, y):
        return self.px[y * self.w + x]

    def fill(self, x0, y0, w, h, rgb):
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                self.put(x, y, rgb)

    def image(self, scale=1):
        im = Image.new('RGB', (self.w, self.h))
        im.putdata(self.px)
        if scale != 1:
            im = im.resize((self.w * scale, self.h * scale), Image.NEAREST)
        return im


def blit_indexed(cv, pal, arr, w, h, ox, oy, transparent=True):
    for y in range(h):
        for x in range(w):
            v = arr[y * w + x]
            if v or not transparent:
                cv.put(ox + x, oy + y, pal[v])


def tile_indexed(cv, pal, arr, w, h, x0, y0, x1, y1, ox, oy, transparent):
    """Tile an indexed texture over [x0, x1) x [y0, y1), phase-aligned to (ox, oy)."""
    for y in range(y0, y1):
        ty = (y - oy) % h
        for x in range(x0, x1):
            tx = (x - ox) % w
            v = arr[ty * w + tx]
            if v or not transparent:
                cv.put(x, y, pal[v])


def render_board(sk, size, cell, stones, area_w=BOARD_W, area_h=BOARD_H):
    """stones: dict (col, row) -> list of sprite field names, drawn in order."""
    pal = sk.palette
    n = sk.name
    grid_w = size * cell
    gx = (area_w - grid_w) // 2
    gy = (area_h - grid_w) // 2
    cv = Canvas(area_w, area_h, pal[sk.margin])
    bt, (bw, bh) = sk.arrays[n + '_board_texture'], sk.dims[n + '_board_texture']
    tile_indexed(cv, pal, bt, bw, bh, 0, 0, area_w, area_h, 0, 0, False)
    mt_name = n + '_margin_texture'
    if mt_name in sk.arrays:
        mt, (mw, mh) = sk.arrays[mt_name], sk.dims[mt_name]
        for y in range(area_h):
            for x in range(area_w):
                if gx <= x < gx + grid_w and gy <= y < gy + grid_w:
                    continue
                v = mt[(y % mh) * mw + (x % mw)]
                if v:
                    cv.put(x, y, pal[v])
    # grid
    half = cell // 2
    line = pal[sk.line]
    for k in range(size):
        x = gx + half + k * cell
        for y in range(gy + half, gy + half + (size - 1) * cell + 1):
            cv.put(x, y, line)
        y = gy + half + k * cell
        for x2 in range(gx + half, gx + half + (size - 1) * cell + 1):
            cv.put(x2, y, line)
    # hoshi
    hoshi = pal[sk.hoshi]
    r = 1 if cell >= 11 else 0
    for (hx, hy) in HOSHI.get(size, []):
        cx = gx + half + hx * cell
        cy = gy + half + hy * cell
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                cv.put(cx + dx, cy + dy, hoshi)
    # stones / markers
    for (col, row), fields in sorted(stones.items()):
        for f in fields:
            name = '%s_%d_%s' % (n, cell, f)
            arr, (w, h) = sk.arrays[name], sk.dims[name]
            blit_indexed(cv, pal, arr, w, h, gx + col * cell, gy + row * cell)
    return cv, gx, gy


# ------------------------------------------------------------------------------------------------
# Butano sprite sheets read back from the generated BMPs
# ------------------------------------------------------------------------------------------------

def read_sheet(path, frame_h):
    im = Image.open(path)
    pal = im.getpalette()
    idx = im.convert('P') if im.mode != 'P' else im
    w, h = im.size
    data = list(idx.getdata())
    frames = []
    for f in range(h // frame_h):
        fr = [data[(f * frame_h + y) * w + x] for y in range(frame_h) for x in range(w)]
        frames.append((w, frame_h, fr))
    colors = [tuple(pal[i * 3:i * 3 + 3]) for i in range(16)]
    return frames, colors


def blit_sprite(cv, frame, colors, ox, oy):
    w, h, data = frame
    for y in range(h):
        for x in range(w):
            v = data[y * w + x]
            if v:
                cv.put(ox + x, oy + y, colors[v])


# ------------------------------------------------------------------------------------------------
# Scenes
# ------------------------------------------------------------------------------------------------

# A small position per board size: (col, row) -> fields. Mirrors mockups 1d / 2b / 2c.
POSITIONS = {
    9: {(2, 2): ['black'], (3, 2): ['black'], (4, 3): ['black'], (2, 4): ['black'],
        (3, 3): ['white'], (4, 2): ['white'], (2, 3): ['white', 'last_marker'],
        (5, 5): ['white'], (6, 5): ['white_atari'], (6, 4): ['black'], (5, 4): ['black'],
        (6, 6): ['black', 'dead_marker'], (7, 6): ['white'],
        (1, 6): ['terr_black'], (1, 7): ['terr_black'], (7, 1): ['terr_white'],
        (4, 6): ['mission_marker'], (5, 2): ['ghost_black'], (0, 0): ['black'], (0, 1): ['white']},
    13: {(3, 3): ['black'], (4, 3): ['black'], (3, 4): ['white'], (4, 4): ['white', 'last_marker'],
         (6, 6): ['black'], (6, 7): ['black_atari'], (7, 6): ['white'], (7, 7): ['white'],
         (9, 3): ['black', 'dead_marker'], (9, 9): ['mission_marker'],
         (2, 9): ['terr_black'], (2, 10): ['terr_black'], (10, 2): ['terr_white'],
         (5, 5): ['ghost_white'], (0, 12): ['black'], (12, 0): ['white']},
    19: {(3, 3): ['black'], (4, 3): ['black'], (3, 4): ['white'], (4, 4): ['white', 'last_marker'],
         (9, 9): ['black'], (9, 10): ['black_atari'], (10, 9): ['white'], (10, 10): ['white'],
         (15, 3): ['black', 'dead_marker'], (15, 15): ['mission_marker'],
         (2, 15): ['terr_black'], (2, 16): ['terr_black'], (16, 2): ['terr_white'],
         (6, 6): ['ghost_black'], (0, 18): ['black'], (18, 0): ['white'],
         (12, 12): ['white'], (12, 13): ['white'], (13, 12): ['black'], (11, 12): ['black']},
}


def board_scene(sk, size, cell, gfx, skin_i, breath=0):
    area_w = max(BOARD_W, size * cell)
    area_h = max(BOARD_H, size * cell + 16)
    cv, gx, gy = render_board(sk, size, cell, POSITIONS[size], area_w, area_h)
    # cursor sprite on an empty point, ghost sprite next to it
    cur_frames, cur_cols = read_sheet(os.path.join(gfx, 'cursor_%d.bmp' % cell),
                                      32 if cell == 16 else 16)
    cur = cur_frames[skin_i * 2 + breath]
    ccol, crow = {9: (5, 2), 13: (5, 5), 19: (6, 6)}[size]
    cw = cur[0]                       # sprite top-left = intersection - size / 2
    blit_sprite(cv, cur, cur_cols, gx + ccol * cell + cell // 2 - cw // 2,
                gy + crow * cell + cell // 2 - cw // 2)
    ghost = read_sheet(os.path.join(gfx, 'ghost_%d.bmp' % cell), 16)
    gcol, grow = {9: (7, 4), 13: (8, 8), 19: (12, 6)}[size]
    blit_sprite(cv, ghost[0][skin_i * 2 + 1], ghost[1],
                gx + gcol * cell + cell // 2 - 8, gy + grow * cell + cell // 2 - 8)
    return cv


def sprite_sheet_scene(sk, gfx, skin_i):
    """Every StoneSet sprite of the skin on its board texture (rows 1-3), then the Butano sprite
    sheets that belong to this skin, read back from the generated BMPs (rows 4+)."""
    pal = sk.palette
    n = sk.name
    pad, step = 4, 20
    cols = len(SET_FIELDS)
    rows = [('cursor_16', 32, 2), ('cursor_11', 16, 2), ('cursor_8', 16, 2),
            ('ghost_16', 16, 2), ('ghost_11', 16, 2), ('ghost_8', 16, 2),
            ('particles', 8, 16)]
    if n == 'dino':
        rows.append(('egg_shards', 8, 6))
    if n == 'pup':
        rows.append(('confetti', 8, 12))
    w = pad + cols * step
    h = pad + 3 * step + sum(max(fh, 16) + 4 for _, fh, _ in rows) + pad
    cv = Canvas(w, h, pal[sk.bg])
    bt, (bw, bh) = sk.arrays[n + '_board_texture'], sk.dims[n + '_board_texture']
    tile_indexed(cv, pal, bt, bw, bh, 0, 0, w, h, 0, 0, False)
    for r, cell in enumerate(CELLS):
        for c, f in enumerate(SET_FIELDS):
            name = '%s_%d_%s' % (n, cell, f)
            arr, (aw, ah) = sk.arrays[name], sk.dims[name]
            blit_indexed(cv, pal, arr, aw, ah, pad + c * step + (16 - aw) // 2,
                         pad + r * step + (16 - ah) // 2)
    y = pad + 3 * step + 2
    for fn, fh, count in rows:
        frames, colors = read_sheet(os.path.join(gfx, fn + '.bmp'), fh)
        first = skin_i * count if len(frames) > count else 0
        x = pad
        for i in range(first, first + count):
            fr = frames[i]
            blit_sprite(cv, fr, colors, x, y)
            x += fr[0] + 2
        y += max(fh, 16) + 4
    return cv


def hstack(canvases, gap=6, bg=(20, 20, 24)):
    w = sum(c.w for c in canvases) + gap * (len(canvases) + 1)
    h = max(c.h for c in canvases) + 2 * gap
    out = Canvas(w, h, bg)
    x = gap
    for c in canvases:
        for yy in range(c.h):
            for xx in range(c.w):
                out.put(x + xx, gap + yy, c.get(xx, yy))
        x += c.w + gap
    return out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--out', default=os.path.join(ROOT, 'build-art'))
    ap.add_argument('--scale', type=int, default=3)
    args = ap.parse_args(argv)
    cpp = os.path.join(args.root, 'src', 'game', 'render', 'skin_data.cpp')
    gfx = os.path.join(args.root, 'graphics')
    skins = parse_cpp(cpp)
    os.makedirs(args.out, exist_ok=True)
    for i, sk in enumerate(skins):
        boards = [board_scene(sk, 9, 16, gfx, i), board_scene(sk, 13, 11, gfx, i),
                  board_scene(sk, 19, 8, gfx, i)]
        hstack(boards).image(args.scale).save(os.path.join(args.out, '%s_boards.png' % sk.name))
        sprite_sheet_scene(sk, gfx, i).image(6).save(
            os.path.join(args.out, '%s_sprites.png' % sk.name))
        board_scene(sk, 9, 16, gfx, i).image(1).save(
            os.path.join(args.out, '%s_9x9_1x.png' % sk.name))
        print('%-8s %2d colours, %d arrays' % (sk.name, len(sk.palette), len(sk.arrays)))
    print('previews in ' + args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
