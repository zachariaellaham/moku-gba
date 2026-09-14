// The play session: cursor, move flow, AI turn, marking phase and pause menu.
#include "game/session.h"
#include "go/score.h"

namespace game {

using campaign::MissionStatus;
using go::BLACK;
using go::Board;
using go::Color;
using go::EMPTY;
using go::PASS;
using go::Point;
using go::WHITE;

namespace {

// The AI budget per level, in frames, before the session forces a decision. Nobody should ever wait
// on a Game Boy Advance for longer than this.
int frame_budget(ai::Level level)
{
    return ai::default_limits(level).frame_budget;
}

}  // namespace

void Session::push(EventKind kind, Point p, int count, Color c)
{
    const uint8_t next = uint8_t((queue_tail_ + 1) % EVENT_QUEUE_SIZE);
    if(next == queue_head_) return;                 // the renderer is behind: drop the oldest news
    queue_[queue_tail_] = Event{ kind, int16_t(p), int16_t(count), uint8_t(c) };
    queue_tail_ = next;
}

bool Session::poll(Event& out)
{
    if(queue_head_ == queue_tail_) return false;
    out = queue_[queue_head_];
    queue_head_ = uint8_t((queue_head_ + 1) % EVENT_QUEUE_SIZE);
    return true;
}

go::Color Session::player_color() const
{
    if(mode_ == Mode::CAMPAIGN && run_.def) return run_.stage->player;
    return config_.player_black ? BLACK : WHITE;
}

bool Session::opponent_is_ai() const
{
    if(mode_ == Mode::HOTSEAT) return false;
    if(mode_ == Mode::CAMPAIGN) return run_.stage && run_.stage->opponent != campaign::Opponent::NONE;
    return true;
}

void Session::start_campaign(const campaign::MissionDef& def, int stage)
{
    mode_ = Mode::CAMPAIGN;
    campaign::mission_start(run_, def, stage);
    level_ = campaign::opponent_level(run_.stage->opponent);
    thinker_.reset_history();
    const int size = run_.game.board().size();
    cursor_x_ = size / 2;
    cursor_y_ = size / 2;
    // start the cursor on the first marked point: the player should see what the mission is about
    if(run_.stage->marked_count > 0)
    {
        cursor_x_ = run_.stage->marked[0].x;
        cursor_y_ = run_.stage->marked[0].y;
    }
    pending_ = go::NO_POINT;
    hint_point_ = go::NO_POINT;
    hud_panel_ = HudPanel::INFO;
    estimate_valid_ = false;
    queue_head_ = queue_tail_ = 0;
    ai_frames_ = 0;
    enter_first_turn();
}

void Session::enter_first_turn()
{
    state_ = (run_.game.to_move() == player_color()) ? PlayState::PLAYER_TURN : PlayState::AI_THINKING;
    if(state_ == PlayState::AI_THINKING) begin_ai_turn();
}

void Session::restore_sandbox(const SandboxConfig& config, const uint16_t* moves, int count)
{
    start_sandbox(config);

    for(int i = 0; i < count; ++i)
    {
        const go::Point p = (moves[i] == 0xFFFF) ? go::PASS : go::Point(moves[i]);

        if(! run_.game.play(p).ok)
        {
            break;
        }

        if(run_.game.phase() != go::Phase::PLAYING)
        {
            break;
        }
    }

    // start_sandbox() may already have started the opponent thinking about move one; after the
    // replay the position is a different one, so the turn is entered again from scratch.
    thinker_.reset_history();
    queue_head_ = queue_tail_ = 0;
    estimate_valid_ = false;
    ai_frames_ = 0;
    pending_ = go::NO_POINT;
    hint_point_ = go::NO_POINT;
    run_.player_moves = run_.game.move_count() / 2;
    enter_first_turn();
}

void Session::start_sandbox(const SandboxConfig& config)
{
    config_ = config;
    mode_ = config.hotseat ? Mode::HOTSEAT : Mode::SANDBOX;
    level_ = config.level;
    thinker_.reset_history();

    go::GameSettings settings;
    settings.size = config.size;
    settings.rules = config.rules;
    settings.komi_x2 = config.komi_x2;
    settings.handicap = config.handicap;
    run_.def = nullptr;
    run_.stage = nullptr;
    run_.status = MissionStatus::PLAYING;
    run_.player_moves = run_.undos = run_.hints = run_.captures = 0;
    run_.game.start(settings);

    cursor_x_ = config.size / 2;
    cursor_y_ = config.size / 2;
    pending_ = go::NO_POINT;
    hint_point_ = go::NO_POINT;
    hud_panel_ = HudPanel::INFO;
    estimate_valid_ = false;
    queue_head_ = queue_tail_ = 0;
    ai_frames_ = 0;
    enter_first_turn();
}

void Session::move_cursor(int dx, int dy)
{
    const int size = run_.game.board().size();
    cursor_x_ = (cursor_x_ + dx + size) % size;     // the cursor wraps, as the mockups show
    cursor_y_ = (cursor_y_ + dy + size) % size;
    if(pending_ != go::NO_POINT) pending_ = go::NO_POINT;   // moving away cancels the ghost stone
    push(EventKind::CURSOR_MOVED, cursor_point());
}

void Session::handle_cursor(const Input& input)
{
    const uint16_t dirs = uint16_t(input.held & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT));
    if(dirs == 0) { last_dir_ = 0; repeat_timer_ = 0; return; }

