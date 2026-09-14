#include "game/scenes/board_demo_scene.h"

#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_display.h"
#include "bn_keypad.h"
#include "bn_sprite_palette_item.h"
#include "bn_string_view.h"

#include "bn_sprite_items_cursor_11.h"
#include "bn_sprite_items_cursor_16.h"
#include "bn_sprite_items_cursor_8.h"

#include "moku_fonts.h"

#include "game/settings.h"

namespace scenes {

namespace {

// Sprite positions are centre relative (screen pixel - 120 / - 80); text generated with
// generate_top_left() is already in screen pixels.
constexpr int SCREEN_X = bn::display::width() / 2;
constexpr int SCREEN_Y = bn::display::height() / 2;

constexpr int SIZES[4] = {7, 9, 13, 19};

alignas(4) constexpr bn::color PAPER_COLORS[16] = {bn::color(0, 0, 0), bn::color(29, 28, 26)};
constexpr bn::sprite_palette_item PAPER_PALETTE(PAPER_COLORS, bn::bpp_mode::BPP_4);

[[nodiscard]] const skins::SkinData& current_skin() {
    return skins::skin(skins::Skin(settings::skin()));
}

scene::Scene* factory(SceneId id, uint32_t arg) {
    (void)arg;

    // SceneId::BOOT is never a real scene, so the renderer demo uses it as its own id: the real
    // play scene owns SceneId::PLAY.
    if (id == SceneId::BOOT) {
        return new board_demo_scene();
    }

    return nullptr;
}

const scene::FactoryRegistrar registrar(factory);

}  // namespace

board_demo_scene::board_demo_scene() :
    _small(fonts::moku_font_8),
    _cursor(bn::sprite_items::cursor_16.create_sprite(0, 0, 0)) {
    _board.set_skin(current_skin());
    _revision = settings::revision();
    _apply_size();
}

void board_demo_scene::_apply_size() {
    int size = SIZES[_size_index];
    _board.set_board_size(size);
    _cursor_x = size / 2;
    _cursor_y = size / 2;
    _fill_demo_pattern();
    _build_texts();
    _update_cursor_sprite();

    g_moku_test.scene_sub = uint8_t(_size_index);
    g_moku_test.board_size = uint8_t(size);
}

// One stone of each colour and one marker of each flag, placed so that every board size shows the
// same picture: this is what the ROM screenshots check.
void board_demo_scene::_fill_demo_pattern() {
    int last = _board.board_size() - 1;

    _board.set_cell(2, 2, render::CELL_BLACK, render::CELL_NONE);
    _board.set_cell(3, 2, render::CELL_BLACK, render::CELL_NONE);
    _board.set_cell(2, 3, render::CELL_BLACK, render::CELL_NONE);

    _board.set_cell(4, 4, render::CELL_WHITE, render::CELL_NONE);
    _board.set_cell(4, 3, render::CELL_WHITE, render::CELL_NONE);
    _board.set_cell(3, 4, render::CELL_WHITE, render::CELL_NONE);

    _board.set_cell(last - 2, 2, render::CELL_BLACK, render::CELL_LAST);
    _board.set_cell(2, last - 2, render::CELL_WHITE, render::CELL_ATARI);
    _board.set_cell(last - 3, last - 3, render::CELL_BLACK, render::CELL_GHOST);
    _board.set_cell(1, last - 1, render::CELL_WHITE, render::CELL_DEAD);

    _board.set_cell(0, 0, render::CELL_EMPTY, render::CELL_TERR_BLACK);
    _board.set_cell(last, 0, render::CELL_EMPTY, render::CELL_TERR_WHITE);
    _board.set_cell(0, last, render::CELL_EMPTY, render::CELL_MISSION);
}

void board_demo_scene::_build_texts() {
    _texts.clear();
    _small.set_left_alignment();
    _small.set_palette_item(PAPER_PALETTE);

    char label[16];
    int size = _board.board_size();
    int length = 0;

    if (size >= 10) {
        label[length++] = char('0' + size / 10);
    }

    label[length++] = char('0' + size % 10);
    label[length++] = 'x';

    if (size >= 10) {
        label[length++] = char('0' + size / 10);
    }

    label[length++] = char('0' + size % 10);
    label[length] = 0;

    _small.generate_top_left(152, 8, bn::string_view(label, length), _texts);
    _small.generate_top_left(152, 24, "L R SIZE", _texts);
    _small.generate_top_left(152, 36, "A STONE", _texts);
    _small.generate_top_left(152, 48, "SEL SHAKE", _texts);
    _small.generate_top_left(152, 60, "B TITLE", _texts);
}

// The three cursor sheets are 6 frames each: skin * 2 + blink phase.
void board_demo_scene::_update_cursor_sprite() {
    bn::point position = _board.cell_to_screen(_cursor_x, _cursor_y);
    int cell = _board.cell_size();
    int frame = settings::skin() * 2 + ((_frame / 16) & 1);
    int art = cell >= 16 ? 16 : (cell >= 11 ? 11 : 8);

    if (art != _cursor_art || frame != _cursor_frame) {
        _cursor_art = art;
        _cursor_frame = frame;

        // set_item() frees the old sprite tiles before allocating the new ones.
        if (art == 16) {
            _cursor.set_item(bn::sprite_items::cursor_16, frame);
        } else if (art == 11) {
            _cursor.set_item(bn::sprite_items::cursor_11, frame);
        } else {
            _cursor.set_item(bn::sprite_items::cursor_8, frame);
        }
    }

    _cursor.set_position(position.x() - SCREEN_X, position.y() - SCREEN_Y);

    g_moku_test.cursor_x = uint8_t(_cursor_x);
    g_moku_test.cursor_y = uint8_t(_cursor_y);
}

void board_demo_scene::update() {
    ++_frame;

    if (_revision != settings::revision()) {
        _revision = settings::revision();
        _board.set_skin(current_skin());
    }

    if (bn::keypad::l_pressed()) {
        _size_index = (_size_index + 3) % 4;
        _apply_size();
    } else if (bn::keypad::r_pressed()) {
        _size_index = (_size_index + 1) % 4;
        _apply_size();
    }

    int last = _board.board_size() - 1;

    if (bn::keypad::left_pressed() && _cursor_x > 0) {
        --_cursor_x;
    } else if (bn::keypad::right_pressed() && _cursor_x < last) {
        ++_cursor_x;
    }

    if (bn::keypad::up_pressed() && _cursor_y > 0) {
        --_cursor_y;
    } else if (bn::keypad::down_pressed() && _cursor_y < last) {
        ++_cursor_y;
    }

    if (bn::keypad::a_pressed()) {
        if (_board.cell_colour(_cursor_x, _cursor_y)) {
            _board.set_cell(_cursor_x, _cursor_y, render::CELL_EMPTY, render::CELL_NONE);
        } else {
            _board.set_cell(_cursor_x, _cursor_y, _next_colour, render::CELL_LAST);
            _next_colour = _next_colour == render::CELL_BLACK ? render::CELL_WHITE : render::CELL_BLACK;
        }
    }

    if (bn::keypad::select_pressed()) {
        _shake = 12;
    }

    if (_shake > 0) {
        --_shake;
        int amount = (_shake / 2) & 1 ? 2 : -2;
        _board.set_offset(amount, _shake > 6 ? -amount : 0);
    } else {
        _board.set_offset(0, 0);
    }

    if (bn::keypad::b_pressed()) {
        scene::request(SceneId::TITLE);
    }

    _update_cursor_sprite();
    _board.flush();

    // Renderer diagnostics for the ROM tests: reserved[0..3] = ticks of the last flush,
    // reserved[4..5] = tiles uploaded.
    uint32_t ticks = uint32_t(_board.last_flush_ticks());
    g_moku_test.reserved[0] = uint8_t(ticks);
    g_moku_test.reserved[1] = uint8_t(ticks >> 8);
    g_moku_test.reserved[2] = uint8_t(ticks >> 16);
    g_moku_test.reserved[3] = uint8_t(ticks >> 24);

    uint32_t tiles = uint32_t(_board.last_flushed_tiles());
    g_moku_test.reserved[4] = uint8_t(tiles);
    g_moku_test.reserved[5] = uint8_t(tiles >> 8);
}

}  // namespace scenes
