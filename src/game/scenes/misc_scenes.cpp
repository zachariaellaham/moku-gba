#include "game/scenes/misc_scenes.h"

#include "bn_display.h"
#include "bn_format.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_bg_panels.h"
#include "bn_string.h"

#include "game/audio.h"
#include "game/scenes/sandbox_scene.h"
#include "bn_sprite_items_opponents.h"

#include "game/settings.h"
#include "game/speech.h"
#include "game/test_iface.h"

namespace scenes {

namespace {

GameResult g_result;

bn::string<16> half_points(int x2)
{
    const int whole = x2 / 2;
    return (x2 % 2) ? bn::format<16>("{}.5", whole) : bn::format<16>("{}", whole);
}

scene::Scene* factory(SceneId id, uint32_t arg)
{
    (void)arg;

    if(id == SceneId::SAVE_SELECT)
    {
        return new save_select_scene();
    }

    if(id == SceneId::GAME_OVER)
    {
        return new game_over_scene();
    }

    return nullptr;
}

scene::FactoryRegistrar registrar(factory);

}  // namespace

void set_game_result(const GameResult& result) { g_result = result; }
const GameResult& game_result() { return g_result; }

// --------------------------------------------------------------------------------------------
// save select
// --------------------------------------------------------------------------------------------
save_select_scene::save_select_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 3);
    _bg->set_priority(2);

    _title.setup(8, 4, render::Font::SMALL, render::Ink::INK, render::Align::LEFT);
    _title.set(tr(Str::TITLE_SAVE_SELECT));
    _nav.setup(232, 4, render::Font::SMALL, render::Ink::MUTED, render::Align::RIGHT);
    _nav.set(tr(Str::NAV_SAVE_SELECT));
    _confirm.setup(120, 140, render::Font::SMALL, render::Ink::ACCENT, render::Align::CENTER);

    _menu.setup(24, 40, 30, 24);

    for(int i = 0; i < save::SLOT_COUNT; ++i)
    {
        _menu.add_item(bn::format<20>("{} {}", tr(Str::HUDL_FILE), i + 1));
        _details[i].setup(44, 54 + i * 30, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    }

    _menu.set_index(settings::slot_index());
    _refresh();
}

save_select_scene::~save_select_scene() = default;

void save_select_scene::_refresh()
{
    for(int i = 0; i < save::SLOT_COUNT; ++i)
    {
        const save::Slot& slot = settings::file().slots[i];

        if(! slot.used)
        {
            _details[i].set(tr(Str::VAL_NEW_GAME));
            continue;
        }

        int stars = 0;

        for(int m = 0; m < campaign::MISSION_COUNT; ++m)
        {
            if(slot.stars_mask & (1u << m)) ++stars;
        }

        _details[i].set(bn::format<32>("{} {}/20 · {} {}", tr(Str::HUDL_MISSIONS), slot.cleared_count,
                                       tr(Str::HUDL_STARS), stars));
    }

    _confirm.set(_erasing ? tr(Str::MSG_ERASE_CONFIRM) : bn::string_view());
}

void save_select_scene::update()
{
    ++_anim;
    _menu.update();

    if(_erasing)
    {
        if(bn::keypad::a_pressed())
        {
            settings::file().slots[_menu.index()] = save::Slot();
            settings::store();
            _erasing = false;
            audio::play(audio::Sfx::MENU_OK);
            _refresh();
        }

        if(bn::keypad::b_pressed())
        {
            _erasing = false;
            audio::play(audio::Sfx::MENU_BACK);
            _refresh();
        }

        return;
    }

    if(bn::keypad::up_pressed() && _menu.move(-1)) { audio::play(audio::Sfx::MENU_MOVE); }
    if(bn::keypad::down_pressed() && _menu.move(1)) { audio::play(audio::Sfx::MENU_MOVE); }

    if(bn::keypad::a_pressed())
    {
        settings::set_slot(_menu.index());
        settings::store();
        audio::play(audio::Sfx::MENU_OK);
        scene::request(SceneId::CAMPAIGN_MAP);
    }

    if(bn::keypad::select_pressed())
    {
        _erasing = true;
        audio::play(audio::Sfx::MENU_BACK);
        _refresh();
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::TITLE);
    }

    g_moku_test.menu_index = uint8_t(_menu.index());
    g_moku_test.scene_sub = uint8_t(_erasing ? 1 : 0);
}

