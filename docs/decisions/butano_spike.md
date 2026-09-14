# Butano skeleton, board renderer and ROM test infrastructure

Area: the Butano skeleton. Files: `src/game/main.cpp`, `scene.h/.cpp`, `settings.h/.cpp`,
`save.cpp`, `test_iface.cpp` + `test_iface_ext.h`, `render/board_layer.h/.cpp`,
`scenes/title_scene.*`, `scenes/board_demo_scene.*`, `tests/rom/run_all.sh`,
`tests/rom/scripts/0*.txt`.

Everything below is measured on the real ROM with `tests/rom/harness/mokurun`, not estimated.

---

## 1. The frame

```cpp
int main() {
    bn::core::init();
    bn::bg_palettes::set_transparent_color(...);   // backdrop: only seen where no bg covers
    test_iface::init();
    settings::init();                              // loads SRAM
    scene::init(SceneId::TITLE);
    while(true) { scene::run_frame(); }
}
```

`scene::run_frame()` is the only place that calls `bn::core::update()`:

```
test_iface::begin_frame()   heartbeat++, frame++, max_missed_frames, cpu_max_permille
test_iface::poll()          runs at most one harness command
apply pending scene switch
current_scene->update()     <- your scene code
apply pending scene switch
test_iface::end_frame()     mirrors settings + current slot into g_moku_test
bn::core::update()
```

**A scene's `update()` must never call `bn::core::update()`.** Anything that waits (a fade, a
"press A" prompt, an AI think) is a state machine spread over frames, not a nested loop.

## 2. Adding a scene

Derive, write a factory, register it with a static object. No file of this area has to change:

```cpp
class options_scene : public scene::Scene {
public:
    void update() override;                                   // one frame
    [[nodiscard]] SceneId id() const override { return SceneId::OPTIONS; }
    bool handle_test_command(uint32_t cmd, uint32_t arg, uint32_t& result) override;   // optional
};

namespace {
scene::Scene* factory(SceneId id, uint32_t arg) {
    if(id == SceneId::OPTIONS) { return new options_scene(arg); }
    return nullptr;                                           // not ours
}
scene::FactoryRegistrar registrar(factory);                   // runs before main()
}
```

* `scene::request(id, arg)` switches at the end of the current frame; `scene::switch_now()` is for
  the command dispatcher only.
* The old scene is **destroyed before** the new one is built, so the two never hold VRAM at the
  same time. Own every `bn::*_ptr` as a member and Butano frees it for you.
* If no factory owns the requested id, the previous scene is rebuilt and the switch reports
  `TRES_REFUSED`. That is how the title survived before the other scenes existed.
* Up to 8 factories; they are tried newest-registered-first, but the order of static
  initialisation between translation units is not defined, so **two factories must never claim the
  same id**. (`board_demo_scene` therefore answers `SceneId::BOOT`, not `PLAY`.)
* The manager publishes `g_moku_test.scene` and clears `scene_sub` on entry. Scenes fill in
  `scene_sub`, `menu_index`, `cursor_x/y`, ... themselves.

**Scene construction costs about 13-14 ms** (Butano decompressing bg tiles, allocating sprite
tiles, generating text sprites). That is most of a frame: do not also do heavy work on the first
`update()` after construction. `g_moku_test.reserved[12..13]` holds the tick count of the last
scene install (262144 ticks = 1 s) if you need to check yours.

## 3. Harness commands (`test_iface`)

`src/game/test_iface.h` is the frozen memory layout; `src/game/test_iface_ext.h` is the runtime
API. A command is offered to, in order:

1. the current scene (`Scene::handle_test_command`),
2. subsystem handlers (`test_iface::add_handler(fn, ctx)`, max 4 — for audio, effects, AI),
3. the built-in defaults.

A handler returns `true` when it owns the command and sets `result` to a `TRES_*` code
(`1` OK, `2` unknown command, `3` bad argument, `4` refused in this state). The dispatcher then
writes `cmd_result` and clears `cmd`, so a script does:

```
write32 $g_moku_test+76 <arg>      # cmd_arg
write32 $g_moku_test+72 <cmd>      # cmd
wait32  $g_moku_test+72 0 20       # the ROM cleared it
assert32 $g_moku_test+80 1         # TRES_OK
```

Everything a command changes is visible **in the same frame** the command is cleared (the settings
mirror runs at the end of the frame, after the scene update).

