#include "game/scenes/campaign_scenes.h"

#include "bn_display.h"
#include "bn_format.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_bg_map.h"
#include "bn_regular_bg_items_bg_panels.h"
#include "bn_sprite_items_map_node.h"
#include "bn_sprite_items_star.h"
#include "bn_string.h"

#include "game/audio.h"
#include "game/scenes/play_scene.h"
#include "game/settings.h"
#include "game/test_iface.h"
#include "game/unlocks.h"

namespace scenes {

namespace {

int g_mission = 0;
MissionResult g_result;

// The node path from the mockup: ten points that repeat, the second ten shifted a screen right.
constexpr int NODE_X[10] = { 16, 40, 64, 88, 112, 136, 160, 184, 208, 232 };
constexpr int NODE_Y[10] = { 108, 96, 110, 88, 100, 76, 86, 64, 74, 52 };

int node_x(int index) { return NODE_X[index % 10] + (index >= 10 ? 240 : 0); }
int node_y(int index) { return NODE_Y[index % 10]; }

Str chapter_of(int index)
{
    if(index < 4) return Str::CHAP_1;
    if(index < 9) return Str::CHAP_2;
    if(index < 15) return Str::CHAP_3;
    return Str::CHAP_4;
}

void split_lines(const char* text, render::text_block* lines, int count)
{
    int line = 0;
    bn::string<40> current;

    for(const char* c = text; line < count; ++c)
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

    for(; line < count; ++line) lines[line].set(bn::string_view());
}

scene::Scene* factory(SceneId id, uint32_t arg)
{
    (void)arg;

    switch(id)
    {
    case SceneId::CAMPAIGN_MAP:   return new map_scene();
    case SceneId::BRIEFING:       return new briefing_scene();
    case SceneId::MISSION_CLEAR:  return new clear_scene();
    default:                      return nullptr;
    }
}

scene::FactoryRegistrar registrar(factory);

}  // namespace

void set_campaign_mission(int index) { g_mission = index; }
int campaign_mission() { return g_mission; }
void set_mission_result(const MissionResult& result) { g_result = result; }
const MissionResult& mission_result() { return g_result; }

// --------------------------------------------------------------------------------------------
// campaign map
// --------------------------------------------------------------------------------------------
map_scene::map_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_map.create_bg(0, 0);
    _bg->set_priority(3);

    _header.setup(8, 4, render::Font::SMALL, render::Ink::INK, render::Align::LEFT);
    _name.setup(8, 140, render::Font::SMALL, render::Ink::ACCENT, render::Align::LEFT);
    _chapter.setup(8, 150, render::Font::SMALL, render::Ink::INK, render::Align::LEFT);
    _nav.setup(232, 150, render::Font::SMALL, render::Ink::INK, render::Align::RIGHT);
    _nav.set(tr(Str::NAV_MAP));
    _more.setup(232, 4, render::Font::SMALL, render::Ink::INK, render::Align::RIGHT);

    for(int i = 0; i < campaign::MISSION_COUNT; ++i)
    {
        bn::sprite_ptr node = bn::sprite_items::map_node.create_sprite(0, 0, 4);
        node.set_bg_priority(1);
        _nodes.push_back(bn::move(node));
        // the number rides on the node, as in the mockup
        _numbers[i].setup(0, 0, render::Font::SMALL, render::Ink::PAPER, render::Align::CENTER);
    }

    for(int i = 0; i < 3; ++i)
    {
        bn::sprite_ptr star = bn::sprite_items::star.create_sprite((96 + i * 10) - (bn::display::width() / 2),
                                                                  142 - (bn::display::height() / 2), 1);
        star.set_bg_priority(1);
        _stars.push_back(bn::move(star));
    }

    _index = settings::slot().cleared_count;

    if(_index >= campaign::MISSION_COUNT)
    {
        _index = campaign::MISSION_COUNT - 1;
    }

    _scroll = _target_scroll = (_index >= 10) ? 240 : 0;
    _refresh();
}

map_scene::~map_scene() = default;

void map_scene::_scroll_to(int index)
{
    _target_scroll = (index >= 10) ? 240 : 0;
}

