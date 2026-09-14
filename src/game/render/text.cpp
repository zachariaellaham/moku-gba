#include "game/render/text.h"

#include "bn_display.h"
#include "bn_sprite_palette_item.h"

namespace render {

namespace {

// One 16-colour palette per ink. Index 1 is the glyph colour, the rest are unused by the fonts.
constexpr bn::color INK_COLOURS[int(Ink::COUNT)] = {
    bn::color(3, 3, 5),        // INK      #1b1f2a
    bn::color(29, 28, 26),     // PAPER    #efe6d2
    bn::color(24, 7, 5),       // ACCENT   #c23b2c
    bn::color(17, 16, 14),     // MUTED    #8a8070
    bn::color(28, 20, 6),      // GOLD     #e0a030
};

alignas(4) bn::color g_palette_colours[int(Ink::COUNT)][16];
bn::optional<bn::sprite_palette_item> g_palette_items[int(Ink::COUNT)];
bn::optional<bn::sprite_text_generator> g_generators[2][int(Ink::COUNT)];
bool g_ready = false;

}  // namespace

void text_init()
{
    if(g_ready)
    {
        return;
    }

    for(int ink = 0; ink < int(Ink::COUNT); ++ink)
    {
        for(int i = 0; i < 16; ++i)
        {
            g_palette_colours[ink][i] = INK_COLOURS[ink];
        }

        g_palette_items[ink].emplace(bn::span<const bn::color>(g_palette_colours[ink], 16), bn::bpp_mode::BPP_4);
        g_generators[0][ink].emplace(fonts::moku_font_8, *g_palette_items[ink]);
        g_generators[1][ink].emplace(fonts::moku_font_16, *g_palette_items[ink]);
    }

    g_ready = true;
}

bn::sprite_text_generator& generator(Font font, Ink ink)
{
    return *g_generators[int(font)][int(ink)];
}

int text_width(const bn::string_view& text, Font font)
{
    return generator(font, Ink::INK).width(text);
}

int chars_that_fit(const bn::string_view& text, int max_width, Font font)
{
    // utf-8: never split a multi-byte character.
    int bytes = 0;
    int last_fit = 0;

    while(bytes < int(text.size()))
    {
        int step = 1;
        const unsigned char c = (unsigned char)text[bytes];

        if(c >= 0xF0)
        {
            step = 4;
        }
        else if(c >= 0xE0)
        {
            step = 3;
        }
        else if(c >= 0xC0)
        {
            step = 2;
        }

        bytes += step;

        if(generator(font, Ink::INK).width(bn::string_view(text.data(), bytes)) > max_width)
        {
            return last_fit;
        }

        last_fit = bytes;
    }

    return last_fit;
}

int wrap(const bn::string_view& text, int max_width, bn::string<32>* out, int max_lines, Font font)
{
    int line = 0;
    bn::string<32> current;
    bn::string<32> word;

    auto flush_word = [&]()
    {
        if(word.empty() || line >= max_lines)
        {
            return;
        }

        bn::string<32> candidate = current;

        if(! candidate.empty())
        {
            candidate.append(" ");
        }

        candidate.append(word);

        if(! current.empty() && text_width(candidate, font) > max_width)
        {
            out[line++] = current;
            current = word;
        }
        else
        {
            current = candidate;
        }

        word.clear();
    };

    for(char c : text)
    {
        if(c == '\n')
        {
            flush_word();

            if(line < max_lines)
            {
                out[line++] = current;
            }

            current.clear();
            continue;
        }

        if(c == ' ')
        {
            flush_word();
            continue;
        }

        if(word.size() + 1 < word.max_size())
        {
            word.push_back(c);
        }
    }

    flush_word();

    if(line < max_lines && ! current.empty())
    {
        out[line++] = current;
    }

    for(int i = line; i < max_lines; ++i)
    {
        out[i].clear();
    }

    return line;
}

void text_block::setup(int x, int y, Font font, Ink ink, Align align)
{
    _x = x;
    _y = y;
    _font = font;
    _ink = ink;
    _align = align;
    _text.clear();
    _sprites.clear();
    _width = 0;
}

void text_block::set(const bn::string_view& text)
{
    if(bn::string_view(_text) == text && ! _dirty)
    {
        return;
    }

    force(text);
}

void text_block::force(const bn::string_view& text)
{
    _text.clear();

    // A block is one line. Multi-line strings from the table are split by the caller; stopping at
    // the first newline here means a stray '\n' shortens the line instead of asserting inside the
    // text generator, which has no glyph for it.
    for(char c : text)
    {
        if(c == '\n' || c == '\r')
        {
            break;
        }

        if(c > 0 && c < ' ')
        {
            continue;                     // other control characters have no glyph either
        }

        if(_text.size() + 1 >= _text.max_size())
        {
            break;
        }

        _text.push_back(c);
    }

    _regenerate();
}

void text_block::_regenerate()
{
    _sprites.clear();

    if(_text.empty() || ! _visible)
    {
        _width = 0;
        _dirty = false;
        return;
    }

    bn::sprite_text_generator& gen = generator(_font, _ink);
    gen.set_alignment(_align);
    gen.set_z_order(_z_order);
    gen.set_bg_priority(_bg_priority);
    _width = gen.width(_text);
    gen.generate(_x - (bn::display::width() / 2), _y - (bn::display::height() / 2), _text, _sprites);
    _dirty = false;
}

void text_block::clear()
{
    _sprites.clear();
    _text.clear();
    _width = 0;
}

void text_block::set_visible(bool visible)
{
    if(_visible == visible)
    {
        return;
    }

    _visible = visible;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_visible(visible);
    }

    if(visible && _sprites.empty() && ! _text.empty())
    {
        _regenerate();
    }
}

void text_block::set_position(int x, int y)
{
    if(_x == x && _y == y)
    {
        return;
    }

    const int dx = x - _x;
    const int dy = y - _y;
    _x = x;
    _y = y;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_position(sprite.x() + dx, sprite.y() + dy);
    }
}

void text_block::set_ink(Ink ink)
{
    if(_ink == ink)
    {
        return;
    }

    _ink = ink;
    _dirty = true;
    _regenerate();
}

void text_block::set_bg_priority(int priority)
{
    _bg_priority = priority;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_bg_priority(priority);
    }
}

void text_block::set_z_order(int z_order)
{
    _z_order = z_order;

    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.set_z_order(z_order);
    }
}

void text_block::set_camera_ignored()
{
    for(bn::sprite_ptr& sprite : _sprites)
    {
        sprite.remove_camera();
    }
}

}  // namespace render
