// Title screen (mockup 1a): ink-wash background, MOKU logo, CAMPAIGN / SANDBOX / OPTIONS menu.
//
// Test block: scene = TITLE, menu_index = highlighted item (0 CAMPAIGN, 1 SANDBOX, 2 OPTIONS).
// A confirms; the requested scene is only entered if some factory owns it, so the title survives
// until the campaign / sandbox / options scenes exist.
#pragma once

#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "game/scene.h"

namespace scenes {

class title_scene : public scene::Scene {

public:
    title_scene();

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::TITLE; }

private:
    static constexpr int MENU_ITEMS = 3;
    static constexpr int MAX_TEXT_SPRITES = 32;

    bn::regular_bg_ptr _bg;
    bn::sprite_ptr _logo_left;
    bn::sprite_ptr _logo_right;
    bn::sprite_ptr _cursor;
    bn::sprite_text_generator _small;
    bn::vector<bn::sprite_ptr, MAX_TEXT_SPRITES> _texts;

    int _index = 0;
    int _frame = 0;
    uint32_t _revision = 0;

    void _build_texts();
    void _move_cursor();
};

}  // namespace scenes
