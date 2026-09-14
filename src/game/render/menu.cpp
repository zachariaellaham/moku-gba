#include "game/render/menu.h"

#include "bn_display.h"
#include "bn_sprite_items_menu_cursor.h"

namespace render {

void menu::setup(int x, int y, int pitch, int value_x, Font font)
{
    _x = x;
    _y = y;
    _pitch = pitch;
    _value_x = value_x;
    _font = font;
    _index = 0;
    clear();
}

void menu::clear()
{
    _rows.clear();
    _cursor.reset();
}

void menu::add_item(const bn::string_view& label)
{
    if(_rows.full())
    {
        return;
    }

    const int row = int(_rows.size());
    _rows.emplace_back();
    Row& r = _rows.back();
    r.kind = RowKind::ITEM;
    r.label.setup(_x, _y + row * _pitch, _font, Ink::PAPER, Align::LEFT);
    r.label.set(label);
    _refresh_cursor();
}

void menu::add_choice(const bn::string_view& label, const bn::string_view& value)
{
    if(_rows.full())
    {
        return;
    }

    const int row = int(_rows.size());
    _rows.emplace_back();
    Row& r = _rows.back();
    r.kind = RowKind::CHOICE;
    r.label.setup(_x, _y + row * _pitch, _font, Ink::MUTED, Align::LEFT);
    r.label.set(label);
    r.value.setup(_value_x, _y + row * _pitch, _font, Ink::PAPER, Align::LEFT);
    r.value.set(value);
    _refresh_cursor();
}

void menu::add_header(const bn::string_view& label)
{
    if(_rows.full())
    {
        return;
    }

    const int row = int(_rows.size());
    _rows.emplace_back();
    Row& r = _rows.back();
    r.kind = RowKind::HEADER;
    r.enabled = false;
    r.label.setup(_x, _y + row * _pitch, _font, Ink::ACCENT, Align::LEFT);
    r.label.set(label);
}

void menu::set_label(int row, const bn::string_view& label)
{
    if(row >= 0 && row < int(_rows.size()))
    {
        _rows[row].label.set(label);
    }
}

void menu::set_value(int row, const bn::string_view& value)
{
    if(row >= 0 && row < int(_rows.size()))
    {
        _rows[row].value.set(value);
    }
}

void menu::set_row_enabled(int row, bool enabled)
{
    if(row < 0 || row >= int(_rows.size()))
    {
        return;
    }

    _rows[row].enabled = enabled;
    _rows[row].label.set_ink(enabled ? (_rows[row].kind == RowKind::CHOICE ? Ink::MUTED : Ink::PAPER) : Ink::MUTED);

    if(! enabled && _index == row)
    {
        move(1);
    }
}

bool menu::move(int delta)
{
    if(_rows.empty())
    {
        return false;
    }

    const int start = _index;
    int index = _index;

    for(int guard = 0; guard < int(_rows.size()); ++guard)
    {
        index += delta > 0 ? 1 : -1;

        if(index < 0)
        {
            index = int(_rows.size()) - 1;
        }
        else if(index >= int(_rows.size()))
        {
            index = 0;
        }

        if(_rows[index].kind != RowKind::HEADER && _rows[index].enabled)
        {
            _index = index;
            _refresh_cursor();
            return _index != start;
        }
    }

    return false;
}

void menu::set_index(int index)
{
    if(index >= 0 && index < int(_rows.size()))
    {
        _index = index;
        _refresh_cursor();
    }
}

void menu::_refresh_cursor()
{
    if(_rows.empty() || ! _visible)
    {
        return;
    }

    // the cursor never rests on a header
    while(_index < int(_rows.size()) && (_rows[_index].kind == RowKind::HEADER || ! _rows[_index].enabled))
    {
        ++_index;
    }

    if(_index >= int(_rows.size()))
    {
        _index = 0;
    }

    const int x = _x - 10 - (bn::display::width() / 2);
    const int y = _y + _index * _pitch - (bn::display::height() / 2);

    if(! _cursor)
    {
        _cursor = bn::sprite_items::menu_cursor.create_sprite(x, y, 0);
        _cursor->set_bg_priority(0);
    }
    else
    {
        _cursor->set_position(x, y);
    }
}

void menu::update()
{
    ++_anim;

    if(_cursor)
    {
        _cursor->set_tiles(bn::sprite_items::menu_cursor.tiles_item(), (_anim / 24) % 2);
    }
}

void menu::set_visible(bool visible)
{
    _visible = visible;

    for(Row& row : _rows)
    {
        row.label.set_visible(visible);
        row.value.set_visible(visible);
    }

    if(_cursor)
    {
        _cursor->set_visible(visible);
    }
    else if(visible)
    {
        _refresh_cursor();
    }
}

}  // namespace render