void map_scene::_refresh()
{
    const int cleared = settings::slot().cleared_count;

    for(int i = 0; i < campaign::MISSION_COUNT; ++i)
    {
        const bool done = i < cleared;
        const bool current = i == _index;
        int frame = done ? 0 : (unlocks::mission_available(i) ? 2 : 4);

        if(current)
        {
            frame = done ? ((_anim / 20) % 2) : ((_anim / 20) % 2 ? 3 : 2);
        }

        _nodes[i].set_tiles(bn::sprite_items::map_node.tiles_item(), frame);
        _nodes[i].set_position(node_x(i) - _scroll - (bn::display::width() / 2),
                               node_y(i) - (bn::display::height() / 2));
        const bool on_screen = node_x(i) - _scroll > -16 && node_x(i) - _scroll < 256;
        _nodes[i].set_visible(on_screen);

        // The number rides on the node, as in the mockup - but only where there is room for it: a
        // mission still out of reach draws a padlock instead, and two glyphs on one 14 px disc
        // read as neither.
        if(on_screen && frame < 4)
        {
            _numbers[i].set_ink(render::Ink::PAPER);
            _numbers[i].set_position(node_x(i) - _scroll, node_y(i));
            _numbers[i].set(bn::format<4>("{}", i + 1));
        }
        else
        {
            _numbers[i].clear();
        }
    }

    const campaign::MissionDef& def = campaign::mission(_index);
    _header.set(bn::format<20>("{} {}", tr(Str::HUDL_MISSION), _index + 1));
    _name.set(unlocks::mission_available(_index) ? tr(Str(def.name_id)) : tr(Str::VAL_LOCKED));
    _chapter.set(tr(chapter_of(_index)));
    _more.set(_index < 10 ? tr(Str::HINT_MAP_MORE) : bn::string_view());

    const int rank = unlocks::mission_rank(_index);

    for(int i = 0; i < 3; ++i)
    {
        // three stars: cleared, rank A or better, and the starred flag (no hints, no undos)
        const bool filled = (i == 0 && rank > 0) ||
                            (i == 1 && rank >= int(campaign::Rank::A)) ||
                            (i == 2 && unlocks::mission_starred(_index));
        _stars[i].set_tiles(bn::sprite_items::star.tiles_item(), filled ? 0 : 1);
    }
}

void map_scene::update()
{
    ++_anim;

    if(_scroll != _target_scroll)
    {
        const int step = _target_scroll > _scroll ? 12 : -12;
        _scroll += step;

        if((step > 0 && _scroll > _target_scroll) || (step < 0 && _scroll < _target_scroll))
        {
            _scroll = _target_scroll;
        }

        _bg->set_x(_scroll);
    }

    int delta = 0;

    if(bn::keypad::left_pressed()) delta = -1;
    if(bn::keypad::right_pressed()) delta = 1;
    if(bn::keypad::up_pressed()) delta = -1;
    if(bn::keypad::down_pressed()) delta = 1;

    if(delta != 0)
    {
        int index = _index + delta;

        if(index < 0) index = 0;
        if(index >= campaign::MISSION_COUNT) index = campaign::MISSION_COUNT - 1;

        if(index != _index)
        {
            _index = index;
            _scroll_to(_index);
            audio::play(audio::Sfx::MENU_MOVE);
        }
    }

    if(bn::keypad::a_pressed())
    {
        if(unlocks::mission_available(_index))
        {
            set_campaign_mission(_index);
            audio::play(audio::Sfx::MENU_OK);
            scene::request(SceneId::BRIEFING);
        }
        else
        {
            audio::play(audio::Sfx::ERROR);
        }
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::TITLE);
    }

    audio::play_music(audio::Track::MAP);
    _refresh();
    g_moku_test.menu_index = uint8_t(_index);
    g_moku_test.scene_sub = uint8_t(_index);
}

