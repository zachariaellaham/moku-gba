// The play session: everything that happens on the board screen, with no Butano in sight.
//
// The scene feeds it one Input per frame and a work budget for the AI, then drains the event queue
// to drive sprites, banners and sound. Because the session is platform-agnostic it is tested on the
// host (tests/host/test_session.cpp) long before it is tested in a ROM.
#pragma once
#include "game/missions.h"
#include "ai/ai.h"

namespace game {

enum class Mode : uint8_t { CAMPAIGN = 0, SANDBOX = 1, HOTSEAT = 2 };

// Buttons, as a bitmask. The scene maps the GBA keypad onto these.
enum Button : uint16_t {
    BTN_UP = 1 << 0, BTN_DOWN = 1 << 1, BTN_LEFT = 1 << 2, BTN_RIGHT = 1 << 3,
    BTN_A = 1 << 4, BTN_B = 1 << 5, BTN_L = 1 << 6, BTN_R = 1 << 7,
    BTN_START = 1 << 8, BTN_SELECT = 1 << 9
};

struct Input {
    uint16_t held = 0;
    uint16_t pressed = 0;      // went down this frame
};

enum class PlayState : uint8_t {
    PLAYER_TURN = 0,   // waiting for a move
    CONFIRM = 1,       // ghost stone shown, waiting for the second A
    AI_THINKING = 2,
    MARK_DEAD = 3,     // after two passes: toggling dead groups
    OVER = 4,
    PAUSED = 5,
};

enum class PauseItem : uint8_t {
    RESUME = 0, UNDO = 1, HINT = 2, PASS = 3, ESTIMATE = 4, RESIGN = 5, OPTIONS = 6, QUIT = 7, COUNT = 8
};

enum class HudPanel : uint8_t { INFO = 0, MOVES = 1, ESTIMATE = 2, COUNT = 3 };

// Everything the renderer may need to react to. The queue is drained every frame.
enum class EventKind : uint8_t {
    NONE = 0,
    CURSOR_MOVED,
    GHOST_SHOWN,
    STONE_PLACED,      // point, colour
    CAPTURE,           // point of the capturing move, count = stones taken, colour = who captured
    ATARI,             // point of a group left with one liberty, colour = the group in danger
    ILLEGAL,           // point: the move the player tried
    KO_BLOCKED,
    PASSED,            // colour
    UNDONE,
    HINT_SHOWN,        // point
    AI_START,
    AI_DONE,           // point = the move
    AI_RESIGNED,
    MARK_PHASE,
    DEAD_TOGGLED,      // point
    SCORED,            // count = margin in half points, black minus white
    MISSION_CLEAR,
    MISSION_FAIL,
    STAGE_CLEAR,
    PAUSE_OPENED,
    PAUSE_CLOSED,
    ESTIMATE_SHOWN,
};

struct Event {
    EventKind kind = EventKind::NONE;
    int16_t point = go::NO_POINT;
    int16_t count = 0;
    uint8_t color = go::EMPTY;
};

struct SandboxConfig {
    uint8_t size = 9;
    bool player_black = true;
    ai::Level level = ai::Level::EASY;
    uint8_t opponent_id = 1;          // index into the opponent characters
    uint8_t handicap = 0;
    go::Ruleset rules = go::Ruleset::AREA;
    int16_t komi_x2 = 15;
    bool hotseat = false;
};

constexpr int EVENT_QUEUE_SIZE = 16;
constexpr int CURSOR_REPEAT_DELAY = 14;    // frames before auto-repeat starts
constexpr int CURSOR_REPEAT_RATE = 4;      // frames between repeats

class Session {
public:
    void start_campaign(const campaign::MissionDef& def, int stage = 0);
    void start_sandbox(const SandboxConfig& config);

    // Replays a saved sandbox game (SRAM resume). `moves` are point indices in order, 0xFFFF for a
    // pass; the colours alternate from whoever moved first. A record that no longer replays - a
    // save from an older board, say - stops at the first illegal move rather than refusing.
    void restore_sandbox(const SandboxConfig& config, const uint16_t* moves, int count);