    bool step = false;
    if(dirs != last_dir_) { step = true; repeat_timer_ = CURSOR_REPEAT_DELAY; last_dir_ = dirs; }
    else if(--repeat_timer_ <= 0) { step = true; repeat_timer_ = CURSOR_REPEAT_RATE; }
    if(!step) return;

    int dx = 0, dy = 0;
    if(dirs & BTN_LEFT) dx = -1;
    else if(dirs & BTN_RIGHT) dx = 1;
    if(dirs & BTN_UP) dy = -1;
    else if(dirs & BTN_DOWN) dy = 1;
    if(dx || dy) move_cursor(dx, dy);
}

void Session::apply_player_move(Point p)
{
    const Color mover = run_.game.to_move();
    if(p != PASS && !run_.game.is_legal(p))
    {
        const Board& b = run_.game.board();
        push(b.ko_point() == p ? EventKind::KO_BLOCKED : EventKind::ILLEGAL, p);
        return;
    }
    go::PlayResult r;
    if(mode_ == Mode::CAMPAIGN) r = campaign::mission_player_move(run_, p);
    else r = run_.game.play(p);
    if(!r.ok) { push(EventKind::ILLEGAL, p); return; }
    hint_point_ = go::NO_POINT;
    estimate_valid_ = false;
    after_move(p, r, mover);
}

void Session::after_move(Point p, const go::PlayResult& r, Color mover)
{
    const Board& b = run_.game.board();
    if(p == PASS)
    {
        push(EventKind::PASSED, PASS, 0, mover);
    }
    else
    {
        push(EventKind::STONE_PLACED, p, 0, mover);
        if(r.captured > 0) push(EventKind::CAPTURE, p, r.captured, mover);
        // report every enemy group this move left with a single liberty
        const int s = b.stride();
        const Point nb[4] = { Point(p - s), Point(p - 1), Point(p + 1), Point(p + s) };
        Point seen[4];
        int seen_count = 0;
        for(int i = 0; i < 4; ++i)
        {
            if(b.at(nb[i]) != go::opponent(mover) || !b.in_atari(nb[i])) continue;
            const Point root = b.group_root(nb[i]);
            bool dup = false;
            for(int k = 0; k < seen_count; ++k) if(seen[k] == root) dup = true;
            if(dup) continue;
            seen[seen_count++] = root;
            push(EventKind::ATARI, nb[i], b.group_size(nb[i]), go::opponent(mover));
        }
        if(r.self_in_atari) push(EventKind::ATARI, p, b.group_size(p), mover);
    }

    if(mode_ == Mode::CAMPAIGN)
    {
        if(run_.status == MissionStatus::CLEAR)
        {
            const bool more = run_.def && run_.def->stage_count > 0 && run_.stage_index + 1 < run_.def->stage_count;
            push(more ? EventKind::STAGE_CLEAR : EventKind::MISSION_CLEAR);
            state_ = PlayState::OVER;
            return;
        }
        if(run_.status == MissionStatus::FAIL)
        {
            push(EventKind::MISSION_FAIL);
            state_ = PlayState::OVER;
            return;
        }
    }

    if(run_.game.phase() == go::Phase::MARK_DEAD)
    {
        // A tactical mission is not a game to be counted: two passes must not end it.
        if(mode_ == Mode::CAMPAIGN && !mission_is_full_game())
        {
            run_.game.resume_play();
            state_ = PlayState::PLAYER_TURN;
            return;
        }
        enter_mark_phase();
        return;
    }
    if(run_.game.phase() == go::Phase::OVER)
    {
        push(EventKind::SCORED, go::NO_POINT, run_.game.final_score().margin_x2);
        state_ = PlayState::OVER;
        return;
    }
    advance_turn();
}

