# Fonts (pixel fonts, `tools/gen_font.py`)

Area: fonts. Files: `tools/gen_font.py`, `graphics/font_8.{bmp,json}`,
`graphics/font_16.{bmp,json}`, `include/moku_fonts.h` (generated — edit the generator, never the
header or the BMPs).

```
python3 tools/gen_font.py                      # regenerate graphics/ + include/
python3 tools/gen_font.py --check              # exit 1 if the committed assets are stale
python3 tools/gen_font.py --preview out.png --scale 4   # sample sheet without a ROM build
```

## What was built

Two original bitmap fonts, drawn glyph by glyph as ASCII art inside the generator (nothing was
downloaded or traced). Both are Butano `bn::sprite_font` objects, 4bpp, ink on palette index 1
only, so the game recolours text by swapping the sprite palette
(`bn::sprite_text_generator::set_palette_item`).

| font | cell | metrics | use |
|---|---|---|---|
| `fonts::moku_font_8` | 8×8, variable width (2–8 px) | caps 6 px (rows 1–6), x‑height 4 (rows 3–6), 1‑px descender (row 7); lowercase accents on rows 0–1 with a gap row; accented caps squeezed to 5 rows with a touching 2‑row accent (the usual 8‑px compromise, baseline unchanged) | dialogue, briefings, HUD, menus |
| `fonts::moku_font_16` | 8×16, variable width (3–8 px) | bold display face, 2‑px strokes, letters 12 px tall (rows 3–14), accents rows 0–1, cedilla/comma/underscore reach row 15; lowercase folds to the capital glyph, so any string renders in either font | banners, titles, big numbers |

Widths are ink width + 1 px side bearing (`space_between_characters` = 0). Digits are tabular
(6 px in font_8, 8 px in font_16) so score columns line up. Space is 3 px / 4 px. Max ink width is
7 px (8 px for `Œ` and `©`); Butano copies whole 8‑px tiles, so nothing may bleed past column 7.

## Character set (141 glyphs + space, identical in both fonts)

ASCII 32..126, plus these UTF‑8 characters, in this order (the order is the tile order in the BMPs
and the index order of `fonts::moku_font_utf8_characters`):

```
À Â Ç É È Ê Ë Î Ï Ô Ù Û Ü Œ à â ç é è ê ë î ï ô ù û ü œ
’ « » … · ★ ☆ ▲ ▼ ▶ ◀ ×        (required)
← ↑ → ↓ ▮ ▯ ©                  (added: the mockups need them)
```

The last seven are a deliberate addition to that list: the mockups use `← →` in the
navigation hints ("← → CHANGE · B BACK"), `▮▯` for the music/SFX bars on the options screen and
`©` on the title screen ("©2026 · SAVE 1/3"). A character outside this set makes Butano's text
generator fail at runtime, so **EN/FR strings must stick to it**. Not supported (and not needed for
French game text): `ä ö ÿ Ÿ æ Æ ± ° “ ” – —`; use `«` `»` for quotes, `’` for the apostrophe,
`…` for the ellipsis and `·` as the separator. Widen the tables in `gen_font.py` if a string really needs more.

## Butano pipeline facts that shaped the files

- `butano/tools/bmp.py` accepts 40‑byte‑header indexed BMPs at 4 or 8 bpp. PIL only writes 8bpp
  `P` BMPs with a 256‑entry palette, so `gen_font.py` writes the 4bpp BMP itself (16‑entry
  palette, bottom‑up rows, 8 px wide). Index 0 (magenta) is the transparent key, index 1 the ink.
- Glyphs are stacked vertically **starting at `!` (33)**: the space has no graphic but owns entry 0
  of the width table, so the table has `1 + 94 + 47 = 142` entries per font and the BMPs hold
  `94 + 47 = 141` glyphs (`bn::sprite_font` asserts on both counts).
- JSON: `{"type": "sprite", "height": 8|16, "bpp_mode": "bpp_4", "compression": "none"}`
  (`sprite_font` asserts on compressed tiles and on 8bpp palettes).
- Both fonts share one `bn::utf8_characters_map` built from the same list.
- ROM cost: font_8 tiles 4 544 bytes, font_16 tiles 9 056 bytes (grit output, no compression).

## Numbers the rest of the game needs

- **Typical French body text in font_8 averages 4.28 px per character → 29 characters fit in the
  128‑px dialogue box** (measured over 16 mission‑style FR sentences: min 25, max 31; all‑caps
  lines ~25). English is the same to within a character. On an 80‑px HUD column: 16–19 characters;
  on 88 px: 17–21; on a full 240‑px screen line: 55.
- Greedy word wrap of every FR/EN mission sentence tried fits in **2 lines at 128 px** (3 lines at
  80 px).
- Recommended line pitch for font_8 body text: **10 px** (8‑px cell + room so a descender never
  touches the next line's accents); 8 px is fine for accent‑free caps HUD labels, 12 px is airy.
  The 48‑px dialogue box holds 4 lines at 10 px.
- font_16 banner widths: `MISSION RÉUSSIE` 107 px, `DERNIER SOUFFLE` 113 px, `MISSION CLEAR` 94 px,
  `FAUX ŒIL ! ATARI?` 113 px, `CAPTURE! +2` 78 px, `ATARI !` 44 px — all fit the 144‑px board area.
- Sprite cost (measured in the validation ROM, Butano packs variable‑width text into 32‑px‑wide
  sprites): a 127‑px font_8 line = **5 sprites**, a 75‑px HUD line = 3, `★★☆` = 1,
  `MISSION RÉUSSIE` in font_16 = 4 (32×16 sprites), `ATARI !` = 2. A 4‑line dialogue box is
  ~20 of the 128 sprites.
- `fonts::text_width(font, "utf-8 literal")` is `constexpr` and returns exactly what
  `bn::sprite_text_generator::width()` returns at runtime — use it for `static_assert`s on fixed
  layouts. The header also exposes `moku_font_8_height/baseline/space_width` (and the 16 versions).

## Validation

Done with the real Butano pipeline, not only the generator preview:

1. `python3 tools/gen_font.py && python3 tools/gen_font.py --check` (assets in sync).
2. Scratch copy of Butano's `template` (`LIBBUTANO := <butano>/butano`) with the two
   BMP/JSON pairs, `moku_fonts.h` and a `main.cpp` rendering four screens: EN/FR body text with a
   128‑px ruler, font_16 banners, HUD text on the dark panel with a vermilion palette swap, and an
   atlas of all 141 glyphs. Built with devkitARM (`make -j2`), run headless with
   `tests/rom/harness/mokurun` (`frames 30`, `screenshot`), and every screenshot was inspected at
   3× zoom. The `static_assert`s in that ROM also pin the metrics above.
3. Fixes made after looking at the screenshots: `@` redrawn (the old one read as a filled box and
   was indistinguishable from `©`), `©` widened to 7 px in font_8 and drawn small (9 rows) in
   font_16 so the ring reads at a glance, plus the seven added glyphs above.

Note for ROM tests: give the ROM **≥ 20 frames** before reading `BN_LOG` output or test‑block
memory — Butano's init has not finished at frame 5.