    // One frame. `ai_work` is how many units of AI thinking the scene can afford this frame; the
    // scene measures its own budget and passes what fits (see docs/decisions/butano_spike.md).
    void update(const Input& input, int ai_work);

    bool poll(Event& out);                       // drains one queued event, false when empty

    Mode mode() const { return mode_; }
    PlayState state() const { return state_; }
    const go::Game& game() const { return run_.game; }
    const campaign::MissionRun& run() const { return run_; }
    campaign::MissionStatus mission_status() const { return run_.status; }
    const campaign::MissionDef* mission() const { return run_.def; }

    int cursor_x() const { return cursor_x_; }
    int cursor_y() const { return cursor_y_; }
    go::Point cursor_point() const { return run_.game.board().point(cursor_x_, cursor_y_); }
    bool ghost_visible() const { return state_ == PlayState::CONFIRM; }
    HudPanel hud_panel() const { return hud_panel_; }
    PauseItem pause_item() const { return pause_item_; }
    bool confirm_two_press() const { return confirm_two_press_; }
    void set_confirm_two_press(bool on) { confirm_two_press_ = on; }
    bool hint_visible() const { return hint_point_ != go::NO_POINT; }
    go::Point hint_point() const { return hint_point_; }
    bool is_dead(go::Point p) const { return run_.game.is_dead(p); }
    bool wants_quit() const { return wants_quit_; }
    bool wants_options() const { return wants_options_; }
    void clear_requests() { wants_quit_ = false; wants_options_ = false; }

    // Score shown by the estimate panel and by the final screen.
    const go::ScoreResult& estimate() const { return estimate_; }
    bool estimate_valid() const { return estimate_valid_; }
    void refresh_estimate();

    // The AI's own view, for the debug overlay.
    const ai::Stats& ai_stats() const { return thinker_.stats(); }
    ai::Level ai_level() const { return level_; }
    bool ai_thinking() const { return state_ == PlayState::AI_THINKING; }
    int ai_frames() const { return ai_frames_; }

    // Test and debug entry points.
    void force_move(go::Point p);                // play for whoever is to move, ignoring the cursor
    void finish_ai_now();
    void set_seed(uint32_t seed) { seed_ = seed ? seed : 1u; }

private:
    void push(EventKind kind, go::Point p = go::NO_POINT, int count = 0, go::Color c = go::EMPTY);
    void move_cursor(int dx, int dy);
    void handle_cursor(const Input& input);
    void handle_player_turn(const Input& input);
    void handle_pause(const Input& input);
    void handle_mark_dead(const Input& input);
    void handle_over(const Input& input);
    void apply_player_move(go::Point p);
    void after_move(go::Point p, const go::PlayResult& r, go::Color mover);
    void begin_ai_turn();
    void enter_first_turn();
    void advance_turn();
    bool mission_is_full_game() const;
    void step_ai(int work);
    void enter_mark_phase();
    bool opponent_is_ai() const;
    go::Color player_color() const;

    Mode mode_ = Mode::SANDBOX;
    PlayState state_ = PlayState::PLAYER_TURN;
    PlayState state_before_pause_ = PlayState::PLAYER_TURN;
    campaign::MissionRun run_;
    ai::Thinker thinker_;
    ai::Level level_ = ai::Level::EASY;
    SandboxConfig config_;

    int cursor_x_ = 4, cursor_y_ = 4;
    int repeat_timer_ = 0;
    uint16_t last_dir_ = 0;
    bool confirm_two_press_ = true;
    go::Point pending_ = go::NO_POINT;
    go::Point hint_point_ = go::NO_POINT;
    HudPanel hud_panel_ = HudPanel::INFO;
    PauseItem pause_item_ = PauseItem::RESUME;
    bool wants_quit_ = false, wants_options_ = false;
    uint32_t seed_ = 12345;
    int ai_frames_ = 0;
    go::ScoreResult estimate_;
    bool estimate_valid_ = false;

    Event queue_[EVENT_QUEUE_SIZE];
    uint8_t queue_head_ = 0, queue_tail_ = 0;
};

}  // namespace game