// --------------------------------------------------------------------------------------------
// briefing
// --------------------------------------------------------------------------------------------
briefing_scene::briefing_scene()
{
    render::text_init();
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 1);
    _bg->set_priority(2);

    const campaign::MissionDef& def = campaign::mission(campaign_mission());

    _number.setup(8, 6, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _number.set(bn::format<20>("{} {}", tr(Str::HUDL_MISSION), def.id));
    _name.setup(8, 18, render::Font::DISPLAY, render::Ink::ACCENT, render::Align::LEFT);
    _name.set(tr(Str(def.name_id)));

    _objective_label.setup(8, 42, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _objective_label.set(tr(Str::HUDL_OBJECTIVE));

    for(int i = 0; i < 2; ++i)
    {
        _objective[i].setup(8, 54 + i * 10, render::Font::SMALL, render::Ink::PAPER, render::Align::LEFT);
    }

    // The objective box is 224 px wide and two lines tall. Wrapping it (rather than splitting on
    // the author's newlines) means a translation that needs its break somewhere else still fits.
    {
        bn::string<32> wrapped[2];
        const int count = render::wrap(tr(Str(def.objective_id)), 112, wrapped, 2);

        for(int i = 0; i < 2; ++i)
        {
            _objective[i].set(i < count ? bn::string_view(wrapped[i]) : bn::string_view());
        }
    }

    _rank_label.setup(8, 78, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _rank_label.set(tr(Str::HUDL_RANK));

    for(int i = 0; i < 2; ++i)
    {
        _rank_hint[i].setup(8, 90 + i * 10, render::Font::SMALL, render::Ink::PAPER, render::Align::LEFT);
    }

    split_lines(def.rank_hint_id ? tr(Str(def.rank_hint_id)) : "", _rank_hint, 2);

    _nav.setup(232, 6, render::Font::SMALL, render::Ink::MUTED, render::Align::RIGHT);
    _nav.set(tr(Str::NAV_BRIEFING));

    const int helper = settings::helper();
    const Str who = helper == 1 ? Str::WHO_INDI : (helper == 2 ? Str::WHO_REX : Str::WHO_SEN);
    _dialogue.show(tr(Str(def.text_before)), tr(who), render::Pose::IDLE1);
}

briefing_scene::~briefing_scene() = default;

void briefing_scene::update()
{
    ++_anim;
    _dialogue.update();

    if(bn::keypad::a_pressed())
    {
        if(_dialogue.skip())
        {
            return;
        }

        const campaign::MissionDef& def = campaign::mission(campaign_mission());

        // From mission 15 the rival has something to say before the game starts.
        if(! _rival_shown && def.id >= 15)
        {
            _rival_shown = true;
            const int rival = int(Str::DLG_RIVAL_M15) + (def.id - 15);
            _dialogue.show(tr(Str(rival)), tr(Str::OPP_EMBER), render::Pose::THINK);
            audio::play(audio::Sfx::MENU_OK);
            return;
        }

        PlayRequest request;
        request.campaign = true;
        request.mission_index = uint8_t(campaign_mission());
        request.stage = 0;
        set_play_request(request);
        audio::play(audio::Sfx::MENU_OK);
        scene::request(SceneId::PLAY);
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::CAMPAIGN_MAP);
    }

    g_moku_test.mission_id = uint8_t(campaign::mission(campaign_mission()).id);
    g_moku_test.scene_sub = uint8_t(_rival_shown ? 1 : 0);
}

// --------------------------------------------------------------------------------------------
// mission clear / fail
// --------------------------------------------------------------------------------------------
clear_scene::clear_scene()
{
    render::text_init();
    // Map 2 is the ink bar across the bottom: the whole screen stays dark, so every line of text
    // on it is light and readable.
    _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 2);
    _bg->set_priority(2);
    _fx.set_skin(settings::skin());
    _fx.set_juicy(settings::effects() != 0);

    const MissionResult& r = mission_result();
    const campaign::MissionDef& def = campaign::mission(r.index);

    _title.setup(120, 10, render::Font::DISPLAY, render::Ink::ACCENT, render::Align::CENTER);
    _title.set(tr(r.cleared ? Str::TITLE_MISSION_CLEAR : Str::TITLE_MISSION_FAIL));
    _subtitle.setup(120, 30, render::Font::SMALL, render::Ink::MUTED, render::Align::CENTER);
    _subtitle.set(bn::format<28>("{} · {}", def.id, tr(Str(def.name_id))));

    const Str labels[4] = { Str::HUDL_MOVES, Str::HUDL_CAPTURES, Str::HUDL_UNDOS, Str::HUDL_HINTS };
    const int values[4] = { r.moves, r.captures, r.undos, r.hints };

    for(int i = 0; i < 4; ++i)
    {
        _labels[i].setup(136, 48 + i * 16, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
        _labels[i].set(tr(labels[i]));
        _values[i].setup(226, 48 + i * 16, render::Font::SMALL, render::Ink::PAPER, render::Align::RIGHT);
        _values[i].set(bn::format<8>("{}", values[i]));
    }

    _stars_label.setup(136, 112, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _stars_label.set(tr(Str::HUDL_STARS));

    for(int i = 0; i < 3; ++i)
    {
        const bool filled = r.cleared && ((i == 0) || (i == 1 && r.rank >= int(campaign::Rank::A)) || (i == 2 && r.stars));
        bn::sprite_ptr star = bn::sprite_items::star.create_sprite((190 + i * 10) - (bn::display::width() / 2),
                                                                  114 - (bn::display::height() / 2), filled ? 0 : 1);
        star.set_bg_priority(1);
        _stars.push_back(bn::move(star));
    }

    _unlock_label.setup(8, 104, render::Font::SMALL, render::Ink::MUTED, render::Align::LEFT);
    _unlock_value.setup(8, 114, render::Font::SMALL, render::Ink::ACCENT, render::Align::LEFT);

    if(r.cleared && r.unlock != campaign::UNLOCK_NONE)
    {
        _unlock_label.set(tr(Str::HUDL_UNLOCKED));
        const int id = int(Str::MSG_UNLOCK_SANDBOX_9_EASY) + (r.unlock - 1);
        _unlock_value.set(tr(Str(id)));
    }

    _menu.setup(28, 134, 13, 28);

    if(r.cleared)
    {
        const bool more = r.index + 1 < campaign::MISSION_COUNT;

        if(more) _menu.add_item(tr(Str::MENU_NEXT));

        _menu.add_item(tr(Str::MENU_MAP));
    }
    else
    {
        _menu.add_item(tr(Str::MENU_RETRY));
        _menu.add_item(tr(Str::MENU_MAP));
    }

    audio::play_music(r.cleared ? audio::Track::CLEAR : audio::Track::LOSE);
}

clear_scene::~clear_scene() = default;

void clear_scene::update()
{
    ++_anim;
    _menu.update();
    _fx.update();

    if(! _stamped && _anim == 16)
    {
        _stamped = true;
        const MissionResult& r = mission_result();

        if(r.cleared)
        {
            _fx.stamp(int(campaign::Rank::S) - r.rank);
            audio::play(audio::Sfx::CHEER);
        }
    }

    if(bn::keypad::up_pressed() && _menu.move(-1)) audio::play(audio::Sfx::MENU_MOVE);
    if(bn::keypad::down_pressed() && _menu.move(1)) audio::play(audio::Sfx::MENU_MOVE);

    if(bn::keypad::a_pressed())
    {
        const MissionResult& r = mission_result();
        const bool has_next = r.cleared && r.index + 1 < campaign::MISSION_COUNT;
        audio::play(audio::Sfx::MENU_OK);

        if(! r.cleared && _menu.index() == 0)
        {
            PlayRequest request;
            request.campaign = true;
            request.mission_index = uint8_t(r.index);
            set_play_request(request);
            scene::request(SceneId::PLAY);
            return;
        }

        if(has_next && _menu.index() == 0)
        {
            set_campaign_mission(r.index + 1);
            scene::request(SceneId::BRIEFING);
            return;
        }

        scene::request(SceneId::CAMPAIGN_MAP);
    }

    if(bn::keypad::b_pressed())
    {
        audio::play(audio::Sfx::MENU_BACK);
        scene::request(SceneId::CAMPAIGN_MAP);
    }

    g_moku_test.menu_index = uint8_t(_menu.index());
    g_moku_test.mission_status = uint8_t(mission_result().cleared ? 1 : 2);
    g_moku_test.mission_id = uint8_t(campaign::mission(mission_result().index).id);
}

}  // namespace scenes
