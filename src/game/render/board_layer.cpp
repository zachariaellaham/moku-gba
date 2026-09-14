#include "game/render/board_layer.h"

#include "bn_assert.h"
#include "bn_bg_palette_item.h"
#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_memory.h"
#include "bn_regular_bg_map_cell.h"
#include "bn_size.h"
#include "bn_span.h"
#include "bn_timer.h"

namespace render {

namespace {

// Plain .bss lands in the 32 KB of IWRAM with the devkitARM GBA linker script; .sbss is the
// zero-initialised EWRAM section.
#define MOKU_EWRAM_BSS __attribute__((section(".sbss")))

constexpr int W = board_layer::WIDTH;
constexpr int H = board_layer::HEIGHT;
constexpr int TW = board_layer::TILES_WIDTH;      // 18
constexpr int TH = board_layer::TILES_HEIGHT;     // 20
constexpr int BOARD_TILES = TW * TH;              // 360
constexpr int MARGIN_TILE = BOARD_TILES;          // 360..363: the four tiles outside the board
constexpr int TILE_COUNT = BOARD_TILES + 4;       // 364 (BPP_8 allocates in 32 byte units: even)
constexpr int MAX_CELLS = board_layer::MAX_BOARD * board_layer::MAX_BOARD;
constexpr int MAP_CELLS = 32;

// Tile rows repainted per flush() while a full redraw is in flight: the 20 rows take five flushes
// (plus the deferred first one), which keeps the worst frame near 5 ms.
constexpr int ROWS_PER_FLUSH = 4;

// More changed cells than this in one frame: repaint the whole layer instead (spread over frames).
constexpr int PROMOTE_LIMIT = 12;

// 8bpp tile: 8 rows of 8 palette indices, rows contiguous.
MOKU_EWRAM_BSS alignas(4) uint8_t s_pixels[TILE_COUNT][64];
MOKU_EWRAM_BSS alignas(4) uint8_t s_board_tex[16][64];    // 4x4 tiles of the 32x32 board texture
MOKU_EWRAM_BSS alignas(4) uint8_t s_margin_tex[4][64];    // 2x2 tiles of the 16x16 margin texture
MOKU_EWRAM_BSS uint32_t s_dirty[TH];                      // one bit per tile column
MOKU_EWRAM_BSS uint8_t s_colour[MAX_CELLS];
MOKU_EWRAM_BSS uint8_t s_flags[MAX_CELLS];
MOKU_EWRAM_BSS uint8_t s_cell_dirty[MAX_CELLS];
MOKU_EWRAM_BSS alignas(4) bn::color s_colors[64];

bool s_instance;
bool s_margin_dirty;

// Bytes from one tile row of the layer to the next.
constexpr int ROW_STRIDE = TW * 64;

// Address of pixel (x, y) in the tile-format shadow buffer. Everything below walks the buffer with
// an incremental cursor instead of calling this per pixel: a byte write plus a recomputed address
// is ~50 cycles of EWRAM and ROM-code traffic, an incremented cursor is ~5.
[[nodiscard]] inline uint8_t* px_addr(int x, int y) {
    return &s_pixels[0][0] + (y >> 3) * ROW_STRIDE + ((x >> 3) << 6) + ((y & 7) << 3) + (x & 7);
}

// Walks one pixel row of the layer left to right.
struct RowCursor {
    uint8_t* p;
    int left_in_tile;

    RowCursor(int x, int y) : p(px_addr(x, y)), left_in_tile(8 - (x & 7)) {}

    inline void next() {
        ++p;

        if (--left_in_tile == 0) {
            left_in_tile = 8;
            p += 56;   // skip the rest of this 8x8 tile: the next tile's same row is 64 bytes on
        }
    }
};

// Walks one pixel column of the layer top to bottom.
struct ColumnCursor {
    uint8_t* p;
    int left_in_tile;

    ColumnCursor(int x, int y) : p(px_addr(x, y)), left_in_tile(8 - (y & 7)) {}