Built in here: `TCMD_RESET_COUNTERS`, `TCMD_SET_LANGUAGE/SKIN/HELPER/EFFECTS/CONFIRM`,
`TCMD_SET_MUSIC_SFX`, `TCMD_SELECT_SLOT`, `TCMD_UNLOCK_ALL` (sets every unlock bit),
`TCMD_SET_SEED` (stores into `rng_seed`), `TCMD_WIPE_SRAM`, `TCMD_SAVE_NOW`, `TCMD_GOTO_TITLE`,
`TCMD_GOTO_SCENE`. Any of them can be overridden by a scene or subsystem handler — e.g. audio
claims `TCMD_SET_MUSIC_SFX` to also change the mixer, and the campaign claims `TCMD_UNLOCK_ALL`
when a test needs ranks as well.

Not implemented here (they belong to the scene that owns the state): `TCMD_LOAD_MISSION`,
`TCMD_START_SANDBOX`, `TCMD_AI_FINISH_NOW`, `TCMD_SKIP_EFFECTS`, `TCMD_PLAY_POINT`, `TCMD_PASS`.
Register a subsystem handler for those and call `scene::switch_now(SceneId::PLAY, arg)` from it.

`reserved[16]` is used for renderer/loop diagnostics, not for game state:

| bytes | meaning |
|---|---|
| 0..3 | ticks of the last `board_layer::flush()` (published by the scene owning the board) |
| 4..5 | tiles uploaded by that flush |
| 12..13 | ticks of the last scene install |

## 4. Settings and SRAM

`settings::file()` is **the** `save::SaveFile` of the game (in `.ewram`). Read it anywhere; change
it through the setters (`settings::set_skin(...)`, ...) or write it directly and call
`settings::touch()`. `settings::revision()` increments on every change — a scene that caches
something derived from the options re-reads it when the revision moves (that is how a skin or
language change from the options screen, or from a harness command, reaches a live scene):

```cpp
if(_revision != settings::revision()) { _revision = settings::revision(); rebuild(); }
```

`settings::store()` writes SRAM (CRC32 over everything after the crc field), `settings::wipe()`
erases it and restores the defaults. `save::load()` falls back to the defaults on a bad magic,
version, size or CRC, so a corrupt cartridge never loads garbage.

The whole save file is 1268 bytes; `wipe()` only clears that region, not all 32 KB (zeroing 32 KB
byte by byte would cost a frame).

## 5. `render::board_layer` — the board renderer

One 8bpp regular background used as a software framebuffer over the left 144x160 px of the screen
(`screen (0,0)-(143,159)`, exactly the board column of mockup 1d), 18x20 tiles plus 4 margin tiles,
written into an EWRAM shadow buffer and uploaded tile by tile when dirty.

### Geometry

| size | cell | stone art | grid x | grid y | margin |
|---|---|---|---|---|---|
| 7x7 | 16 | 16 px | 24..120 | 32..128 | 16 px left/right, 24 px top/bottom |
| 9x9 | 16 | 16 px | 8..136 | 16..144 | 8 px / 8 px — **matches mockup 1d exactly** |
| 13x13 | 11 | 11 px | 6..138 | 14..146 | 6 px / 8 px |
| 19x19 | **7** | 8 px | 9..135 | 17..143 | 9 px / 8 px |

**19x19 uses a 7 px cell, not the 8 px the plan mentions.** 19 lines at 8 px need 145 px and the
board column is 144 px wide (the HUD panel starts at x=144), so an 8 px grid would push the last
column of stones under the HUD. The 8 px stone art has a transparent first row and column, i.e. a
7 px ink disc, so at a 7 px pitch the stones exactly fill their cells — the board looks like a real
19x19 board, only tighter. `stones[2]` (the "8 px" set) is still the art used.

`cell_to_screen(x, y)` returns the screen pixel of an intersection (offset included). A sprite goes
there with `sprite.set_position(p.x() - 120, p.y() - 80)`.

### API

```cpp
render::board_layer board;                       // 9x9, CLASSIC, hidden until first drawn
board.set_skin(skins::skin(skins::Skin::PUP));   // palette + textures + full repaint
board.set_board_size(19);                        // clears every cell + full repaint
board.set_cell(x, y, render::CELL_BLACK, render::CELL_LAST | render::CELL_MISSION);
board.set_offset(dx, dy);                        // screen shake, wraps into the margin texture
board.flush();                                   // once per frame, after the set_cell() calls
```

Flags: `CELL_LAST`, `CELL_ATARI` (flash variant of the stone), `CELL_GHOST` (placement preview),
`CELL_DEAD` (cross), `CELL_TERR_BLACK`, `CELL_TERR_WHITE`, `CELL_MISSION` (objective ring). They
combine; the colour (`CELL_EMPTY/BLACK/WHITE`) picks the stone. Territory and mission marks draw on
empty points too.

