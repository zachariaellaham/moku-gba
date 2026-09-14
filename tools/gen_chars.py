#!/usr/bin/env python3
"""gen_chars.py - character and screen art for MOKU (Tactics of Go).

Every pixel here is drawn by code (PIL primitives + explicit pixel work): the helpers, the five
opponents, the title/map/HUD backgrounds and all the UI plates are original designs, nothing is
traced or copied. Re-running the script is deterministic (no `random`), so the committed BMPs are
byte-identical to a fresh run.

Outputs (relative to the repo root, override with --root):

  sprites (4bpp, first palette colour = transparent key)
    graphics/helper_sen|helper_indi|helper_rex.bmp   64x64, 6 frames:
        0 portrait (48x48 art, framed)   1 idle1   2 idle2   3 cheer   4 sad   5 think
        poses are 56x56 art centred in the 64x64 frame
    graphics/opponents.bmp      64x64, 5 frames (40x40 framed art centred):
        0 pebble  1 sprout  2 koan  3 ember  4 tengen
    graphics/map_node.bmp       16x16, 8 frames: cleared A/B, current A/B, locked A/B, pick A/B
    graphics/rank_stamp.bmp     64x64, 4 frames: S A B C
    graphics/star.bmp           8x8, 3 frames: filled, empty, burst
    graphics/menu_cursor.bmp    8x8, 4 frames: right x2 (bob), down, up
    graphics/banner_plate.bmp   64x32, 6 frames: skin(classic,pup,dino) x style(flat, slanted)
    graphics/egg_crack.bmp      16x16, 4 frames: crack 1..3 + shatter
    graphics/speech_bubble.bmp  64x32, 8 frames: skin(classic,pup,dino) x part(left, mid, right+tail)
                                plus 2 extra: mid with tail, blank
    graphics/title_logo.bmp     64x64, 2 frames: "MO" and "KU" halves of the brush logo

  regular backgrounds (4bpp, 16 colours)
    graphics/bg_title.bmp       256x256 - ink-wash mountains, paper/ink palette
    graphics/bg_map.bmp         512x256 - scrolling ink landscape with the campaign path
    graphics/bg_hud_classic|bg_hud_pup|bg_hud_dino.bmp  256x256 - play scene HUD panel per skin
    graphics/bg_panels.bmp      256x1024, 4 maps: 0 dialogue box, 1 briefing box (portrait well),
                                2 ink bar (32 px), 3 paper header bar (18 px)

Usage:
    python3 tools/gen_chars.py                 # regenerate graphics/
    python3 tools/gen_chars.py --check         # exit 1 if the committed assets are stale
    python3 tools/gen_chars.py --sheet out.png # PIL contact sheet of everything
"""
import argparse
import hashlib
import os
import struct
import sys

from PIL import Image, ImageDraw

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# -------------------------------------------------------------------------------------------------
# Palettes
# -------------------------------------------------------------------------------------------------

# Shared game palette (PLAN.md rule 4 + the mockups).
PAPER = '#efe6d2'
INK = '#1b1f2a'
WOOD = '#d9a95a'
VERM = '#c23b2c'
GREY = '#8a8070'
GREY_D = '#5a5040'
GREY_L = '#c9c0aa'
GOLD = '#d9a95a'


def hex2rgb(h):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


class Palette:
    """Ordered name -> index map. Index 0 is the transparent key for sprites."""

    def __init__(self, key='#ff00ff'):
        self.names = ['__key__']
        self.colors = [hex2rgb(key)]

    def add(self, name, hexstr):
        if name in self.names:
            raise ValueError('duplicate palette entry ' + name)
        rgb = hex2rgb(hexstr)
        if rgb in self.colors:
            raise ValueError('duplicate colour %s (%s)' % (hexstr, name))
        if len(self.names) >= 16:
            raise ValueError('palette overflow adding ' + name)
        self.names.append(name)
        self.colors.append(rgb)
        return len(self.names) - 1

    def __getitem__(self, name):
        return self.names.index(name)

    def i(self, name):
        return self.names.index(name)

    def rgb_list(self):
        return list(self.colors) + [(0, 0, 0)] * (16 - len(self.colors))


def palette_of(key, pairs):
    p = Palette(key)
    for name, col in pairs:
        p.add(name, col)
    return p


# -------------------------------------------------------------------------------------------------
# Canvas: an indexed image drawn with PIL primitives
# -------------------------------------------------------------------------------------------------