bool Session::mission_is_full_game() const
{
    if(mode_ != Mode::CAMPAIGN || !run_.stage) return true;
    const campaign::Objective o = run_.stage->objective;
    return o == campaign::Objective::WIN_GAME || o == campaign::Objective::WIN_BY_MARGIN;
}

void Session::advance_turn()
{
    if(run_.game.phase() != go::Phase::PLAYING) return;
    if(run_.game.to_move() == player_color() || mode_ == Mode::HOTSEAT)
    {
        state_ = PlayState::PLAYER_TURN;
        return;
    }
    if(!opponent_is_ai())
    {
        // A mission with no opponent: the other side simply gives the move back.
        const Color mover = run_.game.to_move();
        campaign::mission_opponent_played(run_, PASS);
        go::PlayResult r;
        r.ok = true;
        after_move(PASS, r, mover);
        return;
    }
    begin_ai_turn();
}

void Session::begin_ai_turn()
{
    state_ = PlayState::AI_THINKING;
    ai_frames_ = 0;
    seed_ = seed_ * 1664525u + 1013904223u;
    thinker_.start(run_.game, level_, seed_);
    push(EventKind::AI_START);
}

void Session::step_ai(int work)
{
    ++ai_frames_;
    if(!thinker_.step(work) && ai_frames_ < frame_budget(level_)) return;
    if(!thinker_.done()) thinker_.finish();

    if(thinker_.wants_resign())
    {
        run_.game.resign(go::opponent(player_color()));
        push(EventKind::AI_RESIGNED);
        push(EventKind::SCORED, go::NO_POINT, run_.game.final_score().margin_x2);
        if(mode_ == Mode::CAMPAIGN) campaign::mission_check(run_);
        state_ = PlayState::OVER;
        return;
    }

    Point m = thinker_.result();
    if(m != PASS && !run_.game.is_legal(m)) m = PASS;     // belt and braces: never play an illegal move
    const Color mover = run_.game.to_move();
    push(EventKind::AI_DONE, m);
    go::PlayResult r;
    if(mode_ == Mode::CAMPAIGN)
    {
        const int caps_before = run_.game.board().captures(mover);
        campaign::mission_opponent_played(run_, m);
        r.ok = true;
        r.captured = run_.game.board().captures(mover) - caps_before;
    }
    else
    {
        r = run_.game.play(m);
    }
    estimate_valid_ = false;
    after_move(m, r, mover);
}

void Session::enter_mark_phase()
{
    run_.game.propose_dead_marks();
    state_ = PlayState::MARK_DEAD;
    push(EventKind::MARK_PHASE);
    refresh_estimate();
}

void Session::refresh_estimate()
{
    estimate_ = (run_.game.phase() == go::Phase::PLAYING)
        ? go::estimate_score(run_.game.board(), run_.game.settings().komi_x2, run_.game.settings().rules)
        : run_.game.score();
    estimate_valid_ = true;
    push(EventKind::ESTIMATE_SHOWN);
}

