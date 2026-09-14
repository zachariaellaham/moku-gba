// A vertical list of rows for the menu screens. Two kinds of row:
//
//   ITEM    one label, chosen with A            (title menu, pause menu, save slots)
//   CHOICE  a label and a value changed with left/right, as the sandbox and options mockups show
//
// The widget owns its text and its cursor sprite; the scene owns the widget, decides what each row
// means, and reads back the selection.
#pragma once

#include <cstdint>

#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_string_view.h"
#include "bn_vector.h"

#include "game/render/text.h"

namespace render {

constexpr int MAX_MENU_ROWS = 10;

enum class RowKind : uint8_t { ITEM = 0, CHOICE = 1, HEADER = 2 };

class menu {

public:
    menu() = default;

    // Rows start at (x, y) and are `pitch` pixels apart; `value_x` is where a CHOICE row's value
    // begins (screen pixels).
    void setup(int x, int y, int pitch, int value_x, Font font = Font::SMALL);

    void clear();
    void add_item(const bn::string_view& label);
    void add_choice(const bn::string_view& label, const bn::string_view& value);
    void add_header(const bn::string_view& label);

    void set_label(int row, const bn::string_view& label);
    void set_value(int row, const bn::string_view& value);
    void set_row_enabled(int row, bool enabled);

    // Moves the cursor, skipping headers and disabled rows. Returns true when it moved.
    bool move(int delta);
    void set_index(int index);

    [[nodiscard]] int index() const { return _index; }
    [[nodiscard]] int rows() const { return int(_rows.size()); }
    [[nodiscard]] RowKind kind(int row) const { return _rows[row].kind; }
    [[nodiscard]] bool enabled(int row) const { return _rows[row].enabled; }

    void update();                       // animates the cursor
    void set_visible(bool visible);

private:
    struct Row {
        text_block label;
        text_block value;
        RowKind kind = RowKind::ITEM;
        bool enabled = true;
    };

    void _refresh_cursor();

    bn::vector<Row, MAX_MENU_ROWS> _rows;
    bn::optional<bn::sprite_ptr> _cursor;
    int _x = 0, _y = 0, _pitch = 12, _value_x = 0;
    int _index = 0;
    int _anim = 0;
    Font _font = Font::SMALL;
    bool _visible = true;
};

}  // namespace render