class Canvas:
    """Palette-index raster. All PIL drawing happens in mode 'L' so nothing is anti-aliased."""

    def __init__(self, w, h, fill=0):
        self.w, self.h = w, h
        self.img = Image.new('L', (w, h), fill)
        self.d = ImageDraw.Draw(self.img)

    # --- primitives (all boxes are inclusive: x0..x1, y0..y1) ---
    def rect(self, x0, y0, x1, y1, c):
        self.d.rectangle([x0, y0, x1, y1], fill=c)

    def frame(self, x0, y0, x1, y1, c, w=1):
        self.d.rectangle([x0, y0, x1, y1], outline=c, width=w)

    def ell(self, x0, y0, x1, y1, c, outline=None, w=1):
        self.d.ellipse([x0, y0, x1, y1], fill=c, outline=outline, width=w)

    def ring(self, x0, y0, x1, y1, c, w=1):
        self.d.ellipse([x0, y0, x1, y1], fill=None, outline=c, width=w)

    def poly(self, pts, c, outline=None):
        self.d.polygon(pts, fill=c, outline=outline)

    def line(self, pts, c, w=1):
        self.d.line(pts, fill=c, width=w)

    def arc(self, x0, y0, x1, y1, a0, a1, c, w=1):
        self.d.arc([x0, y0, x1, y1], a0, a1, fill=c, width=w)

    def pt(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.img.putpixel((x, y), c)

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.img.getpixel((x, y))
        return 0

    def dots(self, pts, c):
        for (x, y) in pts:
            self.pt(x, y, c)

    def hline(self, x0, x1, y, c):
        self.d.line([x0, y, x1, y], fill=c)

    def vline(self, x, y0, y1, c):
        self.d.line([x, y0, x, y1], fill=c)

    def taper_curve(self, pts, c, r0, r1):
        """Brush stroke: a disc whose radius goes from r0 to r1 along the polyline."""
        total = 0
        segs = []
        for i in range(len(pts) - 1):
            (x0, y0), (x1, y1) = pts[i], pts[i + 1]
            n = max(abs(x1 - x0), abs(y1 - y0), 1)
            segs.append(n)
            total += n
        done = 0
        for i, n in enumerate(segs):
            (x0, y0), (x1, y1) = pts[i], pts[i + 1]
            for s in range(n + 1):
                t = (done + s) / total
                r = int(round(r0 + (r1 - r0) * t))
                x = x0 + (x1 - x0) * s // n
                y = y0 + (y1 - y0) * s // n
                self.ell(x - r, y - r, x + r, y + r, c)
            done += n

    def thick_curve(self, pts, c, r):
        """Round-capped polyline: a disc of radius r stamped along each segment."""
        for i in range(len(pts) - 1):
            x0, y0 = pts[i]
            x1, y1 = pts[i + 1]
            steps = max(abs(x1 - x0), abs(y1 - y0), 1)
            for s in range(steps + 1):
                x = x0 + (x1 - x0) * s // steps
                y = y0 + (y1 - y0) * s // steps
                self.ell(x - r, y - r, x + r, y + r, c)

    # --- passes ---
    def contour(self, ink, bg=0, diagonals=False, edges=True):
        """Turn every edge pixel of the silhouette into `ink` (inline outline, keeps the size).

        edges=False leaves the canvas borders alone, so tiled parts join without a seam."""
        src = self.img.copy()
        px = src.load()
        offs = [(-1, 0), (1, 0), (0, -1), (0, 1)]
        if diagonals:
            offs += [(-1, -1), (1, -1), (-1, 1), (1, 1)]
        out = self.img.load()
        for y in range(self.h):
            for x in range(self.w):
                if px[x, y] == bg:
                    continue
                for dx, dy in offs:
                    nx, ny = x + dx, y + dy
                    if nx < 0 or ny < 0 or nx >= self.w or ny >= self.h:
                        if not edges:
                            continue
                        out[x, y] = ink
                        break
                    if px[nx, ny] == bg:
                        out[x, y] = ink
                        break

    def outline_out(self, ink, bg=0):
        """1 px outline drawn *outside* the silhouette (grows it by one pixel)."""
        src = self.img.load()
        todo = []
        for y in range(self.h):
            for x in range(self.w):
                if src[x, y] != bg:
                    continue
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < self.w and 0 <= ny < self.h and src[nx, ny] not in (bg, ink):
                        todo.append((x, y))
                        break
        self.dots(todo, ink)

    def snapshot(self):
        return self.img.copy()

    def clip(self, snap):
        """Erase everything that is transparent in `snap` (keeps a decoration inside a silhouette)."""
        sp = snap.load()
        out = self.img.load()
        for y in range(self.h):
            for x in range(self.w):
                if sp[x, y] == 0:
                    out[x, y] = 0

    def replace(self, a, b):
        src = self.img.load()
        for y in range(self.h):
            for x in range(self.w):
                if src[x, y] == a:
                    src[x, y] = b

    def shade_dir(self, pairs, dx, dy, dist=2):
        """Darken lit -> shaded where the pixel `dist` towards (dx, dy) is background."""
        src = self.img.copy().load()
        out = self.img.load()
        for y in range(self.h):
            for x in range(self.w):
                v = out[x, y]
                if v not in pairs:
                    continue
                nx, ny = x + dx * dist, y + dy * dist
                if nx < 0 or ny < 0 or nx >= self.w or ny >= self.h or src[nx, ny] == 0:
                    out[x, y] = pairs[v]

    def stamp(self, art, x0, y0, legend):
        """ASCII art: one character per pixel, '.' = skip."""
        for j, row in enumerate(art):
            for i, ch in enumerate(row):
                if ch == '.':
                    continue
                self.pt(x0 + i, y0 + j, legend[ch])

    def blit(self, other, ox, oy, transparent=True):
        src = other.img.load()
        for y in range(other.h):
            for x in range(other.w):
                v = src[x, y]
                if v or not transparent:
                    self.pt(ox + x, oy + y, v)

    def sub(self, x0, y0, w, h):
        c = Canvas(w, h)
        c.img.paste(self.img.crop((x0, y0, x0 + w, y0 + h)), (0, 0))
        c.d = ImageDraw.Draw(c.img)
        return c

    def flip_h(self):
        c = Canvas(self.w, self.h)
        c.img.paste(self.img.transpose(Image.FLIP_LEFT_RIGHT), (0, 0))
        c.d = ImageDraw.Draw(c.img)
        return c

    def max_index(self):
        return max(self.img.tobytes())

    def rows(self):
        data = self.img.tobytes()
        return [data[y * self.w:(y + 1) * self.w] for y in range(self.h)]


# -------------------------------------------------------------------------------------------------
# Butano asset writers
# -------------------------------------------------------------------------------------------------

_written = {}
DRY_RUN = False


def _emit(path, data):
    _written[path] = data
    old = None
    if os.path.exists(path):
        with open(path, 'rb') as f:
            old = f.read()
    if old == data:
        return False
    if not DRY_RUN:
        with open(path, 'wb') as f:
            f.write(data)
    return True


def bmp4_bytes(canvas, palette_rgb):
    """Uncompressed 4bpp indexed BMP with a 40-byte header, as butano/tools/bmp.py expects."""
    w, h = canvas.w, canvas.h
    if w % 8:
        raise ValueError('width %d is not a multiple of 8' % w)
    if canvas.max_index() > 15:
        raise ValueError('more than 16 colours')
    row_bytes = ((w * 4 + 31) // 32) * 4
    pixel_offset = 14 + 40 + 16 * 4
    image_size = row_bytes * h
    out = bytearray()
    out += b'BM' + struct.pack('<IHHI', pixel_offset + image_size, 0, 0, pixel_offset)
    out += struct.pack('<IiiHHIIiiII', 40, w, h, 1, 4, 0, image_size, 2835, 2835, 16, 16)
    pal = list(palette_rgb) + [(0, 0, 0)] * (16 - len(palette_rgb))
    for r, g, b in pal[:16]:
        out += bytes((b, g, r, 0))
    for row in reversed(canvas.rows()):
        line = bytearray()
        for x in range(0, w, 2):
            line.append((row[x] << 4) | row[x + 1])
        while len(line) < row_bytes:
            line.append(0)
        out += line
    return bytes(out)


SPRITES = {}   # name -> (canvas, palette, frame_h)
BGS = {}       # name -> (canvas, palette, map_h)


def write_sprite(gfx, name, canvas, palette, frame_h):
    SPRITES[name] = (canvas, palette, frame_h)
    _emit(os.path.join(gfx, name + '.bmp'), bmp4_bytes(canvas, palette.rgb_list()))
    js = ('{\n    "type": "sprite",\n    "height": %d,\n    "bpp_mode": "bpp_4",\n'
          '    "compression": "none"\n}\n' % frame_h)
    _emit(os.path.join(gfx, name + '.json'), js.encode())


def write_bg(gfx, name, canvas, palette, map_h=None):
    BGS[name] = (canvas, palette, map_h or canvas.h)
    _emit(os.path.join(gfx, name + '.bmp'), bmp4_bytes(canvas, palette.rgb_list()))
    fields = ['"type": "regular_bg"', '"bpp_mode": "bpp_4"']
    if map_h:
        fields.append('"height": %d' % map_h)
    fields.append('"tiles_compression": "run_length"')
    fields.append('"palette_compression": "none"')
    # butano cannot index into a compressed cell stream, so a multi-map item needs a raw map
    # (bn_regular_bg_map_item::cells_ptr asserts on compression + maps_count > 1).
    multi = map_h is not None and canvas.h > map_h
    fields.append('"map_compression": "%s"' % ('none' if multi else 'run_length'))
    js = '{\n    ' + ',\n    '.join(fields) + '\n}\n'
    _emit(os.path.join(gfx, name + '.json'), js.encode())


class Sheet:
    """Vertical stack of equally sized frames sharing one palette."""

    def __init__(self, w, h, palette):
        self.w, self.h = w, h
        self.pal = palette
        self.frames = []

    def add(self, canvas):
        if canvas.w != self.w or canvas.h != self.h:
            raise ValueError('frame is %dx%d, expected %dx%d' % (canvas.w, canvas.h, self.w, self.h))
        self.frames.append(canvas)
        return len(self.frames) - 1

    def canvas(self):
        c = Canvas(self.w, self.h * len(self.frames))
        for i, f in enumerate(self.frames):
            c.blit(f, 0, i * self.h, transparent=False)
        return c

    def write(self, gfx, name):
        write_sprite(gfx, name, self.canvas(), self.pal, self.h)


# -------------------------------------------------------------------------------------------------
# Helpers: INDI (blue pup), REX (green t-rex), SEN (old master)
# -------------------------------------------------------------------------------------------------

POSES = ['idle1', 'idle2', 'cheer', 'sad', 'think']

PAL_INDI = palette_of('#ff00ff', [
    ('ink', INK), ('blue', '#5b86cf'), ('blue_d', '#2f5390'), ('blue_l', '#8fb2ea'),
    ('cream', '#f2efe6'), ('cream_d', '#cbc2ae'), ('tan', '#d9a95a'), ('tan_d', '#a97a3a'),
    ('orange', '#f39a3e'), ('orange_d', '#c26f22'), ('white', '#ffffff'), ('pink', '#e4899a'),
    ('paper', PAPER), ('hatch', '#dbd2bd'), ('grey', GREY),
])


def _eye(c, x, y, kind, ink, white, pupil_up=0, skin=0):
    """A 7x9 cartoon eye whose top-left is (x, y). kind: open, happy, sad, half, wide, star."""
    if kind == 'open' or kind == 'wide':
        h = 9 if kind == 'wide' else 8
        c.ell(x, y, x + 6, y + h - 1, white)
        c.ell(x + 1, y + 2 - pupil_up, x + 5, y + h - 2 - pupil_up, ink)
        c.pt(x + 2, y + 3 - pupil_up, white)
        c.pt(x + 3, y + 3 - pupil_up, white)
        c.pt(x + 2, y + 4 - pupil_up, white)
    elif kind == 'happy':          # ^ ^ closed smiling eye
        pts = [(x, y + 6), (x + 1, y + 4), (x + 2, y + 3), (x + 3, y + 2), (x + 4, y + 3),
               (x + 5, y + 4), (x + 6, y + 6)]
        c.dots(pts, ink)
        c.dots([(p[0], p[1] + 1) for p in pts], ink)
    elif kind == 'sad':            # droopy lid over a small pupil
        c.ell(x, y + 1, x + 6, y + 8, white)
        c.ell(x + 1, y + 4, x + 5, y + 8, ink)
        c.pt(x + 2, y + 5, white)
        lid = [(x, y + 1), (x + 1, y + 1), (x + 2, y + 2), (x + 3, y + 3), (x + 4, y + 3),
               (x + 5, y + 4), (x + 6, y + 5)]
        for (px, py) in lid:
            for k in range(y, py):
                c.pt(px, k, skin)
        c.dots(lid, ink)
    elif kind == 'half':           # sleepy / thinking: pupil high, heavy lid
        c.ell(x, y + 1, x + 6, y + 8, white)
        c.ell(x + 1, y + 2, x + 5, y + 7, ink)
        c.pt(x + 2, y + 3, white)
        c.hline(x, x + 6, y + 1, ink)
        c.hline(x, x + 6, y + 2, ink)
    elif kind == 'closed':
        c.hline(x + 1, x + 5, y + 4, ink)
        c.hline(x + 2, x + 4, y + 5, ink)


def part(w, h, fn, ink, diagonals=False):
    """Draw one body mass into its own canvas and give it a 1 px ink contour."""
    c = Canvas(w, h)
    fn(c)
    c.contour(ink, diagonals=diagonals)
    return c




def band_top(c, col, thickness, x0, x1, dy=0):
    """Fill a band of `thickness` rows that follows the top edge of the silhouette (a collar)."""
    for x in range(x0, x1 + 1):
        top = None
        for y in range(c.h):
            if c.get(x, y):
                top = y
                break
        if top is None:
            continue
        for k in range(thickness):
            c.pt(x, top + dy + k, col)


def indi_head(pose, P):
    """INDI's head + ears as one outlined mass (40x35). Shared by the poses and the portrait."""
    ink, blue, bl_d = P['ink'], P['blue'], P['blue_d']
    cream, cream_d = P['cream'], P['cream_d']
    tan, tan_d, white, pink, bl_l = P['tan'], P['tan_d'], P['white'], P['pink'], P['blue_l']
    sad, cheer, think = pose == 'sad', pose == 'cheer', pose == 'think'

    def head(p):
        if sad:                                     # ears fold down along the cheeks
            p.poly([(7, 12), (1, 25), (13, 20)], bl_d)
            p.poly([(30, 12), (36, 25), (24, 20)], bl_d)
            p.poly([(7, 15), (4, 22), (11, 19)], tan_d)
            p.poly([(30, 15), (33, 22), (26, 19)], tan_d)
        else:
            up = 2 if cheer else 0
            p.poly([(8, 16), (2, 2 - up), (17, 10)], bl_d)
            p.poly([(29, 16), (35, 2 - up), (20, 10)], bl_d)
            p.poly([(9, 14), (6, 6 - up), (14, 11)], tan_d)
            p.poly([(28, 14), (31, 6 - up), (23, 11)], tan_d)
        p.ell(3, 6, 34, 31, blue)                   # skull
        p.ell(3, 6, 34, 16, bl_d)                   # dark cap
        p.ell(4, 10, 15, 24, bl_d)                  # heeler patch over one eye
        p.ell(3, 19, 9, 25, tan)                    # tan cheek points
        p.ell(28, 19, 34, 25, tan)
        p.ell(10, 18, 27, 30, cream)                # muzzle
        p.ell(13, 24, 24, 30, cream_d)
        p.ell(9, 8, 14, 11, tan)                    # eyebrow dots
        p.ell(23, 8, 28, 11, tan)
        kind = {'cheer': 'happy', 'sad': 'sad', 'think': 'half'}.get(pose, 'open')
        _eye(p, 8, 12, kind, ink, white, pupil_up=1 if think else 0, skin=bl_d)
        _eye(p, 23, 12, kind, ink, white, pupil_up=1 if think else 0, skin=blue)
        p.ell(15, 19, 21, 22, ink)                  # nose
        p.pt(16, 20, bl_l)
        p.pt(17, 20, bl_l)
        mx, my = 18, 23
        if cheer:
            p.ell(mx - 5, my, mx + 6, my + 7, ink)
            p.ell(mx - 3, my + 4, mx + 4, my + 7, pink)
        elif sad:
            p.dots([(mx - 3, my + 2), (mx - 2, my + 1), (mx - 1, my), (mx, my), (mx + 1, my),
                    (mx + 2, my + 1), (mx + 3, my + 2)], ink)
        else:
            p.vline(mx, my - 1, my, ink)
            p.dots([(mx - 1, my + 1), (mx - 2, my + 1), (mx - 3, my), (mx - 4, my - 1),
                    (mx + 1, my + 1), (mx + 2, my + 1), (mx + 3, my), (mx + 4, my - 1)], ink)
            if not think:
                p.dots([(mx + 2, my + 2), (mx + 3, my + 2), (mx + 4, my + 1)], pink)
    return part(38, 32, head, ink)


def indi_pose(pose):
    """INDI: an original blue cartoon pup - heeler colouring (two blues, tan points, cream chest),
    orange collar, big triangular ears. 56x56 art, feet on row 55."""
    P = PAL_INDI
    ink, blue, bl_d, bl_l = P['ink'], P['blue'], P['blue_d'], P['blue_l']
    cream, cream_d = P['cream'], P['cream_d']
    tan, tan_d, orange, orange_d = P['tan'], P['tan_d'], P['orange'], P['orange_d']
    white = P['white']
    c = Canvas(56, 56)

    bob = 1 if pose == 'idle2' else 0
    sad, cheer, think = pose == 'sad', pose == 'cheer', pose == 'think'
    hdx = 2 if think else 0
    hdy = bob + (3 if sad else 0)

    def arm(p, raised=False):
        if raised:
            p.thick_curve([(8, 16), (5, 10), (5, 6)], blue, 3)
            p.ell(1, 0, 10, 9, cream)
            p.dots([(4, 3), (7, 3)], tan_d)
        else:
            p.thick_curve([(8, 2), (5, 8), (5, 11)], blue, 3)
            p.ell(1, 8, 10, 17, cream)
            p.dots([(4, 14), (7, 14)], tan_d)

    # ---- tail -----------------------------------------------------------------------------
    def tail(p):
        if cheer:
            p.thick_curve([(2, 14), (7, 6), (9, 2)], blue, 3)
            p.thick_curve([(7, 6), (9, 2)], bl_l, 1)
        elif sad:
            p.thick_curve([(2, 4), (7, 10), (8, 15)], bl_d, 3)
        else:
            p.thick_curve([(2, 15), (8, 10), (10, 3 + bob * 2)], blue, 3)
            p.thick_curve([(8, 10), (10, 3 + bob * 2)], bl_l, 1)
    c.blit(part(14, 18, tail, ink), 40, 29 + (6 if sad else 0))

    # ---- feet -----------------------------------------------------------------------------
    def foot(p):
        p.ell(0, 0, 13, 10, cream)
        p.ell(3, 5, 10, 10, cream_d)
        p.dots([(4, 3), (7, 2), (10, 3)], tan_d)
    c.blit(part(14, 11, foot, ink), 11, 45 + bob)
    c.blit(part(14, 11, foot, ink), 31, 45 + bob)

    # ---- arms behind the body --------------------------------------------------------------
    if not cheer and not think:
        y = 32 if sad else 30 + bob
        c.blit(part(12, 18, arm, ink), 5, y)
        c.blit(part(12, 18, arm, ink).flip_h(), 39, y)

    # ---- torso ------------------------------------------------------------------------------
    def torso(p):
        p.ell(0, 0, 29, 23, blue)
        p.ell(0, 0, 13, 19, bl_d)
        p.ell(6, 4, 23, 23, cream)
        p.ell(9, 11, 20, 23, cream_d)
        band_top(p, orange, 4, 2, 27, dy=1)          # collar hugs the shoulders
        band_top(p, orange_d, 1, 2, 27, dy=5)
        band_top(p, ink, 1, 2, 27, dy=6)
    c.blit(part(30, 24, torso, ink), 13, 28 + bob)
    c.blit(part(9, 9, lambda p: (p.ell(0, 0, 8, 8, orange_d), p.ell(1, 1, 6, 6, tan)), ink),
           24, 35 + bob)

    # ---- head -------------------------------------------------------------------------------
    c.blit(indi_head(pose, P), 9 + hdx, hdy)

    # ---- arms in front ----------------------------------------------------------------------
    if cheer:
        c.blit(part(12, 18, lambda p: arm(p, raised=True), ink), 4, 16)
        c.blit(part(12, 18, lambda p: arm(p, raised=True), ink).flip_h(), 40, 16)
    if think:
        c.blit(part(12, 18, arm, ink), 5, 32)
        def paw(p):
            p.thick_curve([(4, 17), (4, 10), (7, 6)], blue, 3)
            p.ell(2, 0, 11, 9, cream)
            p.dots([(5, 3), (8, 3)], tan_d)
        c.blit(part(13, 18, paw, ink), 33, 24)

    # ---- extras -----------------------------------------------------------------------------
    if cheer:
        for (sx, sy) in ((3, 10), (52, 12), (27, 0)):
            c.dots([(sx, sy - 2), (sx, sy - 1), (sx, sy + 1), (sx, sy + 2),
                    (sx - 2, sy), (sx - 1, sy), (sx + 1, sy), (sx + 2, sy)], orange)
            c.pt(sx, sy, white)
    elif sad:
        c.blit(part(5, 7, lambda p: p.ell(0, 0, 4, 6, bl_l), ink), 15, 29)
        c.dots([(16, 31), (16, 32)], white)
    elif think:
        for (dx, dy, r) in ((46, 13, 1), (50, 8, 1), (52, 3, 0)):
            c.blit(part(2 * r + 4, 2 * r + 4,
                        lambda p, r=r: p.ell(1, 1, 2 * r + 2, 2 * r + 2, cream), ink),
                   dx - r - 1, dy - r - 1)
    return c


PAL_REX = palette_of('#ff00ff', [
    ('ink', INK), ('green', '#6aa83c'), ('green_d', '#3f6b22'), ('green_l', '#95d45e'),
    ('cream', '#f2dcb8'), ('cream_d', '#cfae7c'), ('white', '#ffffff'), ('red', '#c23b2c'),
    ('amber', '#e6b24c'), ('amber_d', '#a87524'), ('paper', PAPER), ('hatch', '#dbd2bd'),
    ('grey', GREY), ('sky', '#7fb6e0'),
])


def rex_head(pose, P):
    """REX's head: big snout, round eyes, amber crest. 40x32 outlined mass."""
    ink, green, gr_d, gr_l = P['ink'], P['green'], P['green_d'], P['green_l']
    cream, cream_d, white, red = P['cream'], P['cream_d'], P['white'], P['red']
    amber, amber_d = P['amber'], P['amber_d']
    sad, cheer, think = pose == 'sad', pose == 'cheer', pose == 'think'

    def head(p):
        # crest spikes along the skull
        for (x, y, w, h) in ((9, 1, 5, 6), (16, 0, 6, 7), (24, 1, 5, 6)):
            p.poly([(x, y + h), (x + w // 2, y), (x + w, y + h)], amber)
        p.ell(3, 5, 35, 29, green)                    # skull
        p.ell(3, 5, 35, 16, gr_l)                     # lit top
        p.ell(8, 15, 31, 31, green)                   # snout block
        p.ell(10, 22, 29, 31, cream)                  # jaw / chin
        # nostrils
        p.dots([(16, 19), (17, 19), (22, 19), (23, 19)], gr_d)
        # eyes
        kind = {'cheer': 'happy', 'sad': 'sad', 'think': 'half'}.get(pose, 'wide')
        _eye(p, 7, 8, kind, ink, white, pupil_up=1 if think else 0, skin=gr_l)
        _eye(p, 25, 8, kind, ink, white, pupil_up=1 if think else 0, skin=gr_l)
        # brow ridges
        p.rect(6, 6, 13, 7, gr_d)
        p.rect(25, 6, 32, 7, gr_d)
        # mouth
        if cheer:                                     # roaring grin
            p.ell(10, 22, 29, 31, ink)
            p.ell(13, 27, 26, 31, red)
            for tx in (12, 17, 22, 26):
                p.poly([(tx, 22), (tx + 4, 22), (tx + 2, 26)], white)
            for tx in (14, 20, 24):
                p.poly([(tx, 31), (tx + 4, 31), (tx + 2, 28)], white)
        elif sad:
            p.dots([(14, 28), (15, 27), (16, 26), (17, 26), (18, 26), (19, 26), (20, 26),
                    (21, 26), (22, 27), (23, 28)], ink)
        else:
            p.line([(11, 25), (17, 27), (24, 27), (28, 25)], ink)
            p.poly([(13, 25), (17, 25), (15, 21)], white)   # tooth poking out
            p.poly([(22, 25), (26, 25), (24, 21)], white)
            if not think:
                p.ell(17, 27, 22, 30, red)                  # tongue tip
    return part(40, 32, head, ink)


def rex_pose(pose):
    """REX: a small green cartoon T-rex - cream belly, tiny arms, amber crest, big eyes."""
    P = PAL_REX
    ink, green, gr_d, gr_l = P['ink'], P['green'], P['green_d'], P['green_l']
    cream, cream_d, white, amber = P['cream'], P['cream_d'], P['white'], P['amber']
    c = Canvas(56, 56)
    bob = 1 if pose == 'idle2' else 0
    sad, cheer, think = pose == 'sad', pose == 'cheer', pose == 'think'
    hdx = 2 if think else 0
    hdy = bob + (3 if sad else 0)

    # ---- tail --------------------------------------------------------------------------------
    def tail(p):
        if cheer:
            path = [(1, 16), (8, 11), (15, 4)]
        elif sad:
            path = [(1, 10), (8, 15), (15, 18)]
        else:
            path = [(1, 14), (8, 11 - bob), (16, 8 - bob * 2)]
        p.thick_curve(path, green, 5)
        p.thick_curve(path[1:], green, 3)
        for k in range(3):
            t = (k + 1) / 4.0
            i = 1 if t > 0.5 else 0
            x0, y0 = path[i]
            x1, y1 = path[i + 1]
            u = (t - 0.5 * i) * 2
            sx = int(x0 + (x1 - x0) * u)
            sy = int(y0 + (y1 - y0) * u) - 4 + k
            p.poly([(sx - 2, sy + 4), (sx + 2, sy + 4), (sx, sy)], amber)
    c.blit(part(22, 24, tail, ink), 35, 29)

    # ---- feet --------------------------------------------------------------------------------
    def foot(p):
        p.ell(0, 0, 15, 10, green)
        p.ell(2, 4, 13, 10, gr_l)
        for tx in (1, 6, 11):                         # claws
            p.poly([(tx, 10), (tx + 4, 10), (tx + 2, 6)], cream)
    c.blit(part(16, 11, foot, ink), 8, 45 + bob)
    c.blit(part(16, 11, foot, ink), 32, 45 + bob)

    # ---- body --------------------------------------------------------------------------------
    def torso(p):
        p.ell(0, 0, 29, 23, green)
        p.ell(0, 0, 29, 10, gr_l)
        p.ell(7, 4, 24, 23, cream)
        p.ell(10, 12, 21, 23, cream_d)
        for y in (9, 13, 17):                          # belly plates
            p.hline(9, 22, y, cream_d)
    c.blit(part(30, 24, torso, ink), 13, 28 + bob)

    # ---- tiny arms ---------------------------------------------------------------------------
    def arm(p, raised=False):
        if raised:
            p.thick_curve([(7, 9), (3, 4)], green, 3)
            p.dots([(1, 1), (0, 3), (2, 0)], cream)
        else:
            p.thick_curve([(7, 2), (3, 7)], green, 3)
            p.dots([(1, 9), (0, 7), (2, 10)], cream)
    if cheer:
        c.blit(part(10, 12, lambda p: arm(p, raised=True), ink), 6, 26)
        c.blit(part(10, 12, lambda p: arm(p, raised=True), ink).flip_h(), 40, 26)
    elif think:
        c.blit(part(10, 12, arm, ink), 8, 34)
        c.blit(part(10, 12, lambda p: arm(p, raised=True), ink).flip_h(), 36, 24)
    else:
        y = 36 if sad else 34 + bob
        c.blit(part(10, 12, arm, ink), 8, y)
        c.blit(part(10, 12, arm, ink).flip_h(), 38, y)

    # ---- head --------------------------------------------------------------------------------
    c.blit(rex_head(pose, P), 8 + hdx, hdy)

    # ---- extras ------------------------------------------------------------------------------
    if cheer:
        for (sx, sy) in ((2, 12), (53, 8), (28, 0)):
            c.dots([(sx, sy - 2), (sx, sy - 1), (sx, sy + 1), (sx, sy + 2),
                    (sx - 2, sy), (sx - 1, sy), (sx + 1, sy), (sx + 2, sy)], amber)
            c.pt(sx, sy, white)
    elif sad:
        c.blit(part(5, 7, lambda p: p.ell(0, 0, 4, 6, P['sky']), ink), 14, 26)
        c.dots([(15, 28), (15, 29)], white)
    elif think:
        for (dx, dy, r) in ((46, 13, 1), (50, 8, 1), (52, 3, 0)):
            c.blit(part(2 * r + 4, 2 * r + 4,
                        lambda p, r=r: p.ell(1, 1, 2 * r + 2, 2 * r + 2, cream), ink),
                   dx - r - 1, dy - r - 1)
    return c


PAL_SEN = palette_of('#ff00ff', [
    ('ink', INK), ('robe', '#7e7a6c'), ('robe_d', '#514c42'), ('robe_l', '#a9a292'),
    ('hair', '#f4f1e6'), ('hair_d', '#d5d0c0'), ('skin', '#e0b083'), ('skin_d', '#b98456'),
    ('verm', VERM), ('paper', PAPER), ('hatch', '#dbd2bd'), ('grey', GREY), ('white', '#ffffff'),
    ('gold', GOLD),
])


def sen_head(pose, P):
    """SEN's face: topknot, white hair, long brows, moustache. 28x30 outlined mass."""
    ink, hair, hair_d = P['ink'], P['hair'], P['hair_d']
    skin, skin_d, white = P['skin'], P['skin_d'], P['white']
    sad, cheer, think = pose == 'sad', pose == 'cheer', pose == 'think'

    def head(p):
        p.ell(10, 0, 17, 5, hair)                      # topknot
        p.vline(13, 1, 4, hair_d)
        p.ell(2, 3, 25, 28, skin)                      # face
        p.ell(2, 2, 25, 9, hair)                       # hair cap
        p.ell(5, 5, 22, 11, skin)                      # receded hairline
        p.ell(0, 7, 5, 22, hair)                       # side hair
        p.ell(22, 7, 27, 22, hair)
        p.ell(1, 10, 4, 20, hair_d)
        p.ell(23, 10, 26, 20, hair_d)
        p.ell(4, 4, 23, 8, hair_d)
        # long brows
        if sad:
            p.dots([(4, 15), (5, 14), (6, 13), (7, 13), (8, 13), (9, 14), (10, 15)], hair)
            p.dots([(23, 15), (22, 14), (21, 13), (20, 13), (19, 13), (18, 14), (17, 15)], hair)
        else:
            p.rect(4, 12, 10, 13, hair)
            p.rect(17, 12, 23, 13, hair)
            p.dots([(3, 13), (3, 14), (4, 14), (24, 13), (24, 14), (23, 14)], hair_d)
        # eyes
        ey = 17
        if cheer:
            p.dots([(5, ey + 1), (6, ey), (7, ey - 1), (8, ey), (9, ey + 1)], ink)
            p.dots([(18, ey + 1), (19, ey), (20, ey - 1), (21, ey), (22, ey + 1)], ink)
        elif sad:
            p.dots([(5, ey - 1), (6, ey), (7, ey), (8, ey), (9, ey + 1)], ink)
            p.dots([(18, ey + 1), (19, ey), (20, ey), (21, ey), (22, ey - 1)], ink)
        else:
            p.rect(5, ey, 9, ey + 2, white)
            p.rect(18, ey, 22, ey + 2, white)
            p.rect(6, ey, 8, ey + 2, ink) if think else p.rect(6, ey + 1, 8, ey + 2, ink)
            p.rect(19, ey, 21, ey + 2, ink) if think else p.rect(19, ey + 1, 21, ey + 2, ink)
            p.hline(5, 9, ey - 1, ink)
            p.hline(18, 22, ey - 1, ink)
        p.dots([(13, 19), (13, 20), (14, 21), (12, 21)], skin_d)     # nose
        p.ell(3, 21, 6, 23, P['verm'] if cheer else skin_d)          # cheeks
        p.ell(21, 21, 24, 23, P['verm'] if cheer else skin_d)
        # moustache
        p.ell(4, 22, 23, 27, hair)
        p.dots([(3, 24), (4, 23), (24, 24), (23, 23)], hair_d)
        p.hline(9, 18, 25, hair_d)
        mouth = 27
        if cheer:
            p.ell(11, mouth - 1, 16, mouth + 2, ink)
        elif sad:
            p.dots([(11, mouth + 1), (12, mouth), (13, mouth), (14, mouth), (15, mouth),
                    (16, mouth + 1)], hair_d)
        else:
            p.hline(11, 16, mouth, hair_d)
    return part(28, 30, head, ink)


def sen_beard(P):
    """The long white beard, drawn over the robe. 18x20."""
    ink, hair, hair_d = P['ink'], P['hair'], P['hair_d']

    def beard(p):
        p.ell(0, 0, 17, 17, hair)
        p.poly([(3, 10), (14, 10), (10, 19), (7, 19)], hair)
        p.vline(8, 4, 18, hair_d)
        p.dots([(4, 8), (4, 9), (13, 8), (13, 9), (6, 14), (11, 14)], hair_d)
    return part(18, 20, beard, ink)


def sen_pose(pose):
    """SEN: the old Go master - grey robe, vermilion sash, long white beard, calm."""
    P = PAL_SEN
    ink, robe, robe_d, robe_l = P['ink'], P['robe'], P['robe_d'], P['robe_l']
    skin, verm, gold, white = P['skin'], P['verm'], P['gold'], P['white']
    c = Canvas(56, 56)
    bob = 1 if pose == 'idle2' else 0
    sad, cheer, think = pose == 'sad', pose == 'cheer', pose == 'think'
    hdy = bob + (3 if sad else 0)
    hdx = 1 if think else 0

    # ---- robe ---------------------------------------------------------------------------------
    def robe_body(p):
        p.poly([(7, 0), (26, 0), (33, 23), (0, 23)], robe)
        p.poly([(7, 0), (16, 0), (12, 23), (0, 23)], robe_l)
        snap = p.snapshot()
        p.poly([(10, 0), (17, 8), (24, 0), (24, 3), (17, 12), (10, 3)], robe_d)   # V collar
        p.poly([(11, 0), (17, 7), (23, 0), (23, 1), (17, 9), (11, 1)], robe_l)
        p.rect(0, 13, 33, 16, verm)                                               # sash
        p.hline(0, 33, 17, robe_d)
        for x in (4, 9, 24, 29):
            p.vline(x, 19, 22, robe_d)
        p.clip(snap)
    c.blit(part(34, 24, robe_body, ink), 11, 31 + bob)

    # ---- sleeves ------------------------------------------------------------------------------
    def sleeve(p, raised=False):
        if raised:
            p.ell(0, 4, 12, 16, robe)
            p.ell(0, 4, 5, 16, robe_l)
            p.ell(2, 0, 11, 8, skin)
            p.dots([(4, 2), (7, 2)], P['skin_d'])
        else:
            p.ell(0, 0, 12, 13, robe)
            p.ell(0, 0, 5, 13, robe_l)
            p.ell(2, 7, 11, 15, skin)
            p.dots([(4, 12), (7, 12)], P['skin_d'])
    if cheer:
        c.blit(part(13, 17, lambda p: sleeve(p, raised=True), ink), 2, 22)
        c.blit(part(13, 17, lambda p: sleeve(p, raised=True), ink).flip_h(), 41, 22)
    elif think:
        c.blit(part(13, 16, sleeve, ink), 4, 36)
        def hand(p):                                    # stroking the beard
            p.ell(0, 3, 12, 15, robe)
            p.ell(1, 0, 10, 7, skin)
            p.dots([(3, 3), (6, 3), (9, 4)], P['skin_d'])
        c.blit(part(13, 16, hand, ink), 33, 32)
    else:
        y = 38 if sad else 35 + bob
        c.blit(part(13, 16, sleeve, ink), 4, y)
        c.blit(part(13, 16, sleeve, ink).flip_h(), 39, y)

    # ---- head + beard -------------------------------------------------------------------------
    c.blit(sen_beard(P), 19 + hdx, 26 + hdy)
    c.blit(sen_head(pose, P), 14 + hdx, 1 + hdy)

    if cheer:
        for (sx, sy) in ((3, 12), (52, 14), (28, 0)):
            c.dots([(sx, sy - 2), (sx, sy - 1), (sx, sy + 1), (sx, sy + 2),
                    (sx - 2, sy), (sx - 1, sy), (sx + 1, sy), (sx + 2, sy)], gold)
            c.pt(sx, sy, white)
    if think:
        for (dx, dy, r) in ((46, 13, 1), (50, 8, 1), (52, 3, 0)):
            c.blit(part(2 * r + 4, 2 * r + 4,
                        lambda p, r=r: p.ell(1, 1, 2 * r + 2, 2 * r + 2, P['paper']), ink),
                   dx - r - 1, dy - r - 1)
    return c


# -------------------------------------------------------------------------------------------------
# Portraits (48x48 busts inside a paper frame)
# -------------------------------------------------------------------------------------------------


def portrait_frame(size, P, accent=None):
    """Paper card with a hatched wash and an ink border - the backdrop of every portrait."""
    c = Canvas(size, size)
    paper, hatch, ink, grey = P['paper'], P['hatch'], P['ink'], P['grey']
    c.rect(0, 0, size - 1, size - 1, paper)
    for k in range(-size, size * 2, 4):                 # 45 degree hatch wash
        c.line([(k, 0), (k + size, size)], hatch)
    c.ell(size // 8, size // 8, size - size // 8, size - 1, paper)   # clean area behind the bust
    c.frame(0, 0, size - 1, size - 1, ink)
    c.frame(1, 1, size - 2, size - 2, grey)
    if accent is not None:                              # vermilion seal corner
        c.rect(size - 7, 2, size - 3, 6, accent)
        c.pt(size - 5, 4, paper)
    c.dots([(1, 1), (size - 2, 1), (1, size - 2), (size - 2, size - 2)], ink)
    return c


def indi_portrait():
    P = PAL_INDI
    ink, blue, bl_d, cream = P['ink'], P['blue'], P['blue_d'], P['cream']
    orange, orange_d, tan = P['orange'], P['orange_d'], P['tan']
    c = portrait_frame(48, P, accent=None)

    def chest(p):
        p.ell(0, 0, 35, 17, blue)
        p.ell(0, 0, 15, 14, bl_d)
        p.ell(9, 3, 26, 17, cream)
        band_top(p, orange, 4, 2, 33, dy=1)
        band_top(p, orange_d, 1, 2, 33, dy=5)
        band_top(p, ink, 1, 2, 33, dy=6)
    c.blit(part(36, 18, chest, ink), 6, 33)
    c.blit(part(9, 9, lambda p: (p.ell(0, 0, 8, 8, orange_d), p.ell(1, 1, 6, 6, tan)), ink), 20, 41)
    c.blit(indi_head('idle1', P), 5, 5)
    return c


def rex_portrait():
    P = PAL_REX
    ink, green, gr_l, cream, cream_d = P['ink'], P['green'], P['green_l'], P['cream'], P['cream_d']
    c = portrait_frame(48, P, accent=None)

    def chest(p):
        p.ell(0, 0, 35, 17, green)
        p.ell(0, 0, 35, 7, gr_l)
        p.ell(9, 2, 26, 17, cream)
        p.hline(11, 24, 9, cream_d)
        p.hline(11, 24, 13, cream_d)
    c.blit(part(36, 18, chest, ink), 6, 33)
    c.blit(rex_head('idle1', P), 4, 5)
    return c


def sen_portrait():
    P = PAL_SEN
    ink, robe, robe_d, robe_l, verm = P['ink'], P['robe'], P['robe_d'], P['robe_l'], P['verm']
    c = portrait_frame(48, P, accent=None)

    def chest(p):
        p.poly([(6, 0), (33, 0), (39, 15), (0, 15)], robe)
        p.poly([(6, 0), (18, 0), (13, 15), (0, 15)], robe_l)
        snap = p.snapshot()
        p.poly([(11, 0), (20, 10), (29, 0), (29, 4), (20, 15), (11, 4)], robe_d)
        p.poly([(12, 0), (20, 9), (28, 0), (28, 1), (20, 11), (12, 1)], robe_l)
        p.rect(0, 13, 39, 15, verm)
        p.clip(snap)
    c.blit(part(40, 16, chest, ink), 4, 32)
    c.blit(sen_beard(P), 15, 25)
    c.blit(sen_head('idle1', P), 10, 2)
    return c


# -------------------------------------------------------------------------------------------------
# Opponents (40x40 framed busts, one shared palette)
# -------------------------------------------------------------------------------------------------

PAL_OPP = palette_of('#ff00ff', [
    ('ink', INK), ('paper', PAPER), ('hatch', '#dbd2bd'), ('grey', GREY), ('white', '#f4f1e6'),
    ('green', '#6aa83c'), ('green_d', '#3f6b22'), ('green_l', '#95d45e'), ('brown', '#a06a32'),
    ('brown_d', '#6b451f'), ('straw', '#d9a95a'), ('fox', '#d4682e'), ('verm', VERM),
    ('dark', '#2f3440'), ('skin', '#e0b083'),
])

OPPONENTS = ['pebble', 'sprout', 'koan', 'ember', 'tengen']


def opp_pebble():
    """PEBBLE, the sleepy turtle: heavy shell, half-closed eyes, a drifting zzz."""
    P = PAL_OPP
    ink, green, gr_d, gr_l = P['ink'], P['green'], P['green_d'], P['green_l']
    brown, brown_d, white, grey = P['brown'], P['brown_d'], P['white'], P['grey']
    c = portrait_frame(40, P)

    def shell(p):
        p.ell(0, 0, 33, 19, brown)
        p.ell(2, 2, 31, 14, P['straw'])
        for (x, y) in ((8, 4), (17, 4), (12, 10), (22, 10), (3, 10), (26, 4)):
            p.poly([(x, y), (x + 4, y - 2), (x + 8, y), (x + 8, y + 4), (x + 4, y + 6),
                    (x, y + 4)], brown_d)
        p.hline(0, 33, 17, brown_d)
    c.blit(part(34, 20, shell, ink), 3, 22)

    def head(p):
        p.ell(0, 2, 25, 25, green)
        p.ell(0, 2, 25, 13, gr_l)
        p.ell(2, 0, 10, 8, gr_l)                    # brow bumps
        p.ell(15, 0, 23, 8, gr_l)
        p.hline(4, 10, 12, ink)                     # sleepy lids
        p.hline(15, 21, 12, ink)
        p.dots([(4, 13), (5, 14), (9, 14), (10, 13)], ink)
        p.dots([(15, 13), (16, 14), (20, 14), (21, 13)], ink)
        p.ell(6, 10, 8, 12, ink)
        p.ell(17, 10, 19, 12, ink)
        p.dots([(11, 17), (14, 17)], gr_d)          # nostrils
        p.line([(9, 21), (12, 22), (16, 21)], gr_d)  # small smile
        p.ell(2, 18, 6, 22, gr_d)
        p.ell(19, 18, 23, 22, gr_d)
    c.blit(part(26, 26, head, ink), 7, 8)
    for (x, y, s) in ((30, 6, 4), (33, 2, 3), (35, 13, 3)):     # zzz
        c.hline(x, x + s - 1, y, grey)
        c.hline(x, x + s - 1, y + s - 1, grey)
        c.line([(x, y + s - 1), (x + s - 1, y)], grey)
    return c


def opp_sprout():
    """SPROUT, the eager seedling: green shoot hatching from its seed husk, huge shining eyes."""
    P = PAL_OPP
    ink, green, gr_d, gr_l = P['ink'], P['green'], P['green_d'], P['green_l']
    straw, white, brown, brown_d = P['straw'], P['white'], P['brown'], P['brown_d']
    c = portrait_frame(40, P)

    def leaves(p):
        p.poly([(13, 15), (1, 12), (0, 2), (12, 7)], gr_l)
        p.poly([(16, 15), (28, 12), (29, 2), (17, 7)], green)
        p.line([(2, 5), (11, 12)], gr_d)
        p.line([(27, 5), (18, 12)], gr_d)
        p.rect(13, 9, 15, 15, green)
    c.blit(part(30, 16, leaves, ink), 5, 0)

    def body(p):
        p.ell(0, 0, 27, 27, gr_l)
        p.ell(2, 2, 25, 16, green)                 # shaded crown
        p.ell(4, 0, 23, 12, gr_l)
        # eyes
        p.ell(3, 7, 11, 17, white)
        p.ell(16, 7, 24, 17, white)
        p.ell(5, 9, 10, 16, ink)
        p.ell(18, 9, 23, 16, ink)
        p.dots([(6, 10), (7, 10), (6, 11), (19, 10), (20, 10), (19, 11)], white)
        p.ell(7, 14, 8, 15, white)
        p.ell(20, 14, 21, 15, white)
        # open smile
        p.ell(10, 19, 18, 25, ink)
        p.ell(12, 22, 16, 25, P['verm'])
        p.ell(1, 18, 5, 21, P['verm'])
        p.ell(22, 18, 26, 21, P['verm'])
    c.blit(part(28, 28, body, ink), 6, 6)

    def husk(p):
        p.poly([(0, 4), (3, 0), (7, 5), (11, 0), (15, 5), (19, 0), (23, 5), (27, 0), (30, 4),
                (30, 9), (0, 9)], brown)
        p.poly([(0, 6), (30, 6), (30, 9), (0, 9)], brown_d)
        p.dots([(5, 7), (12, 8), (20, 7), (25, 8)], straw)
    c.blit(part(31, 10, husk, ink), 5, 30)
    return c


def opp_koan():
    """KOAN, the wandering monk: wide straw hat, shadowed eyes, serene smile."""
    P = PAL_OPP
    ink, straw, brown, brown_d = P['ink'], P['straw'], P['brown'], P['brown_d']
    skin, dark, grey, verm = P['skin'], P['dark'], P['grey'], P['verm']
    c = portrait_frame(40, P)

    def robe(p):
        p.poly([(6, 0), (25, 0), (31, 13), (0, 13)], P['brown_d'])
        p.poly([(6, 0), (16, 0), (12, 13), (0, 13)], P['brown'])
        snap = p.snapshot()
        p.poly([(10, 0), (16, 7), (22, 0), (22, 3), (16, 11), (10, 3)], verm)
        p.clip(snap)
    c.blit(part(32, 14, robe, ink), 4, 26)

    def head(p):
        p.ell(2, 0, 21, 21, skin)
        p.rect(2, 0, 21, 9, dark)                    # hat shadow
        p.dots([(6, 11), (7, 10), (8, 10), (9, 11)], ink)
        p.dots([(14, 11), (15, 10), (16, 10), (17, 11)], ink)
        p.line([(9, 16), (12, 17), (15, 16)], brown_d)
        p.ell(3, 13, 6, 16, P['verm'])
        p.ell(17, 13, 20, 16, P['verm'])
    c.blit(part(24, 22, head, ink), 8, 12)

    def hat(p):
        p.poly([(0, 13), (17, 0), (34, 13)], straw)
        p.poly([(0, 13), (34, 13), (30, 16), (4, 16)], straw)
        for k in range(1, 6):                        # straw weave
            p.line([(17 - k * 3, 3 + k * 2), (17 + k * 3, 3 + k * 2)], brown_d)
        p.line([(17, 0), (17, 13)], brown_d)
        p.hline(4, 30, 14, brown_d)
    c.blit(part(35, 17, hat, ink), 2, 6)
    return c


def opp_ember():
    """EMBER, the fox general: sharp amber eyes, lacquered shoulder plates, a crescent crest."""
    P = PAL_OPP
    ink, fox, brown_d, white = P['ink'], P['fox'], P['brown_d'], P['white']
    verm, dark, straw, paper = P['verm'], P['dark'], P['straw'], P['paper']
    c = portrait_frame(40, P)

    def armour(p):
        p.poly([(3, 0), (30, 0), (33, 15), (0, 15)], dark)
        snap = p.snapshot()
        p.rect(0, 0, 33, 1, straw)                   # gold trim
        for y in (4, 8, 12):                          # lacing
            p.hline(0, 33, y, verm)
            p.hline(0, 33, y + 1, brown_d)
        p.poly([(11, 0), (16, 8), (22, 0), (22, 15), (11, 15)], brown_d)
        p.ell(13, 3, 20, 10, straw)                  # chest boss
        p.ell(15, 5, 18, 8, verm)
        p.clip(snap)
    c.blit(part(34, 16, armour, ink), 3, 25)

    def head(p):
        p.poly([(1, 13), (0, 0), (12, 7)], fox)      # ears
        p.poly([(26, 13), (27, 0), (15, 7)], fox)
        p.poly([(3, 10), (2, 3), (9, 7)], brown_d)
        p.poly([(24, 10), (25, 3), (18, 7)], brown_d)
        p.ell(2, 6, 25, 24, fox)                     # skull
        p.poly([(0, 12), (7, 14), (0, 20)], white)   # cheek ruff
        p.poly([(27, 12), (20, 14), (27, 20)], white)
        p.ell(8, 15, 19, 27, white)                  # muzzle
        p.ell(11, 18, 16, 22, ink)                   # nose
        p.vline(13, 22, 24, ink)
        p.line([(10, 26), (13, 24), (17, 26)], ink)
        # sharp eyes
        p.poly([(4, 11), (11, 13), (11, 16), (5, 15)], straw)
        p.poly([(23, 11), (16, 13), (16, 16), (22, 15)], straw)
        p.poly([(6, 12), (9, 13), (9, 16), (6, 15)], ink)
        p.poly([(21, 12), (18, 13), (18, 16), (21, 15)], ink)
        p.dots([(4, 10), (5, 10), (6, 10), (7, 11), (8, 11)], brown_d)
        p.dots([(23, 10), (22, 10), (21, 10), (20, 11), (19, 11)], brown_d)
    c.blit(part(28, 28, head, ink), 6, 8)

    def crest(p):
        p.rect(0, 7, 23, 9, dark)                    # brow band
        p.hline(0, 23, 7, straw)
        p.poly([(5, 8), (8, 2), (11, 0), (12, 2), (13, 0), (16, 2), (19, 8)], straw)
        p.poly([(8, 8), (10, 4), (12, 3), (14, 4), (16, 8)], dark)
        p.ell(10, 4, 13, 7, straw)
    c.blit(part(24, 10, crest, ink), 8, 7)
    return c


def opp_tengen():
    """TENGEN, the old master: black robe, sharp white brows, a long thin beard, unflinching."""
    P = PAL_OPP
    ink, dark, white, grey = P['ink'], P['dark'], P['white'], P['grey']
    skin, verm, straw, paper = P['skin'], P['verm'], P['straw'], P['paper']
    c = portrait_frame(40, P)

    def robe(p):
        p.poly([(4, 0), (29, 0), (33, 15), (0, 15)], ink)
        p.poly([(4, 0), (14, 0), (10, 15), (0, 15)], dark)
        snap = p.snapshot()
        p.poly([(8, 0), (16, 10), (24, 0), (24, 4), (16, 15), (8, 4)], grey)
        p.poly([(9, 0), (16, 9), (23, 0), (23, 1), (16, 11), (9, 1)], white)
        p.clip(snap)
    c.blit(part(34, 16, robe, ink), 3, 25)

    def head(p):
        p.rect(8, 0, 15, 3, white)                   # topknot
        p.ell(9, 0, 14, 3, white)
        p.ell(1, 2, 22, 25, skin)
        p.ell(1, 1, 22, 9, white)                    # swept-back hair
        p.ell(4, 5, 19, 11, skin)
        p.ell(0, 6, 3, 17, white)
        p.ell(20, 6, 23, 17, white)
        p.poly([(2, 12), (10, 8), (10, 11), (3, 14)], white)     # hard brows
        p.poly([(21, 12), (13, 8), (13, 11), (20, 14)], white)
        p.rect(3, 15, 9, 17, paper)
        p.rect(14, 15, 20, 17, paper)
        p.ell(5, 15, 8, 17, ink)
        p.ell(15, 15, 18, 17, ink)
        p.dots([(6, 16), (16, 16)], paper)
        p.dots([(11, 18), (11, 19), (12, 20), (10, 20)], P['brown_d'])
        p.hline(8, 15, 23, P['brown_d'])             # flat mouth
        p.ell(3, 21, 20, 24, white)                  # moustache
        p.hline(8, 15, 22, grey)
    c.blit(part(24, 26, head, ink), 8, 6)

    def beard(p):
        p.poly([(0, 0), (9, 0), (6, 15), (3, 15)], white)
        p.vline(4, 2, 14, grey)
        p.dots([(2, 6), (7, 6), (3, 11), (6, 11)], grey)
    c.blit(part(10, 16, beard, ink), 15, 27)
    return c


OPP_FUNCS = {'pebble': opp_pebble, 'sprout': opp_sprout, 'koan': opp_koan,
             'ember': opp_ember, 'tengen': opp_tengen}


# -------------------------------------------------------------------------------------------------
# UI sprites
# -------------------------------------------------------------------------------------------------

PAL_UI = palette_of('#ff00ff', [
    ('ink', INK), ('ink_l', '#3a4050'), ('paper', PAPER), ('paper_d', '#c9c0aa'),
    ('grey', GREY), ('verm', VERM), ('verm_d', '#8f2a1f'), ('verm_l', '#e4634f'),
    ('gold', GOLD), ('gold_d', '#a8762f'), ('white', '#ffffff'), ('orange', '#f39a3e'),
    ('lava', '#ff6a1a'), ('dark', '#2a1a12'), ('cream', '#f2dcb8'),
])

STAR_ART = [
    '...##...',
    '...##...',
    '.######.',
    '########',
    '.######.',
    '..####..',
    '.##..##.',
    '##....##',
]


def ui_star_sheet():
    """8x8 stars: 0 filled, 1 empty, 2 burst (the mission-clear pop)."""
    P = PAL_UI
    sh = Sheet(8, 8, P)
    gold, gold_d, white, grey = P['gold'], P['gold_d'], P['white'], P['grey']

    full = Canvas(8, 8)
    full.stamp(STAR_ART, 0, 0, {'#': gold})
    full.contour(gold_d)
    full.dots([(3, 2), (4, 2), (3, 3)], white)
    sh.add(full)

    empty = Canvas(8, 8)
    empty.stamp(STAR_ART, 0, 0, {'#': P['paper_d']})
    empty.contour(grey)
    inner = Canvas(8, 8)
    inner.stamp(STAR_ART, 0, 0, {'#': 1})
    inner.contour(0)
    for y in range(8):
        for x in range(8):
            if inner.get(x, y) == 1:
                empty.pt(x, y, 0)
    sh.add(empty)

    burst = Canvas(8, 8)
    burst.stamp(STAR_ART, 0, 0, {'#': white})
    burst.contour(gold)
    burst.dots([(0, 0), (7, 0), (0, 7), (7, 7)], gold)
    sh.add(burst)
    return sh


def ui_cursor_sheet():
    """8x8 menu cursor: 0/1 right-pointing (bob), 2 down, 3 up."""
    P = PAL_UI
    sh = Sheet(8, 8, P)
    verm, verm_l, ink = P['verm'], P['verm_l'], P['ink']

    def tri(dx, points):
        c = Canvas(8, 8)
        c.poly([(x + dx, y) for (x, y) in points], verm)
        c.contour(P['verm_d'])
        return c
    right = [(1, 0), (7, 3), (7, 4), (1, 7)]
    a = tri(0, right)
    a.dots([(2, 2), (2, 3), (3, 3)], verm_l)
    sh.add(a)
    b = tri(-1, right)
    b.dots([(1, 2), (1, 3), (2, 3)], verm_l)
    sh.add(b)
    down = [(0, 1), (7, 1), (4, 7), (3, 7)]
    d = Canvas(8, 8)
    d.poly(down, verm)
    d.contour(P['verm_d'])
    d.dots([(2, 2), (3, 2)], verm_l)
    sh.add(d)
    up = [(0, 6), (7, 6), (4, 0), (3, 0)]
    u = Canvas(8, 8)
    u.poly(up, verm)
    u.contour(P['verm_d'])
    u.dots([(3, 2), (4, 2)], verm_l)
    sh.add(u)
    return sh


def ui_map_node_sheet():
    """16x16 campaign map nodes: cleared, current, locked (2 blink frames each) + a pick bracket."""
    P = PAL_UI
    sh = Sheet(16, 16, P)
    ink, paper, verm, gold = P['ink'], P['paper'], P['verm'], P['gold']
    paper_d, white, grey = P['paper_d'], P['white'], P['grey']

    def disc(fill, rim, glow, big):
        c = Canvas(16, 16)
        r = 7 if big else 6
        c.ell(8 - r, 8 - r, 7 + r, 7 + r, rim)
        c.ell(9 - r, 9 - r, 6 + r, 6 + r, fill)
        if glow is not None:
            c.arc(9 - r, 9 - r, 6 + r, 6 + r, 190, 300, glow)
        c.contour(ink)
        return c
    sh.add(disc(ink, P['ink_l'], grey, False))            # 0 cleared
    sh.add(disc(ink, P['ink_l'], paper_d, True))          # 1 cleared (pulse)
    sh.add(disc(verm, P['verm_l'], white, False))         # 2 current
    sh.add(disc(P['verm_l'], verm, white, True))          # 3 current (pulse)
    locked = disc(paper, paper_d, None, False)
    locked.rect(6, 7, 9, 11, grey)
    locked.contour(ink)
    locked.rect(7, 8, 8, 10, paper_d)
    locked.arc(6, 3, 9, 9, 180, 360, grey)
    sh.add(locked)                                         # 4 locked
    locked2 = disc(paper_d, grey, None, False)
    locked2.rect(6, 7, 9, 11, grey)
    locked2.contour(ink)
    locked2.rect(7, 8, 8, 10, paper)
    locked2.arc(6, 3, 9, 9, 180, 360, grey)
    sh.add(locked2)                                        # 5 locked (dim)

    def bracket(gap):
        c = Canvas(16, 16)
        for (x, y, dx, dy) in ((0, 0, 1, 1), (15, 0, -1, 1), (0, 15, 1, -1), (15, 15, -1, -1)):
            for k in range(5 - gap):
                c.pt(x + dx * k, y, verm)
                c.pt(x, y + dy * k, verm)
            for k in range(4 - gap):
                c.pt(x + dx * k, y + dy, verm)
                c.pt(x + dx, y + dy * k, verm)
        return c
    sh.add(bracket(0))                                     # 6 pick bracket
    sh.add(bracket(1))                                     # 7 pick bracket (blink)
    return sh


def ui_rank_sheet():
    """64x64 rank stamps S / A / B / C: a rough vermilion seal, tilted like a real stamp."""
    P = PAL_UI
    sh = Sheet(64, 64, P)
    verm, verm_d, verm_l, paper = P['verm'], P['verm_d'], P['verm_l'], P['paper']

    def letter(c, ch):
        t = 7                                              # stroke thickness
        if ch == 'S':
            c.d.arc([16, 11, 47, 33], 95, 325, fill=verm, width=t)
            c.d.arc([16, 29, 47, 52], 275, 495, fill=verm, width=t)
        elif ch == 'A':
            c.poly([(30, 12), (34, 12), (48, 52), (40, 52), (32, 24), (24, 52), (16, 52)], verm)
            c.rect(24, 38, 40, 44, verm)
        elif ch == 'B':
            c.d.arc([24, 11, 45, 33], 270, 450, fill=verm, width=t)
            c.d.arc([24, 30, 47, 52], 270, 450, fill=verm, width=t)
            c.rect(16, 11, 34, 17, verm)
            c.rect(16, 28, 35, 34, verm)
            c.rect(16, 46, 36, 52, verm)
            c.rect(16, 11, 23, 52, verm)
        elif ch == 'C':
            c.d.arc([14, 11, 50, 52], 35, 325, fill=verm, width=t)
        return c

    for i, ch in enumerate('SABC'):
        c = Canvas(64, 64)
        c.ring(4, 6, 59, 57, verm, 3)                      # double seal ring
        c.ring(8, 10, 55, 53, verm, 1)
        letter(c, ch)
        # rough up the edges so it looks stamped, never the same two pixels twice
        for y in range(64):
            for x in range(64):
                if c.get(x, y) and ((x * 7 + y * 13 + i * 5) % 23 == 0 or
                                    (x * 3 - y * 5 + i) % 31 == 0):
                    c.pt(x, y, verm_d if (x + y) % 2 else verm_l)
        rot = c.img.rotate(8, resample=Image.NEAREST, fillcolor=0)
        out = Canvas(64, 64)
        out.img.paste(rot, (0, 0))
        out.d = ImageDraw.Draw(out.img)
        sh.add(out)
    return sh


BANNER_SKINS = ['classic', 'pup', 'dino']


def ui_banner_sheet():
    """64x32 banner plates. Frame = skin * 2 + style (0 straight, 1 slanted -3 degrees).

    Tile four of them at x = -96, -32, 32, 96 to cover the 240 px screen; for the slanted style
    put frame k at y = y0 - 4 * k so the plate edges line up across the seams (the art slopes
    4 px per 64 px = the mockup's -3 degrees)."""
    P = PAL_UI
    sh = Sheet(64, 32, P)
    ink = P['ink']
    # main, top highlight, bottom shade, band height, shadow height
    cols = {
        'classic': (P['verm'], P['verm_l'], P['verm_d'], 22, 2),
        'pup': (P['orange'], P['cream'], P['gold_d'], 24, 3),
        'dino': (P['lava'], P['gold'], P['dark'], 26, 0),
    }
    for skin in BANNER_SKINS:
        main, light, dark, height, shadow = cols[skin]
        for slant in (0, 1):
            c = Canvas(64, 32)
            for x in range(64):
                off = (3 - (x * 4) // 64) if slant else 0
                top = 2 + off
                bot = top + height - 1
                if skin == 'dino':                          # jagged lava teeth on top
                    top += [0, 1, 2, 3, 4, 3, 2, 1][x % 8]
                c.vline(x, top, bot, main)
                c.hline(x, x, top, light)
                c.vline(x, bot - 1, bot, dark)
                if skin == 'pup':
                    c.pt(x, top, P['white'])
                    c.pt(x, bot, P['white'])
                if shadow:
                    c.vline(x, bot + 1, bot + shadow, ink)
                elif skin == 'dino':
                    c.vline(x, bot + 1, bot + 2, P['dark'])
            sh.add(c)
    return sh


def ui_egg_crack_sheet():
    """16x16 crack overlay for DINO captures: 3 stages + a burst."""
    P = PAL_UI
    sh = Sheet(16, 16, P)
    ink, cream, white = P['ink'], P['cream'], P['white']
    stages = [
        [[(8, 2), (7, 5), (9, 7)]],
        [[(8, 1), (7, 5), (9, 7), (8, 10)], [(7, 5), (3, 7)]],
        [[(8, 0), (7, 5), (9, 7), (8, 12)], [(7, 5), (2, 7)], [(9, 7), (14, 5)],
         [(8, 10), (12, 13)]],
    ]
    for paths in stages:
        c = Canvas(16, 16)
        for pts in paths:
            c.line(pts, ink)
            c.line([(x + 1, y) for (x, y) in pts], cream)
        sh.add(c)
    burst = Canvas(16, 16)
    for (x, y, w, h) in ((1, 2, 4, 3), (10, 1, 5, 4), (2, 10, 5, 4), (9, 11, 4, 3), (6, 6, 4, 4)):
        burst.rect(x, y, x + w, y + h, cream)
        burst.rect(x + 1, y + 1, x + w - 1, y + h - 1, white)
    burst.contour(ink)
    sh.add(burst)
    return sh


def ui_bubble_sheet():
    """32x32 speech bubble parts: skin * 3 + part (0 left cap, 1 middle, 2 right cap with tail).

    Body rows 0..23, tail rows 24..29 on the right cap. Compose left + n * middle + right."""
    P = PAL_UI
    sh = Sheet(32, 32, P)
    ink = P['ink']
    skins = {
        'classic': (P['paper'], P['paper_d'], P['ink']),
        'pup': (P['white'], P['paper_d'], P['ink']),
        'dino': (P['cream'], P['gold_d'], P['dark']),
    }
    for skin in BANNER_SKINS:
        fill, shade, border = skins[skin]
        for kind in range(3):
            c = Canvas(32, 32)
            c.rect(0, 0, 31, 23, fill)
            c.hline(0, 31, 22, shade)
            if kind == 0:
                c.rect(0, 0, 1, 23, 0)
                c.ell(0, 0, 5, 5, fill)
                c.rect(0, 3, 3, 20, fill)
                c.ell(0, 18, 5, 23, fill)
            elif kind == 2:
                c.rect(30, 0, 31, 23, 0)
                c.ell(26, 0, 31, 5, fill)
                c.rect(28, 3, 31, 20, fill)
                c.ell(26, 18, 31, 23, fill)
                c.poly([(14, 20), (24, 20), (16, 30)], fill)
            c.contour(border, edges=False)
            if kind == 0:
                c.vline(0, 2, 21, border)
            elif kind == 2:
                c.vline(31, 2, 21, border)
            sh.add(c)
    return sh


def ui_title_logo_sheet():
    """The MOKU brush logo as two 64x64 frames: place them side by side (frame 0 then frame 1)."""
    P = PAL_UI
    sh = Sheet(64, 64, P)
    ink, ink_l, paper, verm = P['ink'], P['ink_l'], P['paper'], P['verm']
    white, grey = P['white'], P['grey']

    def letter_m(c, x):
        c.taper_curve([(x + 2, 41), (x + 3, 6)], ink, 4, 3)
        c.taper_curve([(x + 3, 6), (x + 13, 29)], ink, 4, 2)
        c.taper_curve([(x + 13, 29), (x + 23, 6)], ink, 3, 4)
        c.taper_curve([(x + 23, 6), (x + 24, 41)], ink, 4, 3)

    def letter_o(c, x):
        c.taper_curve([(x + 13, 4), (x + 3, 12), (x + 2, 26), (x + 12, 41)], ink, 4, 3)
        c.taper_curve([(x + 13, 4), (x + 23, 12), (x + 24, 26), (x + 12, 41)], ink, 3, 4)

    def letter_k(c, x):
        c.taper_curve([(x + 3, 5), (x + 4, 41)], ink, 4, 3)
        c.taper_curve([(x + 24, 5), (x + 7, 23)], ink, 4, 2)
        c.taper_curve([(x + 7, 22), (x + 25, 41)], ink, 3, 4)

    def letter_u(c, x):
        c.taper_curve([(x + 3, 5), (x + 4, 30), (x + 13, 40)], ink, 4, 3)
        c.taper_curve([(x + 23, 5), (x + 22, 30), (x + 13, 40)], ink, 4, 3)

    for i, pair in enumerate(((letter_m, letter_o), (letter_k, letter_u))):
        c = Canvas(64, 64)
        pair[0](c, 3)
        pair[1](c, 33)
        # brush highlight along the top-left of every stroke + a dry-brush speckle
        src = c.img.copy().load()
        for y in range(64):
            for x in range(64):
                if c.get(x, y) != ink:
                    continue
                if (src[max(x - 2, 0), y] == 0 or src[x, max(y - 2, 0)] == 0):
                    c.pt(x, y, ink_l)
                elif (x * 5 + y * 11 + i) % 37 == 0:
                    c.pt(x, y, ink_l)
        if i == 1:                                          # vermilion seal after the last letter
            c.rect(52, 44, 62, 55, verm)
            c.frame(52, 44, 62, 55, P['verm_d'])
            c.dots([(55, 47), (57, 47), (55, 49), (57, 49), (55, 51), (57, 51),
                    (54, 48), (58, 50)], paper)
        sh.add(c)
    return sh


# -------------------------------------------------------------------------------------------------
# Regular backgrounds
# -------------------------------------------------------------------------------------------------
#
# Butano places a regular bg by its centre, so a bg created at (0, 0) shows bg pixel (8, 48) of a
# 256x256 map at screen (0, 0). Every background below is drawn in *screen* coordinates and then
# rolled by (half_width - 120, half_height - 80), so `item.create_bg(0, 0)` shows exactly what is
# drawn here at screen (0, 0), and `create_bg(-s, 0)` scrolls right by s pixels.


def roll_for_butano(c):
    from PIL import ImageChops
    dx = c.w // 2 - 120
    dy = c.h // 2 - 80
    out = Canvas(c.w, c.h)
    out.img = ImageChops.offset(c.img, dx, dy)
    out.d = ImageDraw.Draw(out.img)
    return out


def dither(c, x0, y0, x1, y1, a, b, invert=False):
    """2x2 ordered dither between two colours, top a -> bottom b."""
    h = max(y1 - y0, 1)
    for y in range(y0, y1 + 1):
        level = ((y - y0) * 4) // h
        for x in range(x0, x1 + 1):
            m = ((x & 1) + (y & 1) * 2)
            v = b if (level > m) != invert else a
            c.pt(x, y, v)


def ridge(c, y_base, amp, period, phase, col, bottom, jag=0):
    """A mountain silhouette: a sum of two sines rasterised column by column."""
    import math
    for x in range(c.w):
        t = (x + phase)
        y = y_base - int(amp * math.sin(t * 6.2831 / period) +
                         (amp / 2) * math.sin(t * 6.2831 / (period * 0.37) + 1.1))
        if jag:
            y += ((x * 7 + phase) % (jag * 2)) - jag
        c.vline(x, y, bottom, col)


PAL_TITLE = palette_of('#ff00ff', [
    ('paper', PAPER), ('paper_l', '#f7f0dd'), ('paper_d', '#e2d8c0'), ('mist', '#ded4bc'),
    ('grey_l', '#c9c0aa'), ('grey', GREY), ('grey_d', GREY_D), ('slate', '#6f7788'),
    ('ink', INK), ('ink_l', '#2a3040'), ('ink_2', '#3a4050'), ('verm', VERM),
    ('white', '#ffffff'), ('gold', GOLD), ('water', '#b9c4c9'),
])


def bg_title():
    P = PAL_TITLE
    c = Canvas(256, 256)
    paper, paper_l, paper_d, mist = P['paper'], P['paper_l'], P['paper_d'], P['mist']
    grey_l, grey, grey_d, slate = P['grey_l'], P['grey'], P['grey_d'], P['slate']
    ink, ink_l, ink_2, verm = P['ink'], P['ink_l'], P['ink_2'], P['verm']
    white, gold, water = P['white'], P['gold'], P['water']

    c.rect(0, 0, 255, 255, paper)
    dither(c, 0, 0, 255, 60, paper_l, paper)          # sky wash
    c.ell(186, 12, 214, 40, paper_l)                  # pale sun
    c.ring(186, 12, 214, 40, grey_l)
    for (x, y) in ((150, 22), (158, 18), (166, 24), (60, 60), (70, 56)):   # birds
        c.dots([(x, y), (x + 1, y - 1), (x + 2, y), (x + 3, y - 1), (x + 4, y)], grey)

    ridge(c, 66, 15, 210, 40, grey_l, 92)             # far range
    ridge(c, 76, 12, 130, 260, slate, 92, jag=1)      # middle range
    ridge(c, 86, 8, 76, 90, grey_d, 92, jag=1)        # near hills
    for y in range(56, 86, 7):                        # mist bands
        c.hline(0, 255, y, mist)
        c.hline(0, 255, y + 1, paper_d)
    for (x, h) in ((12, 11), (20, 15), (29, 10), (216, 13), (228, 17), (238, 11)):
        c.vline(x, 92 - h, 91, ink_l)                 # pines on the near hills
        for k in range(3):
            w = 5 - k
            c.line([(x - w, 92 - h + k * 4 + 4), (x, 92 - h + k * 4), (x + w, 92 - h + k * 4 + 4)],
                   ink_l)
    c.rect(0, 86, 255, 95, water)                     # lake
    c.hline(0, 255, 86, mist)
    for y in (88, 90, 92, 94):
        for x in range(0, 256, 11):
            c.hline(x + (y % 5), x + 5 + (y % 5), y, paper_l)

    dither(c, 0, 96, 255, 159, ink, ink_l)            # menu panel
    c.hline(0, 255, 96, ink_2)
    c.hline(0, 255, 97, ink)
    dither(c, 0, 152, 255, 159, ink_l, ink_2)
    for x in range(0, 256, 16):                       # panel lattice
        c.pt(x, 100, ink_2)
        c.pt(x + 8, 148, ink_2)

    # the two title stones (the logo sprites sit to their right)
    c.ell(24, 18, 37, 31, ink)
    c.ell(26, 20, 30, 24, ink_2)
    c.ell(44, 18, 57, 31, paper_l)
    c.ring(44, 18, 57, 31, grey)
    c.ell(46, 20, 50, 24, white)
    # (the vermilion seal lives on the title_logo sprite, so the sky stays a clean wash)
    c.rect(0, 160, 255, 255, paper)
    dither(c, 0, 160, 255, 255, paper, paper_d)
    return roll_for_butano(c)


PAL_MAP = palette_of('#ff00ff', [
    ('paper', PAPER), ('paper_l', '#f7f0dd'), ('paper_d', '#e2d8c0'), ('mist', '#ded4bc'),
    ('grey_l', '#c9c0aa'), ('grey', GREY), ('grey_d', GREY_D), ('ink', INK),
    ('ink_2', '#3a4050'), ('verm', VERM), ('gold', GOLD), ('green', '#7f8f60'),
    ('green_d', '#4e5c38'), ('water', '#b9c4c9'), ('brown', '#a08050'),
])

# Campaign node anchors in map pixels (chapter = i // 4). The path is painted through these
# points, so the campaign map scene must place its node sprites here (centres).
NODE_POSITIONS = [
    (28, 118), (58, 104), (86, 116), (116, 100),
    (146, 110), (176, 92), (206, 104), (232, 84),
    (258, 96), (286, 78), (314, 90), (342, 72),
    (368, 84), (394, 66), (418, 78), (440, 60),
    (462, 72), (480, 54), (496, 66), (504, 40),
]


def bg_map():
    P = PAL_MAP
    c = Canvas(512, 256)
    paper, paper_l, paper_d, mist = P['paper'], P['paper_l'], P['paper_d'], P['mist']
    grey_l, grey, grey_d, ink = P['grey_l'], P['grey'], P['grey_d'], P['ink']
    verm, gold, green, green_d = P['verm'], P['gold'], P['green'], P['green_d']
    water, brown, ink_2 = P['water'], P['brown'], P['ink_2']

    c.rect(0, 0, 511, 255, paper)
    dither(c, 0, 0, 511, 40, paper_l, paper)
    ridge(c, 34, 14, 260, 10, grey_l, 130)            # far range
    ridge(c, 46, 11, 150, 700, mist, 130)
    ridge(c, 58, 9, 96, 300, grey_l, 130, jag=1)
    for y in range(26, 72, 9):                        # mist bands over the far ranges only
        c.hline(0, 511, y, paper_d)
    # river with a bridge
    for x in range(511):
        y = 96 + ((x - 150) * (x - 150)) // 900
        if y > 130:
            continue
        c.vline(x, y, min(y + 5, 130), water)
        c.hline(x, x, y, mist)
    c.rect(196, 84, 226, 88, brown)                   # bridge deck
    c.hline(196, 226, 83, gold)
    for x in range(198, 226, 6):
        c.vline(x, 88, 94, brown)
    # pines and rocks along the way
    import math
    for i in range(34):
        x = 16 + i * 15 + (i * 37) % 11
        h = 9 + (i * 13) % 7
        y = 124 - (i * 29) % 34
        if 190 < x < 235:
            continue
        if i % 3 == 0:
            c.ell(x - 4, y - 3, x + 4, y + 3, grey_l)   # rock
            c.arc(x - 4, y - 3, x + 4, y + 3, 180, 360, grey_d)
        else:
            c.vline(x, y - h, y, green_d)
            for k in range(3):
                w = 5 - k
                c.line([(x - w, y - h + k * 4 + 4), (x, y - h + k * 4), (x + w, y - h + k * 4 + 4)],
                       green if k else green_d)
    # the dashed path through the mission anchors
    pts = NODE_POSITIONS
    dash = 0
    for i in range(len(pts) - 1):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        n = max(abs(x1 - x0), abs(y1 - y0))
        for s in range(n + 1):
            x = x0 + (x1 - x0) * s // n
            y = y0 + (y1 - y0) * s // n
            dash += 1
            if (dash // 4) % 2 == 0:
                c.pt(x, y + 1, grey_d)
                c.pt(x, y + 2, grey)
                c.pt(x, y + 3, paper_d)
    for (x, y) in pts:                                 # a footprint of trodden ground
        c.ell(x - 8, y + 2, x + 8, y + 8, paper_l)
        c.arc(x - 8, y + 2, x + 8, y + 8, 0, 180, grey_l)
    # a torii gate near the end of the road and a pagoda at the summit
    c.rect(470, 26, 474, 52, verm)
    c.rect(494, 26, 498, 52, verm)
    c.rect(464, 22, 504, 25, verm)
    c.rect(466, 30, 502, 32, verm)
    c.rect(436, 104, 448, 122, brown)                  # pagoda body
    c.rect(439, 110, 445, 122, grey_d)
    for k in range(3):
        w = 16 - k * 3
        y = 104 - k * 10
        c.poly([(442 - w, y), (442 + w, y), (442 + w - 5, y - 6), (442 - w + 5, y - 6)], grey_d)
        c.hline(442 - w, 442 + w, y, ink_2)
        c.rect(439, y - 10, 445, y - 6, brown)
    c.vline(442, 68, 74, verm)
    c.rect(0, 130, 511, 255, paper)
    dither(c, 0, 130, 511, 255, paper, paper_d)
    return roll_for_butano(c)


PAL_HUD_CLASSIC = palette_of('#ff00ff', [
    ('ink', INK), ('ink_l', '#232838'), ('ink_2', '#3a4050'), ('paper', PAPER),
    ('paper_d', '#c9c0aa'), ('grey', GREY), ('grey_d', GREY_D), ('verm', VERM),
    ('verm_d', '#8f2a1f'), ('wood', WOOD),
])

PAL_HUD_PUP = palette_of('#ff00ff', [
    ('sky', '#2b5aa6'), ('sky_d', '#224a8c'), ('sky_l', '#3f6fc4'), ('cloud', '#a9c6ee'),
    ('cloud_l', '#eaf4ff'), ('orange', '#f39a3e'), ('orange_d', '#c26f22'), ('grass', '#8fd05e'),
    ('grass_d', '#4d8a3a'), ('white', '#ffffff'), ('ink', INK), ('bone', '#f2efe6'),
])

PAL_HUD_DINO = palette_of('#ff00ff', [
    ('rock', '#2a1a12'), ('rock_l', '#3a2418'), ('rock_d', '#1a1008'), ('cream', '#f2dcb8'),
    ('tan', '#b89a70'), ('lava', '#ff6a1a'), ('lava_d', '#c24a10'), ('ash', '#5a3a22'),
    ('fern', '#4d6b2a'), ('fern_l', '#6a8f3a'), ('ink', INK), ('smoke', '#4a3a30'),
])

HUD_X = 144          # the play scene board owns x 0..143, the HUD panel owns x 144..239


def bg_hud_classic():
    P = PAL_HUD_CLASSIC
    c = Canvas(256, 256)
    ink, ink_l, ink_2 = P['ink'], P['ink_l'], P['ink_2']
    paper, verm, grey_d = P['paper'], P['verm'], P['grey_d']
    c.rect(HUD_X, 0, 255, 255, ink)
    for y in range(0, 256, 2):                          # faint paper-fibre weave
        for x in range(HUD_X + 2, 256, 4):
            c.pt(x + (y // 2) % 2, y, ink_l)
    c.vline(HUD_X, 0, 255, verm)                        # spine against the board
    c.vline(HUD_X + 1, 0, 255, P['verm_d'])
    c.vline(HUD_X + 2, 0, 255, ink_2)
    # an ink-wash bamboo in the corner, low contrast so HUD text stays readable
    c.vline(250, 96, 158, ink_2)
    c.vline(251, 96, 158, ink_l)
    for y in range(102, 156, 14):
        c.hline(249, 252, y, ink_2)
        c.line([(250, y - 3), (245, y - 7)], ink_2)
        c.line([(245, y - 7), (240, y - 6)], ink_2)
    c.hline(HUD_X + 3, 255, 0, ink_2)
    c.hline(HUD_X + 3, 255, 159, ink_2)
    return roll_for_butano(c)


def bg_hud_pup():
    P = PAL_HUD_PUP
    c = Canvas(256, 256)
    sky, sky_d, sky_l = P['sky'], P['sky_d'], P['sky_l']
    cloud, cloud_l, white = P['cloud'], P['cloud_l'], P['white']
    grass, grass_d, orange, ink, bone = P['grass'], P['grass_d'], P['orange'], P['ink'], P['bone']
    c.rect(HUD_X, 0, 255, 255, sky)
    dither(c, HUD_X, 0, 255, 80, sky_d, sky)
    dither(c, HUD_X, 80, 255, 150, sky, sky_l)
    for (cx, cy, s) in ((176, 26, 1), (222, 58, 1), (196, 108, 0)):    # soft clouds
        col = cloud if s else sky_l
        c.ell(cx - 14, cy - 5, cx + 14, cy + 5, col)
        c.ell(cx - 8, cy - 9, cx + 6, cy + 3, col)
        c.ell(cx + 2, cy - 7, cx + 14, cy + 3, col)
        c.hline(cx - 12, cx + 10, cy - 4, cloud_l if s else cloud)
    c.rect(HUD_X, 150, 255, 255, grass)                  # lawn strip
    c.hline(HUD_X, 255, 150, grass_d)
    for x in range(HUD_X + 2, 256, 6):
        c.vline(x, 146, 150, grass_d)
        c.vline(x + 3, 148, 150, grass)
    c.ell(214, 152, 238, 157, bone)                      # a bone in the grass
    c.ell(212, 150, 218, 156, bone)
    c.ell(212, 154, 218, 159, bone)
    c.ell(234, 150, 240, 156, bone)
    c.ell(234, 154, 240, 159, bone)
    c.vline(HUD_X, 0, 255, orange)                       # spine
    c.vline(HUD_X + 1, 0, 255, P['orange_d'])
    c.hline(HUD_X + 2, 255, 0, sky_l)
    return roll_for_butano(c)


def bg_hud_dino():
    P = PAL_HUD_DINO
    c = Canvas(256, 256)
    rock, rock_l, rock_d = P['rock'], P['rock_l'], P['rock_d']
    lava, lava_d, ash, smoke = P['lava'], P['lava_d'], P['ash'], P['smoke']
    fern, fern_l, cream, ink = P['fern'], P['fern_l'], P['cream'], P['ink']
    c.rect(HUD_X, 0, 255, 255, rock)
    dither(c, HUD_X, 0, 255, 70, rock_d, rock)
    for (x, y) in ((160, 18), (200, 10), (236, 30), (176, 52), (226, 74), (150, 92), (246, 118)):
        c.pt(x, y, smoke)                                # ash motes
        c.pt(x + 1, y + 1, rock_l)
    # a distant volcano in the lower half of the panel
    base, peak = 152, 96
    for x in range(HUD_X, 256):
        d = abs(x - 206)
        y = peak + d * 3 // 4
        if y < base:
            c.vline(x, y, base, rock_l)
            if d < 6:
                c.vline(x, y, y + 3, lava_d)
    c.poly([(200, 96), (212, 96), (210, 100), (202, 100)], lava)
    for k, (x, y) in enumerate(((204, 92), (208, 88), (202, 84), (207, 80))):
        c.ell(x - 2 - k, y - 2, x + 2 + k, y + 2, smoke)
    for x in range(HUD_X, 256, 9):                       # lava cracks in the ground
        c.hline(x, x + 4, 154 + (x % 3), lava_d)
    c.rect(HUD_X, 156, 255, 255, rock_d)
    for x in range(HUD_X + 4, 256, 14):                  # ferns at the foot of the panel
        c.vline(x, 148, 156, fern)
        for k in range(3):
            w = 4 - k
            c.line([(x - w, 150 + k * 3), (x, 148 + k * 3), (x + w, 150 + k * 3)],
                   fern_l if k == 1 else fern)
    c.vline(HUD_X, 0, 255, lava)                         # spine
    c.vline(HUD_X + 1, 0, 255, lava_d)
    c.hline(HUD_X + 2, 255, 0, ash)
    return roll_for_butano(c)


PAL_PANELS = palette_of('#ff00ff', [
    ('paper', PAPER), ('paper_l', '#f7f0dd'), ('paper_d', '#e2d8c0'), ('grey_l', '#c9c0aa'),
    ('grey', GREY), ('ink', INK), ('ink_l', '#2a3040'), ('ink_2', '#3a4050'),
    ('verm', VERM), ('gold', GOLD), ('white', '#ffffff'),
])

PANEL_MAPS = ['dialogue', 'briefing', 'ink_bar', 'header_bar']


def bg_panels():
    """Four 256x256 maps sharing one palette and one tile set:

       0 dialogue   paper box on screen rows 112..159 (the play scene dialogue box)
       1 briefing   the same box with a 48x48 hatched portrait well at the left
       2 ink_bar    ink bar on rows 128..159 (campaign map footer; create_bg(0, -128) for a header)
       3 header_bar paper bar on rows 0..17 (menu screens)
    """
    P = PAL_PANELS
    paper, paper_l, paper_d = P['paper'], P['paper_l'], P['paper_d']
    grey_l, grey, ink = P['grey_l'], P['grey'], P['ink']
    ink_l, ink_2, verm, gold = P['ink_l'], P['ink_2'], P['verm'], P['gold']
    maps = []

    def box(with_well):
        c = Canvas(256, 256)
        c.rect(0, 112, 255, 159, paper)
        for y in range(114, 160, 2):                     # paper fibre
            for x in range((y // 2) % 4, 256, 4):
                c.pt(x, y, paper_l)
        c.hline(0, 255, 112, verm)
        c.hline(0, 255, 113, verm)
        c.hline(0, 255, 114, P['grey_l'])
        c.hline(0, 255, 159, paper_d)
        if with_well:
            c.rect(0, 115, 47, 159, paper_d)
            for k in range(-48, 96, 4):
                for t in range(46):
                    x, y = k + t, 115 + t
                    if 0 <= x <= 47 and y <= 159:
                        c.pt(x, y, grey_l)
            c.vline(48, 115, 159, grey)
            c.vline(49, 115, 159, paper_d)
            c.hline(0, 47, 115, grey_l)
        return roll_for_butano(c)
    maps.append(box(False))
    maps.append(box(True))

    c = Canvas(256, 256)
    c.rect(0, 128, 255, 159, ink)
    for x in range(0, 256, 2):
        c.pt(x, 129 + (x // 2) % 2, ink_l)
    c.hline(0, 255, 128, verm)
    c.hline(0, 255, 129, ink_l)
    c.hline(0, 255, 158, ink_2)
    c.hline(0, 255, 159, ink)
    maps.append(roll_for_butano(c))

    c = Canvas(256, 256)
    c.rect(0, 0, 255, 17, paper)
    for y in range(1, 18, 2):
        for x in range((y // 2) % 4, 256, 4):
            c.pt(x, y, paper_l)
    c.hline(0, 255, 17, grey)
    c.hline(0, 255, 16, grey_l)
    c.hline(0, 255, 0, paper_l)
    maps.append(roll_for_butano(c))

    out = Canvas(256, 256 * len(maps))
    for i, m in enumerate(maps):
        out.blit(m, 0, i * 256, transparent=False)
    return out


# -------------------------------------------------------------------------------------------------
# Assembly
# -------------------------------------------------------------------------------------------------

HELPERS = ['sen', 'indi', 'rex']
HELPER_FRAMES = ['portrait', 'idle1', 'idle2', 'cheer', 'sad', 'think']
POSE_FUNCS = {'sen': sen_pose, 'indi': indi_pose, 'rex': rex_pose}
PORTRAIT_FUNCS = {'sen': sen_portrait, 'indi': indi_portrait, 'rex': rex_portrait}
PALETTES = {'sen': PAL_SEN, 'indi': PAL_INDI, 'rex': PAL_REX}


def centred(art, size=64):
    """Put a smaller drawing in the middle of a `size` x `size` sprite frame."""
    c = Canvas(size, size)
    c.blit(art, (size - art.w) // 2, (size - art.h) // 2)
    return c


def helper_sheet(name):
    sh = Sheet(64, 64, PALETTES[name])
    sh.add(centred(PORTRAIT_FUNCS[name]()))
    for pose in POSES:
        sh.add(centred(POSE_FUNCS[name](pose)))
    return sh


def opponents_sheet():
    sh = Sheet(64, 64, PAL_OPP)
    for name in OPPONENTS:
        sh.add(centred(OPP_FUNCS[name]()))
    return sh


SPRITE_SHEETS = [
    ('helper_sen', lambda: helper_sheet('sen')),
    ('helper_indi', lambda: helper_sheet('indi')),
    ('helper_rex', lambda: helper_sheet('rex')),
    ('opponents', opponents_sheet),
    ('map_node', ui_map_node_sheet),
    ('rank_stamp', ui_rank_sheet),
    ('star', ui_star_sheet),
    ('menu_cursor', ui_cursor_sheet),
    ('banner_plate', ui_banner_sheet),
    ('egg_crack', ui_egg_crack_sheet),
    ('speech_bubble', ui_bubble_sheet),
    ('title_logo', ui_title_logo_sheet),
]

BG_ITEMS = [
    ('bg_title', bg_title, PAL_TITLE, None),
    ('bg_map', bg_map, PAL_MAP, None),
    ('bg_hud_classic', bg_hud_classic, PAL_HUD_CLASSIC, None),
    ('bg_hud_pup', bg_hud_pup, PAL_HUD_PUP, None),
    ('bg_hud_dino', bg_hud_dino, PAL_HUD_DINO, None),
    ('bg_panels', bg_panels, PAL_PANELS, 256),
]


def build_all(gfx):
    for name, fn in SPRITE_SHEETS:
        fn().write(gfx, name)
    for name, fn, pal, map_h in BG_ITEMS:
        write_bg(gfx, name, fn(), pal, map_h)


def contact_sheet(path, scale=3):
    """One PNG with every frame of every asset, for eyeballing the whole set at once."""
    bg = (24, 24, 30)
    pad = 6
    rows = []
    for name, (canvas, pal, frame_h) in SPRITES.items():
        n = canvas.h // frame_h
        rows.append((name, [(canvas.sub(0, i * frame_h, canvas.w, frame_h), pal) for i in range(n)]))
    width = max(sum(f.w * scale + 4 for f, _ in fr) for _, fr in rows) + 120
    height = sum(max(f.h for f, _ in fr) * scale + 20 for _, fr in rows) + pad
    for name, (canvas, pal, map_h) in BGS.items():
        height += (min(canvas.h, map_h) * 160) // 160 + 20
        width = max(width, canvas.w + 120)
    img = Image.new('RGB', (width, height + 40), bg)
    draw = ImageDraw.Draw(img)
    y = pad

    def blit_canvas(c, pal, ox, oy, sc):
        rgb = pal.rgb_list()
        tile = Image.new('RGB', (c.w, c.h))
        px = tile.load()
        for j in range(c.h):
            for i in range(c.w):
                v = c.get(i, j)
                px[i, j] = (58, 58, 68) if v == 0 else rgb[v]
        img.paste(tile.resize((c.w * sc, c.h * sc), Image.NEAREST), (ox, oy))

    for name, frames in rows:
        draw.text((4, y + 4), name, fill=(230, 220, 190))
        x = 116
        for f, pal in frames:
            blit_canvas(f, pal, x, y, scale)
            x += f.w * scale + 4
        y += max(f.h for f, _ in frames) * scale + 20
    from PIL import ImageChops
    for name, (canvas, pal, map_h) in BGS.items():
        maps = canvas.h // map_h
        for m in range(maps):
            draw.text((4, y + 4), name + ('' if maps == 1 else ' [%d]' % m), fill=(230, 220, 190))
            sub = canvas.sub(0, m * map_h, canvas.w, map_h)
            sub.img = ImageChops.offset(sub.img, 120 - canvas.w // 2, 80 - map_h // 2)
            blit_canvas(sub.sub(0, 0, canvas.w, 160), pal, 116, y, 1)
            y += 168
    img.save(path)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--check', action='store_true', help='fail if the committed assets are stale')
    ap.add_argument('--sheet', help='write a PIL contact sheet of every frame to this PNG')
    args = ap.parse_args()
    gfx = os.path.join(args.root, 'graphics')
    os.makedirs(gfx, exist_ok=True)
    if args.check:
        globals()['DRY_RUN'] = True
        build_all(gfx)
        stale = [os.path.basename(p) for p, data in _written.items()
                 if not os.path.exists(p) or open(p, 'rb').read() != data]
        if stale:
            print('gen_chars: stale assets: ' + ', '.join(sorted(stale)))
            return 1
        print('gen_chars: %d files up to date' % len(_written))
        return 0
    build_all(gfx)
    total = sum(len(d) for d in _written.values())
    print('gen_chars: wrote %d files (%d bytes of BMP/JSON) to %s' % (len(_written), total, gfx))
    for name, (canvas, pal, frame_h) in SPRITES.items():
        print('  sprite  %-16s %3dx%-3d %2d frames  palette %2d' %
              (name, canvas.w, frame_h, canvas.h // frame_h, len(pal.names)))
    for name, (canvas, pal, map_h) in BGS.items():
        print('  bg      %-16s %3dx%-3d %2d maps    palette %2d' %
              (name, canvas.w, map_h, canvas.h // map_h, len(pal.names)))
    if args.sheet:
        print('contact sheet: ' + contact_sheet(args.sheet))
    return 0


if __name__ == '__main__':
    sys.exit(main())