void Session::handle_player_turn(const Input& input)
{
    handle_cursor(input);

    if(input.pressed & BTN_START)
    {
        state_before_pause_ = state_;
        state_ = PlayState::PAUSED;
        pause_item_ = PauseItem::RESUME;
        push(EventKind::PAUSE_OPENED);
        return;
    }
    if(input.pressed & (BTN_L | BTN_R))
    {
        // L and R cycle the HUD panels; R alone also passes, so passing needs the pause menu or a
        // long press - the mockup labels R as PASS, so R passes and L cycles the panels.
    }
    if(input.pressed & BTN_L)
    {
        hud_panel_ = HudPanel((int(hud_panel_) + 1) % int(HudPanel::COUNT));
        if(hud_panel_ == HudPanel::ESTIMATE) refresh_estimate();
    }
    if(input.pressed & BTN_R) { apply_player_move(PASS); return; }
    if(input.pressed & BTN_SELECT)
    {
        if(mode_ == Mode::CAMPAIGN) hint_point_ = campaign::mission_hint(run_);
        else hint_point_ = ai::pick_move(run_.game, ai::Level::NORMAL, seed_ + 17u);
        push(EventKind::HINT_SHOWN, hint_point_);
        return;
    }
    if(input.pressed & BTN_B)
    {
        if(pending_ != go::NO_POINT) { pending_ = go::NO_POINT; return; }
        bool undone = false;
        if(mode_ == Mode::CAMPAIGN) undone = campaign::mission_undo(run_);
        else if(run_.game.can_undo())
        {
            undone = run_.game.undo();
            if(undone && opponent_is_ai() && run_.game.to_move() != player_color()) undone = run_.game.undo();
        }
        if(undone)
        {
            estimate_valid_ = false;
            hint_point_ = go::NO_POINT;
            state_ = PlayState::PLAYER_TURN;
            push(EventKind::UNDONE);
        }
        return;
    }
    if(input.pressed & BTN_A)
    {
        const Point p = cursor_point();
        if(!confirm_two_press_) { apply_player_move(p); return; }
        if(pending_ == p) { pending_ = go::NO_POINT; apply_player_move(p); return; }
        if(!run_.game.is_legal(p))
        {
            const Board& b = run_.game.board();
            push(b.ko_point() == p ? EventKind::KO_BLOCKED : EventKind::ILLEGAL, p);
            return;
        }
        pending_ = p;
        state_ = PlayState::CONFIRM;
        push(EventKind::GHOST_SHOWN, p);
    }
}

void Session::handle_pause(const Input& input)
{
    if(input.pressed & (BTN_START | BTN_B))
    {
        state_ = state_before_pause_;
        push(EventKind::PAUSE_CLOSED);
        return;
    }
    if(input.pressed & BTN_UP)
        pause_item_ = PauseItem((int(pause_item_) + int(PauseItem::COUNT) - 1) % int(PauseItem::COUNT));
    if(input.pressed & BTN_DOWN)
        pause_item_ = PauseItem((int(pause_item_) + 1) % int(PauseItem::COUNT));
    if(!(input.pressed & BTN_A)) return;

    switch(pause_item_)
    {
    case PauseItem::RESUME:
        state_ = state_before_pause_;
        push(EventKind::PAUSE_CLOSED);
        break;
    case PauseItem::UNDO:
    {
        bool undone = false;
        if(mode_ == Mode::CAMPAIGN) undone = campaign::mission_undo(run_);
        else if(run_.game.can_undo())
        {
            undone = run_.game.undo();
            if(undone && opponent_is_ai() && run_.game.to_move() != player_color()) run_.game.undo();
        }
        if(undone) { estimate_valid_ = false; push(EventKind::UNDONE); }
        state_ = PlayState::PLAYER_TURN;
        push(EventKind::PAUSE_CLOSED);
        break;
    }
    case PauseItem::HINT:
        if(mode_ == Mode::CAMPAIGN) hint_point_ = campaign::mission_hint(run_);
        else hint_point_ = ai::pick_move(run_.game, ai::Level::NORMAL, seed_ + 31u);
        push(EventKind::HINT_SHOWN, hint_point_);
        state_ = state_before_pause_;
        push(EventKind::PAUSE_CLOSED);
        break;
    case PauseItem::PASS:
        state_ = PlayState::PLAYER_TURN;
        push(EventKind::PAUSE_CLOSED);
        apply_player_move(PASS);
        break;
    case PauseItem::ESTIMATE:
        refresh_estimate();
        hud_panel_ = HudPanel::ESTIMATE;
        break;
    case PauseItem::RESIGN:
        run_.game.resign(player_color());
        if(mode_ == Mode::CAMPAIGN) { run_.status = MissionStatus::FAIL; push(EventKind::MISSION_FAIL); }
        else push(EventKind::SCORED, go::NO_POINT, run_.game.final_score().margin_x2);
        state_ = PlayState::OVER;
        push(EventKind::PAUSE_CLOSED);
        break;
    case PauseItem::OPTIONS:
        wants_options_ = true;
        break;
    case PauseItem::QUIT:
        wants_quit_ = true;
        break;
    default:
        break;
    }
}

