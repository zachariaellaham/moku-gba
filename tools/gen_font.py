#!/usr/bin/env python3
"""MOKU pixel fonts generator.

Draws the two original bitmap fonts of the game and writes them in the exact
format Butano's sprite-font pipeline expects:

  graphics/font_8.bmp  + font_8.json   8x8 cell,  variable width, body text / HUD
  graphics/font_16.bmp + font_16.json  8x16 cell, variable width, bold display face
  include/moku_fonts.h                 bn::sprite_font objects + width tables

BMP layout (see butano/tools/bmp.py and bn_sprite_font.h): indexed 4bpp BMP,
one 8-pixel-wide column, glyphs stacked vertically, ASCII '!'..'~' (94 glyphs)
in order then the UTF-8 glyphs in the order of UTF8_CHARS.  Palette index 0 is
transparent, glyph ink uses index 1 only (the game recolours by palette swap).
The width table has 1 + 94 + len(UTF8_CHARS) entries; entry 0 is the space.

Every glyph is authored below as ASCII art ('#' = ink, '.' = empty) placed at
an explicit top row inside the cell.  Widths are ink width + 1 pixel of gap,
except where a fixed width is forced (digits are tabular).

Usage:
    python3 tools/gen_font.py                 # writes graphics/ + include/
    python3 tools/gen_font.py --check         # exit 1 if those files are stale
    python3 tools/gen_font.py --preview out.png [--scale 3]
"""

import argparse
import os
import struct
import sys

# ---------------------------------------------------------------------------
# Character sets
# ---------------------------------------------------------------------------

ASCII_CHARS = [chr(c) for c in range(33, 127)]  # 94 glyphs, space is not drawn

# Shared by both fonts so that any string that renders in one renders in the other.
UTF8_CHARS = [
    "À", "Â", "Ç", "É", "È", "Ê", "Ë", "Î", "Ï", "Ô", "Ù", "Û", "Ü", "Œ",
    "à", "â", "ç", "é", "è", "ê", "ë", "î", "ï", "ô", "ù", "û", "ü", "œ",
    "’", "«", "»", "…", "·", "★", "☆", "▲", "▼", "▶", "◀", "×",
    "←", "↑", "→", "↓", "▮", "▯", "©",
]

# ---------------------------------------------------------------------------
# font_8 : 8x8 cell.  Caps rows 1-6, x-height rows 3-6, descender row 7,
# lowercase accents rows 0-1 (gap row 2), accented caps squeezed to rows 2-6.
# Glyph = (top_row, [rows...])
# ---------------------------------------------------------------------------

F8 = {}

def g8(ch, top, *rows):
    F8[ch] = (top, list(rows))