    inline void next() {
        p += 8;

        if (--left_in_tile == 0) {
            left_in_tile = 8;
            p += ROW_STRIDE - 64;
        }
    }
};

void mark_tiles(int tx0, int ty0, int tx1, int ty1) {
    uint32_t mask = 0;

    for (int tx = tx0; tx <= tx1; ++tx) {
        mask |= 1u << tx;
    }

    for (int ty = ty0; ty <= ty1; ++ty) {
        s_dirty[ty] |= mask;
    }
}

// Horizontal run, whole 8 px tile rows written as two words.
void hspan(int y, int x0, int x1, uint8_t value) {
    if (x1 < x0) {
        return;
    }

    uint8_t* p = px_addr(x0, y);
    unsigned word = unsigned(value) * 0x01010101u;
    int x = x0;

    while (x <= x1) {
        int in_tile = 8 - (x & 7);
        int run = x1 - x + 1;

        if (run > in_tile) {
            run = in_tile;
        }

        if (run == 8) {
            reinterpret_cast<unsigned*>(p)[0] = word;
            reinterpret_cast<unsigned*>(p)[1] = word;
            p += 8;
        } else {
            for (int index = 0; index < run; ++index) {
                *p++ = value;
            }
        }

        x += run;

        if (((x - 1) & 7) == 7) {
            p += 56;   // the run ended on a tile boundary
        }
    }
}

void vspan(int x, int y0, int y1, uint8_t value) {
    if (y1 < y0) {
        return;
    }

    ColumnCursor cursor(x, y0);

    for (int y = y0; y <= y1; ++y) {
        *cursor.p = value;
        cursor.next();
    }
}

// --- 3x5 micro font for the board coordinates ----------------------------------------------------
// One byte per row, bit 2 = leftmost pixel.

struct Glyph {
    uint8_t rows[5];
};

constexpr Glyph DIGITS[10] = {
    {{0b111, 0b101, 0b101, 0b101, 0b111}},   // 0
    {{0b010, 0b110, 0b010, 0b010, 0b111}},   // 1
    {{0b111, 0b001, 0b111, 0b100, 0b111}},   // 2
    {{0b111, 0b001, 0b111, 0b001, 0b111}},   // 3
    {{0b101, 0b101, 0b111, 0b001, 0b001}},   // 4
    {{0b111, 0b100, 0b111, 0b001, 0b111}},   // 5
    {{0b111, 0b100, 0b111, 0b101, 0b111}},   // 6
    {{0b111, 0b001, 0b010, 0b010, 0b010}},   // 7
    {{0b111, 0b101, 0b111, 0b101, 0b111}},   // 8
    {{0b111, 0b101, 0b111, 0b001, 0b111}},   // 9
};

// A B C D E F G H J K L M N O P Q R S T (Go columns: no I)
constexpr Glyph LETTERS[19] = {
    {{0b111, 0b101, 0b111, 0b101, 0b101}},   // A
    {{0b110, 0b101, 0b110, 0b101, 0b110}},   // B
    {{0b111, 0b100, 0b100, 0b100, 0b111}},   // C
    {{0b110, 0b101, 0b101, 0b101, 0b110}},   // D
    {{0b111, 0b100, 0b111, 0b100, 0b111}},   // E
    {{0b111, 0b100, 0b111, 0b100, 0b100}},   // F
    {{0b111, 0b100, 0b101, 0b101, 0b111}},   // G
    {{0b101, 0b101, 0b111, 0b101, 0b101}},   // H
    {{0b001, 0b001, 0b001, 0b101, 0b011}},   // J
    {{0b101, 0b101, 0b110, 0b101, 0b101}},   // K
    {{0b100, 0b100, 0b100, 0b100, 0b111}},   // L
    {{0b101, 0b111, 0b111, 0b101, 0b101}},   // M
    {{0b101, 0b111, 0b101, 0b101, 0b101}},   // N
    {{0b111, 0b101, 0b101, 0b101, 0b111}},   // O
    {{0b111, 0b101, 0b111, 0b100, 0b100}},   // P
    {{0b111, 0b101, 0b101, 0b111, 0b011}},   // Q
    {{0b111, 0b101, 0b111, 0b110, 0b101}},   // R
    {{0b111, 0b100, 0b111, 0b001, 0b111}},   // S
    {{0b111, 0b010, 0b010, 0b010, 0b010}},   // T
};

// 2 px wide '1' for the tens digit of a two digit row number (bit 1 = leftmost).
constexpr uint8_t NARROW_ONE[5] = {0b01, 0b11, 0b01, 0b01, 0b01};

struct Clip {
    int x0, y0, x1, y1;