Coordinates (A..T without I along the top, `n`..1 down the left) are **drawn by the layer** in a
3x5 pixel font in the skin's line colour. Sprites were the alternative; 19+19 labels would eat a
third of the 128 hardware sprites, and the labels never move. The 13x13 board only has a 6 px left
margin, so the '1' of a two digit row number is a 2 px stem that hangs one pixel off the left edge.

### Cost and the rules that come from it

| operation | cost |
|---|---|
| `flush()` with nothing dirty | 0.18 ms |
| one changed cell (stone placed/removed, marker) | 0.6 ms |
| one band of a full repaint (4 tile rows) | 3.9-5.1 ms |
| a full repaint (size or skin change) | 5 bands over 6 frames, ~24 ms of work in total |
| scene install of the board demo | 14 ms |

* A **full repaint is spread over frames**: `redraw_all()` paints nothing itself, the next
  `flush()` paints nothing either (the frame that asked for it is always a busy one), then four to
  five flushes paint four tile rows each, top to bottom. The board is therefore complete about six
  frames (100 ms) after a size or skin change, which is invisible behind a scene transition.
* The very first repaint after construction keeps the background **hidden** until it is complete,
  so nobody ever sees the uninitialised VRAM tiles.
* More than 12 changed cells in one frame are promoted to a full repaint (cheaper and smoother than
  a dozen separate regions). A capture of 20 stones therefore costs one band, not 12 ms.
* Worst case per frame is one band, ~5 ms. The rest of the frame belongs to the AI and the effects.

### Constraints on the rest of the game

1. **One board_layer at a time** (it owns 25.3 KB of EWRAM statics). Constructing a second one
   while the first lives asserts.
2. **It owns Butano's only 8bpp background palette.** Butano keeps exactly one (always at colour
   slot 0), so every other background must be 4bpp — all current `graphics/*.json` already are.
   Every skin must declare the same `palette_size` (they all use 32), otherwise `set_colors()`
   asserts.
3. **Keep it the backmost background (priority 3, the default).** The map cells outside the board
   point at margin tiles so that a shake never exposes the backdrop; those tiles cover the HUD
   column, and the HUD background (priority < 3) is what hides them.
4. It never touches sprites. Cursor, ghost preview, banners, particles and text are the scene's.

## 6. Butano and toolchain facts that cost time to find

* **`bn::sprite_text_generator::generate_top_left(x, y, ...)` takes screen pixels** (0,0 = top
  left of the screen), unlike every other Butano position, which is centre relative
  (`screen - (120, 80)`). Passing centre-relative coordinates puts the text off screen with no
  error at all.
* **Plain `.bss` lands in IWRAM** with the devkitARM GBA linker script (32 KB total). Any buffer
  over a few hundred bytes needs `__attribute__((section(".sbss")))` (zero-initialised EWRAM) or
  `.ewram` (EWRAM loaded from ROM). `.sbss` is NOLOAD: a variable with non-zero initialisers put
  there would silently lose them — `settings::g_file` is in `.ewram` for exactly that reason.
* **`regular_bg_tiles_ptr::allocate(count, BPP_8)` counts 32-byte units**, not 8bpp tiles: an
  8bpp tile is two of them, and `count` must be even. Allocate with `allow_offset == false` so the
  tile indices written into an allocated map are absolute.
* An **allocated map is written as raw half words** (`tile_index | palette << 12 | flips`).
  Building 1024 `regular_bg_map_cell_info` objects instead costs 3 ms.
* **Integer `%` is a software divide on the ARM7** (tens of cycles). Two of them per pixel in the
  texture builder cost 7 ms; wrapping with subtractions made it 0.3 ms.
* Writing pixels through a recomputed address (`s_row[y] + ((x >> 3) << 6) + (x & 7)`) costs ~50
  cycles per pixel in ROM code over EWRAM. Walking with an incremental cursor (and writing whole
  8 px tile rows as two words) is ~5. That one change took a 19x19 repaint from 28 ms to 5 ms.
* `bn::core::last_cpu_usage()` is a fraction of one frame: `> 1.0` means the frame was missed.
  `g_moku_test.cpu_max_permille` is that value in permille since the last `TCMD_RESET_COUNTERS`.

## 7. Memory and ROM (full build, 2026-09-13)

