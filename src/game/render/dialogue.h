// The dialogue box: the paper panel across the bottom 48 px, a 48x48 helper portrait, a name
// plate and up to three lines of text that type themselves out. Used by the briefing, by the play
// scene's tutor lines and by the mission-clear screen.
//
// The text comes from the string table already broken into lines with '\n'; nothing re-wraps it.
#pragma once

#include <cstdint>

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_string_view.h"

#include "game/render/text.h"

namespace render {

// Which pose the portrait shows.
enum class Pose : uint8_t { PORTRAIT = 0, IDLE1 = 1, IDLE2 = 2, CHEER = 3, SAD = 4, THINK = 5 };

class dialogue_box {

public:
    dialogue_box();

    ~dialogue_box();

    dialogue_box(const dialogue_box&) = delete;
    dialogue_box& operator=(const dialogue_box&) = delete;

    // Shows the box with `text` (up to three '\n'-separated lines) spoken by the current helper.
    // `who` is the name on the plate (nullptr hides it).
    void show(const bn::string_view& text, const bn::string_view& who, Pose pose = Pose::IDLE1);

    // Same, without a portrait: a plain message strip.
    void show_plain(const bn::string_view& text);

    void hide();

    // One frame: types the next characters and animates the portrait. Returns true while typing.
    bool update();

    // Skips to the end of the current text. Returns false if it was already finished.
    bool skip();

    [[nodiscard]] bool visible() const { return _visible; }
    [[nodiscard]] bool typing() const { return _typing; }
    [[nodiscard]] bool finished() const { return _visible && ! _typing; }
    [[nodiscard]] const char* probe_text() const { return _probe; }

    void set_pose(Pose pose);
    void set_prompt_visible(bool visible);

private:
    void _apply_visible_text();
    void _update_portrait();

    bn::optional<bn::regular_bg_ptr> _bg;
    bn::optional<bn::sprite_ptr> _portrait;
    bn::optional<bn::sprite_ptr> _prompt;
    text_block _name;
    text_block _lines[3];

    char _full[3][40] = {};
    int _line_bytes[3] = {};
    int _shown = 0;          // bytes typed so far, across all lines
    int _total = 0;
    int _tick = 0;
    int _anim = 0;
    int _helper = -1;
    Pose _pose = Pose::IDLE1;
    bool _visible = false;
    bool _typing = false;
    bool _has_portrait = false;
    char _probe[32] = {};
};

}  // namespace render
