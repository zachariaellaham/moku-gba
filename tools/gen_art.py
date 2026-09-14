#!/usr/bin/env python3
"""gen_art.py - board / stone / marker art for MOKU's three skins + cursor / particle sprites.

Everything here is drawn procedurally but deterministically (no random module), so a re-run
always produces byte-identical output.

Outputs (paths relative to the repo root, override with --root):
  src/game/render/skin_data.cpp   8bpp palette-index arrays consumed by the software board
                                  renderer; contract = src/game/render/skin_data.h
  graphics/cursor_16|11|8.bmp/.json   corner-bracket cursor, 6 frames each = skin*2 + breath
  graphics/particles.bmp/.json        8x8 squares, 48 frames = skin*16 + colour*4 + size(8,6,4,2)
  graphics/egg_shards.bmp/.json       DINO egg-crack shards, 6 frames = colour*3 + shard
  graphics/confetti.bmp/.json         PUP confetti, 12 frames = colour*3 + shape(4x4, 4x2, 2x4)
  graphics/ghost_16|11|8.bmp/.json    ghost stones as sprites, 6 frames = skin*2 + colour

Board cell convention (shared with board_layer): a stone sprite is cell x cell pixels and the
grid intersection sits at pixel (cell/2, cell/2) = (8,8) / (5,5) / (4,4). Stones are drawn
centred on that point: 16 px cell -> 14 px stone at 1..14 (+1 px drop shadow at 15),
11 px cell -> 9 px stone at 1..9 (+shadow at 10), 8 px cell -> 7 px stone at 1..7.
Cursor / ghost sprites are centred on the intersection: put the sprite's centre pixel
(8,8 for 16x16, 16,16 for 32x32) on the intersection.

Usage: python3 tools/gen_art.py [--root DIR] [--check]
"""
import argparse
import math
import os
import struct
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

SKIN_NAMES = ['classic', 'pup', 'dino']
CELLS = [16, 11, 8]

# ------------------------------------------------------------------------------------------------
# Pixel primitives
# ------------------------------------------------------------------------------------------------


class Pix:
    """A w x h grid of palette indices, row-major. Writes outside the grid are ignored."""

    def __init__(self, w, h, fill=0):
        self.w, self.h = w, h
        self.d = [fill] * (w * h)

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.d[y * self.w + x]
        return 0

    def put(self, x, y, v):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.d[y * self.w + x] = v

    def rect(self, x, y, w, h, v):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.put(xx, yy, v)

    def blit(self, other, ox, oy, transparent=True):
        for y in range(other.h):
            for x in range(other.w):
                v = other.get(x, y)
                if v or not transparent:
                    self.put(ox + x, oy + y, v)

    def rows(self):
        return [self.d[y * self.w:(y + 1) * self.w] for y in range(self.h)]

    def used(self):
        return set(self.d)

    def ascii(self):
        return ['.' if v == 0 else ('%x' % v if v < 16 else chr(ord('A') + v - 16))
                for v in self.d]


def hex2rgb(h):
    h = h.lstrip('#')
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def rgb2bgr555(rgb):
    r, g, b = rgb
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def blend(a, b, t):
    """Mix colour a towards colour b by t (0..1). Used to derive shades from the palette hexes."""
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


class Palette:
    """Index 0 is the transparent key (magenta in BMPs, unused in the board palette)."""

    def __init__(self, name, limit):
        self.name = name
        self.limit = limit
        self.colors = [(255, 0, 255)]
        self.names = {}

    def add(self, name, rgb):
        if isinstance(rgb, str):
            rgb = hex2rgb(rgb)
        rgb = tuple(int(c) for c in rgb)
        if name in self.names:
            raise ValueError('duplicate colour name ' + name)
        if rgb in self.colors[1:]:
            idx = self.colors.index(rgb)
        else:
            self.colors.append(rgb)
            idx = len(self.colors) - 1
        if len(self.colors) > self.limit:
            raise ValueError('%s: more than %d colours' % (self.name, self.limit))
        self.names[name] = idx
        return idx

    def __getitem__(self, name):
        return self.names[name]

    def rgb(self, idx):
        return self.colors[idx]


# ------------------------------------------------------------------------------------------------
# Shapes
# ------------------------------------------------------------------------------------------------


def blob_mask(x0, y0, w, h, egg=0.0, plump=0.0):
    """Set of (x, y) inside an ellipse of box (x0, y0, w, h).
    egg > 0 narrows the top rows (fat end at the bottom). plump > 0 squares the shape a little
    (superellipse), which makes small pixel circles read rounder."""
    cx = x0 + (w - 1) / 2.0
    cy = y0 + (h - 1) / 2.0
    rx = w / 2.0
    ry = h / 2.0
    pts = set()
    for y in range(y0, y0 + h):
        ty = (y - cy) / ry                      # -1 (top) .. +1 (bottom)
        hw = rx * (1.0 + egg * ty) if egg else rx
        for x in range(x0, x0 + w):
            tx = (x - cx) / hw
            n = 2.0 + plump
            if abs(tx) ** n + abs(ty) ** n <= 1.0 + 0.5 / max(w, h):
                pts.add((x, y))
    return pts


def mask_bbox(mask):
    xs = [p[0] for p in mask]
    ys = [p[1] for p in mask]
    return min(xs), min(ys), max(xs), max(ys)


def rim(mask):
    """Pixels of the mask that touch the outside (4-neighbourhood)."""
    out = set()
    for (x, y) in mask:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (x + dx, y + dy) not in mask:
                out.add((x, y))
                break
    return out


def shadow_of(mask, dx=1, dy=1):
    """Drop shadow: the mask shifted by (dx, dy), minus the mask itself."""
    return {(x + dx, y + dy) for (x, y) in mask} - mask


# ------------------------------------------------------------------------------------------------
# Stone rendering
# ------------------------------------------------------------------------------------------------

# Geometry per cell size: stone box (x0, y0, diameter), whether a drop shadow fits.
STONE_GEOM = {16: (1, 1, 14, True), 11: (1, 1, 9, True), 8: (1, 1, 7, False)}