```
ROM            619 040 B   (.text 191 KB + .rodata 401 KB)
IWRAM           13 264 B / 32 768   (.iwram code 9 296, .bss 3 816, .data 152)
EWRAM          237 264 B / 262 144  (.ewram 1 496 + .sbss 235 768)  -> ~24 KB left for the heap
  ai.o 104.9 KB · board_layer.o 25.3 KB · missions.o 21.8 · score.o 18.2 · levels.o 11.1 ·
  game.o 10.7 · Butano managers ~31 KB
board VRAM     23 296 B of bg tiles (364 8bpp tiles) + 2 KB map = 12 of the 32 bg blocks
```

`board_layer` statics: 23.3 KB shadow buffer (tile format, so a flush is a memcpy), 1.3 KB of
texture tiles, 1.1 KB of cell state. **EWRAM is 90 % used**: anything new and large has to come out
of someone's existing budget, and `new` (Butano's EWRAM heap) has ~24 KB.

## 8. ROM tests

```
make -j2 && make dist          # dist writes dist/moku.gba (see the note below)
tests/rom/run_all.sh           # every tests/rom/scripts/*.txt, PASS/FAIL per script
tests/rom/run_all.sh 02 03     # only scripts matching those substrings
ROM=... SYM=... HARNESS=... tests/rom/run_all.sh
```

`run_all.sh` builds nothing. It regenerates `build/moku.sym` from `moku.elf` when it is missing or
stale, runs each script from the repo root (so `symbols build/moku.sym` resolves), writes
`tests/rom/out/<script>.log` plus the screenshots, prints one PASS/FAIL line per script and exits
non-zero if any failed.

Scripts owned here:

| script | what it proves |
|---|---|
| `00_boot.txt` | heartbeat moves, `scene == TITLE` in under 120 frames, default options, menu up/down, no missed frame |
| `01_save_roundtrip.txt` | wipe -> set FR -> save -> reset keeps FR; wipe -> reset restores EN; `sram_save`/`sram_load` round trip |
| `02_board_demo.txt` | 9/13/19/7 boards, every cell flag, incremental edit, three skins, a shake, `max_missed_frames == 0` |
| `03_soak_smoke.txt` | 3000 random frames over menus and the board renderer, heartbeat still moving, no missed frame |

Writing a script: offsets are `$g_moku_test+OFFSET` from `src/game/test_iface.h` (scene 8,
scene_sub 9, language 10, board_size 15, cursor 18/19, max_missed_frames 38, cmd 72, cmd_arg 76,
cmd_result 80, cpu_max_permille 84, menu_index 126, frame 132). Give the ROM ~20 frames after boot
before reading anything.

`board_demo_scene` (the renderer proof) is reached with `TCMD_GOTO_SCENE` **arg 0**: it answers
`SceneId::BOOT`, which no real scene uses. D-pad moves the cursor, A places/removes a stone, L/R
change the board size, SELECT shakes, B returns to the title.

## 9. The EWRAM heap is nearly full (blocking the play scene)

Scenes are heap objects (`new` in the factory, `bn::unique_ptr` in the manager). Butano's heap is
whatever EWRAM is left after the statics:

```
__eheap_start = 0x02039ED0  ->  24 880 bytes of heap
```

`src/game/scenes/play_scene.cpp` asks `new` for **32 728 bytes**, so entering PLAY currently stops
the ROM with `ERROR in bn_memory.cpp.h operator new::16 - Allocation failed. Size in bytes: 32728`.
A scene object that big has to move its buffers into `.sbss` statics (the way `board_layer` does:
25.3 KB of statics, a 200 byte scene object), or the static EWRAM budget has to come down
(`ai.o` alone holds 104.9 KB). Keep scene objects to a few hundred bytes.

## 10. Two bugs fixed outside this area (both were blocking)

* `tests/rom/harness/mokurun.c` now includes `<mgba/flags.h>` first. `struct mCore` has
  `#ifdef USE_DEBUGGERS` members, and the Debian library is built with them: without the flags
  header the harness saw a shorter struct and called the wrong function pointer for
  `savedataClone`, so **`sram_save` and `sram_load` never worked** (`savedataClone` returned
  garbage and a null pointer). Rebuild with `make -C tests/rom/harness`.
* `make dist` still fails: the recipe reads `$(BUILD)/$(TARGET).elf` but Butano links `moku.elf`
  into the project root, so the `nm` step errors out after copying the ROM (the Makefile
  needs `$(TARGET).elf`). `run_all.sh` works around it by regenerating
  `build/moku.sym` itself.
