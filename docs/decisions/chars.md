# Character & screen art (`tools/gen_chars.py`)

Area: character and screen art. Files: `tools/gen_chars.py` and the `graphics/*.bmp|.json` listed
below (generated - **edit the generator, never the BMPs**).

```
python3 tools/gen_chars.py                 # regenerate graphics/
python3 tools/gen_chars.py --check         # exit 1 if the committed assets are stale (writes nothing)
python3 tools/gen_chars.py --sheet o.png   # PIL contact sheet of every frame of every asset
```

Everything is drawn by code with PIL primitives (ellipse / polygon / line / round-capped brush
strokes) into palette-index canvases, then written as uncompressed indexed BMPs. No randomness, so a
re-run is byte-identical. All designs are original: INDI, REX, SEN and the five opponents were
constructed from scratch here, nothing traced or copied.

## Sprite items (`bn::sprite_items::`)

| item | frame | frames | index meaning |
|---|---|---|---|
| `helper_sen`, `helper_indi`, `helper_rex` | 64×64 | 6 | 0 portrait (48×48 art), 1 idle1, 2 idle2, 3 cheer, 4 sad, 5 think (poses are 56×56 art) |
| `opponents` | 64×64 | 5 | 0 pebble, 1 sprout, 2 koan, 3 ember, 4 tengen (40×40 art) |
| `map_node` | 16×16 | 8 | 0/1 cleared (blink), 2/3 current, 4/5 locked, 6/7 selection bracket |
| `rank_stamp` | 64×64 | 4 | 0 S, 1 A, 2 B, 3 C |
| `star` | 8×8 | 3 | 0 filled, 1 empty, 2 burst |
| `menu_cursor` | 8×8 | 4 | 0/1 ▶ (1 px bob), 2 ▼, 3 ▲ |
| `banner_plate` | 64×32 | 6 | skin×2 + style; skin 0 classic / 1 pup / 2 dino, style 0 straight / 1 slanted |
| `egg_crack` | 16×16 | 4 | 0..2 crack stages, 3 shatter burst |
| `speech_bubble` | 32×32 | 9 | skin×3 + part; part 0 left cap, 1 middle, 2 right cap with tail |
| `title_logo` | 64×64 | 2 | 0 "MO", 1 "KU" (+ the vermilion seal) |

Art smaller than the frame is **centred** in it, so a sprite placed at the centre of the box it
should fill is always right: a 48×48 portrait in the 48×48 dialogue well at screen (0,112)-(47,159)
is `create_sprite(24 - 120, 136 - 80, 0)`; a 56×56 pose is centred the same way.

All eight UI sheets (`map_node` … `title_logo`) share one identical 16-colour palette, and Butano
hashes 4bpp palettes (`palettes_bank::find_bpp_4`), so they occupy **one** hardware sprite palette
between them. The three helpers and `opponents` carry one palette each.

### Banner recipe (the mockups' full-width plate)
Four plates cover the 240 px screen. Straight: `create_sprite(-96 + i * 64, y, skin * 2)`.
Slanted (the −3° capture banner): `create_sprite(-96 + i * 64, y - 4 * i, skin * 2 + 1)` — the art
slopes 4 px per 64 px, so the plate edges line up across the seams. Classic is the sober flat
vermilion bar with a 2 px ink drop shadow (mockup 1e), pup is orange with a white keyline and a 3 px
shadow, dino has the jagged lava top edge.

### Speech bubble recipe
`left cap + n × middle + right cap`, 32 px apart; the caps' inner edges carry no outline, so the
parts join seamlessly. Body is rows 0..23 of the frame, the tail hangs from the right cap
(rows 24..29). Skin 0 paper, 1 white, 2 dino cream.

## Regular backgrounds (`bn::regular_bg_items::`)

| item | size | maps | content |
|---|---|---|---|
| `bg_title` | 256×256 | 1 | ink-wash mountains, lake, pale sun, the two title stones, the dark menu panel (rows 96..159) |
| `bg_map` | 512×256 | 1 | campaign landscape: hills, river + bridge, pines, torii, pagoda, the dashed path |
| `bg_hud_classic` / `_pup` / `_dino` | 256×256 | 1 | play scene HUD panel, **x 144..239 only**; x < 144 is colour 0 so the board bg shows through |
| `bg_panels` | 256×256 | 4 | 0 dialogue box (rows 112..159), 1 briefing box (+ 48×48 hatched portrait well), 2 ink bar (rows 128..159), 3 paper header bar (rows 0..17) |