# Hand-tuned 7 px masks (parametric ellipses look lumpy at this size).
MASK7_ROUND = [
    '.#####.',
    '#######',
    '#######',
    '#######',
    '#######',
    '#######',
    '.#####.',
]
MASK7_EGG = [
    '..###..',
    '.#####.',
    '#######',
    '#######',
    '#######',
    '#######',
    '.#####.',
]
MASK9_EGG = [
    '...###...',
    '..#####..',
    '.#######.',
    '#########',
    '#########',
    '#########',
    '#########',
    '.#######.',
    '..#####..',
]


def ascii_mask(rows, x0, y0):
    pts = set()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == '#':
                pts.add((x0 + x, y0 + y))
    return pts


def stone_mask(cell, shape):
    x0, y0, d, _ = STONE_GEOM[cell]
    if d == 7:
        return ascii_mask(MASK7_EGG if shape == 'egg' else MASK7_ROUND, x0, y0)
    if d == 9 and shape == 'egg':
        return ascii_mask(MASK9_EGG, x0, y0)
    if shape == 'egg':
        return blob_mask(x0, y0, d, d, egg=0.16, plump=0.15)
    return blob_mask(x0, y0, d, d, plump=0.25 if d <= 9 else 0.1)


# Speckle positions (relative to the stone box) chosen by hand so they look placed, not random.
SPECKLES = {
    14: [(4, 9), (9, 4), (10, 9), (6, 11)],
    9: [(3, 6), (6, 3), (6, 6)],
    7: [],
}


def draw_stone(cell, shape, c, style, speckle=False, dither=False, no_shadow=False):
    """Returns a cell x cell Pix.
    c: dict of palette indices: body, hi (highlight spot), mid (soft highlight band), shade,
       outline, shadow (optional), speck (optional).
    style: 'classic' | 'pup' | 'dino' (affects highlight placement only)."""
    x0, y0, d, has_shadow = STONE_GEOM[cell]
    p = Pix(cell, cell)
    mask = stone_mask(cell, shape)
    edge = rim(mask)
    inner = mask - edge
    cx = x0 + (d - 1) / 2.0
    cy = y0 + (d - 1) / 2.0
    r = d / 2.0
    if shape == 'egg':
        cy += 0.5                                  # fat end is lower: centre of mass sits lower
    # drop shadow first (under everything)
    if has_shadow and not no_shadow and c.get('shadow'):
        for (x, y) in shadow_of(mask):
            if x < cell and y < cell:
                p.put(x, y, c['shadow'])
    # highlight geometry
    hx, hy = cx - 0.38 * r, cy - 0.42 * r
    spot_r = 0.24 * r if d >= 9 else 0.5
    mid_r = 0.66 * r
    for (x, y) in inner:
        v = c['body']
        dxh, dyh = x - hx, y - hy
        dh = math.hypot(dxh, dyh)
        dxc, dyc = x - cx, y - cy
        dc = math.hypot(dxc, dyc)
        if d >= 9:
            if dh <= spot_r and c.get('hi'):
                v = c['hi']
            elif dh <= mid_r and c.get('mid'):
                v = c['mid']
            if c.get('shade') and dc >= r - 2.2 and (dxc + dyc) > 0.55 * r:
                v = c['shade']
        else:
            # 7 px: 2 tones + outline. One 2-px highlight at the upper left.
            if c.get('hi') and (x, y) in ((x0 + 2, y0 + 1), (x0 + 1, y0 + 2), (x0 + 2, y0 + 2)):
                v = c['hi'] if (x, y) == (x0 + 2, y0 + 2) else c.get('mid', c['hi'])
        p.put(x, y, v)
    if speckle and c.get('speck'):
        for (sx, sy) in SPECKLES[d]:
            if (x0 + sx, y0 + sy) in inner:
                p.put(x0 + sx, y0 + sy, c['speck'])
    for (x, y) in edge:
        p.put(x, y, c['outline'])
    if d == 7:
        # the rim leaves a square 5x5 body; knocking the inner corners out makes it read round
        for (x, y) in ((x0 + 1, y0 + 1), (x0 + 5, y0 + 1), (x0 + 1, y0 + 5), (x0 + 5, y0 + 5)):
            if (x, y) in inner:
                p.put(x, y, c['outline'])
    if dither:
        # ghost: keep the outline, checkerboard the interior
        for (x, y) in inner:
            if (x + y) & 1:
                p.put(x, y, 0)
        if has_shadow:
            for (x, y) in shadow_of(mask):
                p.put(x, y, 0)
    return p


# ------------------------------------------------------------------------------------------------
# Markers
# ------------------------------------------------------------------------------------------------


def centre_box(cell, size):
    """Top-left of a size x size box centred on the stone centre (same parity trick as stones)."""
    x0, _, d, _ = STONE_GEOM[cell]
    c = x0 + (d - 1) / 2.0
    return int(round(c - (size - 1) / 2.0 - 0.25))


def draw_last_marker(cell, fill, ring):
    """Small contrasting square centred on the stone: ring (light) outside, accent inside, so it
    reads on a black stone and on a white one."""
    p = Pix(cell, cell)
    size = {16: 6, 11: 5, 8: 5}[cell]
    o = centre_box(cell, size)
    p.rect(o, o, size, size, ring)
    p.rect(o + 1, o + 1, size - 2, size - 2, fill)
    return p


def halo_of(pts, diagonal):
    """1 px outline around a stroke set. The 8-neighbourhood is too fat below 16 px, so smaller
    markers only get an orthogonal halo."""
    out = set()
    deltas = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if diagonal:
        deltas += [(-1, -1), (1, -1), (-1, 1), (1, 1)]
    for (x, y) in pts:
        for (dx, dy) in deltas:
            if (x + dx, y + dy) not in pts:
                out.add((x + dx, y + dy))
    return out