    [[nodiscard]] bool hits(int rx0, int ry0, int rx1, int ry1) const {
        return rx0 <= x1 && rx1 >= x0 && ry0 <= y1 && ry1 >= y0;
    }
};

void draw_bits(int x, int y, const uint8_t* rows, int width, uint8_t value, const Clip& clip) {
    if (x > clip.x1 || x + width - 1 < clip.x0 || y > clip.y1 || y + 5 <= clip.y0) {
        return;
    }

    for (int row = 0; row < 5; ++row) {
        int py = y + row;

        if (py < clip.y0 || py > clip.y1) {
            continue;
        }

        uint8_t bits = rows[row];
        int first = clip.x0 - x > 0 ? clip.x0 - x : 0;
        int last = clip.x1 - x < width - 1 ? clip.x1 - x : width - 1;
        RowCursor cursor(x + first, py);

        for (int column = first; column <= last; ++column) {
            if ((bits >> (width - 1 - column)) & 1u) {
                *cursor.p = value;
            }

            cursor.next();
        }
    }
}

// Sprite blit: index 0 is transparent. The clip rectangle is resolved once per row.
void blit(int x, int y, const skins::Sprite8& sprite, const Clip& clip) {
    int width = sprite.w;
    int height = sprite.h;

    if (!sprite.px || !width) {
        return;
    }

    int first = clip.x0 - x > 0 ? clip.x0 - x : 0;
    int last = clip.x1 - x < width - 1 ? clip.x1 - x : width - 1;

    if (first > last) {
        return;
    }

    for (int row = 0; row < height; ++row) {
        int py = y + row;

        if (py < clip.y0 || py > clip.y1) {
            continue;
        }

        const uint8_t* source = sprite.px + row * width + first;
        RowCursor cursor(x + first, py);

        for (int column = first; column <= last; ++column) {
            uint8_t value = *source++;

            if (value) {
                *cursor.p = value;
            }

            cursor.next();
        }
    }
}

// Turns a w x h tileable texture into 8x8 tiles. A null texture gives flat `fallback` tiles.
// The source coordinates wrap with subtractions: an integer modulo per pixel is a software divide
// on the ARM7 and costs several milliseconds over the 20 tiles this builds.
void build_texture(uint8_t (*out)[64], int out_tiles_x, int out_tiles_y, const skins::Sprite8& texture,
                   uint8_t fallback) {
    int width = texture.px ? texture.w : 0;
    int height = texture.px ? texture.h : 0;

    for (int ty = 0; ty < out_tiles_y; ++ty) {
        for (int tx = 0; tx < out_tiles_x; ++tx) {
            uint8_t* tile = out[ty * out_tiles_x + tx];

            if (!width || !height) {
                for (int index = 0; index < 64; ++index) {
                    tile[index] = fallback;
                }

                continue;
            }

            int source_y = ty * 8;

            while (source_y >= height) {
                source_y -= height;
            }

            int first_x = tx * 8;

            while (first_x >= width) {
                first_x -= width;
            }

            for (int row = 0; row < 8; ++row) {
                const uint8_t* source = texture.px + source_y * width;
                int source_x = first_x;

                for (int column = 0; column < 8; ++column) {
                    uint8_t value = source[source_x];
                    tile[row * 8 + column] = value ? value : fallback;

                    if (++source_x == width) {
                        source_x = 0;
                    }
                }

                if (++source_y == height) {
                    source_y = 0;
                }
            }
        }
    }
}

[[nodiscard]] int cell_for_size(int size) {
    switch (size) {

    case 7:
    case 9:
        return 16;

    case 13:
        return 11;

    case 19:
        // 19 lines at 8 px need 145 px and the board column is 144 px wide, so the spacing is 7.
        // The 8 px stone art has a transparent first row/column, i.e. a 7 px stone: it fits exactly.
        return 7;

    default:
        return 16;
    }
}

[[nodiscard]] int stone_set_for_size(int size) {
    switch (size) {

    case 13:
        return 1;

    case 19:
        return 2;

    default:
        return 0;
    }
}

// Standard hoshi points, as (x, y) pairs.
constexpr uint8_t HOSHI_7[] = {3, 3};
constexpr uint8_t HOSHI_9[] = {2, 2, 6, 2, 4, 4, 2, 6, 6, 6};
constexpr uint8_t HOSHI_13[] = {3, 3, 9, 3, 6, 6, 3, 9, 9, 9};
constexpr uint8_t HOSHI_19[] = {3, 3, 9, 3, 15, 3, 3, 9, 9, 9, 15, 9, 3, 15, 9, 15, 15, 15};

// First / last grid index whose line falls inside [.., limit] of the clip rectangle.
[[nodiscard]] inline int index_range_start(int clip_low, int origin, int cell) {
    int delta = clip_low - origin;

    if (delta <= 0) {
        return 0;
    }

    return (delta + cell - 1) / cell;
}

[[nodiscard]] inline int index_range_end(int clip_high, int origin, int cell, int size) {
    int delta = clip_high - origin;

    if (delta < 0) {
        return -1;
    }

    int index = delta / cell;
    return index < size - 1 ? index : size - 1;
}

[[nodiscard]] const uint8_t* hoshi_points(int size, int& count) {
    switch (size) {

    case 7:
        count = 1;
        return HOSHI_7;

    case 13:
        count = 5;
        return HOSHI_13;

    case 19:
        count = 9;
        return HOSHI_19;

    default:
        count = 5;
        return HOSHI_9;
    }
}

[[nodiscard]] bn::regular_bg_tiles_ptr create_tiles() {
    // allow_offset == false keeps the tile set 8-block aligned, so the map cell tile indices this
    // class writes are absolute (Butano only fixes up indices when it commits cells itself).
    return bn::regular_bg_tiles_ptr::allocate(TILE_COUNT * 2, bn::bpp_mode::BPP_8, false);
}

[[nodiscard]] bn::bg_palette_ptr create_palette(const skins::SkinData& skin) {
    int count = skin.palette_size;

    if (count < 16) {
        count = 16;
    } else if (count > 64) {
        count = 64;
    }

    count = (count + 15) & ~15;

    for (int index = 0; index < count; ++index) {
        s_colors[index].set_data(index < skin.palette_size ? skin.palette[index] : 0);
    }

    bn::span<const bn::color> span(s_colors, count);
    return bn::bg_palette_ptr::create(bn::bg_palette_item(span, bn::bpp_mode::BPP_8));
}

}  // namespace

board_layer::board_layer() :
    _tiles(create_tiles()),
    _palette(create_palette(skins::skin(skins::Skin::CLASSIC))),
    _map(bn::regular_bg_map_ptr::allocate(bn::size(MAP_CELLS, MAP_CELLS), _tiles, _palette)),
    _bg(bn::regular_bg_ptr::create(_map)),
    _skin(&skins::skin(skins::Skin::CLASSIC)) {
    BN_ASSERT(!s_instance, "Only one board_layer can exist");
    s_instance = true;

    // Map: the board area points at its own tile, everything else at the four margin tiles, so a
    // screen shake wraps into board margin instead of the backdrop.
    // A regular map cell is tile_index | palette_id << 12 | flips; this map only ever uses tile
    // indices with palette 0 and no flips, so the cells are written as plain half words (building
    // 1024 regular_bg_map_cell_info objects instead costs about three milliseconds).
    if (auto cells = _map.vram()) {
        bn::regular_bg_map_cell* data = cells->data();

        for (int cy = 0; cy < MAP_CELLS; ++cy) {
            bn::regular_bg_map_cell* row = data + cy * MAP_CELLS;
            auto margin = bn::regular_bg_map_cell(MARGIN_TILE + (cy & 1) * 2);

            if (cy < TH) {
                int first = cy * TW;

                for (int cx = 0; cx < TW; ++cx) {
                    row[cx] = bn::regular_bg_map_cell(first + cx);
                }

                for (int cx = TW; cx < MAP_CELLS; ++cx) {
                    row[cx] = bn::regular_bg_map_cell(margin + (cx & 1));
                }
            } else {
                for (int cx = 0; cx < MAP_CELLS; ++cx) {
                    row[cx] = bn::regular_bg_map_cell(margin + (cx & 1));
                }
            }
        }
    }

    _bg.set_priority(3);
    _bg.set_top_left_position(0, 0);
    _bg.set_visible(false);   // the allocated VRAM tiles hold garbage until the first full redraw

    _rebuild_geometry();
    clear_cells();   // the cell arrays are shared file statics: a new layer must not inherit them
    set_skin(skins::skin(skins::Skin::CLASSIC));
}

board_layer::~board_layer() {
    s_instance = false;
}

void board_layer::set_skin(const skins::SkinData& skin) {
    _skin = &skin;

    int count = skin.palette_size;

    if (count < 16) {
        count = 16;
    } else if (count > 64) {
        count = 64;
    }

    count = (count + 15) & ~15;

    for (int index = 0; index < count; ++index) {
        s_colors[index].set_data(index < skin.palette_size ? skin.palette[index] : 0);
    }

    // Butano refuses a colour count different from the one the palette was created with, so every
    // skin must declare the same palette_size (they all use 32).
    BN_ASSERT(count == _palette.colors_count(), "Skin palettes must all have the same size");
    _palette.set_colors(bn::span<const bn::color>(s_colors, count));

    build_texture(s_board_tex, 4, 4, skin.board_texture, skin.bg_index);
    _margin_texture = skin.margin_texture.px && skin.margin_texture.w;

    if (_margin_texture) {
        build_texture(s_margin_tex, 2, 2, skin.margin_texture, skin.margin_index);
    } else {
        // No margin decoration: reuse the board texture so the two areas are indistinguishable.
        for (int index = 0; index < 4; ++index) {
            const uint8_t* source = s_board_tex[(index >> 1) * 4 + (index & 1)];

            for (int byte = 0; byte < 64; ++byte) {
                s_margin_tex[index][byte] = source[byte];
            }
        }
    }

    for (int index = 0; index < 4; ++index) {
        for (int byte = 0; byte < 64; ++byte) {
            s_pixels[MARGIN_TILE + index][byte] = s_margin_tex[index][byte];
        }
    }

    s_margin_dirty = true;
    redraw_all();
}

void board_layer::set_board_size(int size) {
    if (size != 7 && size != 9 && size != 13 && size != 19) {
        size = 9;
    }

    _size = size;
    _cell = cell_for_size(size);
    _stone_set = stone_set_for_size(size);
    _rebuild_geometry();
    clear_cells();
    redraw_all();
}

void board_layer::_rebuild_geometry() {
    int span = (_size - 1) * _cell;
    _grid_x = (W - span) / 2;
    _grid_y = (H - span) / 2;
}

void board_layer::set_cell(int x, int y, uint8_t colour, uint8_t flags) {
    if (x < 0 || y < 0 || x >= _size || y >= _size) {
        return;
    }

    int index = y * MAX_BOARD + x;

    if (s_colour[index] == colour && s_flags[index] == flags) {
        return;
    }

    s_colour[index] = colour;
    s_flags[index] = flags;

    if (!s_cell_dirty[index]) {
        s_cell_dirty[index] = 1;
        ++_dirty_cells;
    }
}

void board_layer::clear_cells() {
    for (int index = 0; index < MAX_CELLS; ++index) {
        s_colour[index] = CELL_EMPTY;
        s_flags[index] = CELL_NONE;
        s_cell_dirty[index] = 0;
    }

    redraw_all();
}

uint8_t board_layer::cell_colour(int x, int y) const {
    if (x < 0 || y < 0 || x >= _size || y >= _size) {
        return CELL_EMPTY;
    }

    return s_colour[y * MAX_BOARD + x];
}

uint8_t board_layer::cell_flags(int x, int y) const {
    if (x < 0 || y < 0 || x >= _size || y >= _size) {
        return CELL_NONE;
    }

    return s_flags[y * MAX_BOARD + x];
}

// A full repaint of the 144x160 layer costs several milliseconds, so it is spread over a few
// frames: each flush() paints the next band of tile rows top to bottom. Cell changes made while a
// repaint is in flight are still applied immediately by the incremental path below.
void board_layer::redraw_all() {
    _full_redraw = true;
    _redraw_row = -1;   // painting starts on the flush after this one: see flush()
    _dirty_cells = 0;

    for (int index = 0; index < MAX_CELLS; ++index) {
        s_cell_dirty[index] = 0;
    }
}

void board_layer::set_offset(int dx, int dy) {
    _offset_x = dx;
    _offset_y = dy;
    _bg.set_top_left_position(dx, dy);
}

bn::point board_layer::cell_to_screen(int x, int y) const {
    return bn::point(_grid_x + x * _cell + _offset_x, _grid_y + y * _cell + _offset_y);
}

bn::point board_layer::screen_to_cell(int screen_x, int screen_y) const {
    int x = (screen_x - _offset_x - _grid_x + _cell / 2) / _cell;
    int y = (screen_y - _offset_y - _grid_y + _cell / 2) / _cell;

    if (x < 0) { x = 0; } else if (x >= _size) { x = _size - 1; }
    if (y < 0) { y = 0; } else if (y >= _size) { y = _size - 1; }

    return bn::point(x, y);
}

void board_layer::set_visible(bool visible) {
    _visible = visible;
    _bg.set_visible(visible && _drawn);
}

void board_layer::set_priority(int priority) {
    _bg.set_priority(priority);
}

// Repaints a rectangle of 8x8 tiles from scratch: board or margin texture, grid, hoshi,
// coordinates, then every stone whose art touches the rectangle.
void board_layer::_paint_region(int tx0, int ty0, int tx1, int ty1) {
    if (tx0 < 0) { tx0 = 0; }
    if (ty0 < 0) { ty0 = 0; }
    if (tx1 > TW - 1) { tx1 = TW - 1; }
    if (ty1 > TH - 1) { ty1 = TH - 1; }

    if (tx0 > tx1 || ty0 > ty1) {
        return;
    }

    const skins::SkinData& skin = *_skin;
    const skins::StoneSet& stones = skin.stones[_stone_set];
    int art = stones.cell;
    int art_half = art / 2;

    int last = _grid_x + (_size - 1) * _cell;
    int board_x0 = (_grid_x - art_half) / 8;
    int board_x1 = (last + art_half - 1) / 8;
    int last_y = _grid_y + (_size - 1) * _cell;
    int board_y0 = (_grid_y - art_half) / 8;
    int board_y1 = (last_y + art_half - 1) / 8;

    if (board_x0 < 0) { board_x0 = 0; }
    if (board_y0 < 0) { board_y0 = 0; }
    if (board_x1 > TW - 1) { board_x1 = TW - 1; }
    if (board_y1 > TH - 1) { board_y1 = TH - 1; }

    bool margin_texture = _margin_texture;

    for (int ty = ty0; ty <= ty1; ++ty) {
        bool inside_y = !margin_texture || (ty >= board_y0 && ty <= board_y1);

        for (int tx = tx0; tx <= tx1; ++tx) {
            bool inside = inside_y && (!margin_texture || (tx >= board_x0 && tx <= board_x1));
            const uint8_t* source = inside ? s_board_tex[(ty & 3) * 4 + (tx & 3)]
                                           : s_margin_tex[(ty & 1) * 2 + (tx & 1)];
            const unsigned* from = reinterpret_cast<const unsigned*>(source);
            unsigned* to = reinterpret_cast<unsigned*>(s_pixels[ty * TW + tx]);

            for (int word = 0; word < 16; word += 4) {
                to[word] = from[word];
                to[word + 1] = from[word + 1];
                to[word + 2] = from[word + 2];
                to[word + 3] = from[word + 3];
            }
        }
    }

    Clip clip = {tx0 * 8, ty0 * 8, tx1 * 8 + 7, ty1 * 8 + 7};

    // Grid.
    int grid_last_x = _grid_x + (_size - 1) * _cell;
    int grid_last_y = _grid_y + (_size - 1) * _cell;
    int line_y0 = _grid_y > clip.y0 ? _grid_y : clip.y0;
    int line_y1 = grid_last_y < clip.y1 ? grid_last_y : clip.y1;
    int line_x0 = _grid_x > clip.x0 ? _grid_x : clip.x0;
    int line_x1 = grid_last_x < clip.x1 ? grid_last_x : clip.x1;

    const uint8_t line_index = skin.line_index;

    // Only the grid indices that actually fall inside the clip are visited: a one cell repaint
    // must not walk all 19 lines and all 38 coordinate labels.
    int first_column = index_range_start(clip.x0, _grid_x, _cell);
    int last_column = index_range_end(clip.x1, _grid_x, _cell, _size);
    int first_row = index_range_start(clip.y0, _grid_y, _cell);
    int last_row = index_range_end(clip.y1, _grid_y, _cell, _size);

    for (int index = first_column; index <= last_column; ++index) {
        vspan(_grid_x + index * _cell, line_y0, line_y1, line_index);
    }

    for (int index = first_row; index <= last_row; ++index) {
        hspan(_grid_y + index * _cell, line_x0, line_x1, line_index);
    }

    // Hoshi: 3x3 dots centred on the intersection.
    int hoshi_count = 0;
    const uint8_t* hoshi = hoshi_points(_size, hoshi_count);
    const uint8_t hoshi_index = skin.hoshi_index;

    for (int index = 0; index < hoshi_count; ++index) {
        int cx = _grid_x + hoshi[index * 2] * _cell;
        int cy = _grid_y + hoshi[index * 2 + 1] * _cell;

        if (cx + 1 < clip.x0 || cx - 1 > clip.x1 || cy + 1 < clip.y0 || cy - 1 > clip.y1) {
            continue;
        }

        int x0 = cx - 1 > clip.x0 ? cx - 1 : clip.x0;
        int x1 = cx + 1 < clip.x1 ? cx + 1 : clip.x1;

        for (int row = -1; row <= 1; ++row) {
            int py = cy + row;

            if (py >= clip.y0 && py <= clip.y1) {
                hspan(py, x0, x1, hoshi_index);
            }
        }
    }

    // Coordinates: column letters above the top line, row numbers left of the leftmost line.
    int letter_y = _grid_y - 8;

    if (letter_y < 0) {
        letter_y = 0;
    }

    if (letter_y <= clip.y1 && letter_y + 4 >= clip.y0) {
        // A 3 px glyph is centred on its line, so the range is one pixel wider than the lines.
        int letter_first = index_range_start(clip.x0 - 1, _grid_x, _cell);
        int letter_last = index_range_end(clip.x1 + 1, _grid_x, _cell, _size);

        for (int index = letter_first; index <= letter_last; ++index) {
            draw_bits(_grid_x + index * _cell - 1, letter_y, LETTERS[index].rows, 3, line_index, clip);
        }
    }

    int digit_x = _grid_x - 5;
    int tens_x = _grid_x - 8;

    if (digit_x < 0) { digit_x = 0; }

    // 13x13 only leaves a 6 px margin: the narrow '1' hangs one pixel off the left edge (it keeps
    // its stem) so that the units digit still clears the first grid line.
    if (tens_x < -1) { tens_x = -1; }

    if (tens_x <= clip.x1 && digit_x + 2 >= clip.x0) {
        // A 5 px tall glyph straddles its line by two pixels either side.
        int number_first = index_range_start(clip.y0 - 2, _grid_y, _cell);
        int number_last = index_range_end(clip.y1 + 2, _grid_y, _cell, _size);

        for (int index = number_first; index <= number_last; ++index) {
            int number = _size - index;
            int y = _grid_y + index * _cell - 2;

            if (number >= 10) {
                draw_bits(tens_x, y, NARROW_ONE, 2, line_index, clip);
                draw_bits(tens_x + 3, y, DIGITS[number - 10].rows, 3, line_index, clip);
            } else {
                draw_bits(digit_x, y, DIGITS[number].rows, 3, line_index, clip);
            }
        }
    }

    // Stones and markers, in board order so overlaps are consistent.
    int first_cx = (clip.x0 - _grid_x - art) / _cell;
    int last_cx = (clip.x1 - _grid_x + art) / _cell;
    int first_cy = (clip.y0 - _grid_y - art) / _cell;
    int last_cy = (clip.y1 - _grid_y + art) / _cell;

    if (first_cx < 0) { first_cx = 0; }
    if (first_cy < 0) { first_cy = 0; }
    if (last_cx > _size - 1) { last_cx = _size - 1; }
    if (last_cy > _size - 1) { last_cy = _size - 1; }

    for (int cy = first_cy; cy <= last_cy; ++cy) {
        for (int cx = first_cx; cx <= last_cx; ++cx) {
            int index = cy * MAX_BOARD + cx;
            uint8_t colour = s_colour[index];
            uint8_t flags = s_flags[index];

            if (!colour && !flags) {
                continue;
            }

            int x = _grid_x + cx * _cell - art_half;
            int y = _grid_y + cy * _cell - art_half;

            if (!clip.hits(x, y, x + art - 1, y + art - 1)) {
                continue;
            }

            bool black = colour == CELL_BLACK;

            if (colour) {
                if (flags & CELL_GHOST) {
                    blit(x, y, black ? stones.ghost_black : stones.ghost_white, clip);
                } else if (flags & CELL_ATARI) {
                    blit(x, y, black ? stones.black_atari : stones.white_atari, clip);
                } else {
                    blit(x, y, black ? stones.black : stones.white, clip);
                }

                if (flags & CELL_LAST) {
                    blit(x, y, stones.last_marker, clip);
                }

                if (flags & CELL_DEAD) {
                    blit(x, y, stones.dead_marker, clip);
                }
            }

            if (flags & CELL_TERR_BLACK) {
                blit(x, y, stones.terr_black, clip);
            }

            if (flags & CELL_TERR_WHITE) {
                blit(x, y, stones.terr_white, clip);
            }

            if (flags & CELL_MISSION) {
                blit(x, y, stones.mission_marker, clip);
            }
        }
    }

    mark_tiles(tx0, ty0, tx1, ty1);
}

// One flush() repaints either the next band of a full redraw or the cells changed since the last
// one, never both, and promotes a large batch of changes to a full redraw. The worst case is
// therefore one band (~7 ms), which leaves the rest of the frame to the AI and the effects.
void board_layer::flush() {
    bn::timer timer;

    if (_full_redraw) {
        if (_redraw_row < 0) {
            // redraw_all() is always called from an expensive operation (scene construction, a
            // skin or board size change). Painting the first band on that same frame is what
            // pushes it over 16.7 ms, so the first band waits one frame.
            _redraw_row = 0;
            _upload();
            _last_ticks = timer.elapsed_ticks();
            return;
        }

        int end = _redraw_row + ROWS_PER_FLUSH;

        if (end >= TH) {
            end = TH;
            _full_redraw = false;
        }

        _paint_region(0, _redraw_row, TW - 1, end - 1);
        _redraw_row = end;

        if (!_full_redraw && !_drawn) {
            _drawn = true;   // the first complete picture: safe to show now
            _bg.set_visible(_visible);
        }

        _upload();
        _last_ticks = timer.elapsed_ticks();
        return;
    }

    if (_dirty_cells > PROMOTE_LIMIT) {
        // Cheaper (and smoother) than repainting dozens of separate cell regions.
        redraw_all();
        _paint_region(0, 0, TW - 1, ROWS_PER_FLUSH - 1);
        _redraw_row = ROWS_PER_FLUSH;
        _upload();
        _last_ticks = timer.elapsed_ticks();
        return;
    }

    if (!_dirty_cells) {
        _last_flushed = 0;
        _last_ticks = timer.elapsed_ticks();
        return;
    }

    const skins::StoneSet& stones = _skin->stones[_stone_set];
    int art = stones.cell;
    int art_half = art / 2;

    for (int cy = 0; cy < _size; ++cy) {
        for (int cx = 0; cx < _size; ++cx) {
            int index = cy * MAX_BOARD + cx;

            if (!s_cell_dirty[index]) {
                continue;
            }

            s_cell_dirty[index] = 0;
            --_dirty_cells;

            int x = _grid_x + cx * _cell - art_half;
            int y = _grid_y + cy * _cell - art_half;
            _paint_region(x >> 3, y >> 3, (x + art - 1) >> 3, (y + art - 1) >> 3);
        }
    }

    _upload();
    _last_ticks = timer.elapsed_ticks();
}

void board_layer::_upload() {
    auto vram = _tiles.vram();
    _last_flushed = 0;

    if (!vram) {
        return;
    }

    unsigned* destination = reinterpret_cast<unsigned*>(vram->data());

    if (s_margin_dirty) {
        s_margin_dirty = false;
        bn::memory::copy(*reinterpret_cast<const unsigned*>(s_pixels[MARGIN_TILE]), 4 * 16,
                         *(destination + MARGIN_TILE * 16));
        _last_flushed += 4;
    }

    for (int ty = 0; ty < TH; ++ty) {
        uint32_t mask = s_dirty[ty];

        if (!mask) {
            continue;
        }

        s_dirty[ty] = 0;
        int tx = 0;

        while (tx < TW) {
            if (!((mask >> tx) & 1u)) {
                ++tx;
                continue;
            }

            int start = tx;

            while (tx < TW && ((mask >> tx) & 1u)) {
                ++tx;
            }

            int first = ty * TW + start;
            int count = tx - start;
            bn::memory::copy(*reinterpret_cast<const unsigned*>(s_pixels[first]), count * 16,
                             *(destination + first * 16));
            _last_flushed += count;
        }
    }
}

}  // namespace render
