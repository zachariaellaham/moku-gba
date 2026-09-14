// Sandbox setup (mockup 1g) and the "choose opponent" screen (2a).
//
// Both live in one file because they are two views of the same configuration: the opponent screen
// is what the mockup puts in place of the AI row, and it hands its choice straight back.
#pragma once

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "game/render/menu.h"
#include "game/render/text.h"
#include "game/scene.h"
#include "game/session.h"
#include "game/strings_ids.h"

namespace scenes {

// The five opponent characters, each bound to one AI level.
struct Opponent {
    const char* name;
    Str name_id;
    Str title_id;
    Str blurb_id;
    ai::Level level;
    int unlock_id;
};

[[nodiscard]] const Opponent& opponent(int index);
constexpr int OPPONENT_COUNT = 5;

class sandbox_scene : public scene::Scene {

public:
    sandbox_scene();

    ~sandbox_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::SANDBOX_SETUP; }

private:
    void _refresh();
    void _change(int delta);
    void _start();
    void _resume();

    bn::optional<bn::regular_bg_ptr> _bg;
    render::menu _menu;
    render::text_block _title;
    render::text_block _nav;
    render::text_block _blurb[2];
    int _anim = 0;
};

class opponent_scene : public scene::Scene {

public:
    opponent_scene();

    ~opponent_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::CHOOSE_OPPONENT; }

private:
    void _refresh();

    bn::optional<bn::regular_bg_ptr> _bg;
    bn::vector<bn::sprite_ptr, OPPONENT_COUNT> _portraits;
    bn::optional<bn::sprite_ptr> _frame;
    render::text_block _title;
    render::text_block _nav;
    render::text_block _names[OPPONENT_COUNT];
    render::text_block _name;
    render::text_block _subtitle;
    render::text_block _level;
    render::text_block _level_pips;
    render::text_block _blurb[2];
    render::text_block _record;
    int _index = 0;
    int _anim = 0;
};

// The configuration the sandbox screen is editing (kept between the two scenes).
[[nodiscard]] game::SandboxConfig& sandbox_config();

}  // namespace scenes