def draw_dead_marker(cell, accent, halo):
    """X over a dead stone: accent strokes with a 1-px halo so it reads on both stone colours."""
    p = Pix(cell, cell)
    size = {16: 9, 11: 7, 8: 5}[cell]
    o = centre_box(cell, size)
    pts = set()
    for i in range(size):
        pts.add((o + i, o + i))
        pts.add((o + size - 1 - i, o + i))
    if cell == 16:
        # 2 px strokes
        pts |= {(x + 1, y) for (x, y) in pts}
    for (x, y) in halo_of(pts, cell == 16):
        p.put(x, y, halo)
    for (x, y) in pts:
        p.put(x, y, accent)
    return p


def draw_terr_marker(cell, fill, outline):
    p = Pix(cell, cell)
    size = {16: 6, 11: 5, 8: 5}[cell]
    o = centre_box(cell, size)
    p.rect(o, o, size, size, outline)
    p.rect(o + 1, o + 1, size - 2, size - 2, fill)
    return p


def draw_mission_marker(cell, accent, halo):
    """Triangle (the Go 'marked point' convention) in the accent colour with a 1-px light halo."""
    p = Pix(cell, cell)
    if cell == 16:
        rows = ['....##....',
                '...####...',
                '...####...',
                '..######..',
                '..######..',
                '.########.',
                '.########.',
                '##########']
        o = centre_box(cell, 10)
        oy = o - 1
    elif cell == 11:
        rows = ['...#...',
                '..###..',
                '..###..',
                '.#####.',
                '#######']
        o = centre_box(cell, 7)
        oy = o + 1
    else:
        rows = ['..#..',
                '.###.',
                '#####']
        o = centre_box(cell, 5)
        oy = o + 1
    pts = ascii_mask(rows, o, oy)
    for (x, y) in halo_of(pts, cell == 16):
        p.put(x, y, halo)
    for (x, y) in pts:
        p.put(x, y, accent)
    return p


# ------------------------------------------------------------------------------------------------
# Textures
# ------------------------------------------------------------------------------------------------


def hash2(x, y, salt=0):
    """Deterministic 0..255 hash for scattering texture specks."""
    v = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xffffffff
    v = ((v ^ (v >> 13)) * 1274126177) & 0xffffffff
    return (v ^ (v >> 16)) & 0xff


def texture_wood(pal):
    """32x32 tileable wood: base with wavy vertical grain lines, ~6 px apart, low contrast."""
    t = Pix(32, 32, pal['wood'])
    for k, (x0, phase, amp, col) in enumerate([
            (2, 0.0, 1.0, 'grain'), (9, 1.7, 1.4, 'grain2'), (14, 3.4, 1.0, 'grain'),
            (21, 0.9, 1.2, 'grain'), (27, 2.6, 1.3, 'grain2')]):
        for y in range(32):
            x = x0 + int(round(amp * math.sin(2 * math.pi * y / 32.0 + phase)))
            t.put(x % 32, y, pal[col])
            if k == 1 and (y % 11) == 5:
                t.put((x + 1) % 32, y, pal[col])
    for y in range(32):
        for x in range(32):
            if t.get(x, y) == pal['wood'] and hash2(x, y, 7) < 9:
                t.put(x, y, pal['wood_hi'])
    return t


