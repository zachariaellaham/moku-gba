#include "game/scenes/sandbox_scene.h"

#include "bn_display.h"
#include "bn_format.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_bg_panels.h"
#include "bn_sprite_items_opponents.h"
#include "bn_sprite_items_particles.h"
#include "bn_string.h"

#include "game/audio.h"
#include "game/scenes/play_scene.h"
#include "game/settings.h"
#include "game/test_iface.h"
#include "game/unlocks.h"

namespace scenes {

namespace {

const Opponent OPPONENTS[OPPONENT_COUNT] = {
    { "PEBBLE", Str::OPP_PEBBLE, Str::OPPT_PEBBLE, Str::BLURB_PEBBLE, ai::Level::VERY_EASY, campaign::UNLOCK_NONE },
    { "SPROUT", Str::OPP_SPROUT, Str::OPPT_SPROUT, Str::BLURB_SPROUT, ai::Level::EASY,      campaign::UNLOCK_SANDBOX_9_EASY },
    { "KOAN",   Str::OPP_KOAN,   Str::OPPT_KOAN,   Str::BLURB_KOAN,   ai::Level::NORMAL,    campaign::UNLOCK_NORMAL },
    { "EMBER",  Str::OPP_EMBER,  Str::OPPT_EMBER,  Str::BLURB_EMBER,  ai::Level::HARD,      campaign::UNLOCK_HARD },
    { "TENGEN", Str::OPP_TENGEN, Str::OPPT_TENGEN, Str::BLURB_TENGEN, ai::Level::MASTER,    campaign::UNLOCK_19_MASTER },
};

game::SandboxConfig g_config;

enum Row : int { ROW_BOARD = 0, ROW_COLOUR, ROW_OPPONENT, ROW_HANDICAP, ROW_SCORING, ROW_KOMI, ROW_SKIN, ROW_PLAYERS, ROW_START, ROW_RESUME, ROW_COUNT };

// The saved game only shows up when there is one, so the screen keeps the mockup's shape until the
// player actually leaves a game unfinished.
[[nodiscard]] bool has_saved_game()
{
    return settings::file().sandbox.valid != 0;
}

constexpr int BOARD_SIZES[4] = { 7, 9, 13, 19 };

int board_index(int size)
{
    for(int i = 0; i < 4; ++i)
    {
        if(BOARD_SIZES[i] == size) return i;
    }

    return 1;
}

bn::string<16> half_points(int x2)
{
    const int whole = x2 / 2;
    return (x2 % 2) ? bn::format<16>("{}.5", whole) : bn::format<16>("{}", whole);
}

void split_two_lines(const char* text, render::text_block* lines)
{
    int line = 0;
    bn::string<40> current;

    for(const char* c = text; line < 2; ++c)
    {
        if(*c == '\n' || *c == 0)
        {
            lines[line++].set(current);
            current.clear();

            if(*c == 0) break;
            continue;
        }

        if(current.size() + 1 < current.max_size()) current.push_back(*c);
    }

    for(; line < 2; ++line) lines[line].set(bn::string_view());
}

scene::Scene* factory(SceneId id, uint32_t arg)
{
    (void)arg;

    if(id == SceneId::SANDBOX_SETUP)
    {
        return new sandbox_scene();
    }

    if(id == SceneId::CHOOSE_OPPONENT)
    {
        return new opponent_scene();
    }

    return nullptr;
}

scene::FactoryRegistrar registrar(factory);

}  // namespace

const Opponent& opponent(int index) { return OPPONENTS[index < 0 ? 0 : (index >= OPPONENT_COUNT ? OPPONENT_COUNT - 1 : index)]; }
game::SandboxConfig& sandbox_config() { return g_config; }

// --------------------------------------------------------------------------------------------
// sandbox setup
// --------------------------------------------------------------------------------------------
sandbox_scene::sandbox_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 3);
    _bg->set_priority(2);

    _title.setup(8, 4, render::Font::SMALL, render::Ink::INK, render::Align::LEFT);
    _title.set(tr(Str::TITLE_SANDBOX));
    _nav.setup(232, 4, render::Font::SMALL, render::Ink::MUTED, render::Align::RIGHT);
    _nav.set(tr(Str::NAV_SANDBOX));

    for(int i = 0; i < 2; ++i)
    {
        _blurb[i].setup(8, 136 + i * 11, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    }

    // One more row than the mockup needs a pixel back from each gap; without the resume row the
    // spacing is exactly the mockup's.
    _menu.setup(16, 24, has_saved_game() ? 11 : 12, 120);
    _menu.add_choice(tr(Str::HUDL_BOARD), "");
    _menu.add_choice(tr(Str::HUDL_YOU_PLAY), "");
    _menu.add_choice(tr(Str::HUDL_AI), "");
    _menu.add_choice(tr(Str::HUDL_HANDICAP), "");
    _menu.add_choice(tr(Str::HUDL_SCORING), "");
    _menu.add_choice(tr(Str::HUDL_KOMI), "");
    _menu.add_choice(tr(Str::HUDL_SKIN), "");
    _menu.add_choice(tr(Str::HUDL_PLAYERS), "");
    _menu.add_item(tr(Str::MENU_START_GAME));

    if(has_saved_game())
    {
        _menu.add_item(tr(Str::MENU_RESUME));
    }

    _refresh();
}