# --- punctuation ------------------------------------------------------------
g8('!', 1, "#", "#", "#", "#", ".", "#")
g8('"', 1, "#.#", "#.#")
g8('#', 1, ".#.#.", "#####", ".#.#.", ".#.#.", "#####", ".#.#.")
g8('$', 0, "..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#..")
g8('%', 1, "##..#", "##.#.", "...#.", "..#..", ".#.##", "#..##")
g8('&', 1, ".##..", "#..#.", ".##..", "#.#.#", "#..#.", ".##.#")
g8("'", 1, "#", "#")
g8('(', 1, ".#", "#.", "#.", "#.", "#.", ".#")
g8(')', 1, "#.", ".#", ".#", ".#", ".#", "#.")
g8('*', 1, "..#..", "#.#.#", ".###.", "#.#.#", "..#..")
g8('+', 2, "..#..", "..#..", "#####", "..#..", "..#..")
g8(',', 6, ".#", "#.")
g8('-', 4, "###")
g8('.', 6, "#")
g8('/', 1, "..#", "..#", ".#.", ".#.", "#..", "#..")
# --- digits (tabular, width forced to 6) -------------------------------------
g8('0', 1, ".###.", "#...#", "#..##", "#.#.#", "##..#", ".###.")
g8('1', 1, "..#..", ".##..", "..#..", "..#..", "..#..", ".###.")
g8('2', 1, ".###.", "#...#", "...#.", "..#..", ".#...", "#####")
g8('3', 1, "####.", "....#", "..##.", "....#", "#...#", ".###.")
g8('4', 1, "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.")
g8('5', 1, "#####", "#....", "####.", "....#", "#...#", ".###.")
g8('6', 1, ".###.", "#....", "####.", "#...#", "#...#", ".###.")
g8('7', 1, "#####", "....#", "...#.", "..#..", ".#...", ".#...")
g8('8', 1, ".###.", "#...#", ".###.", "#...#", "#...#", ".###.")
g8('9', 1, ".###.", "#...#", "#...#", ".####", "....#", ".###.")
g8(':', 3, "#", ".", ".", "#")
g8(';', 3, ".#", "..", "..", ".#", "#.")
g8('<', 2, "..#", ".#.", "#..", ".#.", "..#")
g8('=', 3, "####", "....", "####")
g8('>', 2, "#..", ".#.", "..#", ".#.", "#..")
g8('?', 1, ".###.", "#...#", "...#.", "..#..", ".....", "..#..")
g8('@', 1, ".###.", "#...#", "#.#.#", "#.###", "#....", ".###.")
# --- capitals (5 wide, rows 1-6) ----------------------------------------------
g8('A', 1, ".###.", "#...#", "#...#", "#####", "#...#", "#...#")
g8('B', 1, "####.", "#...#", "####.", "#...#", "#...#", "####.")
g8('C', 1, ".###.", "#...#", "#....", "#....", "#...#", ".###.")
g8('D', 1, "####.", "#...#", "#...#", "#...#", "#...#", "####.")
g8('E', 1, "#####", "#....", "####.", "#....", "#....", "#####")
g8('F', 1, "#####", "#....", "####.", "#....", "#....", "#....")
g8('G', 1, ".###.", "#....", "#.###", "#...#", "#...#", ".###.")
g8('H', 1, "#...#", "#...#", "#####", "#...#", "#...#", "#...#")
g8('I', 1, "###", ".#.", ".#.", ".#.", ".#.", "###")
g8('J', 1, "..##", "...#", "...#", "...#", "#..#", ".##.")
g8('K', 1, "#...#", "#..#.", "###..", "#..#.", "#...#", "#...#")
g8('L', 1, "#....", "#....", "#....", "#....", "#....", "#####")
g8('M', 1, "#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#")
g8('N', 1, "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#")
g8('O', 1, ".###.", "#...#", "#...#", "#...#", "#...#", ".###.")
g8('P', 1, "####.", "#...#", "#...#", "####.", "#....", "#....")
g8('Q', 1, ".###.", "#...#", "#...#", "#.#.#", "#..#.", ".##.#")
g8('R', 1, "####.", "#...#", "#...#", "####.", "#..#.", "#...#")
g8('S', 1, ".####", "#....", ".###.", "....#", "#...#", ".###.")
g8('T', 1, "#####", "..#..", "..#..", "..#..", "..#..", "..#..")
g8('U', 1, "#...#", "#...#", "#...#", "#...#", "#...#", ".###.")
g8('V', 1, "#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#..")
g8('W', 1, "#...#", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#.")
g8('X', 1, "#...#", ".#.#.", "..#..", "..#..", ".#.#.", "#...#")
g8('Y', 1, "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..")
g8('Z', 1, "#####", "....#", "...#.", "..#..", ".#...", "#####")
g8('[', 1, "##", "#.", "#.", "#.", "#.", "##")
g8('\\', 1, "#..", "#..", ".#.", ".#.", "..#", "..#")
g8(']', 1, "##", ".#", ".#", ".#", ".#", "##")
g8('^', 1, ".#.", "#.#")
g8('_', 7, "#####")
g8('`', 1, "#.", ".#")
# --- lowercase (x-height rows 3-6, ascenders from row 1, descender row 7) ------
g8('a', 3, ".###", "#..#", "#..#", ".###")
g8('b', 1, "#...", "#...", "###.", "#..#", "#..#", "###.")
g8('c', 3, ".###", "#...", "#...", ".###")
g8('d', 1, "...#", "...#", ".###", "#..#", "#..#", ".###")
g8('e', 3, ".##.", "####", "#...", ".###")
g8('f', 1, ".##", "#..", "###", "#..", "#..", "#..")
g8('g', 3, ".###", "#..#", "#..#", ".###", "###.")
g8('h', 1, "#...", "#...", "###.", "#..#", "#..#", "#..#")
g8('i', 1, "#", ".", "#", "#", "#", "#")
g8('j', 1, ".#", "..", ".#", ".#", ".#", ".#", "#.")
g8('k', 1, "#...", "#...", "#.#.", "##..", "#.#.", "#..#")
g8('l', 1, "#.", "#.", "#.", "#.", "#.", "##")
g8('m', 3, "####.", "#.#.#", "#.#.#", "#.#.#")
g8('n', 3, "###.", "#..#", "#..#", "#..#")
g8('o', 3, ".##.", "#..#", "#..#", ".##.")
g8('p', 3, "###.", "#..#", "#..#", "###.", "#...")
g8('q', 3, ".###", "#..#", "#..#", ".###", "...#")
g8('r', 3, "#.#", "##.", "#..", "#..")
g8('s', 3, ".###", "##..", "..##", "###.")
g8('t', 2, "#..", "###", "#..", "#..", ".##")
g8('u', 3, "#..#", "#..#", "#..#", ".###")
g8('v', 3, "#.#", "#.#", "#.#", ".#.")
g8('w', 3, "#.#.#", "#.#.#", "#.#.#", ".#.#.")
g8('x', 3, "#.#", ".#.", ".#.", "#.#")
g8('y', 3, "#..#", "#..#", "#..#", ".###", "###.")
g8('z', 3, "####", "..#.", ".#..", "####")
g8('{', 1, ".##", ".#.", "#..", ".#.", ".#.", ".##")
g8('|', 1, "#", "#", "#", "#", "#", "#")
g8('}', 1, "##.", ".#.", "..#", ".#.", ".#.", "##.")
g8('~', 3, ".##.#", "#..#.")

# --- UTF-8: accented capitals (accent rows 0-1 touching a 5-row letter) -------
A5 = [".###.", "#...#", "#####", "#...#", "#...#"]
E5 = ["#####", "#....", "####.", "#....", "#####"]
I5 = ["###", ".#.", ".#.", ".#.", "###"]
O5 = [".###.", "#...#", "#...#", "#...#", ".###."]
U5 = ["#...#", "#...#", "#...#", "#...#", ".###."]
GRAVE5, ACUTE5 = [".#...", "..#.."], ["...#.", "..#.."]
CIRC5, DIAER5 = ["..#..", ".#.#."], [".#.#.", "....."]

g8('À', 0, *GRAVE5, *A5)
g8('Â', 0, *CIRC5, *A5)
g8('Ç', 1, ".###.", "#...#", "#....", "#....", "#...#", ".###.", "..#..")
g8('É', 0, *ACUTE5, *E5)
g8('È', 0, *GRAVE5, *E5)
g8('Ê', 0, *CIRC5, *E5)
g8('Ë', 0, *DIAER5, *E5)
g8('Î', 0, ".#.", "#.#", *I5)
g8('Ï', 0, "#.#", "...", *I5)
g8('Ô', 0, *CIRC5, *O5)
g8('Ù', 0, *GRAVE5, *U5)
g8('Û', 0, *CIRC5, *U5)
g8('Ü', 0, *DIAER5, *U5)
g8('Œ', 1, ".######", "#..#...", "#..####", "#..#...", "#..#...", ".######")

# --- UTF-8: accented lowercase (accent rows 0-1, gap row 2, letter rows 3-6) ---
GRAVE4, ACUTE4 = [".#..", "..#.", "...."], ["..#.", ".#..", "...."]
CIRC4, DIAER4 = ["..#.", ".#.#", "...."], ["....", "#..#", "...."]
LA, LE, LO, LU = F8['a'][1], F8['e'][1], F8['o'][1], F8['u'][1]

g8('à', 0, *GRAVE4, *LA)
g8('â', 0, *CIRC4, *LA)
g8('ç', 3, ".###", "#...", "#...", ".###", ".#..")
g8('é', 0, *ACUTE4, *LE)
g8('è', 0, *GRAVE4, *LE)
g8('ê', 0, *CIRC4, *LE)
g8('ë', 0, *DIAER4, *LE)
g8('î', 0, ".#.", "#.#", "...", ".#.", ".#.", ".#.", ".#.")
g8('ï', 0, "...", "#.#", "...", ".#.", ".#.", ".#.", ".#.")
g8('ô', 0, *CIRC4, *LO)
g8('ù', 0, *GRAVE4, *LU)
g8('û', 0, *CIRC4, *LU)
g8('ü', 0, *DIAER4, *LU)
g8('œ', 3, ".##.##.", "#..####", "#..#...", ".##.###")

# --- UTF-8: typography & HUD symbols ------------------------------------------
g8('’', 1, ".#", "#.")
g8('«', 3, ".#.#", "#.#.", ".#.#")
g8('»', 3, "#.#.", ".#.#", "#.#.")
g8('…', 6, "#.#.#")
g8('·', 4, "#")
g8('★', 1, "...#...", "..###..", "#######", ".#####.", "..###..", ".##.##.")
g8('☆', 1, "...#...", "..#.#..", "##...##", ".#...#.", "..###..", ".##.##.")
g8('▲', 2, "..#..", ".###.", "#####")
g8('▼', 2, "#####", ".###.", "..#..")
g8('▶', 0, "#...", "##..", "###.", "####", "###.", "##..", "#...")
g8('◀', 0, "...#", "..##", ".###", "####", ".###", "..##", "...#")
g8('×', 2, "#.#", ".#.", "#.#")
g8('←', 2, "..#..", ".#...", "#####", ".#...", "..#..")
g8('↑', 1, "..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..")
g8('→', 2, "..#..", "...#.", "#####", "...#.", "..#..")
g8('↓', 1, "..#..", "..#..", "..#..", "#.#.#", ".###.", "..#..")
g8('▮', 2, "###", "###", "###", "###", "###")
g8('▯', 2, "###", "#.#", "#.#", "#.#", "###")
g8('©', 1, ".#####.", "#.....#", "#..##.#", "#.#...#", "#..##.#", ".#####.")

F8_SPACE = 3
F8_FIXED_WIDTH = {c: 6 for c in "0123456789"}

# ---------------------------------------------------------------------------
# font_16 : 8x16 cell, bold display face (2 px strokes).  Letters rows 3-14,
# accents rows 0-1 (gap row 2), cedilla / comma / underscore may use row 15.
# Lowercase folds to the capital bitmaps (caps-only display face).
# ---------------------------------------------------------------------------

F16 = {}

def g16(ch, top, *rows):
    F16[ch] = (top, list(rows))

CAPS16 = {
    'A': [".#####.", "#######", "##...##", "##...##", "##...##", "#######",
          "#######", "##...##", "##...##", "##...##", "##...##", "##...##"],
    'B': ["######.", "#######", "##...##", "##...##", "######.", "######.",
          "##...##", "##...##", "##...##", "##...##", "#######", "######."],
    'C': [".#####.", "#######", "##...##", "##.....", "##.....", "##.....",
          "##.....", "##.....", "##.....", "##...##", "#######", ".#####."],
    'D': ["######.", "#######", "##...##", "##...##", "##...##", "##...##",
          "##...##", "##...##", "##...##", "##...##", "#######", "######."],
    'E': ["#######", "#######", "##.....", "##.....", "#####..", "#####..",
          "##.....", "##.....", "##.....", "##.....", "#######", "#######"],
    'F': ["#######", "#######", "##.....", "##.....", "#####..", "#####..",
          "##.....", "##.....", "##.....", "##.....", "##.....", "##....."],
    'G': [".#####.", "#######", "##...##", "##.....", "##.....", "##.####",
          "##.####", "##...##", "##...##", "##...##", "#######", ".#####."],
    'H': ["##...##", "##...##", "##...##", "##...##", "#######", "#######",
          "##...##", "##...##", "##...##", "##...##", "##...##", "##...##"],
    'I': ["####", "####", ".##.", ".##.", ".##.", ".##.",
          ".##.", ".##.", ".##.", ".##.", "####", "####"],
    'J': ["..####", "..####", "....##", "....##", "....##", "....##",
          "....##", "....##", "##..##", "##..##", "######", ".####."],
    'K': ["##...##", "##...##", "##..##.", "##.##..", "####...", "####...",
          "####...", "##.##..", "##..##.", "##..##.", "##...##", "##...##"],
    'L': ["##.....", "##.....", "##.....", "##.....", "##.....", "##.....",
          "##.....", "##.....", "##.....", "##.....", "#######", "#######"],
    'M': ["##...##", "###.###", "#######", "#######", "##.#.##", "##.#.##",
          "##...##", "##...##", "##...##", "##...##", "##...##", "##...##"],
    'N': ["##...##", "###..##", "###..##", "####.##", "####.##", "##.####",
          "##.####", "##..###", "##..###", "##...##", "##...##", "##...##"],
    'O': [".#####.", "#######", "##...##", "##...##", "##...##", "##...##",
          "##...##", "##...##", "##...##", "##...##", "#######", ".#####."],
    'P': ["######.", "#######", "##...##", "##...##", "##...##", "#######",
          "######.", "##.....", "##.....", "##.....", "##.....", "##....."],
    'Q': [".#####.", "#######", "##...##", "##...##", "##...##", "##...##",
          "##...##", "##...##", "##...##", "##.#.##", "######.", ".####.#"],
    'R': ["######.", "#######", "##...##", "##...##", "##...##", "#######",
          "######.", "##.##..", "##..##.", "##..##.", "##...##", "##...##"],
    'S': [".#####.", "#######", "##...##", "##.....", "###....", ".#####.",
          "..#####", "....###", ".....##", "##...##", "#######", ".#####."],
    'T': ["#######", "#######", "..##...", "..##...", "..##...", "..##...",
          "..##...", "..##...", "..##...", "..##...", "..##...", "..##..."],
    'U': ["##...##", "##...##", "##...##", "##...##", "##...##", "##...##",
          "##...##", "##...##", "##...##", "##...##", "#######", ".#####."],
    'V': ["##...##", "##...##", "##...##", "##...##", "##...##", "##...##",
          "##...##", ".##.##.", ".##.##.", ".#####.", "..###..", "...#..."],
    'W': ["##...##", "##...##", "##...##", "##...##", "##...##", "##...##",
          "##...##", "##.#.##", "##.#.##", "#######", "#######", ".##.##."],
    'X': ["##...##", "##...##", ".##.##.", ".##.##.", "..###..", "..###..",
          "..###..", "..###..", ".##.##.", ".##.##.", "##...##", "##...##"],
    'Y': ["##...##", "##...##", ".##.##.", ".##.##.", "..###..", "..###..",
          "..##...", "..##...", "..##...", "..##...", "..##...", "..##..."],
    'Z': ["#######", "#######", ".....##", "....##.", "....##.", "...##..",
          "..##...", "..##...", ".##....", "##.....", "#######", "#######"],
}
for _c, _rows in CAPS16.items():
    g16(_c, 3, *_rows)
    g16(_c.lower(), 3, *_rows)

DIGITS16 = {
    '0': [".#####.", "#######", "##...##", "##...##", "##..###", "##.#.##",
          "###..##", "##...##", "##...##", "##...##", "#######", ".#####."],
    '1': ["...##..", "..###..", ".####..", "...##..", "...##..", "...##..",
          "...##..", "...##..", "...##..", "...##..", ".#####.", ".#####."],
    '2': [".#####.", "#######", "##...##", ".....##", ".....##", "....##.",
          "...##..", "..##...", ".##....", "##.....", "#######", "#######"],
    '3': [".#####.", "#######", "##...##", ".....##", ".....##", "..####.",
          "..####.", ".....##", ".....##", "##...##", "#######", ".#####."],
    '4': ["....##.", "...###.", "..####.", ".##.##.", "##..##.", "##..##.",
          "#######", "#######", "....##.", "....##.", "....##.", "....##."],
    '5': ["#######", "#######", "##.....", "##.....", "######.", "#######",
          ".....##", ".....##", ".....##", "##...##", "#######", ".#####."],
    '6': [".#####.", "#######", "##...##", "##.....", "##.....", "######.",
          "#######", "##...##", "##...##", "##...##", "#######", ".#####."],
    '7': ["#######", "#######", ".....##", ".....##", "....##.", "....##.",
          "...##..", "...##..", "..##...", "..##...", "..##...", "..##..."],
    '8': [".#####.", "#######", "##...##", "##...##", "##...##", ".#####.",
          ".#####.", "##...##", "##...##", "##...##", "#######", ".#####."],
    '9': [".#####.", "#######", "##...##", "##...##", "##...##", "#######",
          ".######", ".....##", ".....##", "##...##", "#######", ".#####."],
}
for _c, _rows in DIGITS16.items():
    g16(_c, 3, *_rows)

g16('!', 3, "##", "##", "##", "##", "##", "##", "##", "##", "##", "..", "##", "##")
g16('"', 3, "##.##", "##.##", "#..#.")
g16('#', 4, ".##.##.", ".##.##.", "#######", "#######", ".##.##.", ".##.##.",
    "#######", "#######", ".##.##.", ".##.##.")
g16('$', 3, "...#...", ".#####.", "#######", "##.#.##", "##.#...", "#####..",
    ".#####.", "..#####", "...#.##", "##.#.##", "#######", ".#####.", "...#...")
g16('%', 3, "##....#", "##...#.", "....#..", "....#..", "...#...", "...#...",
    "..#....", "..#....", ".#.....", ".#.....", "#....##", ".....##")
g16('&', 3, ".####..", "######.", "##..##.", "##..##.", ".####..", ".###...",
    "####.##", "##.####", "##..##.", "##..##.", "#######", ".####.#")
g16("'", 3, "##", "##", "##")
g16('(', 3, "..#", ".##", "##.", "##.", "##.", "##.", "##.", "##.", "##.", "##.", ".##", "..#")
g16(')', 3, "#..", "##.", ".##", ".##", ".##", ".##", ".##", ".##", ".##", ".##", "##.", "#..")
g16('*', 4, "...#...", "##.#.##", ".#####.", "..###..", ".#####.", "##.#.##", "...#...")
g16('+', 6, "..##..", "..##..", "######", "######", "..##..", "..##..")
g16(',', 13, "##", "##", "#.")
g16('-', 8, "#####", "#####")
g16('.', 13, "##", "##")
g16('/', 3, ".....##", ".....##", "....##.", "....##.", "...##..", "...##..",
    "..##...", "..##...", ".##....", ".##....", "##.....", "##.....")
g16(':', 6, "##", "##", "..", "..", "..", "..", "..", "##", "##")
g16(';', 6, ".##", ".##", "...", "...", "...", "...", "...", ".##", ".##", "##.")
g16('<', 5, "....#", "...##", "..##.", ".##..", "##...", ".##..", "..##.", "...##", "....#")
g16('=', 7, "######", "######", "......", "######", "######")
g16('>', 5, "#....", "##...", ".##..", "..##.", "...##", "..##.", ".##..", "##...", "#....")
g16('?', 3, ".#####.", "#######", "##...##", ".....##", "....##.", "...##..",
    "..##...", "..##...", "..##...", ".......", "..##...", "..##...")
g16('@', 3, ".#####.", "#######", "##...##", "##.####", "##.####", "##.#.##",
    "##.#.##", "##.####", "##.###.", "##.....", "#######", ".#####.")
g16('[', 3, "###", "###", "##.", "##.", "##.", "##.", "##.", "##.", "##.", "##.", "###", "###")
g16('\\', 3, "##.....", "##.....", ".##....", ".##....", "..##...", "..##...",
    "...##..", "...##..", "....##.", "....##.", ".....##", ".....##")
g16(']', 3, "###", "###", ".##", ".##", ".##", ".##", ".##", ".##", ".##", ".##", "###", "###")
g16('^', 3, "..#..", ".###.", "##.##")
g16('_', 14, "#######", "#######")
g16('`', 3, "##.", ".##")
g16('{', 3, "..##", ".##.", ".##.", ".##.", ".##.", "##..", ".##.", ".##.", ".##.", ".##.", ".##.", "..##")
g16('|', 3, "##", "##", "##", "##", "##", "##", "##", "##", "##", "##", "##", "##")
g16('}', 3, "##..", ".##.", ".##.", ".##.", ".##.", "..##", ".##.", ".##.", ".##.", ".##.", ".##.", "##..")
g16('~', 8, ".###.##", "##.###.")

# --- UTF-8 accented capitals (accent rows 0-1, gap row 2, letter rows 3-14) ---
GRAVE7, ACUTE7 = [".##....", "..##...", "......."], ["....##.", "...##..", "......."]
CIRC7, DIAER7 = ["..###..", ".##.##.", "......."], [".##.##.", ".##.##.", "......."]

g16('À', 0, *GRAVE7, *CAPS16['A'])
g16('Â', 0, *CIRC7, *CAPS16['A'])
g16('Ç', 3, ".#####.", "#######", "##...##", "##.....", "##.....", "##.....",
    "##.....", "##.....", "##...##", "#######", ".#####.", "...##..", "..###..")
g16('É', 0, *ACUTE7, *CAPS16['E'])
g16('È', 0, *GRAVE7, *CAPS16['E'])
g16('Ê', 0, *CIRC7, *CAPS16['E'])
g16('Ë', 0, *DIAER7, *CAPS16['E'])
g16('Î', 0, ".##.", "#..#", "....", *CAPS16['I'])
g16('Ï', 0, "#..#", "#..#", "....", *CAPS16['I'])
g16('Ô', 0, *CIRC7, *CAPS16['O'])
g16('Ù', 0, *GRAVE7, *CAPS16['U'])
g16('Û', 0, *CIRC7, *CAPS16['U'])
g16('Ü', 0, *DIAER7, *CAPS16['U'])
g16('Œ', 3, ".######", "#######", "##.##..", "##.##..", "##.###.", "##.###.",
    "##.##..", "##.##..", "##.##..", "##.##..", "#######", ".######")
for _lo, _up in zip("àâçéèêëîïôùûüœ", "ÀÂÇÉÈÊËÎÏÔÙÛÜŒ"):
    F16[_lo] = F16[_up]

g16('’', 3, "##", "##", "#.")
g16('«', 6, "...#..#", "..##.##", ".##.##.", "##.##..", ".##.##.", "..##.##", "...#..#")
g16('»', 6, "#..#...", "##.##..", ".##.##.", "..##.##", ".##.##.", "##.##..", "#..#...")
g16('…', 13, "#..#..#", "#..#..#")
g16('·', 8, "##", "##")
g16('★', 5, "...#...", "..###..", "#######", ".#####.", "..###..", ".##.##.", "##...##")
g16('☆', 5, "...#...", "..#.#..", "##...##", ".#...#.", "..###..", ".##.##.", "##...##")
g16('▲', 6, "...#...", "..###..", ".#####.", "#######")
g16('▼', 6, "#######", ".#####.", "..###..", "...#...")
g16('▶', 4, "#....", "##...", "###..", "####.", "#####", "####.", "###..", "##...", "#....")
g16('◀', 4, "....#", "...##", "..###", ".####", "#####", ".####", "..###", "...##", "....#")
g16('×', 6, "##..##", ".####.", "..##..", "..##..", ".####.", "##..##")
g16('←', 5, "...##..", "..##...", ".##....", "#######", "#######", ".##....", "..##...", "...##..")
g16('↑', 5, "...#...", "..###..", ".#####.", "#######", "..###..", "..###..", "..###..", "..###..")
g16('→', 5, "..##...", "...##..", "....##.", "#######", "#######", "....##.", "...##..", "..##...")
g16('↓', 5, "..###..", "..###..", "..###..", "..###..", "#######", ".#####.", "..###..", "...#...")
g16('▮', 4, "#####", "#####", "#####", "#####", "#####", "#####", "#####", "#####", "#####", "#####")
g16('▯', 4, "#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "#####")
g16('©', 6, ".#####.", "#.....#", "#.###.#", "#.#...#", "#.#...#", "#.#...#", "#.###.#", "#.....#",
    ".#####.")

F16_SPACE = 4
F16_FIXED_WIDTH = {c: 8 for c in "0123456789"}

# ---------------------------------------------------------------------------
# Rasterisation
# ---------------------------------------------------------------------------

def render_glyph(spec, cell_h, name):
    """Returns (bitmap rows as list of lists of 0/1 with 8 columns, ink width)."""
    top, rows = spec
    bitmap = [[0] * 8 for _ in range(cell_h)]
    ink_w = 0
    if top < 0 or top + len(rows) > cell_h:
        raise ValueError(f"glyph {name!r}: rows {top}..{top + len(rows) - 1} exceed cell height {cell_h}")
    for y, row in enumerate(rows):
        if len(row) > 8:
            raise ValueError(f"glyph {name!r}: row {y} wider than 8: {row!r}")
        for x, ch in enumerate(row):
            if ch == '#':
                bitmap[top + y][x] = 1
                ink_w = max(ink_w, x + 1)
            elif ch != '.':
                raise ValueError(f"glyph {name!r}: bad char {ch!r}")
    if ink_w == 0:
        raise ValueError(f"glyph {name!r} is empty")
    return bitmap, ink_w


def build_font(glyphs, cell_h, space_w, fixed_w):
    chars = ASCII_CHARS + UTF8_CHARS
    missing = [c for c in chars if c not in glyphs]
    if missing:
        raise ValueError("missing glyphs: " + " ".join(missing))
    extra = [c for c in glyphs if c not in chars]
    if extra:
        raise ValueError("glyphs not in character set: " + " ".join(extra))
    bitmaps, widths = [], [space_w]
    for c in chars:
        bmp, ink_w = render_glyph(glyphs[c], cell_h, c)
        w = fixed_w.get(c, ink_w + 1)
        if w < ink_w:
            raise ValueError(f"glyph {c!r}: fixed width {w} < ink width {ink_w}")
        if w > 8:
            raise ValueError(f"glyph {c!r}: width {w} > 8")
        bitmaps.append(bmp)
        widths.append(w)
    return chars, bitmaps, widths


# 16-colour palette: index 0 = transparent key, index 1 = ink (game ink #1b1f2a),
# the rest are unused but distinct so tools never merge them.
PALETTE = [(255, 0, 255), (0x1b, 0x1f, 0x2a)] + [
    (0xef, 0xe6, 0xd2), (0xd9, 0xa9, 0x5a), (0xc2, 0x3b, 0x2c), (0x5a, 0x3a, 0x12),
    (0x8a, 0x80, 0x70), (0x3f, 0x6f, 0xc4), (0xf3, 0x9a, 0x3e), (0x8f, 0xd0, 0x5e),
    (0xff, 0x6a, 0x1a), (0xe0, 0xa0, 0x30), (0x2b, 0x5a, 0xa6), (0x2a, 0x1a, 0x12),
    (0x40, 0x40, 0x40), (0xff, 0xff, 0xff),
]


def build_bmp4(bitmaps, cell_h):
    """Builds an uncompressed 4bpp indexed BMP, 8 px wide, glyphs stacked top-down."""
    width = 8
    height = cell_h * len(bitmaps)
    row_bytes = 4  # 8 pixels * 4 bits, already a multiple of 4
    pixel_offset = 14 + 40 + 16 * 4
    image_size = row_bytes * height
    out = bytearray()
    out += b'BM' + struct.pack('<IHHI', pixel_offset + image_size, 0, 0, pixel_offset)
    out += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 4, 0, image_size, 2835, 2835, 16, 16)
    for r, g, b in PALETTE:
        out += bytes((b, g, r, 0))
    rows = [row for bmp in bitmaps for row in bmp]
    for row in reversed(rows):  # BMP stores bottom-up
        for x in range(0, 8, 2):
            out.append((row[x] << 4) | row[x + 1])
    return bytes(out)


def build_json(cell_h):
    return ('{\n    "type": "sprite",\n    "height": %d,\n    "bpp_mode": "bpp_4",\n'
            '    "compression": "none"\n}\n' % cell_h)


def cpp_string_literal(ch):
    return '"' + ch.encode('utf-8').decode('utf-8') + '"'


def width_table_lines(chars, widths):
    lines = ['    %d,  // 32 space' % widths[0]]
    for c, w in zip(chars, widths[1:]):
        code = ord(c)
        shown = 'backslash' if c == '\\' else c  # a trailing backslash would continue the comment
        label = ('%d %s' % (code, shown)) if code < 127 else ('U+%04X %s' % (code, c))
        lines.append('    %d,  // %s' % (w, label))
    return '\n'.join(lines)


def build_widths_header(chars, widths8, widths16):
    """The metrics on their own, with no Butano in them, so host tests can measure real pixels."""
    utf8_rows = [UTF8_CHARS[i:i + 14] for i in range(0, len(UTF8_CHARS), 14)]
    utf8_list = ',\n    '.join(', '.join(cpp_string_literal(c) for c in row) for row in utf8_rows)
    return f'''/*
 * MOKU - Tactics of Go: font metrics, without Butano.
 * Generated by tools/gen_font.py - do not edit by hand, edit the generator.
 *
 * moku_fonts.h builds the bn::sprite_font objects from these same tables, so a width measured
 * here is the width the GBA draws. The host tests use it to prove that every string fits the box
 * it is drawn in, in pixels rather than in characters.
 */

#ifndef MOKU_FONT_WIDTHS_H
#define MOKU_FONT_WIDTHS_H

#include <cstdint>

namespace fonts
{{

// ASCII 32..126 in order, then these UTF-8 glyphs, in this order.
constexpr const char* moku_font_utf8_strings[] = {{
    {utf8_list}
}};

constexpr int moku_font_utf8_count = int(sizeof(moku_font_utf8_strings) / sizeof(moku_font_utf8_strings[0]));

constexpr int8_t moku_font_8_widths[] = {{
{width_table_lines(chars, widths8)}
}};

constexpr int8_t moku_font_16_widths[] = {{
{width_table_lines(chars, widths16)}
}};

// Width in pixels of a UTF-8 string, by the same metric bn::sprite_text_generator uses:
// the sum of the character widths, no extra spacing. Control characters count as 0; an unknown
// UTF-8 character counts as 0 and sets `unknown`.
[[nodiscard]] constexpr int moku_text_width(const char* text, const int8_t* widths, bool* unknown = nullptr)
{{
    int width = 0;

    while(*text)
    {{
        const unsigned char c = static_cast<unsigned char>(*text);

        if(c < 128)
        {{
            if(c == 32)
            {{
                width += widths[0];
            }}
            else if(c > 32 && c < 127)
            {{
                width += widths[c - 32];
            }}

            ++text;
            continue;
        }}

        int length = 1;

        if((c & 0xE0) == 0xC0)      length = 2;
        else if((c & 0xF0) == 0xE0) length = 3;
        else if((c & 0xF8) == 0xF0) length = 4;

        int found = -1;

        for(int i = 0; i < moku_font_utf8_count; ++i)
        {{
            const char* candidate = moku_font_utf8_strings[i];
            int j = 0;

            while(j < length && candidate[j] && candidate[j] == text[j])
            {{
                ++j;
            }}

            if(j == length && candidate[j] == 0)
            {{
                found = i;
                break;
            }}
        }}

        if(found >= 0)
        {{
            width += widths[95 + found];
        }}
        else if(unknown)
        {{
            *unknown = true;
        }}

        text += length;
    }}

    return width;
}}

}}

#endif
'''


def build_header(chars, widths8, widths16):
    utf8_rows = [UTF8_CHARS[i:i + 14] for i in range(0, len(UTF8_CHARS), 14)]
    utf8_list = ',\n    '.join(', '.join(cpp_string_literal(c) for c in row) for row in utf8_rows)
    text = f'''/*
 * MOKU - Tactics of Go: pixel fonts.
 * Generated by tools/gen_font.py - do not edit by hand, edit the generator.
 *
 * font_8  : 8x8 cell, variable width. Caps 6 px tall, x-height 4, 1 px descender.
 * font_16 : 8x16 cell, variable width, bold caps-only display face (lowercase
 *           folds to the capital glyphs).
 * Both fonts accept the same character set: ASCII 32..126 plus the UTF-8
 * glyphs listed in moku_font_utf8_characters. Ink is palette index 1 only.
 */

#ifndef MOKU_FONTS_H
#define MOKU_FONTS_H

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_font_8.h"
#include "bn_sprite_items_font_16.h"

// The width tables themselves, with no Butano in them, so host tests measure the same pixels.
#include "moku_font_widths.h"

namespace fonts
{{

constexpr bn::utf8_character moku_font_utf8_characters[] = {{
    {utf8_list}
}};

constexpr bn::span<const int8_t> moku_font_8_character_widths(moku_font_8_widths);

constexpr bn::span<const int8_t> moku_font_16_character_widths(moku_font_16_widths);

constexpr bn::span<const bn::utf8_character> moku_font_utf8_characters_span(moku_font_utf8_characters);

constexpr auto moku_font_utf8_characters_map = bn::utf8_characters_map<moku_font_utf8_characters_span>();

constexpr bn::sprite_font moku_font_8(
        bn::sprite_items::font_8, moku_font_utf8_characters_map.reference(), moku_font_8_character_widths);

constexpr bn::sprite_font moku_font_16(
        bn::sprite_items::font_16, moku_font_utf8_characters_map.reference(), moku_font_16_character_widths);

constexpr int moku_font_8_height = 8;    //!< Cell height; use 8 for tight lines, 10 for dialogue.
constexpr int moku_font_8_baseline = 7;  //!< Rows above the descender row (baseline is row 6, descender row 7).
constexpr int moku_font_8_space_width = {widths8[0]};
constexpr int moku_font_16_height = 16;
constexpr int moku_font_16_baseline = 15;  //!< Letters sit on row 14; row 15 holds cedillas / commas.
constexpr int moku_font_16_space_width = {widths16[0]};

/**
 * @brief Width in pixels of a UTF-8 text rendered with the given MOKU font
 * (same metric bn::sprite_text_generator uses: character widths, no extra spacing).
 * ASCII control characters count as 0 pixels; an unknown UTF-8 character is an error.
 */
[[nodiscard]] constexpr int text_width(const bn::sprite_font& font, const char* text)
{{
    const bn::span<const int8_t>& widths = font.character_widths_ref();
    const bn::utf8_characters_map_ref& utf8_map = font.utf8_characters_ref();
    int width = 0;

    while(*text)
    {{
        unsigned char c = static_cast<unsigned char>(*text);

        if(c == ' ')
        {{
            width += widths[0];
            ++text;
        }}
        else if(c < 128)
        {{
            if(c > 32 && c < 127)
            {{
                width += widths[c - 32];
            }}

            ++text;
        }}
        else
        {{
            bn::utf8_character utf8_char(*text);
            width += widths[95 + utf8_map.index(utf8_char)];
            text += utf8_char.size();
        }}
    }}

    return width;
}}

}}

#endif
'''
    return text


# ---------------------------------------------------------------------------
# Preview (PIL, optional): renders sample strings the way Butano lays them out.
# ---------------------------------------------------------------------------

def draw_text(canvas, font, x0, y0, text, ink):
    chars, bitmaps, widths = font
    index = {c: i for i, c in enumerate(chars)}
    cell_h = len(bitmaps[0])
    x = x0
    for c in text:
        if c == ' ':
            x += widths[0]
            continue
        i = index[c]
        for y, row in enumerate(bitmaps[i]):
            for dx, v in enumerate(row):
                if v:
                    canvas.putpixel((x + dx, y0 + y), ink)
        x += widths[i + 1]
    return x - x0


def preview(path, scale, font8, font16):
    from PIL import Image
    paper, ink, dark, light = (0xef, 0xe6, 0xd2), (0x1b, 0x1f, 0x2a), (0x1b, 0x1f, 0x2a), (0xef, 0xe6, 0xd2)
    W, H = 240, 400
    im = Image.new('RGB', (W, H), paper)
    y = 2
    lines8 = [
        "Capture la pierre blanche marquée.",
        "En 1 coup, sans annuler.",
        "Do it in 1 move, no undo.",
        "White has one breath left at E4.",
        "Où ça ? À l’île, être prêt… « oui »",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "abcdefghijklmnopqrstuvwxyz",
        "0123456789 ★★☆ ▶ 7.5 B 4 ▲ · O 0",
        "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
        "ÀÂÇÉÈÊËÎÏÔÙÛÜŒ àâçéèêëîïôùûüœ",
        "9×9 · CH.2 CAPTURE ▼ A ◀ ▶",
        "← ↑ → ↓ · MUSIC ▮▮▮▮▮▮▯▯ · ©2026",
    ]
    for s in lines8:
        draw_text(im, font8, 4, y, s, ink)
        y += 10
    y += 4
    for s in ["MISSION RÉUSSIE", "DERNIER SOUFFLE", "MOKU 0123456789", "FAUX ŒIL ! ATARI?",
              "ÀÂÇÉÈÊËÎÏÔÙÛÜŒ", "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ",
              "+-.:'\",/()&★☆▶◀▲▼×…«»", "←↑→↓ ▮▯ ©2026"]:
        draw_text(im, font16, 4, y, s, ink)
        y += 18
    # dark HUD strip
    for yy in range(y, H):
        for xx in range(W):
            im.putpixel((xx, yy), dark)
    y += 4
    for s in ["TO PLAY  CAPT B 2 · W 0", "MOVE 14  KOMI 7.5  ★★☆"]:
        draw_text(im, font8, 4, y, s, light)
        y += 10
    draw_text(im, font16, 4, y, "MISSION CLEAR", light)
    im = im.crop((0, 0, W, min(H, y + 20)))
    im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    im.save(path)


def emit(path, content, check_only):
    """Writes content (bytes or str) to path, or compares it when check_only. Returns True if equal."""
    data = content.encode('utf-8') if isinstance(content, str) else content

    if check_only:
        try:
            with open(path, 'rb') as f:
                current = f.read()
        except FileNotFoundError:
            print('MISSING  ' + path)
            return False

        if current != data:
            print('STALE    ' + path)
            return False

        print('ok       ' + path)
        return True

    with open(path, 'wb') as f:
        f.write(data)

    return True


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--preview', metavar='PNG', help='render sample text to PNG instead of writing assets')
    ap.add_argument('--scale', type=int, default=3, help='preview zoom factor (default 3)')
    ap.add_argument('--check', action='store_true',
                    help='do not write: exit 1 if the assets on disk differ from the generator')
    ap.add_argument('--root', default=root, help='project root (default: parent of tools/)')
    args = ap.parse_args()

    font8 = build_font(F8, 8, F8_SPACE, F8_FIXED_WIDTH)
    font16 = build_font(F16, 16, F16_SPACE, F16_FIXED_WIDTH)

    if args.preview:
        preview(args.preview, args.scale, font8, font16)
        print('preview written to', args.preview)
        return 0

    gfx = os.path.join(args.root, 'graphics')
    inc = os.path.join(args.root, 'include')

    if not args.check:
        os.makedirs(gfx, exist_ok=True)
        os.makedirs(inc, exist_ok=True)

    chars, bitmaps8, widths8 = font8
    _, bitmaps16, widths16 = font16
    outputs = [
        (os.path.join(gfx, 'font_8.bmp'), build_bmp4(bitmaps8, 8)),
        (os.path.join(gfx, 'font_8.json'), build_json(8)),
        (os.path.join(gfx, 'font_16.bmp'), build_bmp4(bitmaps16, 16)),
        (os.path.join(gfx, 'font_16.json'), build_json(16)),
        (os.path.join(inc, 'moku_fonts.h'), build_header(chars, widths8, widths16)),
        (os.path.join(inc, 'moku_font_widths.h'), build_widths_header(chars, widths8, widths16)),
    ]
    ok = True

    for path, content in outputs:
        ok &= emit(path, content, args.check)

    if args.check:
        if not ok:
            print('font assets are out of date: run python3 tools/gen_font.py')
            return 1

        print('font assets are up to date')
        return 0

    print('font_8 : %d glyphs, widths %s' % (len(bitmaps8), sorted(set(widths8))))
    print('font_16: %d glyphs, widths %s' % (len(bitmaps16), sorted(set(widths16))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