def texture_lawn(pal):
    """32x32 mown lawn: alternating 8-px bands (two greens) + short blades + a few clover dots."""
    t = Pix(32, 32, pal['lawn'])
    for y in range(32):
        for x in range(32):
            band = (x // 8) & 1
            if band:
                t.put(x, y, pal['lawn_band'])
    blades = [(1, 3), (6, 9), (13, 2), (19, 6), (26, 4), (3, 17), (10, 20), (17, 14), (23, 19),
              (29, 16), (8, 27), (15, 30), (21, 25), (28, 28), (4, 12), (12, 11), (25, 11), (30, 22)]
    for i, (x, y) in enumerate(blades):
        col = pal['lawn_dark'] if i % 3 else pal['lawn_light']
        t.put(x, y, col)
        t.put(x, (y + 1) % 32, col)
        if i % 4 == 0:
            t.put((x + 1) % 32, y, col)
    return t


def texture_slab(pal):
    """32x32 cracked stone slab: flat base, two faint strata lines, organic cracks, sparse flecks.

    Deliberately low-contrast: the grid lines (#3f3020) and the stones must stay the loudest things
    on the board, so the slab only gives a gritty, weathered impression.
    """
    t = Pix(32, 32, pal['slab'])
    for y in range(32):
        for x in range(32):
            h = hash2(x, y, 3)
            if h < 12:
                t.put(x, y, pal['slab_light'])
            elif h > 246:
                t.put(x, y, pal['slab_dark'])
    # two faint strata lines with a 1 px wobble (they tile: the wobble is periodic in 32)
    for y0 in (5, 21):
        for x in range(32):
            y = y0 + (1 if (x % 16) in (5, 6, 7, 8, 9, 10) else 0)
            t.put(x, y, pal['joint'])
            if hash2(x, y, 11) < 90:
                t.put(x, y + 1, pal['slab_dark'])
    # organic cracks: thin polylines that start near a strata line and wander downwards
    cracks = [
        [(4, 6), (6, 9), (5, 12), (8, 15), (7, 18)],
        [(19, 6), (21, 8), (20, 11), (23, 13)],
        [(28, 22), (27, 25), (29, 28), (28, 30)],
        [(11, 22), (13, 25), (12, 28)],
        [(2, 24), (4, 27)],
    ]
    for line in cracks:
        for (ax, ay), (bx, by) in zip(line, line[1:]):
            n = max(abs(bx - ax), abs(by - ay))
            for i in range(n + 1):
                t.put(ax + (bx - ax) * i // n, ay + (by - ay) * i // n, pal['crack'])
    # a lit lip on the left of each crack so it reads as a fissure, not a scratch
    for line in cracks:
        for (x, y) in line[:-1]:
            t.put(x - 1, y, pal['slab_light'])
    return t


def _wrap_stamp(t, rows, ox, oy, cols):
    """Stamp an ascii sprite into a tileable texture, wrapping in x and y.
    cols maps each ascii character to a palette index."""
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in cols:
                t.put((ox + x) % t.w, (oy + y) % t.h, cols[ch])


# Margin textures are 16x16 but only ever seen as two 8 px bands: the board area is 160 px tall and
# the grid is 144/143/152 px, so the top margin is rows 0..7 of the tile and the bottom margin is
# rows 8..15 (both phase-aligned to the board area origin). Rows 0, 7, 8 and 15 are kept empty so a
# 9 px band (13x13) never cuts a decoration in half.

TUFT = ['#.#',
        '.#.',
        '.#.']
DAISY = ['.o.',
         'o+o',
         '.o.']
FERN = ['..#..',
        '.#|#.',
        '#.|.#',
        '.#|#.',
        '#.|.#',
        '.#|#.']
FOOT = ['#.#.#.#',
        '#.#.#.#',
        '.#####.',
        '#######',
        '#######',
        '.#####.']


def margin_grass(pal):
    """16x16 grass edge for PUP: blade tufts and two daisies, one band top, one band bottom."""
    t = Pix(16, 16)
    tuft_d = {'#': pal['lawn_dark']}
    tuft_l = {'#': pal['lawn_light']}
    daisy = {'o': pal['flower'], '+': pal['flower_heart']}
    for (x, y, c) in ((0, 4, tuft_d), (4, 3, tuft_l), (12, 4, tuft_d),
                      (9, 9, tuft_d), (13, 11, tuft_l), (1, 11, tuft_d), (5, 10, tuft_d)):
        _wrap_stamp(t, TUFT, x, y, c)
    _wrap_stamp(t, DAISY, 8, 3, daisy)
    _wrap_stamp(t, DAISY, 4, 12, daisy)
    # a few single blades to fill the bands without adding clutter
    for (x, y) in ((3, 6), (7, 6), (11, 2), (15, 5), (2, 9), (7, 13), (12, 9), (15, 13)):
        t.put(x, y, pal['lawn_dark'])
    return t


def margin_footprints(pal):
    """16x16 jungle edge for DINO: a three-toed footprint band on top, fern fronds below."""
    t = Pix(16, 16)
    foot = {'#': pal['print']}
    fern = {'#': pal['fern'], '|': pal['fern_dark']}
    # Both bands carry a footprint and a fern, because which band a margin strip lands on depends
    # on the board size (19x19 shows rows 0..7 top and bottom).
    _wrap_stamp(t, FOOT, 1, 1, foot)
    _wrap_stamp(t, FERN, 10, 1, fern)
    _wrap_stamp(t, FERN, 1, 9, fern)
    _wrap_stamp(t, FOOT, 8, 9, foot)
    for (x, y) in ((1, 1), (3, 1), (5, 1), (8, 9), (10, 9), (12, 9)):
        t.put(x, y, pal['print_deep'])      # toe tips press deeper into the mud
    return t


# ------------------------------------------------------------------------------------------------
# Skin definitions
# ------------------------------------------------------------------------------------------------


class SkinArt:
    def __init__(self, name):
        self.name = name
        self.pal = Palette(name, 48)
        self.sets = {}          # cell -> dict of Pix
        self.board_texture = None
        self.margin_texture = None
        self.bg = self.line = self.hoshi = self.margin = 0
        self.accent_rgb = None
        self.cursor_rgb = None          # (main, shadow, light)
        self.particle_rgbs = None       # 4 colours
        self.black_rgbs = None          # (body, outline) for ghost sprites
        self.white_rgbs = None


def stone_set(sk, shape, style, black, white, accent, speckle):
    """Build the 3 StoneSets. black/white: colour dicts (indices)."""
    pal = sk.pal
    for cell in CELLS:
        s = {}
        s['black'] = draw_stone(cell, shape, black, style, speckle=speckle)
        s['white'] = draw_stone(cell, shape, white, style, speckle=speckle)
        ba = dict(black)
        ba.update({'body': black['atari_body'], 'mid': black['atari_mid'], 'hi': black['atari_hi'],
                   'outline': accent, 'shade': black['atari_body']})
        wa = dict(white)
        wa.update({'body': white['atari_body'], 'mid': white['atari_body'], 'hi': white['atari_hi'],
                   'outline': accent, 'shade': white['atari_body']})
        s['black_atari'] = draw_stone(cell, shape, ba, style, speckle=False, no_shadow=True)
        s['white_atari'] = draw_stone(cell, shape, wa, style, speckle=False, no_shadow=True)
        gb = {'body': black['body'], 'outline': black['outline']}
        gw = {'body': white['body'], 'outline': white['outline']}
        s['ghost_black'] = draw_stone(cell, shape, gb, style, dither=True)
        s['ghost_white'] = draw_stone(cell, shape, gw, style, dither=True)
        s['last_marker'] = draw_last_marker(cell, accent, pal['paper'])
        s['dead_marker'] = draw_dead_marker(cell, accent, pal['paper'])
        s['terr_black'] = draw_terr_marker(cell, black['body'], pal['paper'])
        s['terr_white'] = draw_terr_marker(cell, white['body'], black['body'])
        s['mission_marker'] = draw_mission_marker(cell, accent, pal['paper'])
        sk.sets[cell] = s


def build_classic():
    sk = SkinArt('classic')
    pal = sk.pal
    wood = hex2rgb('#d9a95a')
    ink = hex2rgb('#1b1f2a')
    pal.add('wood', wood)
    pal.add('grain', blend(wood, ink, 0.07))
    pal.add('grain2', blend(wood, ink, 0.11))
    pal.add('wood_hi', blend(wood, (255, 255, 255), 0.10))
    pal.add('line', '#5a3a12')
    pal.add('shadow', blend(wood, (0, 0, 0), 0.35))
    pal.add('paper', '#efe6d2')
    pal.add('accent', '#c23b2c')
    slate = hex2rgb('#1d1d22')
    shell = hex2rgb('#f2efe6')
    pal.add('b_body', slate)
    pal.add('b_mid', blend(slate, (255, 255, 255), 0.13))
    pal.add('b_hi', blend(slate, (255, 255, 255), 0.30))
    pal.add('b_out', blend(slate, (0, 0, 0), 0.55))
    pal.add('b_at_body', blend(slate, (255, 255, 255), 0.20))
    pal.add('b_at_mid', blend(slate, (255, 255, 255), 0.34))
    pal.add('b_at_hi', blend(slate, (255, 255, 255), 0.50))
    pal.add('w_body', shell)
    pal.add('w_hi', (255, 255, 255))
    pal.add('w_shade', blend(shell, ink, 0.14))
    pal.add('w_out', blend(shell, ink, 0.42))
    pal.add('w_at_body', (255, 255, 255))
    pal.add('w_at_hi', (255, 255, 255))
    sk.board_texture = texture_wood(pal)
    sk.margin_texture = None
    sk.bg = pal['wood']
    sk.line = pal['line']
    sk.hoshi = pal['line']
    sk.margin = pal['wood']
    black = {'body': pal['b_body'], 'mid': pal['b_mid'], 'hi': pal['b_hi'], 'outline': pal['b_out'],
             'shadow': pal['shadow'], 'atari_body': pal['b_at_body'], 'atari_mid': pal['b_at_mid'],
             'atari_hi': pal['b_at_hi']}
    white = {'body': pal['w_body'], 'mid': pal['w_body'], 'hi': pal['w_hi'], 'shade': pal['w_shade'],
             'outline': pal['w_out'], 'shadow': pal['shadow'], 'atari_body': pal['w_at_body'],
             'atari_hi': pal['w_at_hi']}
    stone_set(sk, 'round', 'classic', black, white, pal['accent'], speckle=False)
    sk.accent_rgb = hex2rgb('#c23b2c')
    sk.cursor_rgb = (hex2rgb('#c23b2c'), ink, hex2rgb('#efe6d2'))
    sk.particle_rgbs = [ink, hex2rgb('#c23b2c'), hex2rgb('#efe6d2'), blend(wood, (255, 255, 255), 0.35)]
    sk.black_rgbs = (pal.rgb(pal['b_body']), pal.rgb(pal['b_out']))
    sk.white_rgbs = (pal.rgb(pal['w_body']), pal.rgb(pal['w_out']))
    return sk


def build_pup():
    sk = SkinArt('pup')
    pal = sk.pal
    lawn = hex2rgb('#8fd05e')
    ink = hex2rgb('#1b1f2a')
    pal.add('lawn', lawn)
    pal.add('lawn_band', blend(lawn, (0, 0, 0), 0.05))
    pal.add('lawn_dark', blend(lawn, (20, 80, 20), 0.30))
    pal.add('lawn_light', blend(lawn, (255, 255, 200), 0.28))
    pal.add('line', '#f4f1e6')
    pal.add('shadow', blend(lawn, (0, 0, 0), 0.30))
    pal.add('paper', '#ffffff')
    pal.add('accent', '#f39a3e')
    pal.add('flower', '#ffffff')
    pal.add('flower_heart', '#ffe066')
    blue = hex2rgb('#3f6fc4')
    orange = hex2rgb('#f39a3e')
    pal.add('b_body', blue)
    pal.add('b_mid', blend(blue, (255, 255, 255), 0.18))
    pal.add('b_hi', blend(blue, (255, 255, 255), 0.50))
    pal.add('b_speck', blend(blue, (255, 255, 255), 0.36))
    pal.add('b_shade', blend(blue, ink, 0.22))
    pal.add('b_out', blend(blue, ink, 0.50))
    pal.add('b_at_body', blend(blue, (255, 255, 255), 0.30))
    pal.add('b_at_mid', blend(blue, (255, 255, 255), 0.45))
    pal.add('b_at_hi', blend(blue, (255, 255, 255), 0.70))
    pal.add('w_body', orange)
    pal.add('w_mid', blend(orange, (255, 255, 255), 0.20))
    pal.add('w_hi', blend(orange, (255, 255, 255), 0.62))
    pal.add('w_speck', blend(orange, (255, 255, 255), 0.40))
    pal.add('w_shade', blend(orange, ink, 0.22))
    pal.add('w_out', blend(orange, ink, 0.52))
    pal.add('w_at_body', blend(orange, (255, 255, 255), 0.35))
    pal.add('w_at_hi', blend(orange, (255, 255, 255), 0.75))
    sk.board_texture = texture_lawn(pal)
    sk.margin_texture = margin_grass(pal)
    sk.bg = pal['lawn']
    sk.line = pal['line']
    sk.hoshi = pal['line']
    sk.margin = pal['lawn']
    black = {'body': pal['b_body'], 'mid': pal['b_mid'], 'hi': pal['b_hi'], 'speck': pal['b_speck'],
             'shade': pal['b_shade'], 'outline': pal['b_out'], 'shadow': pal['shadow'],
             'atari_body': pal['b_at_body'], 'atari_mid': pal['b_at_mid'], 'atari_hi': pal['b_at_hi']}
    white = {'body': pal['w_body'], 'mid': pal['w_mid'], 'hi': pal['w_hi'], 'speck': pal['w_speck'],
             'shade': pal['w_shade'], 'outline': pal['w_out'], 'shadow': pal['shadow'],
             'atari_body': pal['w_at_body'], 'atari_hi': pal['w_at_hi']}
    stone_set(sk, 'round', 'pup', black, white, pal['accent'], speckle=True)
    sk.accent_rgb = orange
    sk.cursor_rgb = (orange, ink, (255, 255, 255))
    sk.particle_rgbs = [blue, orange, (255, 255, 255), hex2rgb('#ffe066')]
    sk.black_rgbs = (pal.rgb(pal['b_body']), pal.rgb(pal['b_out']))
    sk.white_rgbs = (pal.rgb(pal['w_body']), pal.rgb(pal['w_out']))
    return sk


def build_dino():
    sk = SkinArt('dino')
    pal = sk.pal
    slab = hex2rgb('#8a7a5a')
    ink = hex2rgb('#2a1a12')
    pal.add('slab', slab)
    pal.add('slab_light', blend(slab, (255, 255, 230), 0.10))
    pal.add('slab_dark', blend(slab, ink, 0.10))
    pal.add('joint', blend(slab, ink, 0.28))
    pal.add('crack', blend(slab, ink, 0.40))
    pal.add('line', '#3f3020')
    pal.add('shadow', blend(slab, (0, 0, 0), 0.40))
    pal.add('paper', '#f2dcb8')
    pal.add('accent', '#ff6a1a')
    pal.add('print', blend(slab, ink, 0.50))
    pal.add('print_deep', blend(slab, ink, 0.68))
    pal.add('fern', '#5f8034')
    pal.add('fern_dark', '#4d6b2a')
    obs = hex2rgb('#1a1410')
    amber = hex2rgb('#e0a030')
    pal.add('b_body', obs)
    pal.add('b_mid', blend(obs, (200, 170, 150), 0.14))
    pal.add('b_hi', blend(obs, (220, 200, 180), 0.34))
    pal.add('b_speck', blend(obs, (200, 170, 150), 0.10))
    pal.add('b_out', (5, 3, 2))
    pal.add('b_at_body', blend(obs, (255, 200, 150), 0.24))
    pal.add('b_at_mid', blend(obs, (255, 200, 150), 0.38))
    pal.add('b_at_hi', blend(obs, (255, 220, 180), 0.55))
    pal.add('w_body', amber)
    pal.add('w_mid', blend(amber, (255, 255, 230), 0.22))
    pal.add('w_hi', blend(amber, (255, 255, 230), 0.62))
    pal.add('w_speck', blend(amber, ink, 0.30))
    pal.add('w_shade', blend(amber, ink, 0.24))
    pal.add('w_out', blend(amber, ink, 0.56))
    pal.add('w_at_body', blend(amber, (255, 255, 220), 0.38))
    pal.add('w_at_hi', blend(amber, (255, 255, 230), 0.78))
    sk.board_texture = texture_slab(pal)
    sk.margin_texture = margin_footprints(pal)
    sk.bg = pal['slab']
    sk.line = pal['line']
    sk.hoshi = pal['line']
    sk.margin = pal['slab']
    black = {'body': pal['b_body'], 'mid': pal['b_mid'], 'hi': pal['b_hi'], 'speck': pal['b_speck'],
             'outline': pal['b_out'], 'shadow': pal['shadow'], 'atari_body': pal['b_at_body'],
             'atari_mid': pal['b_at_mid'], 'atari_hi': pal['b_at_hi']}
    white = {'body': pal['w_body'], 'mid': pal['w_mid'], 'hi': pal['w_hi'], 'speck': pal['w_speck'],
             'shade': pal['w_shade'], 'outline': pal['w_out'], 'shadow': pal['shadow'],
             'atari_body': pal['w_at_body'], 'atari_hi': pal['w_at_hi']}
    stone_set(sk, 'egg', 'dino', black, white, pal['accent'], speckle=True)
    sk.accent_rgb = hex2rgb('#ff6a1a')
    sk.cursor_rgb = (hex2rgb('#ff6a1a'), ink, hex2rgb('#f2dcb8'))
    sk.particle_rgbs = [hex2rgb('#ff6a1a'), amber, blend(obs, (200, 170, 150), 0.24), hex2rgb('#ffd050')]
    sk.black_rgbs = (pal.rgb(pal['b_body']), pal.rgb(pal['b_out']))
    sk.white_rgbs = (pal.rgb(pal['w_body']), pal.rgb(pal['w_out']))
    return sk


def build_all():
    return [build_classic(), build_pup(), build_dino()]


# ------------------------------------------------------------------------------------------------
# skin_data.cpp emitter
# ------------------------------------------------------------------------------------------------

SET_FIELDS = ['black', 'white', 'black_atari', 'white_atari', 'ghost_black', 'ghost_white',
              'last_marker', 'dead_marker', 'terr_black', 'terr_white', 'mission_marker']


def emit_array(out, name, pix, comment=True):
    out.append('alignas(4) constexpr uint8_t %s[%d] = {' % (name, pix.w * pix.h))
    a = pix.ascii()
    rows = pix.rows()
    for y, row in enumerate(rows):
        body = ','.join('%d' % v for v in row)
        if comment:
            out.append('    %s,  // %s' % (body, ''.join(a[y * pix.w:(y + 1) * pix.w])))
        else:
            out.append('    %s,' % body)
    out.append('};')


def sprite_ref(name, pix):
    return '{%d, %d, %s}' % (pix.w, pix.h, name)


def emit_cpp(skins):
    out = []
    out.append('// Generated by tools/gen_art.py - DO NOT EDIT (edit the generator and re-run it).')
    out.append('// 8bpp palette-index pixel data for the software board renderer. See skin_data.h.')
    out.append('#include "skin_data.h"')
    out.append('')
    out.append('namespace skins {')
    out.append('namespace {')
    total = 0
    for sk in skins:
        n = sk.name
        out.append('')
        out.append('// ' + '=' * 96)
        out.append('// %s' % n.upper())
        out.append('// ' + '=' * 96)
        pal = [rgb2bgr555(c) for c in sk.pal.colors]
        pal[0] = 0
        while len(pal) % 16:
            pal.append(0)
        out.append('// palette: %d used colours (+ index 0 transparent), padded to %d for bn::bg_palette_item'
                   % (len(sk.pal.colors) - 1, len(pal)))
        names = sorted(sk.pal.names.items(), key=lambda kv: kv[1])
        out.append('// ' + ' '.join('%d=%s' % (i, nm) for nm, i in names))
        out.append('alignas(4) constexpr uint16_t %s_palette[%d] = {' % (n, len(pal)))
        for i in range(0, len(pal), 8):
            out.append('    ' + ','.join('0x%04x' % v for v in pal[i:i + 8]) + ',')
        out.append('};')
        total += len(pal) * 2
        emit_array(out, n + '_board_texture', sk.board_texture, comment=False)
        total += sk.board_texture.w * sk.board_texture.h
        if sk.margin_texture is not None:
            emit_array(out, n + '_margin_texture', sk.margin_texture)
            total += sk.margin_texture.w * sk.margin_texture.h
        for cell in CELLS:
            for f in SET_FIELDS:
                pix = sk.sets[cell][f]
                emit_array(out, '%s_%d_%s' % (n, cell, f), pix)
                total += pix.w * pix.h
    out.append('')
    out.append('constexpr SkinData k_skins[SKIN_COUNT] = {')
    for sk in skins:
        n = sk.name
        pal_len = len(sk.pal.colors)
        pal_len += (16 - pal_len % 16) % 16
        out.append('    {')
        out.append('        %s_palette, %d,' % (n, pal_len))
        out.append('        %d, %d, %d, %d,  // bg, line, hoshi, margin' % (sk.bg, sk.line, sk.hoshi, sk.margin))
        out.append('        %s,' % sprite_ref(n + '_board_texture', sk.board_texture))
        if sk.margin_texture is not None:
            out.append('        %s,' % sprite_ref(n + '_margin_texture', sk.margin_texture))
        else:
            out.append('        {0, 0, nullptr},')
        out.append('        {')
        for cell in CELLS:
            refs = ', '.join(sprite_ref('%s_%d_%s' % (n, cell, f), sk.sets[cell][f]) for f in SET_FIELDS)
            out.append('            {%d, %s},' % (cell, refs))
        out.append('        },')
        out.append('    },')
    out.append('};')
    out.append('')
    out.append('}  // namespace')
    out.append('')
    out.append('const SkinData& skin(Skin s) { return k_skins[static_cast<int>(s)]; }')
    out.append('')
    out.append('}  // namespace skins')
    out.append('')
    return '\n'.join(out), total


# ------------------------------------------------------------------------------------------------
# Butano 4bpp sprite sheets
# ------------------------------------------------------------------------------------------------


def write_bmp4(path, pix, palette_rgb):
    """Uncompressed 4bpp indexed BMP (40-byte header, bottom-up rows) as read by butano/tools/bmp.py.
    pix.w must be a multiple of 8. palette_rgb: up to 16 (r, g, b), entry 0 = transparent key."""
    if pix.w % 8 or len(palette_rgb) > 16:
        raise ValueError('bad sprite sheet %s' % path)
    if max(pix.d) > 15:
        raise ValueError('%s uses more than 16 colours' % path)
    row_bytes = ((pix.w * 4 + 31) // 32) * 4
    pixel_offset = 14 + 40 + 16 * 4
    image_size = row_bytes * pix.h
    out = bytearray()
    out += b'BM' + struct.pack('<IHHI', pixel_offset + image_size, 0, 0, pixel_offset)
    out += struct.pack('<IiiHHIIiiII', 40, pix.w, pix.h, 1, 4, 0, image_size, 2835, 2835, 16, 16)
    pal = list(palette_rgb) + [(0, 0, 0)] * (16 - len(palette_rgb))
    for r, g, b in pal:
        out += bytes((b, g, r, 0))
    for row in reversed(pix.rows()):
        line = bytearray()
        for x in range(0, pix.w, 2):
            line.append((row[x] << 4) | row[x + 1])
        while len(line) < row_bytes:
            line.append(0)
        out += line
    with open(path, 'wb') as f:
        f.write(out)


def write_sprite_json(path, frame_h):
    with open(path, 'w') as f:
        f.write('{\n    "type": "sprite",\n    "height": %d,\n    "bpp_mode": "bpp_4",\n'
                '    "compression": "none"\n}\n' % frame_h)


class Sheet:
    """Vertical stack of equal frames sharing one 16-colour palette."""

    def __init__(self, frame_w, frame_h):
        self.fw, self.fh = frame_w, frame_h
        self.frames = []
        self.pal = Palette('sheet', 16)

    def add(self, pix):
        if pix.w != self.fw or pix.h != self.fh:
            raise ValueError('frame size mismatch')
        self.frames.append(pix)

    def col(self, name, rgb):
        if name in self.pal.names:
            return self.pal[name]
        return self.pal.add(name, rgb)

    def write(self, base):
        sheet = Pix(self.fw, self.fh * len(self.frames))
        for i, fr in enumerate(self.frames):
            sheet.blit(fr, 0, i * self.fh, transparent=False)
        write_bmp4(base + '.bmp', sheet, self.pal.colors)
        write_sprite_json(base + '.json', self.fh)


def draw_cursor(size, cell, breath, main, shadow, light):
    """Corner brackets around a (cell + 2) box centred on the sprite centre; breath 1 = 1 px tighter."""
    p = Pix(size, size)
    box = {16: 18, 11: 13, 8: 10}[cell] - 2 * breath
    arm = {16: 6, 11: 4, 8: 3}[cell] - breath
    thick = 2 if cell == 16 else 1
    # Butano places a sprite of side `size` with its top-left at (intersection - size / 2), so the
    # intersection is sprite pixel size/2: centre the bracket box on that pixel (+1 for odd boxes).
    o = (size - box + 1) // 2
    x1 = o + box - 1
    y1 = o + box - 1
    strokes = set()
    for t in range(thick):
        for i in range(arm):
            # top-left
            strokes.add((o + i, o + t))
            strokes.add((o + t, o + i))
            # top-right
            strokes.add((x1 - i, o + t))
            strokes.add((x1 - t, o + i))
            # bottom-left
            strokes.add((o + i, y1 - t))
            strokes.add((o + t, y1 - i))
            # bottom-right
            strokes.add((x1 - i, y1 - t))
            strokes.add((x1 - t, y1 - i))
    for (x, y) in strokes:
        if (x + 1, y + 1) not in strokes:
            p.put(x + 1, y + 1, shadow)
    for (x, y) in strokes:
        p.put(x, y, main)
    if cell == 16:
        # inner light edge on the two-pixel brackets gives a bevelled, "lit" look
        for (x, y) in strokes:
            if (x - 1, y - 1) not in strokes and (x, y - 1) not in strokes and (x - 1, y) not in strokes:
                p.put(x, y, light)
    return p


def write_cursors(skins, gfx):
    for cell in CELLS:
        size = 32 if cell == 16 else 16
        sh = Sheet(size, size)
        for sk in skins:
            main, shadow, light = sk.cursor_rgb
            m = sh.col(sk.name + '_main', main)
            s = sh.col(sk.name + '_shadow', shadow)
            l = sh.col(sk.name + '_light', light)
            for breath in (0, 1):
                sh.add(draw_cursor(size, cell, breath, m, s, l))
        sh.write(os.path.join(gfx, 'cursor_%d' % cell))


def write_particles(skins, gfx):
    sh = Sheet(8, 8)
    for sk in skins:
        for ci, rgb in enumerate(sk.particle_rgbs):
            c = sh.col('%s_%d' % (sk.name, ci), rgb)
            for size in (8, 6, 4, 2):
                p = Pix(8, 8)
                o = (8 - size) // 2
                p.rect(o, o, size, size, c)
                sh.add(p)
    sh.write(os.path.join(gfx, 'particles'))


SHARDS = [
    ['........',
     '..#.....',
     '.###....',
     '.####...',
     '..###...',
     '...##...',
     '........',
     '........'],
    ['........',
     '.....#..',
     '...###..',
     '..####..',
     '..###...',
     '..##....',
     '...#....',
     '........'],
    ['........',
     '........',
     '..####..',
     '.#####..',
     '..#####.',
     '...###..',
     '........',
     '........'],
]


def write_egg_shards(dino, gfx):
    sh = Sheet(8, 8)
    body_b, out_b = dino.black_rgbs
    body_w, out_w = dino.white_rgbs
    sets = [
        (sh.col('obs', body_b), sh.col('obs_out', out_b), sh.col('obs_hi', blend(body_b, (220, 200, 180), 0.34)),
         sh.col('inner', hex2rgb('#f2dcb8'))),
        (sh.col('amber', body_w), sh.col('amber_out', out_w), sh.col('amber_hi', blend(body_w, (255, 255, 230), 0.62)),
         sh.col('inner', hex2rgb('#f2dcb8'))),
    ]
    for body, outline, hi, inner in sets:
        for rows in SHARDS:
            pts = ascii_mask(rows, 0, 0)
            p = Pix(8, 8)
            edge = rim(pts)
            for (x, y) in pts:
                p.put(x, y, body)
            for (x, y) in edge:
                # the broken edge (right/bottom side) shows the pale inside of the shell
                if (x + 1, y) not in pts or (x, y + 1) not in pts:
                    p.put(x, y, inner)
                else:
                    p.put(x, y, outline)
            x0, y0, _, _ = mask_bbox(pts)
            p.put(x0 + 1, y0 + 1, hi)
            sh.add(p)
    sh.write(os.path.join(gfx, 'egg_shards'))


def write_confetti(pup, gfx):
    sh = Sheet(8, 8)
    colours = [('orange', pup.accent_rgb), ('blue', hex2rgb('#3f6fc4')), ('white', (255, 255, 255)),
               ('pink', hex2rgb('#ff85c2'))]
    for name, rgb in colours:
        c = sh.col(name, rgb)
        for (w, h) in ((4, 4), (4, 2), (2, 4)):
            p = Pix(8, 8)
            p.rect((8 - w) // 2, (8 - h) // 2, w, h, c)
            sh.add(p)
    sh.write(os.path.join(gfx, 'confetti'))


def write_ghosts(skins, gfx):
    """Ghost stones as sprites (alternative to blitting StoneSet::ghost_* into the board layer).
    The sprite centre sits on the intersection, like the cursor."""
    for cell in CELLS:
        sh = Sheet(16, 16)
        off = {16: 0, 11: 3, 8: 4}[cell]
        for sk in skins:
            for colour in ('black', 'white'):
                src = sk.sets[cell]['ghost_' + colour]
                body_rgb, out_rgb = sk.black_rgbs if colour == 'black' else sk.white_rgbs
                body = sh.col('%s_%s' % (sk.name, colour), body_rgb)
                outline = sh.col('%s_%s_out' % (sk.name, colour), out_rgb)
                src_body = sk.pal['b_body' if colour == 'black' else 'w_body']
                p = Pix(16, 16)
                for y in range(cell):
                    for x in range(cell):
                        v = src.get(x, y)
                        if v == 0:
                            continue
                        p.put(off + x, off + y, body if v == src_body else outline)
                sh.add(p)
        sh.write(os.path.join(gfx, 'ghost_%d' % cell))


# ------------------------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------------------------


def validate(skins):
    for sk in skins:
        if len(sk.pal.colors) > 48:
            raise ValueError('%s palette has %d colours' % (sk.name, len(sk.pal.colors)))
        if 0 in sk.board_texture.used():
            raise ValueError('%s board texture uses index 0' % sk.name)
        for cell in CELLS:
            for f in SET_FIELDS:
                pix = sk.sets[cell][f]
                if pix.w != cell or pix.h != cell:
                    raise ValueError('%s %d %s has size %dx%d' % (sk.name, cell, f, pix.w, pix.h))
                if 0 not in pix.used():
                    raise ValueError('%s %d %s is fully opaque' % (sk.name, cell, f))
                if len(pix.used()) < 2:
                    raise ValueError('%s %d %s is empty' % (sk.name, cell, f))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=ROOT, help='repo root (default: parent of tools/)')
    ap.add_argument('--check', action='store_true', help='only validate, write nothing')
    args = ap.parse_args(argv)
    skins = build_all()
    validate(skins)
    cpp, total = emit_cpp(skins)
    if args.check:
        print('ok: skin_data.cpp would hold %d bytes of pixel/palette data' % total)
        return 0
    cpp_path = os.path.join(args.root, 'src', 'game', 'render', 'skin_data.cpp')
    with open(cpp_path, 'w') as f:
        f.write(cpp)
    gfx = os.path.join(args.root, 'graphics')
    os.makedirs(gfx, exist_ok=True)
    write_cursors(skins, gfx)
    write_particles(skins, gfx)
    write_egg_shards(skins[2], gfx)
    write_confetti(skins[1], gfx)
    write_ghosts(skins, gfx)
    for sk in skins:
        print('%-8s %2d colours' % (sk.name, len(sk.pal.colors) - 1))
    print('wrote %s (%d bytes of const data)' % (os.path.relpath(cpp_path, args.root), total))
    return 0


if __name__ == '__main__':
    sys.exit(main())
