// The campaign screens: the map (mockup 1b), the briefing (1c / 2d) and the clear screen (1f).
#pragma once

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "game/missions.h"
#include "game/render/dialogue.h"
#include "game/render/effects.h"
#include "game/render/menu.h"
#include "game/render/text.h"
#include "game/scene.h"

namespace scenes {

// Which mission the briefing and the clear screen are talking about (0-based).
void set_campaign_mission(int index);
[[nodiscard]] int campaign_mission();

// Filled in by the play scene when a mission ends, read by the clear screen.
struct MissionResult {
    int index = 0;
    int rank = 0;              // campaign::Rank
    bool stars = false;
    bool cleared = false;
    int moves = 0;
    int captures = 0;
    int undos = 0;
    int hints = 0;
    uint16_t fail_text = 0;
    uint8_t unlock = 0;
};

void set_mission_result(const MissionResult& result);
[[nodiscard]] const MissionResult& mission_result();

class map_scene : public scene::Scene {

public:
    map_scene();

    ~map_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::CAMPAIGN_MAP; }

private:
    void _refresh();
    void _scroll_to(int index);

    bn::optional<bn::regular_bg_ptr> _bg;
    bn::vector<bn::sprite_ptr, campaign::MISSION_COUNT> _nodes;
    bn::vector<bn::sprite_ptr, 3> _stars;
    render::text_block _numbers[campaign::MISSION_COUNT];
    render::text_block _header;
    render::text_block _name;
    render::text_block _chapter;
    render::text_block _nav;
    render::text_block _more;
    int _index = 0;
    int _anim = 0;
    int _scroll = 0;
    int _target_scroll = 0;
};

class briefing_scene : public scene::Scene {

public:
    briefing_scene();

    ~briefing_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::BRIEFING; }

private:
    bn::optional<bn::regular_bg_ptr> _bg;
    render::dialogue_box _dialogue;
    render::text_block _number;
    render::text_block _name;
    render::text_block _objective_label;
    render::text_block _objective[2];
    render::text_block _rank_label;
    render::text_block _rank_hint[2];
    render::text_block _nav;
    bool _rival_shown = false;
    int _anim = 0;
};

class clear_scene : public scene::Scene {

public:
    clear_scene();

    ~clear_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::MISSION_CLEAR; }

private:
    bn::optional<bn::regular_bg_ptr> _bg;
    bn::vector<bn::sprite_ptr, 3> _stars;
    render::effects _fx;
    render::text_block _title;
    render::text_block _subtitle;
    render::text_block _labels[4];
    render::text_block _values[4];
    render::text_block _stars_label;
    render::text_block _unlock_label;
    render::text_block _unlock_value;
    render::menu _menu;
    int _anim = 0;
    bool _stamped = false;
};

}  // namespace scenes