**Positioning.** Butano centres a bg, so at position (0,0) a 256-wide map shows its pixel 8 at screen
x 0. Every background here is pre-rolled by `(w/2 - 120, h/2 - 80)`, which cancels that exactly:

* `item.create_bg(0, 0)` draws the art **at screen (0,0)** — no offset arithmetic in the scenes.
* `item.create_bg(-s, 0)` scrolls right by `s` px (the campaign map uses s = 0..272).
* `bg_panels.create_bg(0, 0, map_index)` picks the panel; e.g. `create_bg(0, -128, 2)` lifts the ink
  bar from the bottom to the top of the screen for a mission-clear header.

**Campaign node anchors** (`NODE_POSITIONS` in the generator) — the path is painted through these
map pixels, so the campaign map scene must place `map_node` sprite centres exactly here:

```
 1 (28,118)   2 (58,104)   3 (86,116)   4 (116,100)   5 (146,110)
 6 (176, 92)  7 (206,104)  8 (232, 84)  9 (258, 96)  10 (286, 78)
11 (314, 90) 12 (342, 72) 13 (368, 84) 14 (394, 66)  15 (418, 78)
16 (440, 60) 17 (462, 72) 18 (480, 54) 19 (496, 66)  20 (504, 40)
```
Screen x = map x + scroll; missions 1-10 sit in the first screenful, 11-20 after scrolling right.

## Decisions

* **CLASSIC HUD panel is ink, not paper.** The first sketch said "CLASSIC paper panel", but
  mockups 1d/1e/2d all draw the classic chrome as ink `#1b1f2a` with paper text, and PLAN rule 4 says
  match the mockups. The panel is therefore ink with a faint paper-fibre weave, a vermilion spine at
  x=144 and a low-contrast ink-wash bamboo in the lower right corner. HUD text stays paper-coloured.
* **Rank stamps are 64×64, not 32×32.** Mockup 1f draws a 72 px seal; 64 is the largest GBA sprite,
  and at 32 the double ring plus letter turned to mush. The stamp is drawn upright and rotated 8°
  with a nearest-neighbour rotate, then speckled, so it reads as a real ink stamp. A "slam" can scale
  it down from 2× with an affine sprite.
* **Speech bubble parts are 32×32, not one 64×24 sprite.** 64×24 is not a legal sprite size, and a
  fixed 64 px bubble cannot hold a line of dialogue; three 32 px parts stretch to any width in 32 px
  steps.
* **The dialogue box is a 256×256 bg, not 256×64.** Butano requires regular bg dimensions to be
  multiples of 256. The unused rows are colour 0 and collapse to one tile, so the whole 4-map
  `bg_panels` item costs 24 tiles + 4 maps.
* **`bg_panels` keeps an uncompressed map.** `bn_regular_bg_map_item::cells_ptr()` asserts on
  `compression != NONE && maps_count > 1` (you cannot index into an RLE stream), which hangs the ROM
  at `create_bg(..., map_index)`. Multi-map items therefore emit `"map_compression": "none"`; tiles
  stay run-length compressed. **Any other multi-map bg must do the same.**
* Each body mass (head, torso, arm, ear, tail, foot) is drawn into its own canvas and given a 1 px
  inline ink contour before being composited, which is what makes the parts read separately at
  native resolution instead of merging into one blob.
* Sprite sizes used: 8×8, 16×16, 32×32, 64×32, 64×64 — all legal GBA shapes, verified by a real
  Butano build.

## ROM cost (grit totals, run-length tiles)

```
sprites  helper_sen/indi/rex 12 320 B each   opponents 10 272   rank_stamp 8 224
         banner_plate 6 176   speech_bubble 4 640   title_logo 4 128   map_node 1 056
         egg_crack 544   menu_cursor 160   star 128                     ~72 KB total
bgs      bg_map 11 668   bg_panels 8 484   bg_title 4 844   bg_hud_pup 2 404
         bg_hud_dino 2 332   bg_hud_classic 1 568                       ~31 KB total
```
A helper pose occupies 64 sprite tiles of VRAM while it is on screen; `bg_map` needs 450 bg tiles
(14.4 KB) plus two screenblocks.

## Verified

Built into a scratch Butano project (a copy of Butano's `template`) with every asset on screen and
screenshotted headless with `tests/rom/harness/mokurun`: title + logo + cursor + stars, campaign map
+ nodes + ink bar, play HUD per skin + banner + dialogue/briefing box + bubble + egg cracks, the six
frames of each helper, the five opponents and the four rank stamps.
