#include "game/scenes/title_scene.h"

#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_display.h"
#include "bn_keypad.h"
#include "bn_sprite_palette_item.h"
#include "bn_string_view.h"

#include "bn_regular_bg_items_bg_title.h"
#include "bn_sprite_items_menu_cursor.h"
#include "bn_sprite_items_title_logo.h"

#include "moku_fonts.h"

#include "game/audio.h"
#include "game/scenes/options_scene.h"
#include "game/settings.h"
#include "game/unlocks.h"
#include "game/strings_ids.h"

namespace scenes {

namespace {

// Butano positions sprites from the centre of the screen, so a mockup pixel (x, y) is a sprite
// position (x - 120, y - 80). bn::sprite_text_generator::generate_top_left() is the exception: it
// already takes the top-left corner in screen pixels.
constexpr int SCREEN_X = bn::display::width() / 2;    // 120
constexpr int SCREEN_Y = bn::display::height() / 2;   // 80

// Text palettes: the fonts draw with palette index 1, so recolouring text is a palette swap.
#define MOKU_TEXT_PALETTE(name, red, green, blue)                                   \
    alignas(4) constexpr bn::color name##_COLORS[16] = {                            \
        bn::color(0, 0, 0), bn::color(red, green, blue)};                           \
    constexpr bn::sprite_palette_item name##_PALETTE(name##_COLORS, bn::bpp_mode::BPP_4)

MOKU_TEXT_PALETTE(PAPER, 29, 28, 26);       // #efe6d2
MOKU_TEXT_PALETTE(VERMILION, 24, 7, 5);     // #c23b2c
MOKU_TEXT_PALETTE(GREY, 17, 16, 14);        // #8a8070

constexpr int MENU_X = 86;
constexpr int MENU_Y = 106;
constexpr int MENU_PITCH = 14;
constexpr int CURSOR_X = 78;

constexpr Str MENU_STRINGS[] = {Str::MENU_CAMPAIGN, Str::MENU_SANDBOX, Str::MENU_OPTIONS};
// CAMPAIGN goes through the file screen: the footer promises three of them.
constexpr SceneId MENU_TARGETS[] = {SceneId::SAVE_SELECT, SceneId::SANDBOX_SETUP, SceneId::OPTIONS};

scene::Scene* factory(SceneId id, uint32_t arg) {
    (void)arg;

    if (id == SceneId::TITLE) {
        return new title_scene();
    }

    return nullptr;
}

const scene::FactoryRegistrar registrar(factory);

}  // namespace

title_scene::title_scene() :
    _bg(bn::regular_bg_items::bg_title.create_bg(0, 0)),
    _logo_left(bn::sprite_items::title_logo.create_sprite(64 + 32 - SCREEN_X, 6 + 32 - SCREEN_Y, 0)),
    _logo_right(bn::sprite_items::title_logo.create_sprite(128 + 32 - SCREEN_X, 6 + 32 - SCREEN_Y, 1)),
    _cursor(bn::sprite_items::menu_cursor.create_sprite(CURSOR_X + 4 - SCREEN_X, MENU_Y + 4 - SCREEN_Y, 0)),
    _small(fonts::moku_font_8) {
    _bg.set_priority(3);
    _build_texts();
    _move_cursor();
}

void title_scene::_build_texts() {
    _texts.clear();
    _revision = settings::revision();

    _small.set_left_alignment();

    _small.set_palette_item(VERMILION_PALETTE);
    _small.generate_top_left(70, 68, tr(Str::TITLE_TACTICS), _texts);

    _small.set_palette_item(PAPER_PALETTE);

    for (int index = 0; index < MENU_ITEMS; ++index) {
        _small.generate_top_left(MENU_X, MENU_Y + index * MENU_PITCH, tr(MENU_STRINGS[index]), _texts);
    }

    // "(c)2026 . SAVE" + the slot number, as in the mockup footer.
    char footer[32];
    const char* prefix = tr(Str::MSG_TITLE_FOOTER);
    int length = 0;

    while (prefix[length] && length < 24) {
        footer[length] = prefix[length];
        ++length;
    }

    footer[length++] = ' ';
    footer[length++] = char('1' + settings::slot_index());
    footer[length++] = '/';
    footer[length++] = char('0' + save::SLOT_COUNT);
    footer[length] = 0;

    _small.set_palette_item(GREY_PALETTE);
    _small.generate_top_left(8, 150, bn::string_view(footer, length), _texts);
}

void title_scene::_move_cursor() {
    _cursor.set_y(MENU_Y + _index * MENU_PITCH + 4 - SCREEN_Y);
    g_moku_test.menu_index = uint8_t(_index);
}

void title_scene::update() {
    ++_frame;

    if (_revision != settings::revision()) {
        _build_texts();
    }

    if (bn::keypad::up_pressed()) {
        _index = (_index + MENU_ITEMS - 1) % MENU_ITEMS;
        _move_cursor();
        audio::play(audio::Sfx::MENU_MOVE);
    } else if (bn::keypad::down_pressed()) {
        _index = (_index + 1) % MENU_ITEMS;
        _move_cursor();
        audio::play(audio::Sfx::MENU_MOVE);
    }

    // 1 px bob, as in the art sheet (frames 0 and 1).
    _cursor.set_tiles(bn::sprite_items::menu_cursor.tiles_item(), (_frame / 20) & 1);

    if (bn::keypad::a_pressed()) {
        audio::play(audio::Sfx::MENU_OK);

        if (_index == 2) {
            scenes::set_options_return(SceneId::TITLE);
        }

        scene::request(MENU_TARGETS[_index]);
    }

    // Holding SELECT on the title unlocks the sandbox for testing.
    if (bn::keypad::select_held() && bn::keypad::start_pressed()) {
        unlocks::grant_all();
        settings::store();
        audio::play(audio::Sfx::UNLOCK);
    }

    audio::play_music(audio::Track::TITLE);
}

}  // namespace scenes
