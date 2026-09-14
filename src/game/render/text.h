// Text on screen, in the MOKU fonts, with as little churn as possible.
//
// A `text_block` owns the sprites of a few lines of text. Calling set() with the same string is
// free; calling it with a new one regenerates only that block. Scenes keep one block per region
// of the screen (HUD labels, HUD values, the dialogue box, a menu column) so a changing move
// counter never rebuilds the whole screen.
#pragma once

#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_vector.h"

#include "moku_fonts.h"

namespace render {

enum class Font : uint8_t { SMALL = 0, DISPLAY = 1 };
using Align = bn::sprite_text_generator::alignment_type;

// Palette index 1 of every font sprite is recoloured through these named palettes.
enum class Ink : uint8_t { INK = 0, PAPER = 1, ACCENT = 2, MUTED = 3, GOLD = 4, COUNT = 5 };

// Must be called once, after bn::core::init().
void text_init();

// The shared generators, already set up with the right font and palette. Prefer text_block.
bn::sprite_text_generator& generator(Font font, Ink ink);

// Width in pixels the small font needs for `text` (utf-8 aware).
[[nodiscard]] int text_width(const bn::string_view& text, Font font = Font::SMALL);

// Number of characters of `text` that fit in `max_width` pixels (for the typewriter effect).
[[nodiscard]] int chars_that_fit(const bn::string_view& text, int max_width, Font font = Font::SMALL);

// Breaks `text` at spaces so no line is wider than `max_width` pixels, writing at most `max_lines`
// lines into `out`. Explicit '\n' in the string still starts a new line. Returns the line count.
int wrap(const bn::string_view& text, int max_width, bn::string<32>* out, int max_lines,
         Font font = Font::SMALL);

constexpr int MAX_BLOCK_SPRITES = 24;

class text_block {

public:
    text_block() = default;

    // x, y are screen pixels (0..239, 0..159); the block places its baseline at y.
    void setup(int x, int y, Font font = Font::SMALL, Ink ink = Ink::INK, Align align = Align::LEFT);

    // Replaces the text. Does nothing when `text` is what is already shown.
    void set(const bn::string_view& text);

    // Replaces the text even if it looks unchanged (after a palette or language switch).
    void force(const bn::string_view& text);

    void clear();
    void set_visible(bool visible);
    void set_position(int x, int y);
    void set_ink(Ink ink);
    void set_z_order(int z_order);
    // Butano gives a new sprite background-priority 3, which puts it behind every panel in this
    // game. Text belongs in front, so a block defaults to 0 and says so out loud.
    void set_bg_priority(int priority);
    void set_camera_ignored();

    [[nodiscard]] bool empty() const { return _sprites.empty(); }
    [[nodiscard]] int width() const { return _width; }

private:
    void _regenerate();

    bn::vector<bn::sprite_ptr, MAX_BLOCK_SPRITES> _sprites;
    bn::string<48> _text;
    int _x = 0, _y = 0;
    int _width = 0;
    int _z_order = 0;
    int _bg_priority = 0;
    Font _font = Font::SMALL;
    Ink _ink = Ink::INK;
    Align _align = Align::LEFT;
    bool _visible = true;
    bool _dirty = false;
};

}  // namespace render