sandbox_scene::~sandbox_scene() = default;

void sandbox_scene::_refresh()
{
    // never offer a locked board or a locked opponent
    while(! unlocks::board_size_available(g_config.size))
    {
        g_config.size = uint8_t(BOARD_SIZES[(board_index(g_config.size) + 3) % 4]);
    }

    while(! unlocks::level_available(opponent(g_config.opponent_id).level))
    {
        g_config.opponent_id = uint8_t((g_config.opponent_id + OPPONENT_COUNT - 1) % OPPONENT_COUNT);
    }

    if(g_config.size == 7 && g_config.handicap > 0)
    {
        g_config.handicap = 0;               // 7x7 has no hoshi to place them on
    }

    _menu.set_value(ROW_BOARD, bn::format<16>("{}x{}", g_config.size, g_config.size));
    _menu.set_value(ROW_COLOUR, tr(g_config.player_black ? Str::VAL_BLACK : Str::VAL_WHITE));
    _menu.set_value(ROW_OPPONENT, g_config.hotseat ? tr(Str::VAL_HOTSEAT) : tr(opponent(g_config.opponent_id).name_id));
    _menu.set_value(ROW_HANDICAP, bn::format<8>("{}", g_config.handicap));
    _menu.set_value(ROW_SCORING, tr(g_config.rules == go::Ruleset::AREA ? Str::VAL_AREA : Str::VAL_TERRITORY));
    _menu.set_value(ROW_KOMI, half_points(g_config.komi_x2));
    const int skin = settings::skin();
    _menu.set_value(ROW_SKIN, tr(skin == 1 ? Str::VAL_SKIN_PUP : (skin == 2 ? Str::VAL_SKIN_DINO : Str::VAL_SKIN_CLASSIC)));
    _menu.set_value(ROW_PLAYERS, tr(g_config.hotseat ? Str::VAL_HOTSEAT : Str::VAL_VS_AI));
    _menu.set_row_enabled(ROW_OPPONENT, ! g_config.hotseat);
    _menu.set_row_enabled(ROW_HANDICAP, g_config.size > 7);

    split_two_lines(g_config.hotseat ? tr(Str::MSG_PASS_DEVICE) : tr(opponent(g_config.opponent_id).blurb_id), _blurb);
    g_config.level = opponent(g_config.opponent_id).level;
}

