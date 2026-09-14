// Software board renderer: one 8bpp regular background used as a 144x160 framebuffer.
//
// GEOMETRY (matches mockup 1d)
//   The layer covers the left 144 px of the screen, full height: screen (0,0)-(143,159).
//   Grid geometry per board size (all inside the 144 px column, so the HUD never clips a stone):
//
//     size  cell  span  grid x        grid y        (screen pixels of the first/last line)
//       7    16    96   24..120       32..128
//       9    16   128    8..136       16..144       <- exactly the mockup
//      13    11   132    6..138       14..146
//      19     7   126    9..135       17..143
//
//   Stones are the skin's cell-sized sprites drawn centred on the intersection; their ink is one
//   pixel smaller than the sprite (row 0 and column 0 of every stone sprite are transparent), so
//   a 16 px sprite draws a 15 px stone and the 8 px sprite draws a 7 px stone.
//
// COORDINATES are drawn by this layer, not by the scene: column letters (A..T, no I) in a 3x5
//   micro font just above the top grid line, row numbers (n..1, top to bottom) just left of the
//   leftmost line, both in the skin's grid-line colour. Sprites would cost 19+19 of the 128
//   hardware sprites on a 19x19 board, so they are pixels.
//
// USAGE
//   board_layer board;                       // 9x9, CLASSIC
//   board.set_skin(skins::skin(skins::Skin::PUP));
//   board.set_board_size(13);
//   board.set_cell(3, 3, CELL_BLACK, CELL_LAST);
//   board.flush();                           // once per frame, after all set_cell() calls
//
//   set_cell() only records the change; flush() repaints the dirty cells and copies the affected
//   8x8 tiles to VRAM. Calling flush() every frame is free when nothing changed.
//
// COST (measured on hardware timings, see docs/decisions/butano_spike.md)
//   19x19 redraw_all() + flush(): ~5.5 ms; one stone: well under 0.1 ms.
//
// CONSTRAINTS
//   * Only one board_layer may exist at a time (it owns a 22.6 KB EWRAM shadow buffer).
//   * It owns Butano's single 8bpp background palette: every other background must be 4bpp.
//   * Map cells outside the board point at a flat margin tile, so a screen shake
//     (set_offset) never exposes the backdrop. Keep this the backmost background (priority 3).
#pragma once

#include <cstdint>

#include "bn_bg_palette_ptr.h"
#include "bn_point.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"

#include "game/render/skin_data.h"

namespace render {

// board_layer::set_cell() colours.
enum : uint8_t {
    CELL_EMPTY = 0,
    CELL_BLACK = 1,   // == go::BLACK
    CELL_WHITE = 2,   // == go::WHITE
};

// board_layer::set_cell() flags.
enum CellFlag : uint8_t {
    CELL_NONE = 0,
    CELL_LAST = 1 << 0,        // last-move marker on top of the stone
    CELL_ATARI = 1 << 1,       // draw the atari-flash variant of the stone
    CELL_GHOST = 1 << 2,       // draw the stone as a placement preview
    CELL_DEAD = 1 << 3,        // dead-stone cross (marking phase)
    CELL_TERR_BLACK = 1 << 4,  // black territory mark
    CELL_TERR_WHITE = 1 << 5,  // white territory mark
    CELL_MISSION = 1 << 6,     // objective marker (ring), over a stone or an empty point
};

class board_layer {

public:
    static constexpr int WIDTH = 144;
    static constexpr int HEIGHT = 160;
    static constexpr int TILES_WIDTH = WIDTH / 8;     // 18
    static constexpr int TILES_HEIGHT = HEIGHT / 8;   // 20
    static constexpr int MAX_BOARD = 19;

    board_layer();

    ~board_layer();

    board_layer(const board_layer&) = delete;
    board_layer& operator=(const board_layer&) = delete;

    void set_skin(const skins::SkinData& skin);

    // 7, 9, 13 or 19. Clears every cell and repaints.
    void set_board_size(int size);

    [[nodiscard]] int board_size() const { return _size; }
    [[nodiscard]] int cell_size() const { return _cell; }

    void set_cell(int x, int y, uint8_t colour, uint8_t flags);
    void clear_cells();

    [[nodiscard]] uint8_t cell_colour(int x, int y) const;
    [[nodiscard]] uint8_t cell_flags(int x, int y) const;

    // Repaints everything, spread over the next few flush() calls (see board_layer.cpp).
    void redraw_all();

    // True while a full repaint is still in flight.
    [[nodiscard]] bool redrawing() const { return _full_redraw; }

    // Repaints dirty cells and uploads the changed tiles. Call once per frame.
    void flush();

    // Screen shake / scroll, in pixels. The board wraps into its margin tiles, so the edges of the
    // screen never show the backdrop.
    void set_offset(int dx, int dy);
    [[nodiscard]] bn::point offset() const { return bn::point(_offset_x, _offset_y); }

    // Screen pixel of an intersection (includes the current offset): use it to place the cursor,
    // ghost or effect sprites. Butano sprite position = cell_to_screen(x, y) - (120, 80).
    [[nodiscard]] bn::point cell_to_screen(int x, int y) const;

    // Nearest intersection to a screen pixel, clamped to the board.
    [[nodiscard]] bn::point screen_to_cell(int screen_x, int screen_y) const;

    void set_visible(bool visible);
    void set_priority(int priority);

    [[nodiscard]] bn::regular_bg_ptr& bg() { return _bg; }

    // Profiling: tiles uploaded by the last flush(), and how long that flush took
    // (bn::timer ticks, 262144 per second: ticks * 1000 / 262 = microseconds).
    [[nodiscard]] int last_flushed_tiles() const { return _last_flushed; }
    [[nodiscard]] int last_flush_ticks() const { return _last_ticks; }

private:
    bn::regular_bg_tiles_ptr _tiles;
    bn::bg_palette_ptr _palette;
    bn::regular_bg_map_ptr _map;
    bn::regular_bg_ptr _bg;

    const skins::SkinData* _skin;
    int _size = 9;
    int _cell = 16;
    int _grid_x = 8;
    int _grid_y = 16;
    int _stone_set = 0;
    int _offset_x = 0;
    int _offset_y = 0;
    int _last_flushed = 0;
    int _last_ticks = 0;
    int _redraw_row = 0;
    int _dirty_cells = 0;
    bool _full_redraw = true;
    bool _visible = true;
    bool _drawn = false;
    bool _margin_texture = false;

    void _rebuild_geometry();
    void _paint_region(int tx0, int ty0, int tx1, int ty1);
    void _upload();
};

}  // namespace render