// --------------------------------------------------------------------------------------------
// game over / score
// --------------------------------------------------------------------------------------------
game_over_scene::game_over_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 1);
    _bg->set_priority(2);

    const GameResult& r = game_result();
    const go::ScoreResult& s = r.score;

    _title.setup(120, 10, render::Font::DISPLAY, render::Ink::ACCENT, render::Align::CENTER);
    _title.set(tr(Str::TITLE_SCORE));

    _result.setup(120, 46, render::Font::SMALL, render::Ink::INK, render::Align::CENTER);

    const int margin = s.margin_x2 > 0 ? s.margin_x2 : -s.margin_x2;

    if(s.winner == go::EMPTY)
    {
        _result.set(tr(Str::MSG_DRAW));
    }
    else if(r.resigned)
    {
        _result.set(bn::format<40>("{} {}", tr(s.winner == go::BLACK ? Str::MSG_BLACK_WINS : Str::MSG_WHITE_WINS),
                                   tr(Str::MSG_BY_RESIGNATION)));
    }
    else
    {
        _result.set(bn::format<40>("{} {} {}", tr(s.winner == go::BLACK ? Str::MSG_BLACK_WINS_BY : Str::MSG_WHITE_WINS_BY),
                                   half_points(margin), tr(Str::MSG_POINTS)));
    }

    const Str labels[4] = { Str::HUDL_TERRITORY, Str::HUDL_AREA, Str::HUDL_PRISONERS, Str::HUDL_TOTAL };
    const bn::string<24> values[4] = {
        bn::format<24>("{} · {}", s.black_territory, s.white_territory),
        bn::format<24>("{} · {}", s.black_stones, s.white_stones),
        bn::format<24>("{} · {}", s.black_captures, s.white_captures),
        bn::format<24>("{} · {}", half_points(s.black_x2), half_points(s.white_x2)),
    };

    for(int i = 0; i < 4; ++i)
    {
        _labels[i].setup(40, 62 + i * 16, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
        _labels[i].set(tr(labels[i]));
        _values[i].setup(200, 62 + i * 16, render::Font::SMALL, render::Ink::INK, render::Align::RIGHT);
        _values[i].set(values[i]);
    }

    // The opponent has the last word (mockup 2b): their portrait, and the line that fits how it
    // went for them. A hot-seat game has nobody to say it.
    if(! r.hotseat && r.opponent_id < save::OPPONENT_COUNT)
    {
        const bool player_won = (s.winner == (r.player_black ? go::BLACK : go::WHITE));
        const int who = int(r.opponent_id);
        _portrait = bn::sprite_items::opponents.create_sprite(76 - (bn::display::width() / 2),
                                                             28 - (bn::display::height() / 2), who);
        _portrait->set_bg_priority(1);

        bn::string<32> lines[2];
        const int count = render::wrap(tr(Str(game::say_of(who, player_won ? game::Say::LOSE : game::Say::WIN))),
                                       150, lines, 2);

        for(int i = 0; i < count; ++i)
        {
            _say[i].setup(108, 22 + i * 12, render::Font::SMALL, render::Ink::INK, render::Align::LEFT);
            _say[i].set(lines[i]);
        }
    }

    _menu.setup(40, 128, 14, 40);
    _menu.add_item(tr(Str::MENU_SANDBOX));
    _menu.add_item(tr(Str::MENU_MAP));

    // record the result against the opponent
    if(! r.hotseat && r.opponent_id < save::OPPONENT_COUNT)
    {
        save::OpponentRecord& record = settings::slot().records[r.opponent_id];
        const bool player_won = (s.winner == (r.player_black ? go::BLACK : go::WHITE));

        if(player_won)
        {
            ++record.wins;
            const int player_margin = r.player_black ? s.margin_x2 : -s.margin_x2;

            if(player_margin > record.best_margin_x2)
            {
                record.best_margin_x2 = int16_t(player_margin);
            }
        }
        else
        {
            ++record.losses;
        }

        settings::touch();
        settings::store();
    }
}

game_over_scene::~game_over_scene() = default;

void game_over_scene::update()
{
    ++_anim;
    _menu.update();

    if(bn::keypad::up_pressed() && _menu.move(-1)) { audio::play(audio::Sfx::MENU_MOVE); }
    if(bn::keypad::down_pressed() && _menu.move(1)) { audio::play(audio::Sfx::MENU_MOVE); }

    if(bn::keypad::a_pressed())
    {
        audio::play(audio::Sfx::MENU_OK);
        scene::request(_menu.index() == 0 ? SceneId::SANDBOX_SETUP : SceneId::TITLE);
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::TITLE);
    }

    g_moku_test.menu_index = uint8_t(_menu.index());
    g_moku_test.winner = uint8_t(game_result().score.winner);
    g_moku_test.score_margin_x2 = game_result().score.margin_x2;
}

}  // namespace scenes