void sandbox_scene::_change(int delta)
{
    switch(_menu.index())
    {
    case ROW_BOARD:
    {
        int index = board_index(g_config.size);

        for(int guard = 0; guard < 4; ++guard)
        {
            index = (index + 4 + delta) % 4;

            if(unlocks::board_size_available(BOARD_SIZES[index]))
            {
                g_config.size = uint8_t(BOARD_SIZES[index]);
                break;
            }
        }
        break;
    }

    case ROW_COLOUR:
        g_config.player_black = ! g_config.player_black;
        break;

    case ROW_OPPONENT:
    {
        int index = g_config.opponent_id;

        for(int guard = 0; guard < OPPONENT_COUNT; ++guard)
        {
            index = (index + OPPONENT_COUNT + delta) % OPPONENT_COUNT;

            if(unlocks::level_available(opponent(index).level))
            {
                g_config.opponent_id = uint8_t(index);
                break;
            }
        }
        break;
    }

    case ROW_HANDICAP:
    {
        int value = g_config.handicap + delta;

        if(value == 1) value = delta > 0 ? 2 : 0;       // handicap 1 is not a thing
        if(value < 0) value = 5;
        if(value > 5) value = 0;
        g_config.handicap = uint8_t(value);
        break;
    }

    case ROW_SCORING:
        g_config.rules = (g_config.rules == go::Ruleset::AREA) ? go::Ruleset::TERRITORY : go::Ruleset::AREA;
        g_config.komi_x2 = go::GameSettings::default_komi_x2(g_config.rules);
        break;

    case ROW_KOMI:
    {
        int value = g_config.komi_x2 + delta;

        if(value < 0) value = 18;
        if(value > 18) value = 0;
        g_config.komi_x2 = int16_t(value);
        break;
    }

    case ROW_SKIN:
        settings::set_skin((settings::skin() + 3 + delta) % 3);
        break;

    case ROW_PLAYERS:
        g_config.hotseat = ! g_config.hotseat;
        break;

    default:
        return;
    }

    audio::play(audio::Sfx::MENU_OK);
    _refresh();
}

void sandbox_scene::_start()
{
    PlayRequest request;
    request.campaign = false;
    request.sandbox = g_config;
    set_play_request(request);
    audio::play(audio::Sfx::MENU_OK);
    scene::request(SceneId::PLAY);
}

// Rebuilds the configuration the saved game was played with, so the board, the komi and the
// opponent come back exactly as they were rather than as the setup screen currently reads.
void sandbox_scene::_resume()
{
    const save::SandboxGame& saved = settings::file().sandbox;

    PlayRequest request;
    request.campaign = false;
    request.resume = true;
    request.sandbox.size = saved.size;
    request.sandbox.player_black = saved.player_black != 0;
    request.sandbox.opponent_id = saved.opponent;
    request.sandbox.level = opponent(saved.opponent).level;
    request.sandbox.handicap = saved.handicap;
    request.sandbox.rules = saved.rules ? go::Ruleset::TERRITORY : go::Ruleset::AREA;
    request.sandbox.komi_x2 = saved.komi_x2;
    request.sandbox.hotseat = saved.hotseat != 0;
    set_play_request(request);
    audio::play(audio::Sfx::MENU_OK);
    scene::request(SceneId::PLAY);
}

void sandbox_scene::update()
{
    ++_anim;
    _menu.update();

    if(bn::keypad::up_pressed() && _menu.move(-1)) audio::play(audio::Sfx::MENU_MOVE);
    if(bn::keypad::down_pressed() && _menu.move(1)) audio::play(audio::Sfx::MENU_MOVE);
    if(bn::keypad::left_pressed()) _change(-1);
    if(bn::keypad::right_pressed()) _change(1);

    if(bn::keypad::a_pressed())
    {
        if(_menu.index() == ROW_START)
        {
            _start();
        }
        else if(_menu.index() == ROW_RESUME && has_saved_game())
        {
            _resume();
        }
        else if(_menu.index() == ROW_OPPONENT && ! g_config.hotseat)
        {
            audio::play(audio::Sfx::MENU_OK);
            scene::request(SceneId::CHOOSE_OPPONENT);
        }
        else
        {
            _change(1);
        }
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::TITLE);
    }

    g_moku_test.menu_index = uint8_t(_menu.index());
    g_moku_test.board_size = g_config.size;
    g_moku_test.ai_level = uint8_t(g_config.level);
    g_moku_test.opponent_id = uint8_t(g_config.hotseat ? 0xFF : g_config.opponent_id);
}

