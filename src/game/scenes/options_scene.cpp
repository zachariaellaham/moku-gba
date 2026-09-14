#include "game/scenes/options_scene.h"

#include "bn_display.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_bg_panels.h"
#include "bn_sprite_items_helper_indi.h"
#include "bn_sprite_items_helper_rex.h"
#include "bn_sprite_items_helper_sen.h"
#include "bn_string.h"

#include "game/audio.h"
#include "game/settings.h"
#include "game/test_iface.h"

namespace scenes {

namespace {

enum Row : int { ROW_LANGUAGE = 0, ROW_HELPER, ROW_SKIN, ROW_EFFECTS, ROW_CONFIRM, ROW_MUSIC, ROW_SFX, ROW_COUNT };

SceneId g_return = SceneId::TITLE;

const bn::sprite_item& helper_item(int helper)
{
    switch(helper)
    {
    case 1:  return bn::sprite_items::helper_indi;
    case 2:  return bn::sprite_items::helper_rex;
    default: return bn::sprite_items::helper_sen;
    }
}

// "▮▮▮▮▮▮▯▯" for 6 of 8, as the mockup shows.
bn::string<24> bar(int value)
{
    bn::string<24> out;

    for(int i = 0; i < 8; ++i)
    {
        const char* glyph = i < value ? "#" : "-";
        out.append(glyph);
    }

    return out;
}

scene::Scene* factory(SceneId id, uint32_t arg)
{
    (void)arg;

    if(id == SceneId::OPTIONS)
    {
        return new options_scene();
    }

    return nullptr;
}

scene::FactoryRegistrar registrar(factory);

}  // namespace

void set_options_return(SceneId id) { g_return = id; }

options_scene::options_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 3);
    _bg->set_priority(2);

    _title.setup(8, 4, render::Font::SMALL, render::Ink::INK, render::Align::LEFT);
    _title.set(tr(Str::TITLE_OPTIONS));
    _nav.setup(232, 4, render::Font::SMALL, render::Ink::MUTED, render::Align::RIGHT);
    _nav.set(tr(Str::NAV_OPTIONS));
    _note.setup(8, 148, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);

    _menu.setup(16, 28, 15, 108);
    _menu.add_choice(tr(Str::HUDL_LANGUAGE), "");
    _menu.add_choice(tr(Str::HUDL_HELPER), "");
    _menu.add_choice(tr(Str::HUDL_SKIN), "");
    _menu.add_choice(tr(Str::HUDL_EFFECTS), "");
    _menu.add_choice(tr(Str::HUDL_CONFIRM), "");
    _menu.add_choice(tr(Str::HUDL_MUSIC), "");
    _menu.add_choice(tr(Str::HUDL_SFX), "");
    _refresh_values();
}

options_scene::~options_scene() = default;

void options_scene::_refresh_values()
{
    _menu.set_value(ROW_LANGUAGE, tr(settings::language() == 0 ? Str::VAL_LANG_EN : Str::VAL_LANG_FR));

    const int helper = settings::helper();
    _menu.set_value(ROW_HELPER, tr(helper == 1 ? Str::WHO_INDI : (helper == 2 ? Str::WHO_REX : Str::WHO_SEN)));

    const int skin = settings::skin();
    _menu.set_value(ROW_SKIN, tr(skin == 1 ? Str::VAL_SKIN_PUP : (skin == 2 ? Str::VAL_SKIN_DINO : Str::VAL_SKIN_CLASSIC)));
    _menu.set_value(ROW_EFFECTS, tr(settings::effects() ? Str::VAL_EFFECTS_JUICY : Str::VAL_EFFECTS_OFF));
    _menu.set_value(ROW_CONFIRM, tr(settings::confirm_2a() ? Str::VAL_CONFIRM_2A : Str::VAL_CONFIRM_1A));
    _menu.set_value(ROW_MUSIC, bar(settings::music()));
    _menu.set_value(ROW_SFX, bar(settings::sfx()));

    _note.set(tr(helper == 1 ? Str::MSG_HELPER_INDI : (helper == 2 ? Str::MSG_HELPER_REX : Str::MSG_HELPER_SEN)));

    if(_preview_helper != helper)
    {
        _preview_helper = helper;
        _preview = helper_item(helper).create_sprite(196 - (bn::display::width() / 2),
                                                     92 - (bn::display::height() / 2), 1);
        _preview->set_bg_priority(1);
    }

    // The title and the labels themselves change with the language.
    _title.force(tr(Str::TITLE_OPTIONS));
    _nav.force(tr(Str::NAV_OPTIONS));
    _menu.set_label(ROW_LANGUAGE, tr(Str::HUDL_LANGUAGE));
    _menu.set_label(ROW_HELPER, tr(Str::HUDL_HELPER));
    _menu.set_label(ROW_SKIN, tr(Str::HUDL_SKIN));
    _menu.set_label(ROW_EFFECTS, tr(Str::HUDL_EFFECTS));
    _menu.set_label(ROW_CONFIRM, tr(Str::HUDL_CONFIRM));
    _menu.set_label(ROW_MUSIC, tr(Str::HUDL_MUSIC));
    _menu.set_label(ROW_SFX, tr(Str::HUDL_SFX));
}

void options_scene::_change(int delta)
{
    switch(_menu.index())
    {
    case ROW_LANGUAGE:
        settings::set_language((settings::language() + 2 + delta) % 2);
        break;

    case ROW_HELPER:
        settings::set_helper((settings::helper() + 3 + delta) % 3);
        break;

    case ROW_SKIN:
        settings::set_skin((settings::skin() + 3 + delta) % 3);
        break;

    case ROW_EFFECTS:
        settings::set_effects(settings::effects() ? 0 : 1);
        break;

    case ROW_CONFIRM:
        settings::set_confirm_2a(settings::confirm_2a() ? 0 : 1);
        break;

    case ROW_MUSIC:
    {
        const int value = settings::music() + delta;
        settings::set_music(value < 0 ? 0 : (value > 8 ? 8 : value));
        audio::apply_volumes();
        break;
    }

    case ROW_SFX:
    {
        const int value = settings::sfx() + delta;
        settings::set_sfx(value < 0 ? 0 : (value > 8 ? 8 : value));
        audio::apply_volumes();
        break;
    }

    default:
        break;
    }

    audio::play(audio::Sfx::MENU_OK);
    _refresh_values();
}

void options_scene::update()
{
    ++_anim;
    _menu.update();

    if(bn::keypad::up_pressed() && _menu.move(-1))
    {
        audio::play(audio::Sfx::MENU_MOVE);
    }

    if(bn::keypad::down_pressed() && _menu.move(1))
    {
        audio::play(audio::Sfx::MENU_MOVE);
    }

    if(bn::keypad::left_pressed())
    {
        _change(-1);
    }

    if(bn::keypad::right_pressed())
    {
        _change(1);
    }

    if(bn::keypad::b_pressed() || bn::keypad::start_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        settings::store();
        scene::request(g_return);
    }

    g_moku_test.menu_index = uint8_t(_menu.index());
    g_moku_test.scene_sub = uint8_t(_menu.index());
}

}  // namespace scenes
