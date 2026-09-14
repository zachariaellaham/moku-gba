// Renderer proof: the board layer at every size, with a stone of each colour and a marker of each
// flag, a blinking cursor, incremental updates and a screen shake.
//
// Reached with TCMD_GOTO_SCENE and arg 0: the demo answers SceneId::BOOT, which is never a real
// scene, so it cannot collide with the play scene (which owns SceneId::PLAY).
//
// Controls: D-pad moves the cursor, A places / removes a stone, L and R change the board size,
// SELECT shakes the screen, B goes back to the title.
// Test block: scene = BOOT (0), scene_sub = size index (0:7, 1:9, 2:13, 3:19), board_size,
// cursor_x/y.
#pragma once

#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "game/render/board_layer.h"
#include "game/scene.h"

namespace scenes {

class board_demo_scene : public scene::Scene {

public:
    board_demo_scene();

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::BOOT; }

private:
    static constexpr int MAX_TEXT_SPRITES = 12;

    render::board_layer _board;
    bn::sprite_text_generator _small;
    bn::vector<bn::sprite_ptr, MAX_TEXT_SPRITES> _texts;
    bn::sprite_ptr _cursor;

    int _size_index = 1;
    int _cursor_x = 4;
    int _cursor_y = 4;
    int _frame = 0;
    int _shake = 0;
    int _cursor_art = 16;
    int _cursor_frame = 0;
    uint8_t _next_colour = render::CELL_BLACK;
    uint32_t _revision = 0;

    void _apply_size();
    void _fill_demo_pattern();
    void _build_texts();
    void _update_cursor_sprite();
};

}  // namespace scenes
