#include "game/render/dialogue.h"

#include "bn_display.h"
#include "bn_regular_bg_items_bg_panels.h"
#include "bn_sprite_tiles_ptr.h"
#include "bn_string.h"
#include "bn_sprite_items_helper_indi.h"
#include "bn_sprite_items_helper_rex.h"
#include "bn_sprite_items_helper_sen.h"
#include "bn_sprite_items_menu_cursor.h"

#include "game/audio.h"
#include "game/settings.h"
#include "game/test_iface.h"

namespace render {

namespace {

constexpr int BOX_TOP = 112;             // the paper panel covers rows 112..159
constexpr int TEXT_X = 62;               // right of the 48x48 portrait well
constexpr int TEXT_X_PLAIN = 12;
constexpr int LINE_Y[3] = { 120, 132, 144 };
constexpr int TYPE_FRAMES = 2;           // frames per character
constexpr int TICK_EVERY = 3;            // one typewriter click every three characters

const bn::sprite_item& helper_item(int helper)
{
    switch(helper)
    {
    case 1:  return bn::sprite_items::helper_indi;
    case 2:  return bn::sprite_items::helper_rex;
    default: return bn::sprite_items::helper_sen;
    }
}

int sprite_x(int screen_x) { return screen_x - (bn::display::width() / 2); }
int sprite_y(int screen_y) { return screen_y - (bn::display::height() / 2); }

}  // namespace

dialogue_box::dialogue_box() = default;

dialogue_box::~dialogue_box() = default;

void dialogue_box::show(const bn::string_view& text, const bn::string_view& who, Pose pose)
{
    _has_portrait = true;
    _pose = pose;
    _helper = settings::helper();

    if(! _bg)
    {
        _bg = bn::regular_bg_items::bg_panels.create_bg(0, 0, 0);
        _bg->set_priority(1);
    }

    _portrait = helper_item(_helper).create_sprite(sprite_x(BOX_TOP - 76), sprite_y(BOX_TOP + 24), int(pose));
    _portrait->set_position(sprite_x(32), sprite_y(136));
    _portrait->set_bg_priority(0);
    _portrait->set_z_order(-1);

    _name.setup(TEXT_X, BOX_TOP - 4, Font::SMALL, Ink::ACCENT, Align::LEFT);
    _name.set(who);

    // split into lines
    for(int i = 0; i < 3; ++i)
    {
        _full[i][0] = 0;
        _line_bytes[i] = 0;
        _lines[i].setup(TEXT_X, LINE_Y[i], Font::SMALL, Ink::INK, Align::LEFT);
        _lines[i].clear();
    }

    int line = 0;
    int col = 0;
    _total = 0;

    for(char c : text)
    {
        if(c == '\n')
        {
            _full[line][col] = 0;
            _line_bytes[line] = col;

            if(++line >= 3)
            {
                break;
            }

            col = 0;
            continue;
        }

        if(col < 39)
        {
            _full[line][col++] = c;
            ++_total;
        }
    }

    if(line < 3)
    {
        _full[line][col] = 0;
        _line_bytes[line] = col;
    }

    for(int i = 0; i < 32; ++i)
    {
        _probe[i] = (i < 31 && _full[0][i]) ? _full[0][i] : 0;
    }

    bn::string<32> probe;
    for(int i = 0; i < 31 && _full[0][i]; ++i)
    {
        probe.push_back(_full[0][i]);
    }

    for(int i = 0; i < 32; ++i)
    {
        g_moku_test.text_probe[i] = i < int(probe.size()) ? probe[i] : 0;
    }

    _shown = 0;
    _tick = 0;
    _typing = _total > 0;
    _visible = true;
    g_moku_test.dialogue_visible = 1;
    g_moku_test.event_flags |= 0x80;
    _apply_visible_text();
    set_prompt_visible(false);
}

void dialogue_box::show_plain(const bn::string_view& text)
{
    show(text, bn::string_view(), Pose::IDLE1);
    _has_portrait = false;
    _portrait.reset();
    _name.clear();

    for(int i = 0; i < 3; ++i)
    {
        _lines[i].set_position(TEXT_X_PLAIN, LINE_Y[i]);
    }
}

void dialogue_box::hide()
{
    _visible = false;
    _typing = false;
    _bg.reset();
    _portrait.reset();
    _prompt.reset();
    _name.clear();

    for(text_block& line : _lines)
    {
        line.clear();
    }

    g_moku_test.dialogue_visible = 0;
    g_moku_test.event_flags = uint8_t(g_moku_test.event_flags & ~0x80);
}

void dialogue_box::_apply_visible_text()
{
    int left = _shown;

    for(int i = 0; i < 3; ++i)
    {
        if(_line_bytes[i] <= 0)
        {
            _lines[i].set(bn::string_view());
            continue;
        }

        int take = left;

        if(take > _line_bytes[i])
        {
            take = _line_bytes[i];
        }

        if(take < 0)
        {
            take = 0;
        }

        // never cut a utf-8 sequence in half
        while(take > 0 && take < _line_bytes[i] && ((unsigned char)_full[i][take] & 0xC0) == 0x80)
        {
            --take;
        }

        _lines[i].set(bn::string_view(_full[i], take));
        left -= _line_bytes[i];

        if(left < 0)
        {
            left = 0;
        }
    }
}

void dialogue_box::_update_portrait()
{
    if(! _portrait || ! _has_portrait)
    {
        return;
    }

    // idle breathes between its two frames; every other pose holds
    if(_pose != Pose::IDLE1 && _pose != Pose::IDLE2)
    {
        return;
    }

    if(++_anim >= 40)
    {
        _anim = 0;
        _pose = (_pose == Pose::IDLE1) ? Pose::IDLE2 : Pose::IDLE1;
        _portrait->set_tiles(helper_item(_helper).tiles_item(), int(_pose));
    }
}

bool dialogue_box::update()
{
    if(! _visible)
    {
        return false;
    }

    _update_portrait();

    if(! _typing)
    {
        if(_prompt)
        {
            // the "press A" arrow bobs
            _prompt->set_y(_prompt->y() + ((_anim / 16) % 2 == 0 ? 0 : 0));
        }

        return false;
    }

    if(++_tick >= TYPE_FRAMES)
    {
        _tick = 0;
        ++_shown;

        if(_shown % TICK_EVERY == 0)
        {
            audio::play(audio::Sfx::TYPE);
        }

        _apply_visible_text();

        if(_shown >= _total)
        {
            _typing = false;
            set_prompt_visible(true);
        }
    }

    return _typing;
}

bool dialogue_box::skip()
{
    if(! _visible || ! _typing)
    {
        return false;
    }

    _shown = _total;
    _typing = false;
    _apply_visible_text();
    set_prompt_visible(true);
    return true;
}

void dialogue_box::set_pose(Pose pose)
{
    _pose = pose;

    if(_portrait && _has_portrait)
    {
        _portrait->set_tiles(helper_item(_helper).tiles_item(), int(pose));
    }
}

void dialogue_box::set_prompt_visible(bool visible)
{
    if(! visible)
    {
        _prompt.reset();
        return;
    }

    if(! _prompt)
    {
        _prompt = bn::sprite_items::menu_cursor.create_sprite(sprite_x(228), sprite_y(150), 2);
        _prompt->set_bg_priority(0);
    }
}

}  // namespace render