// --------------------------------------------------------------------------------------------
// choose opponent
// --------------------------------------------------------------------------------------------
opponent_scene::opponent_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 2);
    _bg->set_priority(2);

    _title.setup(8, 4, render::Font::SMALL, render::Ink::PAPER, render::Align::LEFT);
    _title.set(tr(Str::TITLE_CHOOSE_OPPONENT));
    _nav.setup(232, 4, render::Font::SMALL, render::Ink::MUTED, render::Align::RIGHT);
    _nav.set(tr(Str::NAV_OPPONENT));

    for(int i = 0; i < OPPONENT_COUNT; ++i)
    {
        const int x = 28 + i * 46;
        _portraits.push_back(bn::sprite_items::opponents.create_sprite(x - (bn::display::width() / 2),
                                                                      46 - (bn::display::height() / 2), i));
        _portraits.back().set_bg_priority(1);
        _names[i].setup(x, 70, render::Font::SMALL, render::Ink::MUTED, render::Align::CENTER);
        _names[i].set(tr(opponent(i).name_id));
    }

    _name.setup(16, 92, render::Font::SMALL, render::Ink::ACCENT, render::Align::LEFT);
    _subtitle.setup(224, 92, render::Font::SMALL, render::Ink::MUTED, render::Align::RIGHT);
    _level.setup(16, 104, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _level.set(tr(Str::HUDL_LEVEL));
    _level_pips.setup(64, 104, render::Font::SMALL, render::Ink::ACCENT, render::Align::LEFT);

    for(int i = 0; i < 2; ++i)
    {
        _blurb[i].setup(16, 118 + i * 10, render::Font::SMALL, render::Ink::PAPER, render::Align::LEFT);
    }

    _record.setup(16, 146, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _index = sandbox_config().opponent_id;
    _refresh();
}

opponent_scene::~opponent_scene() = default;

void opponent_scene::_refresh()
{
    const Opponent& o = opponent(_index);
    const int x = 28 + _index * 46;

    if(! _frame)
    {
        _frame = bn::sprite_items::particles.create_sprite(0, 0, 0);
    }

    _frame->set_position(x - (bn::display::width() / 2), 46 - (bn::display::height() / 2));
    _frame->set_visible(false);            // the blink is drawn by dimming the other portraits

    for(int i = 0; i < OPPONENT_COUNT; ++i)
    {
        const bool available = unlocks::level_available(opponent(i).level);
        _names[i].set_ink(i == _index ? render::Ink::ACCENT : (available ? render::Ink::MUTED : render::Ink::INK));
        _portraits[i].set_visible(available || i == _index);
    }

    _name.set(tr(o.name_id));
    _subtitle.set(tr(o.title_id));
    split_two_lines(tr(o.blurb_id), _blurb);

    const save::OpponentRecord& record = settings::slot().records[_index];
    _record.set(record.wins + record.losses == 0
                    ? bn::string<32>(tr(Str::MSG_NO_RECORD))
                    : bn::format<32>("{}{} · {}{}", record.wins, tr(Str::VAL_W), record.losses, tr(Str::VAL_L)));

    bn::string<16> pips;

    for(int i = 0; i < OPPONENT_COUNT; ++i)
    {
        pips.append(i <= int(o.level) ? "#" : "-");
    }

    _level_pips.set(pips);
}

void opponent_scene::update()
{
    ++_anim;

    for(int i = 0; i < OPPONENT_COUNT; ++i)
    {
        _portraits[i].set_y((i == _index && (_anim / 20) % 2 == 0 ? 45 : 46) - (bn::display::height() / 2));
    }

    if(bn::keypad::left_pressed() || bn::keypad::right_pressed())
    {
        const int delta = bn::keypad::left_pressed() ? -1 : 1;

        for(int guard = 0; guard < OPPONENT_COUNT; ++guard)
        {
            _index = (_index + OPPONENT_COUNT + delta) % OPPONENT_COUNT;

            if(unlocks::level_available(opponent(_index).level)) break;
        }

        audio::play(audio::Sfx::MENU_MOVE);
        _refresh();
    }

    if(bn::keypad::a_pressed())
    {
        sandbox_config().opponent_id = uint8_t(_index);
        sandbox_config().level = opponent(_index).level;
        sandbox_config().hotseat = false;
        audio::play(audio::Sfx::MENU_OK);
        scene::request(SceneId::SANDBOX_SETUP);
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::SANDBOX_SETUP);
    }

    g_moku_test.menu_index = uint8_t(_index);
    g_moku_test.opponent_id = uint8_t(_index);
}

}  // namespace scenes
