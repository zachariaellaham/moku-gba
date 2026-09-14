#include "game/scenes/play_scene.h"

#include "bn_bg_palettes.h"
#include "bn_colors.h"
#include "bn_format.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_bg_hud_classic.h"
#include "bn_regular_bg_items_bg_hud_dino.h"
#include "bn_regular_bg_items_bg_hud_pup.h"
#include "bn_sprite_items_cursor_11.h"
#include "bn_sprite_items_cursor_16.h"
#include "bn_sprite_items_cursor_8.h"
#include "bn_sprite_items_ghost_11.h"
#include "bn_sprite_items_ghost_16.h"
#include "bn_sprite_items_ghost_8.h"
#include "bn_string.h"
#include "bn_timers.h"

#include "game/audio.h"
#include "game/scenes/campaign_scenes.h"
#include "game/scenes/misc_scenes.h"
#include "game/settings.h"
#include "game/test_iface_ext.h"
#include "game/unlocks.h"

namespace scenes {

namespace {

PlayRequest g_request;
game::Session g_session GO_EWRAM_BSS;

// Failures per mission since the console was switched on. Not saved: "twice in a row" is about the
// sitting the player is in, and a mission they walked away from yesterday deserves a fresh start.
uint8_t g_fail_counts[campaign::MISSION_COUNT];

constexpr int HUD_X = 150;               // the panel starts at 144; 6 px of padding
constexpr int HUD_WIDTH = 84;            // 150..234: what a line of HUD text may use
constexpr int LABEL_INK_Y[4] = { 6, 32, 56, 80 };
constexpr int PANEL_LABEL_Y = 104;
constexpr int PANEL_LINE_Y[3] = { 116, 126, 136 };
constexpr int HINT_Y[2] = { 146, 154 };

// Roughly 12 ms of a 16.6 ms frame, in hardware timer ticks.
constexpr int AI_TICK_BUDGET = (bn::timers::ticks_per_frame() * 12) / 17;

const char* tr_c(uint16_t id) { return tr(Str(id)); }

int cell_index_for(int size) { return size <= 9 ? 0 : (size <= 13 ? 1 : 2); }

const bn::sprite_item& cursor_item(int cell_index)
{
    switch(cell_index)
    {
    case 1:  return bn::sprite_items::cursor_11;
    case 2:  return bn::sprite_items::cursor_8;
    default: return bn::sprite_items::cursor_16;
    }
}

const bn::sprite_item& ghost_item(int cell_index)
{
    switch(cell_index)
    {
    case 1:  return bn::sprite_items::ghost_11;
    case 2:  return bn::sprite_items::ghost_8;
    default: return bn::sprite_items::ghost_16;
    }
}

bn::regular_bg_ptr make_hud_bg()
{
    switch(settings::skin())
    {
    case 1:  return bn::regular_bg_items::bg_hud_pup.create_bg(0, 0);
    case 2:  return bn::regular_bg_items::bg_hud_dino.create_bg(0, 0);
    default: return bn::regular_bg_items::bg_hud_classic.create_bg(0, 0);
    }
}

// Stone colour names follow the skin: blue and orange pebbles, obsidian and amber eggs.
Str colour_name(go::Color c)
{
    const bool black = c == go::BLACK;

    switch(settings::skin())
    {
    case 1:  return black ? Str::VAL_BLUE : Str::VAL_ORANGE;
    case 2:  return black ? Str::VAL_OBSIDIAN : Str::VAL_AMBER;
    default: return black ? Str::VAL_BLACK : Str::VAL_WHITE;
    }
}

Str colour_letter(go::Color c)
{
    const bool black = c == go::BLACK;

    switch(settings::skin())
    {
    case 1:  return black ? Str::VAL_LTR_BLUE : Str::VAL_LTR_ORANGE;
    case 2:  return black ? Str::VAL_LTR_OBSIDIAN : Str::VAL_LTR_AMBER;
    default: return black ? Str::VAL_LTR_BLACK : Str::VAL_LTR_WHITE;
    }
}

bn::string<16> half_points(int x2)
{
    const int whole = x2 / 2;
    return (x2 % 2) ? bn::format<16>("{}.5", whole) : bn::format<16>("{}", whole);
}

scene::Scene* factory(SceneId id, uint32_t arg)
{
    if(id == SceneId::PLAY)
    {
        return new play_scene(arg);
    }

    return nullptr;
}

scene::FactoryRegistrar registrar(factory);

// Starting a game is not something the play scene can do while it is not running, so these two
// commands live next to it rather than inside it.
bool start_command(void* ctx, uint32_t cmd, uint32_t arg, uint32_t& result)
{
    (void)ctx;

    if(cmd == TCMD_LOAD_MISSION)
    {
        if(arg < 1 || arg > uint32_t(campaign::MISSION_COUNT))
        {
            result = test_iface::TRES_BAD_ARG;
            return true;
        }

        PlayRequest request;
        request.campaign = true;
        request.mission_index = uint8_t(arg - 1);
        request.stage = 0;
        set_play_request(request);
        set_campaign_mission(int(arg) - 1);
        result = scene::switch_now(SceneId::PLAY, 0) ? test_iface::TRES_OK : test_iface::TRES_REFUSED;
        return true;
    }

    if(cmd == TCMD_START_SANDBOX)
    {
        // size index | player_black<<4 | level<<8 | handicap<<12 | rules<<16 | komi_x2<<20 | hotseat<<28
        constexpr int SIZES[4] = { 7, 9, 13, 19 };
        const int size_index = int(arg & 0xF);
        const int level = int((arg >> 8) & 0xF);
        const int komi = int((arg >> 20) & 0xFF);

        if(size_index > 3 || level >= ai::LEVEL_COUNT || komi > 18)
        {
            result = test_iface::TRES_BAD_ARG;
            return true;
        }

        PlayRequest request;
        request.campaign = false;
        request.sandbox.size = uint8_t(SIZES[size_index]);
        request.sandbox.player_black = ((arg >> 4) & 0xF) != 0;
        request.sandbox.level = ai::Level(level);
        request.sandbox.opponent_id = uint8_t(level);
        request.sandbox.handicap = uint8_t((arg >> 12) & 0xF);
        request.sandbox.rules = ((arg >> 16) & 0xF) ? go::Ruleset::TERRITORY : go::Ruleset::AREA;
        request.sandbox.komi_x2 = int16_t(komi);
        request.sandbox.hotseat = ((arg >> 28) & 0xF) != 0;
        set_play_request(request);
        result = scene::switch_now(SceneId::PLAY, 0) ? test_iface::TRES_OK : test_iface::TRES_REFUSED;
        return true;
    }

    return false;
}

struct StartCommandRegistrar {
    StartCommandRegistrar() { test_iface::add_handler(start_command, nullptr); }
};

StartCommandRegistrar start_registrar;

}  // namespace

game::Session& play_scene::session() { return g_session; }

void set_play_request(const PlayRequest& request) { g_request = request; }
const PlayRequest& play_request() { return g_request; }

play_scene::play_scene(uint32_t arg)
{
    (void)arg;
    render::text_init();
    g_moku_test.reserved[8] = 0;       // lines the opponent has said this game
    g_moku_test.reserved[10] = 0;

    _board.set_skin(skins::skin(skins::Skin(settings::skin())));
    _fx.set_skin(settings::skin());
    _fx.set_juicy(settings::effects() != 0);

    if(g_request.campaign)
    {
        const campaign::MissionDef& def = campaign::mission(g_request.mission_index);
        session().start_campaign(def, g_request.stage);
    }
    else if(g_request.resume && settings::file().sandbox.valid)
    {
        const save::SandboxGame& saved = settings::file().sandbox;
        session().restore_sandbox(g_request.sandbox, saved.moves, int(saved.move_count));
    }
    else
    {
        session().start_sandbox(g_request.sandbox);
    }

    session().set_confirm_two_press(settings::confirm_2a());
    _board.set_board_size(session().game().board().size());
    _hud_bg = make_hud_bg();
    _hud_bg->set_priority(2);
    _build_hud();
    _sync_board();
    _sync_cursor();
    _update_hud();
    audio::play_music(audio::Track::PLAY);

    if(g_request.campaign && session().mission())
    {
        _show_line(session().run().stage->text_before, render::Pose::IDLE1);
        _dialogue_gate = true;

        // Hints are due after two failures. SELECT always gives one - taking it away
        // would only hide the HUD's own "SEL HINT" - so instead the helper offers after the second
        // failure, once the mission's own opening line has been read.
        if(g_request.mission_index < campaign::MISSION_COUNT &&
           g_fail_counts[g_request.mission_index] >= 2)
        {
            _pending_line = uint16_t(Str::MSG_STUCK);
            g_moku_test.hint_offered = 1;
        }
        else
        {
            g_moku_test.hint_offered = 0;
        }
    }
}

// A sandbox game that is still going is kept in SRAM, so the cartridge can be switched off in the
// middle of it. A campaign mission is not kept: missions are
// short and restart from their own position, and the campaign's own progress is already saved.
void play_scene::_store_sandbox_game()
{
    if(g_request.campaign)
    {
        return;
    }

    save::SandboxGame& saved = settings::file().sandbox;
    const go::Game& g = session().game();
    const bool in_progress = g.phase() == go::Phase::PLAYING && g.move_count() > 0 &&
                             g.move_count() <= int(sizeof(saved.moves) / sizeof(saved.moves[0]));

    if(! in_progress)
    {
        if(saved.valid)
        {
            saved.valid = 0;
            settings::touch();
            settings::store();
        }

        return;
    }

    saved.valid = 1;
    saved.size = uint8_t(g_request.sandbox.size);
    saved.player_black = uint8_t(g_request.sandbox.player_black ? 1 : 0);
    saved.opponent = uint8_t(g_request.sandbox.opponent_id);
    saved.handicap = uint8_t(g_request.sandbox.handicap);
    saved.rules = uint8_t(g_request.sandbox.rules == go::Ruleset::TERRITORY ? 1 : 0);
    saved.hotseat = uint8_t(g_request.sandbox.hotseat ? 1 : 0);
    saved.komi_x2 = g_request.sandbox.komi_x2;
    saved.first_mover = uint8_t(g.setup_count() > 0 ? go::WHITE : go::BLACK);
    saved.move_count = uint16_t(g.move_count());

    for(int i = 0; i < g.move_count(); ++i)
    {
        const go::Move& m = g.moves()[i];
        saved.moves[i] = uint16_t(m.p == go::PASS ? 0xFFFF : m.p);
    }

    settings::touch();
    settings::store();
}

play_scene::~play_scene()
{
    _store_sandbox_game();
}

void play_scene::_build_hud()
{
    // The CLASSIC panel is ink-dark, so a muted label reads well on it and gives the values some
    // hierarchy. The PUP sky and the DINO volcano are busier: there the labels go full paper.
    const render::Ink label_ink = settings::skin() == 0 ? render::Ink::MUTED : render::Ink::PAPER;
    _label_turn.setup(HUD_X, LABEL_INK_Y[0], render::Font::SMALL, label_ink);
    _value_turn.setup(HUD_X, LABEL_INK_Y[0] + 12, render::Font::SMALL, render::Ink::PAPER);
    _label_capt.setup(HUD_X, LABEL_INK_Y[1], render::Font::SMALL, label_ink);
    _value_capt.setup(HUD_X, LABEL_INK_Y[1] + 12, render::Font::SMALL, render::Ink::PAPER);
    _label_move.setup(HUD_X, LABEL_INK_Y[2], render::Font::SMALL, label_ink);
    _value_move.setup(HUD_X, LABEL_INK_Y[2] + 12, render::Font::SMALL, render::Ink::PAPER);
    _label_komi.setup(HUD_X, LABEL_INK_Y[3], render::Font::SMALL, label_ink);
    _value_komi.setup(HUD_X, LABEL_INK_Y[3] + 12, render::Font::SMALL, render::Ink::PAPER);
    _label_panel.setup(HUD_X, PANEL_LABEL_Y, render::Font::SMALL, label_ink);

    for(int i = 0; i < 3; ++i)
    {
        _panel_lines[i].setup(HUD_X, PANEL_LINE_Y[i], render::Font::SMALL, render::Ink::PAPER);
    }

    for(int i = 0; i < 2; ++i)
    {
        _hints[i].setup(HUD_X, HINT_Y[i], render::Font::SMALL, label_ink);
    }

    _label_turn.set(tr(Str::HUDL_TO_PLAY));
    _label_capt.set(tr(Str::HUDL_CAPT));
    _label_move.set(tr(Str::HUDL_MOVE));
    _label_komi.set(tr(Str::HUDL_KOMI));
    _hints[0].set(tr(Str::HINT_A_PLACE));
    _hints[1].set(tr(Str::HINT_SEL_HINT));
}

int play_scene::_cell_index() const
{
    return cell_index_for(session().game().board().size());
}

void play_scene::_sync_board()
{
    const go::Board& b = session().game().board();
    const go::Point last = b.last_move();
    const bool marking = session().state() == game::PlayState::MARK_DEAD;
    const bool estimating = session().hud_panel() == game::HudPanel::ESTIMATE && session().estimate_valid();

    go::Point marked[8];
    int marked_count = 0;

    if(session().mode() == game::Mode::CAMPAIGN && session().mission())
    {
        marked_count = campaign::mission_marked_points(session().run(), marked, 8);
    }

    for(int y = 0; y < b.size(); ++y)
    {
        for(int x = 0; x < b.size(); ++x)
        {
            const go::Point p = b.point(x, y);
            const go::Color c = b.at(p);
            uint8_t flags = render::CELL_NONE;

            if(c != go::EMPTY)
            {
                if(p == last) flags = uint8_t(flags | render::CELL_LAST);
                if(b.in_atari(p)) flags = uint8_t(flags | render::CELL_ATARI);
                if(marking && session().is_dead(p)) flags = uint8_t(flags | render::CELL_DEAD);
            }
            else if(estimating)
            {
                const uint8_t owner = session().estimate().owner[p];

                if(owner == go::OWN_BLACK) flags = uint8_t(flags | render::CELL_TERR_BLACK);
                else if(owner == go::OWN_WHITE) flags = uint8_t(flags | render::CELL_TERR_WHITE);
            }

            for(int i = 0; i < marked_count; ++i)
            {
                if(marked[i] == p) flags = uint8_t(flags | render::CELL_MISSION);
            }

            _board.set_cell(x, y, c, flags);
        }
    }
}

void play_scene::_sync_cursor()
{
    const int cell = _cell_index();
    const bn::point screen = _board.cell_to_screen(session().cursor_x(), session().cursor_y());
    const int frame = settings::skin() * 2 + ((_anim / 24) % 2);

    if(! _cursor)
    {
        _cursor = cursor_item(cell).create_sprite(screen.x() - 120, screen.y() - 80, frame);
        _cursor->set_bg_priority(0);
    }

    _cursor->set_position(screen.x() - 120, screen.y() - 80);
    _cursor->set_tiles(cursor_item(cell).tiles_item(), frame);
    _cursor->set_visible(session().state() != game::PlayState::AI_THINKING &&
                         session().state() != game::PlayState::OVER);

    if(session().ghost_visible())
    {
        const int gframe = settings::skin() * 2 + (session().game().to_move() == go::BLACK ? 0 : 1);

        if(! _ghost)
        {
            _ghost = ghost_item(cell).create_sprite(screen.x() - 120, screen.y() - 80, gframe);
            _ghost->set_bg_priority(0);
            _ghost->set_z_order(1);
        }

        _ghost->set_position(screen.x() - 120, screen.y() - 80);
        _ghost->set_tiles(ghost_item(cell).tiles_item(), gframe);
        _ghost->set_visible(true);
    }
    else if(_ghost)
    {
        _ghost.reset();
    }
}

void play_scene::_update_hud()
{
    const go::Game& g = session().game();
    const go::Board& b = g.board();

    _value_turn.set(tr(colour_name(g.to_move())));
    _value_capt.set(bn::format<24>("{} {} · {} {}", tr(colour_letter(go::BLACK)), b.captures(go::BLACK),
                                   tr(colour_letter(go::WHITE)), b.captures(go::WHITE)));
    _value_move.set(bn::format<8>("{}", g.move_count()));
    _value_komi.set(half_points(g.settings().komi_x2));

    switch(session().hud_panel())
    {
    case game::HudPanel::MOVES:
    {
        _label_panel.set(tr(Str::HUDL_MOVES));
        const int count = g.move_count();

        for(int i = 0; i < 3; ++i)
        {
            const int index = count - 3 + i;

            if(index < 0)
            {
                _panel_lines[i].set(bn::string_view());
                continue;
            }

            const go::Move m = g.moves()[index];
            const char* who = tr(colour_letter(m.c));

            if(m.p == go::PASS)
            {
                _panel_lines[i].set(bn::format<24>("{} {} --", index + 1, who));
            }
            else
            {
                const char* letters = "ABCDEFGHJKLMNOPQRST";
                _panel_lines[i].set(bn::format<24>("{} {} {}{}", index + 1, who,
                                                   letters[b.x_of(m.p)], b.size() - b.y_of(m.p)));
            }
        }
        break;
    }

    case game::HudPanel::ESTIMATE:
    {
        _label_panel.set(tr(Str::HUDL_ESTIMATE));
        const go::ScoreResult& s = session().estimate();
        _panel_lines[0].set(bn::format<24>("{} {}", tr(colour_letter(go::BLACK)), half_points(s.black_x2)));
        _panel_lines[1].set(bn::format<24>("{} {}", tr(colour_letter(go::WHITE)), half_points(s.white_x2)));
        const int margin = s.margin_x2;
        _panel_lines[2].set(margin == 0 ? bn::string<24>(tr(Str::MSG_DRAW))
                                        : bn::format<24>("{} +{}", tr(colour_letter(margin > 0 ? go::BLACK : go::WHITE)),
                                                         half_points(margin > 0 ? margin : -margin)));
        break;
    }

    default:
        if(session().mode() == game::Mode::CAMPAIGN && session().mission())
        {
            _label_panel.set(tr(Str::HUDL_OBJECTIVE));
            // The objective is written for a 26 character dialogue line; the HUD column is half
            // that, so it is re-wrapped to the width that is actually there.
            bn::string<32> lines[3];
            render::wrap(tr_c(session().run().stage->objective_id), HUD_WIDTH, lines, 3);
            for(int i = 0; i < 3; ++i) _panel_lines[i].set(lines[i]);
        }
        else
        {
            _label_panel.set(tr(Str::HUDL_INFO));
            _panel_lines[0].set(bn::format<24>("{}x{}", b.size(), b.size()));
            _panel_lines[1].set(tr(g.settings().rules == go::Ruleset::AREA ? Str::VAL_AREA : Str::VAL_TERRITORY));
            _panel_lines[2].set(session().mode() == game::Mode::HOTSEAT
                                    ? bn::string<24>(tr(Str::VAL_HOTSEAT))
                                    : bn::string<24>(tr(Str(int(Str::VAL_LVL_VERY_EASY) + int(session().ai_level())))));
        }
        break;
    }
}

void play_scene::_sync_pause()
{
    const bool paused = session().state() == game::PlayState::PAUSED;

    if(paused && ! _pause_built)
    {
        _pause_built = true;
        _pause_title.setup(72, 16, render::Font::DISPLAY, render::Ink::ACCENT, render::Align::CENTER);
        _pause_title.set(tr(Str::TITLE_PAUSED));
        _pause_nav.setup(72, 150, render::Font::SMALL, render::Ink::MUTED, render::Align::CENTER);
        _pause_nav.set(tr(Str::NAV_PAUSE));
        _pause_menu.setup(30, 36, 14, 30);
        _pause_menu.add_item(tr(Str::MENU_RESUME));
        _pause_menu.add_item(tr(Str::MENU_UNDO));
        _pause_menu.add_item(tr(Str::MENU_HINT));
        _pause_menu.add_item(tr(Str::MENU_PASS));
        _pause_menu.add_item(tr(Str::MENU_ESTIMATE));
        _pause_menu.add_item(tr(Str::MENU_RESIGN));
        _pause_menu.add_item(tr(Str::MENU_OPTIONS));
        _pause_menu.add_item(tr(Str::MENU_QUIT));
        // the board dims behind the menu, as the mockup shows
        bn::bg_palettes::set_fade(bn::color(3, 3, 5), bn::fixed(0.55));
    }
    else if(! paused && _pause_built)
    {
        _pause_built = false;
        _pause_title.clear();
        _pause_nav.clear();
        _pause_menu.clear();
        bn::bg_palettes::set_fade_intensity(0);
    }

    if(_pause_built)
    {
        _pause_menu.set_index(int(session().pause_item()));
        _pause_menu.update();
    }
}

void play_scene::_show_line(uint16_t string_id, render::Pose pose)
{
    const int helper = settings::helper();
    const Str who = helper == 1 ? Str::WHO_INDI : (helper == 2 ? Str::WHO_REX : Str::WHO_SEN);
    _dialogue.show(tr_c(string_id), tr(who), pose);
}

int play_scene::_opponent_index() const
{
    const int id = int(g_request.sandbox.opponent_id);
    return id < 5 ? id : 0;
}

bool play_scene::_opponent_speaks() const
{
    return session().mode() == game::Mode::SANDBOX && ! g_request.sandbox.hotseat;
}

void play_scene::_opponent_say(game::Say what, bool force)
{
    if(! _opponent_speaks() || (_say_cooldown > 0 && ! force))
    {
        return;
    }

    const int who = _opponent_index();
    _fx.opponent_popin(tr_c(game::say_of(who, what)), who, 80);
    _say_cooldown = 150;
    // reserved[8] latches every line said this game, reserved[10] is the most recent one.
    g_moku_test.reserved[8] = uint8_t(g_moku_test.reserved[8] | (1 << int(what)));
    g_moku_test.reserved[10] = uint8_t(int(what) + 1);
}

void play_scene::_drain_events()
{
    game::Event e;

    while(session().poll(e))
    {
        switch(e.kind)
        {
        case game::EventKind::CURSOR_MOVED:
            audio::play(audio::Sfx::MENU_MOVE);
            break;

        case game::EventKind::GHOST_SHOWN:
            break;

        case game::EventKind::STONE_PLACED:
            audio::play(audio::Sfx::STONE);
            _sync_board();
            break;

        case game::EventKind::CAPTURE:
        {
            audio::play(audio::Sfx::CAPTURE);
            const bn::point at = _board.cell_to_screen(session().game().board().x_of(e.point),
                                                       session().game().board().y_of(e.point));
            _fx.burst(at.x(), at.y(), 6 + (e.count > 2 ? 2 : e.count), true);
            _fx.banner(bn::format<16>("{} +{}", tr(Str::BAN_CAPTURE), e.count), render::BannerStyle::SLANTED,
                       render::Ink::PAPER);
            _fx.shake(4, 2);

            const bool ai_captured = _opponent_speaks() &&
                                     e.color != (g_request.sandbox.player_black ? go::BLACK : go::WHITE);

            if(_opponent_speaks())
            {
                _opponent_say(ai_captured ? game::Say::CAPTURE : game::Say::CAPTURED, true);
            }
            else if(_fx.juicy())
            {
                // The helper cheers the player on; the tutor lines belong to the dialogue box.
                audio::play_helper_voice(true);
                _fx.helper_popin(tr_c(game::intj_of(settings::helper(), game::Intj::CAPTURE)),
                                 render::Pose::CHEER, 60);
            }
            break;
        }

        case game::EventKind::ATARI:
        {
            audio::play(audio::Sfx::ATARI);
            const Str banner = settings::skin() == 1 ? Str::BAN_ATARI_PUP
                             : settings::skin() == 2 ? Str::BAN_ATARI_DINO : Str::BAN_ATARI;
            _fx.banner(tr(banner), render::BannerStyle::STRAIGHT, render::Ink::PAPER);
            _fx.shake(4, 2);
            _fx.tint(6);

            if(e.color == (g_request.sandbox.player_black ? go::BLACK : go::WHITE))
            {
                _opponent_say(game::Say::ATARI);
            }

            _sync_board();
            break;
        }

        case game::EventKind::ILLEGAL:
            audio::play(audio::Sfx::ERROR);
            break;

        case game::EventKind::KO_BLOCKED:
            audio::play(audio::Sfx::ERROR);
            _fx.banner(tr(Str::BAN_KO));
            break;

        case game::EventKind::PASSED:
            audio::play(audio::Sfx::PASS);
            _fx.banner(tr(Str::BAN_PASS));
            break;

        case game::EventKind::UNDONE:
            audio::play(audio::Sfx::MENU_BACK);
            _sync_board();
            break;

        case game::EventKind::HINT_SHOWN:
            audio::play(audio::Sfx::HINT);
            break;

        case game::EventKind::AI_START:
            audio::play(audio::Sfx::THINK);

            if(! _said_intro)
            {
                _said_intro = true;
                _opponent_say(game::Say::INTRO, true);
            }
            break;

        case game::EventKind::AI_DONE:
            _sync_board();
            break;

        case game::EventKind::AI_RESIGNED:
            _fx.banner(tr(Str::BAN_RESIGN));
            break;

        case game::EventKind::MARK_PHASE:
            _dialogue.show_plain(tr(Str::MSG_MARK_DEAD));
            _sync_board();
            break;

        case game::EventKind::DEAD_TOGGLED:
            audio::play(audio::Sfx::MENU_OK);
            _sync_board();
            break;

        case game::EventKind::MISSION_CLEAR:
        case game::EventKind::STAGE_CLEAR:
            if(g_request.mission_index < campaign::MISSION_COUNT)
            {
                g_fail_counts[g_request.mission_index] = 0;
            }

            audio::play_music(audio::Track::CLEAR);
            _show_line(session().run().stage->text_win, render::Pose::CHEER);
            _dialogue_gate = true;
            break;

        case game::EventKind::MISSION_FAIL:
            if(g_request.mission_index < campaign::MISSION_COUNT &&
               g_fail_counts[g_request.mission_index] < 255)
            {
                ++g_fail_counts[g_request.mission_index];
            }

            audio::play_music(audio::Track::LOSE);
            _show_line(session().run().stage->text_fail, render::Pose::SAD);
            _dialogue_gate = true;
            break;

        case game::EventKind::SCORED:
            _sync_board();
            break;

        default:
            break;
        }
    }
}

int play_scene::_ai_work_budget() const
{
    return _ai_work;
}

// Playouts per second, on screen. L+R+SELECT toggles it; it draws over the top
// of the board and nowhere near the HUD, and it costs nothing while it is off.
void play_scene::_sync_debug_overlay()
{
    if(! _debug_overlay)
    {
        for(int i = 0; i < 4; ++i)
        {
            _debug[i].clear();
            _debug_shadow[i].clear();
        }

        return;
    }

    // Rebuilding eight blocks of text is real work, and the overlay is most interesting exactly
    // when the frame is already busy. Eight times a second is faster than anyone can read.
    if((_anim & 7) != 0 && ! _debug[0].empty())
    {
        return;
    }

    const ai::Stats& stats = session().ai_stats();
    const int frames = session().ai_frames();
    const int per_sec = (frames > 0 && stats.playouts > 0) ? (stats.playouts * 60) / frames : 0;

    const bn::string<32> text[4] = {
        bn::format<32>("L{} P{} {}/S", int(session().ai_level()), stats.playouts, per_sec),
        bn::format<32>("N{} C{} R{}", stats.nodes, stats.candidates, stats.ladders_read),
        bn::format<32>("CPU {}% M{}", int(g_moku_test.cpu_max_permille) / 10,
                       int(g_moku_test.max_missed_frames)),
        bn::format<32>("W{} PH{}", stats.winrate_permille, int(stats.phase)),
    };

    for(int i = 0; i < 4; ++i)
    {
        // Wood, lawn and slate are all different colours, so the overlay carries its own contrast:
        // dark text one pixel behind light text reads on every skin.
        if(_debug[i].empty())
        {
            _debug_shadow[i].setup(19, 21 + i * 11, render::Font::SMALL, render::Ink::INK,
                                   render::Align::LEFT);
            _debug_shadow[i].set_z_order(-39);
            _debug[i].setup(18, 20 + i * 11, render::Font::SMALL, render::Ink::PAPER,
                            render::Align::LEFT);
            _debug[i].set_z_order(-40);
        }

        _debug_shadow[i].set(text[i]);
        _debug[i].set(text[i]);
    }
}

void play_scene::_publish()
{
    const go::Game& g = session().game();
    g_moku_test.scene_sub = uint8_t(session().state());
    g_moku_test.board_size = uint8_t(g.board().size());
    g_moku_test.to_move = uint8_t(g.to_move());
    g_moku_test.phase = uint8_t(g.phase());
    g_moku_test.cursor_x = uint8_t(session().cursor_x());
    g_moku_test.cursor_y = uint8_t(session().cursor_y());
    g_moku_test.move_count = uint16_t(g.move_count());
    g_moku_test.caps_black = uint16_t(g.board().captures(go::BLACK));
    g_moku_test.caps_white = uint16_t(g.board().captures(go::WHITE));
    g_moku_test.mission_id = uint8_t(session().mission() ? session().mission()->id : 0);
    g_moku_test.mission_status = uint8_t(session().mission_status());
    g_moku_test.mission_stage = uint8_t(session().run().stage_index);
    g_moku_test.ai_level = uint8_t(session().ai_level());
    g_moku_test.ai_thinking = uint8_t(session().ai_thinking() ? 1 : 0);
    g_moku_test.playouts = uint32_t(session().ai_stats().playouts);
    g_moku_test.reserved[9] = uint8_t(_debug_overlay ? 1 : 0);
    g_moku_test.hud_panel = uint8_t(session().hud_panel());
    g_moku_test.menu_index = uint8_t(session().pause_item());
    g_moku_test.pending_confirm = uint8_t(session().ghost_visible() ? 1 : 0);
    g_moku_test.score_margin_x2 = int16_t(session().estimate_valid() ? session().estimate().margin_x2 : 0);
    g_moku_test.winner = uint8_t(g.winner());
    g_moku_test.opponent_id = uint8_t(session().mode() == game::Mode::HOTSEAT ? 0xFF : g_request.sandbox.opponent_id);

    if(session().ai_frames() > 0 && session().ai_stats().playouts > 0)
    {
        g_moku_test.playouts_per_sec = uint32_t((session().ai_stats().playouts * 60) / session().ai_frames());
    }
}

void play_scene::_on_finished()
{
    if(_finished_handled)
    {
        return;
    }

    _finished_handled = true;

    if(session().mode() == game::Mode::CAMPAIGN && session().mission())
    {
        const campaign::MissionRun& run = session().run();
        const bool cleared = session().mission_status() == campaign::MissionStatus::CLEAR;

        // A five-problem mission is only finished when the fifth problem is.
        if(cleared && run.def->stage_count > 0 && run.stage_index + 1 < run.def->stage_count)
        {
            PlayRequest next = g_request;
            next.stage = uint8_t(run.stage_index + 1);
            set_play_request(next);
            scene::request(SceneId::PLAY);
            return;
        }

        MissionResult result;
        result.index = g_request.mission_index;
        result.rank = int(campaign::mission_rank(run));
        result.stars = campaign::mission_stars(run);
        result.cleared = cleared;
        result.moves = run.player_moves;
        result.captures = run.captures;
        result.undos = run.undos;
        result.hints = run.hints;
        result.fail_text = run.fail_reason_id;
        result.unlock = run.def->unlock;
        set_mission_result(result);

        if(cleared)
        {
            unlocks::record_clear(result.index, result.rank, result.stars, run.def->unlock);
            settings::store();
        }

        scene::request(SceneId::MISSION_CLEAR);
        return;
    }

    GameResult result;
    result.score = session().game().phase() == go::Phase::OVER ? session().game().final_score() : session().game().score();
    result.resigned = session().game().end_reason() == go::EndReason::RESIGN;
    result.player_black = g_request.sandbox.player_black;
    result.hotseat = g_request.sandbox.hotseat;
    result.opponent_id = g_request.sandbox.opponent_id;
    result.board_size = g_request.sandbox.size;
    set_game_result(result);
    scene::request(SceneId::GAME_OVER);
}

void play_scene::update()
{
    ++_anim;

    if(_say_cooldown > 0)
    {
        --_say_cooldown;
    }

    if(_revision != settings::revision())
    {
        _revision = settings::revision();
        session().set_confirm_two_press(settings::confirm_2a());
        _fx.set_juicy(settings::effects() != 0);
        _fx.set_skin(settings::skin());
        _board.set_skin(skins::skin(skins::Skin(settings::skin())));
        _hud_bg = make_hud_bg();
        _hud_bg->set_priority(2);
        _build_hud();
        _sync_board();
    }

    game::Input input;
    input.held = uint16_t((bn::keypad::up_held() ? game::BTN_UP : 0) |
                          (bn::keypad::down_held() ? game::BTN_DOWN : 0) |
                          (bn::keypad::left_held() ? game::BTN_LEFT : 0) |
                          (bn::keypad::right_held() ? game::BTN_RIGHT : 0) |
                          (bn::keypad::a_held() ? game::BTN_A : 0) |
                          (bn::keypad::b_held() ? game::BTN_B : 0) |
                          (bn::keypad::l_held() ? game::BTN_L : 0) |
                          (bn::keypad::r_held() ? game::BTN_R : 0) |
                          (bn::keypad::start_held() ? game::BTN_START : 0) |
                          (bn::keypad::select_held() ? game::BTN_SELECT : 0));
    input.pressed = uint16_t((bn::keypad::up_pressed() ? game::BTN_UP : 0) |
                             (bn::keypad::down_pressed() ? game::BTN_DOWN : 0) |
                             (bn::keypad::left_pressed() ? game::BTN_LEFT : 0) |
                             (bn::keypad::right_pressed() ? game::BTN_RIGHT : 0) |
                             (bn::keypad::a_pressed() ? game::BTN_A : 0) |
                             (bn::keypad::b_pressed() ? game::BTN_B : 0) |
                             (bn::keypad::l_pressed() ? game::BTN_L : 0) |
                             (bn::keypad::r_pressed() ? game::BTN_R : 0) |
                             (bn::keypad::start_pressed() ? game::BTN_START : 0) |
                             (bn::keypad::select_pressed() ? game::BTN_SELECT : 0));

    if(bn::keypad::l_held() && bn::keypad::r_held() && bn::keypad::select_pressed())
    {
        _debug_overlay = ! _debug_overlay;
        audio::play(audio::Sfx::MENU_OK);
        input.pressed = uint16_t(input.pressed & ~(game::BTN_SELECT | game::BTN_L | game::BTN_R));
        input.held = uint16_t(input.held & ~(game::BTN_L | game::BTN_R));
    }

    // A dialogue holds the game until it is read.
    if(_dialogue_gate)
    {
        _dialogue.update();

        if(input.pressed & game::BTN_A)
        {
            if(! _dialogue.skip())
            {
                if(_pending_line)
                {
                    _dialogue.show_plain(tr_c(_pending_line));
                    _pending_line = 0;
                }
                else
                {
                    _dialogue.hide();
                    _dialogue_gate = false;

                    if(session().state() == game::PlayState::OVER) _on_finished();
                }
            }
        }
    }
    else
    {
        // A skips a running effect rather than placing a stone
        if((input.pressed & game::BTN_A) && _fx.busy())
        {
            _fx.skip();
            input.pressed = uint16_t(input.pressed & ~game::BTN_A);
        }

        bn::timer timer;
        session().update(input, _ai_work);

        // The AI is stepped one unit at a time and every unit is sized to fit inside a frame, so
        // there is nothing to tune here: the level's frame budget decides how much thinking it
        // gets, and the display keeps its sixty frames a second either way. The timer stays to
        // publish the worst frame we ever saw.
        const int ticks = timer.elapsed_ticks();

        {   // publish the worst frame seen, and what the session was doing when it happened
            const uint32_t worst = uint32_t(g_moku_test.reserved[4]) | (uint32_t(g_moku_test.reserved[5]) << 8);
            if(uint32_t(ticks) > worst)
            {
                g_moku_test.reserved[4] = uint8_t(ticks);
                g_moku_test.reserved[5] = uint8_t(ticks >> 8);
                g_moku_test.reserved[6] = uint8_t(session().ai_stats().phase);
                g_moku_test.reserved[7] = uint8_t(_ai_work);
            }
        }

        (void)AI_TICK_BUDGET;

        _dialogue.update();
    }

    _drain_events();

    if(session().state() == game::PlayState::OVER && ! _dialogue_gate) _on_finished();

    _sync_pause();
    _sync_debug_overlay();
    _fx.update();
    _board.set_offset(_fx.offset().x().integer(), _fx.offset().y().integer());
    _sync_cursor();
    _update_hud();
    _board.flush();
    _publish();
}

bool play_scene::handle_test_command(uint32_t cmd, uint32_t arg, uint32_t& result)
{
    switch(cmd)
    {
    case TCMD_PLAY_POINT:
    {
        const int x = int(arg & 0xFF);
        const int y = int((arg >> 8) & 0xFF);
        const go::Board& b = session().game().board();

        if(x >= b.size() || y >= b.size())
        {
            result = test_iface::TRES_BAD_ARG;
            return true;
        }

        session().force_move(b.point(x, y));
        _sync_board();
        result = test_iface::TRES_OK;
        return true;
    }

    case TCMD_PASS:
        session().force_move(go::PASS);
        result = test_iface::TRES_OK;
        return true;

    case TCMD_AI_FINISH_NOW:
        session().finish_ai_now();
        _sync_board();
        result = test_iface::TRES_OK;
        return true;

    case TCMD_BENCH_PLAYOUTS:
    {
        const int count = int(arg);

        if(count < 1 || count > 500)
        {
            result = test_iface::TRES_BAD_ARG;
            return true;
        }

        // The benchmark borrows the board the search plays its own simulations on, so it may only
        // run while nothing is thinking.
        if(session().ai_thinking())
        {
            result = test_iface::TRES_REFUSED;
            return true;
        }

        const go::Game& g = session().game();
        bn::timer timer;
        const int sum = ai::bench_playouts(g.board(), g.to_move(), g.settings().komi_x2, 0x5EEDu, count);
        const int ticks = int(timer.elapsed_ticks());
        (void)sum;
        // 32 bits only, deliberately: a 64-bit divide here drags libgcc's __aeabi_uldivmod (3 KB)
        // into IWRAM, and IWRAM is where the stack lives. count <= 500 and the timer runs at
        // 16 384 ticks a second, so the product is at most 8.2 million.
        result = ticks > 0 ? uint32_t((uint32_t(count) * uint32_t(bn::timers::ticks_per_second())) / uint32_t(ticks))
                           : 0u;
        return true;
    }

    case TCMD_AUTOPLAY:
    {
        const int max_moves = int(arg);

        if(max_moves < 1 || max_moves > 400)
        {
            result = test_iface::TRES_BAD_ARG;
            return true;
        }

        // Plays both sides with the policy a simulation uses, so a whole game reaches its score
        // screen in a frame instead of in the minutes the real opponents would take. The session
        // still does every piece of bookkeeping: events, passes, the end of the game.
        uint32_t seed = g_moku_test.rng_seed ? g_moku_test.rng_seed : 0xA51Eu;
        int played = 0;

        while(played < max_moves && session().game().phase() == go::Phase::PLAYING)
        {
            const go::Game& g = session().game();
            session().force_move(ai::quick_move(g.board(), g.to_move(), seed));
            ++played;
        }

        _sync_board();
        result = uint32_t(played);
        return true;
    }

    case TCMD_SKIP_EFFECTS:
        _fx.skip();

        if(_dialogue.visible())
        {
            _dialogue.skip();
            _dialogue.hide();
            _dialogue_gate = false;
        }

        result = test_iface::TRES_OK;
        return true;

    case TCMD_SET_SEED:
        session().set_seed(arg);
        result = test_iface::TRES_OK;
        return true;

    default:
        return false;
    }
}

}  // namespace scenes
