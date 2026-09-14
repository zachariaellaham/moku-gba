// The board screen: board, HUD, cursor, tutor dialogue, effects and the AI turn.
//
// The scene owns no game logic. game::Session decides what happens; the scene turns the keypad
// into game::Input, gives the session as much AI time as the frame can spare, and draws whatever
// comes out of the session's event queue.
#pragma once

#include <cstdint>

#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_timer.h"
#include "bn_vector.h"

#include "game/render/board_layer.h"
#include "game/render/dialogue.h"
#include "game/render/effects.h"
#include "game/render/menu.h"
#include "game/render/text.h"
#include "game/scene.h"
#include "game/session.h"
#include "game/speech.h"

namespace scenes {

// How the play scene was entered, and what to do when it ends.
struct PlayRequest {
    bool campaign = false;
    bool resume = false;             // replay the sandbox game saved in SRAM instead of starting one
    uint8_t mission_index = 0;       // 0-based
    uint8_t stage = 0;
    game::SandboxConfig sandbox;
};

// Set before switching to SceneId::PLAY.
void set_play_request(const PlayRequest& request);
[[nodiscard]] const PlayRequest& play_request();

class play_scene : public scene::Scene {

public:
    explicit play_scene(uint32_t arg);

    ~play_scene() override;

    void update() override;

    [[nodiscard]] SceneId id() const override { return SceneId::PLAY; }

    [[nodiscard]] bool handle_test_command(uint32_t cmd, uint32_t arg, uint32_t& result) override;

    // One session, alive for the whole run: putting it in the scene object would need a 26 KB
    // heap allocation, and EWRAM has no room for that.
    [[nodiscard]] static game::Session& session();

private:
    void _build_hud();
    void _sync_board();
    void _sync_cursor();
    void _update_hud();
    void _drain_events();
    void _sync_pause();
    void _sync_debug_overlay();
    void _publish();
    void _store_sandbox_game();
    void _show_line(uint16_t string_id, render::Pose pose);
    // The sandbox opponent speaking over the board (mockup 2b). Campaign missions stay quiet:
    // there the helper does the talking.
    void _opponent_say(game::Say what, bool force = false);
    [[nodiscard]] int _opponent_index() const;
    [[nodiscard]] bool _opponent_speaks() const;
    void _on_finished();
    [[nodiscard]] int _ai_work_budget() const;
    [[nodiscard]] int _cell_index() const;

    render::board_layer _board;
    bn::optional<bn::regular_bg_ptr> _hud_bg;
    bn::optional<bn::sprite_ptr> _cursor;
    bn::optional<bn::sprite_ptr> _ghost;
    bn::optional<bn::sprite_ptr> _turn_stone;
    bn::optional<bn::sprite_ptr> _thinking;
    render::dialogue_box _dialogue;
    render::effects _fx;

    render::text_block _label_turn, _value_turn;
    render::text_block _label_capt, _value_capt;
    render::text_block _label_move, _value_move;
    render::text_block _label_komi, _value_komi;
    render::text_block _label_panel;
    render::text_block _panel_lines[3];
    render::text_block _hints[2];
    render::text_block _debug[4];
    render::text_block _debug_shadow[4];
    render::text_block _pause_title;
    render::text_block _pause_nav;
    render::menu _pause_menu;
    bool _pause_built = false;

    uint32_t _revision = 0xFFFFFFFF;
    int _anim = 0;
    int _ai_work = 1;   // one unit per frame: every unit is sized to fit in one
    bool _dialogue_gate = false;       // the dialogue is holding the game
    bool _finished_handled = false;
    uint8_t _last_state = 0xFF;
    uint8_t _say_cooldown = 0;         // frames before the opponent may speak again
    bool _said_intro = false;
    uint16_t _pending_line = 0;        // shown when the opening dialogue is dismissed
    bool _debug_overlay = false;       // L+R+SELECT: playouts/s, CPU, stalls
};

}  // namespace scenes
