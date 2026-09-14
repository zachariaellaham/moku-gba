// Options (mockup 2e): language, helper, skin, effects, confirm mode, music, sfx, with a live
// preview of the chosen helper. Left and right change a value; B goes back to wherever we came from.
#pragma once

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"

#include "game/render/menu.h"
#include "game/render/text.h"
#include "game/scene.h"

namespace scenes {

// Where B should return to.
void set_options_return(SceneId id);

class options_scene : public scene::Scene {

public:
    options_scene();

    ~options_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::OPTIONS; }

private:
    void _refresh_values();
    void _change(int delta);

    bn::optional<bn::regular_bg_ptr> _bg;
    bn::optional<bn::sprite_ptr> _preview;
    render::menu _menu;
    render::text_block _title;
    render::text_block _nav;
    render::text_block _note;
    int _anim = 0;
    int _preview_helper = -1;
};

}  // namespace scenes