void Session::handle_mark_dead(const Input& input)
{
    handle_cursor(input);
    if(input.pressed & BTN_A)
    {
        const Point p = cursor_point();
        if(run_.game.board().at(p) != EMPTY)
        {
            run_.game.toggle_dead(p);
            push(EventKind::DEAD_TOGGLED, p);
            refresh_estimate();
        }
        return;
    }
    if(input.pressed & BTN_START)
    {
        run_.game.finish_marking();
        push(EventKind::SCORED, go::NO_POINT, run_.game.final_score().margin_x2);
        if(mode_ == Mode::CAMPAIGN) campaign::mission_check(run_);
        state_ = PlayState::OVER;
        return;
    }
    if(input.pressed & BTN_B)
    {
        run_.game.resume_play();                 // the players disagree: play it out
        state_ = PlayState::PLAYER_TURN;
        advance_turn();
    }
}

void Session::handle_over(const Input& input)
{
    (void)input;
}

void Session::update(const Input& input, int ai_work)
{
    switch(state_)
    {
    case PlayState::PLAYER_TURN:
    case PlayState::CONFIRM:
        handle_player_turn(input);
        if(state_ == PlayState::CONFIRM && pending_ == go::NO_POINT) state_ = PlayState::PLAYER_TURN;
        break;
    case PlayState::AI_THINKING:
        if(input.pressed & BTN_START)
        {
            state_before_pause_ = state_;
            state_ = PlayState::PAUSED;
            pause_item_ = PauseItem::RESUME;
            push(EventKind::PAUSE_OPENED);
            break;
        }
        step_ai(ai_work > 0 ? ai_work : 1);
        break;
    case PlayState::MARK_DEAD:
        handle_mark_dead(input);
        break;
    case PlayState::PAUSED:
        handle_pause(input);
        break;
    case PlayState::OVER:
        handle_over(input);
        break;
    }
}

void Session::force_move(Point p)
{
    const Color mover = run_.game.to_move();
    if(mover == player_color() || mode_ == Mode::HOTSEAT)
    {
        pending_ = go::NO_POINT;
        apply_player_move(p);
    }
    else
    {
        go::PlayResult r;
        if(mode_ == Mode::CAMPAIGN)
        {
            const int caps_before = run_.game.board().captures(mover);
            campaign::mission_opponent_played(run_, p);
            r.ok = true;
            r.captured = run_.game.board().captures(mover) - caps_before;
        }
        else r = run_.game.play(p);
        after_move(p, r, mover);
    }
}

void Session::finish_ai_now()
{
    if(state_ != PlayState::AI_THINKING) return;
    thinker_.finish();
    ai_frames_ = frame_budget(level_);
    step_ai(1);
}

}  // namespace game
