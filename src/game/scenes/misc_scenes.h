// Save select (three slots, as the title screen promises) and the end-of-game score screen.
#pragma once

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "game/render/menu.h"
#include "game/render/text.h"
#include "game/scene.h"
#include "go/score.h"

namespace scenes {

// Filled in by the play scene when a sandbox game ends.
struct GameResult {
    go::ScoreResult score;
    bool resigned = false;
    bool player_black = true;
    bool hotseat = false;
    uint8_t opponent_id = 0;
    uint8_t board_size = 9;
};

void set_game_result(const GameResult& result);
[[nodiscard]] const GameResult& game_result();

class save_select_scene : public scene::Scene {

public:
    save_select_scene();

    ~save_select_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::SAVE_SELECT; }

private:
    void _refresh();

    bn::optional<bn::regular_bg_ptr> _bg;
    render::menu _menu;
    render::text_block _title;
    render::text_block _nav;
    render::text_block _details[3];
    render::text_block _confirm;
    int _anim = 0;
    bool _erasing = false;
};

class game_over_scene : public scene::Scene {

public:
    game_over_scene();

    ~game_over_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::GAME_OVER; }

private:
    bn::optional<bn::regular_bg_ptr> _bg;
    render::text_block _title;
    render::text_block _result;
    render::text_block _labels[4];
    render::text_block _values[4];
    render::text_block _say[2];
    bn::optional<bn::sprite_ptr> _portrait;
    render::menu _menu;
    int _anim = 0;
};

}  // namespace scenes
