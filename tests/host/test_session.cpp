// The play session drives the board screen. These tests exercise it exactly as the scene does:
// one Input per frame, then drain the event queue.
#include "doctest.h"
#include "game/session.h"

using namespace game;
using go::BLACK;
using go::PASS;
using go::Point;
using go::WHITE;

namespace {

Session g_session;   // holds a Game: far too big for the stack

Input none() { return Input{}; }
Input press(uint16_t bits) { return Input{ bits, bits }; }
Input hold(uint16_t bits) { return Input{ bits, 0 }; }

// Runs frames until the session leaves the AI's turn, or gives up.
bool settle(Session& s, int max_frames = 4000)
{
    for(int i = 0; i < max_frames; ++i)
    {
        if(s.state() != PlayState::AI_THINKING) return true;
        s.update(none(), 64);
    }
    return false;
}

int count_events(Session& s, EventKind kind)
{
    int n = 0;
    Event e;
    while(s.poll(e)) if(e.kind == kind) ++n;
    return n;
}

bool saw(Session& s, EventKind kind, Event* out = nullptr)
{
    Event e;
    bool found = false;
    while(s.poll(e)) if(e.kind == kind && !found) { found = true; if(out) *out = e; }
    return found;
}

SandboxConfig quick(int size = 9, ai::Level level = ai::Level::VERY_EASY, bool hotseat = false)
{
    SandboxConfig c;
    c.size = uint8_t(size);
    c.level = level;
    c.hotseat = hotseat;
    return c;
}

}  // namespace

TEST_CASE("the cursor wraps around the board and auto-repeats")
{
    g_session.start_sandbox(quick());
    const int start_x = g_session.cursor_x();
    g_session.update(press(BTN_LEFT), 0);
    CHECK(g_session.cursor_x() == start_x - 1);

    // walking off the left edge comes back on the right
    for(int i = 0; i < 40; ++i) g_session.update(press(BTN_LEFT), 0);
    CHECK(g_session.cursor_x() >= 0);
    CHECK(g_session.cursor_x() < 9);

    // holding a direction repeats only after the delay
    g_session.start_sandbox(quick());
    const int x0 = g_session.cursor_x();
    g_session.update(press(BTN_RIGHT), 0);
    CHECK(g_session.cursor_x() == x0 + 1);
    for(int i = 0; i < CURSOR_REPEAT_DELAY - 1; ++i) g_session.update(hold(BTN_RIGHT), 0);
    CHECK(g_session.cursor_x() == x0 + 1);
    g_session.update(hold(BTN_RIGHT), 0);
    CHECK(g_session.cursor_x() == x0 + 2);
}

TEST_CASE("A twice places a stone, and moving away cancels the ghost")
{
    g_session.start_sandbox(quick());
    g_session.set_confirm_two_press(true);
    const Point p = g_session.cursor_point();

    g_session.update(press(BTN_A), 0);
    CHECK(g_session.state() == PlayState::CONFIRM);
    CHECK(g_session.ghost_visible());
    CHECK(saw(g_session, EventKind::GHOST_SHOWN));
    CHECK(g_session.game().board().at(p) == go::EMPTY);

    g_session.update(press(BTN_RIGHT), 0);
    CHECK(g_session.state() == PlayState::PLAYER_TURN);
    CHECK_FALSE(g_session.ghost_visible());

    const Point q = g_session.cursor_point();
    g_session.update(press(BTN_A), 0);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.game().board().at(q) == BLACK);
    CHECK(saw(g_session, EventKind::STONE_PLACED));
}

TEST_CASE("single-press mode places on the first A")
{
    g_session.start_sandbox(quick());
    g_session.set_confirm_two_press(false);
    const Point p = g_session.cursor_point();
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.game().board().at(p) == BLACK);
}

TEST_CASE("the AI answers and the session comes back to the player")
{
    g_session.start_sandbox(quick(9, ai::Level::EASY));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.state() == PlayState::AI_THINKING);
    CHECK(saw(g_session, EventKind::AI_START));
    REQUIRE(settle(g_session));
    CHECK(g_session.state() == PlayState::PLAYER_TURN);
    CHECK(g_session.game().move_count() == 2);
    CHECK(saw(g_session, EventKind::AI_DONE));
}

TEST_CASE("hot seat never hands the turn to the AI")
{
    g_session.start_sandbox(quick(9, ai::Level::EASY, /*hotseat=*/true));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.state() == PlayState::PLAYER_TURN);
    CHECK(g_session.game().to_move() == WHITE);
    g_session.update(press(BTN_RIGHT), 0);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.game().to_move() == BLACK);
    CHECK(g_session.game().move_count() == 2);
}

TEST_CASE("an illegal move is reported, not played")
{
    g_session.start_sandbox(quick());
    g_session.set_confirm_two_press(false);
    const Point p = g_session.cursor_point();
    g_session.update(press(BTN_A), 0);
    REQUIRE(settle(g_session));
    while(g_session.cursor_point() != p) g_session.update(press(BTN_LEFT), 0);
    count_events(g_session, EventKind::NONE);          // drain
    g_session.update(press(BTN_A), 0);
    CHECK(saw(g_session, EventKind::ILLEGAL));
}

TEST_CASE("B takes back the player move and the AI reply")
{
    g_session.start_sandbox(quick(9, ai::Level::EASY));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_A), 0);
    REQUIRE(settle(g_session));
    REQUIRE(g_session.game().move_count() == 2);
    g_session.update(press(BTN_B), 0);
    CHECK(g_session.game().move_count() == 0);
    CHECK(g_session.game().to_move() == BLACK);
    CHECK(g_session.state() == PlayState::PLAYER_TURN);
}

TEST_CASE("R passes and two passes open the marking phase")
{
    g_session.start_sandbox(quick(7, ai::Level::VERY_EASY));
    // play both sides by hand so the game can end quickly
    g_session.start_sandbox(quick(7, ai::Level::VERY_EASY, /*hotseat=*/true));
    g_session.update(press(BTN_R), 0);
    CHECK(saw(g_session, EventKind::PASSED));
    g_session.update(press(BTN_R), 0);
    CHECK(g_session.state() == PlayState::MARK_DEAD);
    CHECK(g_session.game().phase() == go::Phase::MARK_DEAD);
}

TEST_CASE("the marking phase toggles dead groups and scores on START")
{
    g_session.start_sandbox(quick(7, ai::Level::VERY_EASY, true));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_A), 0);                 // one black stone
    const Point stone = g_session.game().board().last_move();
    g_session.update(press(BTN_R), 0);                 // white passes
    g_session.update(press(BTN_R), 0);                 // black passes
    g_session.update(press(BTN_R), 0);                 // white passes: two in a row
    REQUIRE(g_session.state() == PlayState::MARK_DEAD);

    while(g_session.cursor_point() != stone)
    {
        const int before = g_session.cursor_point();
        g_session.update(press(BTN_RIGHT), 0);
        if(g_session.cursor_point() == before) g_session.update(press(BTN_DOWN), 0);
    }
    const bool was_dead = g_session.is_dead(stone);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.is_dead(stone) != was_dead);
    CHECK(saw(g_session, EventKind::DEAD_TOGGLED));

    g_session.update(press(BTN_START), 0);
    CHECK(g_session.state() == PlayState::OVER);
    CHECK(g_session.game().phase() == go::Phase::OVER);
}

TEST_CASE("START opens the pause menu and every entry does something")
{
    g_session.start_sandbox(quick(9, ai::Level::EASY));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_START), 0);
    CHECK(g_session.state() == PlayState::PAUSED);
    CHECK(g_session.pause_item() == PauseItem::RESUME);
    CHECK(saw(g_session, EventKind::PAUSE_OPENED));

    g_session.update(press(BTN_DOWN), 0);
    CHECK(g_session.pause_item() == PauseItem::UNDO);
    g_session.update(press(BTN_UP), 0);
    CHECK(g_session.pause_item() == PauseItem::RESUME);
    g_session.update(press(BTN_UP), 0);
    CHECK(g_session.pause_item() == PauseItem::QUIT);   // the list wraps

    g_session.update(press(BTN_A), 0);
    CHECK(g_session.wants_quit());
    g_session.clear_requests();

    // estimate
    g_session.start_sandbox(quick(9, ai::Level::EASY));
    g_session.update(press(BTN_START), 0);
    for(int i = 0; i < int(PauseItem::ESTIMATE); ++i) g_session.update(press(BTN_DOWN), 0);
    REQUIRE(g_session.pause_item() == PauseItem::ESTIMATE);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.estimate_valid());
    CHECK(g_session.hud_panel() == HudPanel::ESTIMATE);

    // resign ends the game
    g_session.start_sandbox(quick(9, ai::Level::EASY));
    g_session.update(press(BTN_START), 0);
    for(int i = 0; i < int(PauseItem::RESIGN); ++i) g_session.update(press(BTN_DOWN), 0);
    REQUIRE(g_session.pause_item() == PauseItem::RESIGN);
    g_session.update(press(BTN_A), 0);
    CHECK(g_session.state() == PlayState::OVER);
    CHECK(g_session.game().end_reason() == go::EndReason::RESIGN);
    CHECK(g_session.game().winner() == WHITE);
}

TEST_CASE("L cycles the HUD panels")
{
    g_session.start_sandbox(quick());
    CHECK(g_session.hud_panel() == HudPanel::INFO);
    g_session.update(press(BTN_L), 0);
    CHECK(g_session.hud_panel() == HudPanel::MOVES);
    g_session.update(press(BTN_L), 0);
    CHECK(g_session.hud_panel() == HudPanel::ESTIMATE);
    CHECK(g_session.estimate_valid());
    g_session.update(press(BTN_L), 0);
    CHECK(g_session.hud_panel() == HudPanel::INFO);
}

TEST_CASE("SELECT shows a hint that is a legal move")
{
    g_session.start_sandbox(quick());
    g_session.update(press(BTN_SELECT), 0);
    CHECK(g_session.hint_visible());
    CHECK(g_session.game().is_legal(g_session.hint_point()));
    Event e;
    CHECK(saw(g_session, EventKind::HINT_SHOWN, &e));
    CHECK(e.point == g_session.hint_point());
}

TEST_CASE("a campaign mission runs through the session and clears")
{
    const campaign::MissionDef& take_it = campaign::mission(2);     // capture in one move
    g_session.start_campaign(take_it, 0);
    CHECK(g_session.state() == PlayState::PLAYER_TURN);
    CHECK(g_session.mode() == Mode::CAMPAIGN);

    const Point target = g_session.game().board().point(take_it.solution[0].x, take_it.solution[0].y);
    g_session.set_confirm_two_press(false);
    g_session.force_move(target);
    CHECK(g_session.mission_status() == campaign::MissionStatus::CLEAR);
    CHECK(g_session.state() == PlayState::OVER);
    CHECK(saw(g_session, EventKind::MISSION_CLEAR));
}

TEST_CASE("a capture raises a capture event with the right count")
{
    const campaign::MissionDef& last_breath = campaign::mission(6);  // capture a two-stone group
    g_session.start_campaign(last_breath, 0);
    const Point p = g_session.game().board().point(last_breath.solution[0].x, last_breath.solution[0].y);
    g_session.force_move(p);
    Event e;
    REQUIRE(saw(g_session, EventKind::CAPTURE, &e));
    CHECK(e.count == 2);
    CHECK(e.color == BLACK);
}

TEST_CASE("an atari on the opponent raises an atari event")
{
    const campaign::MissionDef& breath = campaign::mission(1);       // reduce a stone to one liberty
    g_session.start_campaign(breath, 0);
    for(int i = 0; i < breath.solution_len; ++i)
    {
        const Point p = g_session.game().board().point(breath.solution[i].x, breath.solution[i].y);
        g_session.force_move(p);
    }
    CHECK(g_session.mission_status() == campaign::MissionStatus::CLEAR);
}

TEST_CASE("the mission opponent plays through the session")
{
    const campaign::MissionDef& ladder = campaign::mission(5);
    g_session.start_campaign(ladder, 0);
    const Point first = g_session.game().board().point(ladder.solution[0].x, ladder.solution[0].y);
    g_session.force_move(first);
    CHECK(g_session.state() == PlayState::AI_THINKING);
    REQUIRE(settle(g_session));
    CHECK(g_session.game().move_count() == 2);
    CHECK(g_session.state() == PlayState::PLAYER_TURN);
}

TEST_CASE("finish_ai_now ends the AI turn in a single frame")
{
    g_session.start_sandbox(quick(9, ai::Level::HARD));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_A), 0);
    REQUIRE(g_session.state() == PlayState::AI_THINKING);
    g_session.finish_ai_now();
    CHECK(g_session.state() != PlayState::AI_THINKING);
    CHECK(g_session.game().move_count() == 2);
}

TEST_CASE("the AI never exceeds its frame budget")
{
    g_session.start_sandbox(quick(9, ai::Level::HARD));
    g_session.set_confirm_two_press(false);
    g_session.update(press(BTN_A), 0);
    int frames = 0;
    while(g_session.state() == PlayState::AI_THINKING && frames < 1000)
    {
        g_session.update(none(), 4);               // a deliberately small slice
        ++frames;
    }
    CHECK(g_session.state() != PlayState::AI_THINKING);
    CHECK(frames <= ai::default_limits(ai::Level::HARD).frame_budget + 1);
}

TEST_CASE("a full sandbox game reaches a score")
{
    g_session.start_sandbox(quick(7, ai::Level::VERY_EASY));
    g_session.set_confirm_two_press(false);
    int guard = 0;
    while(g_session.state() != PlayState::OVER && guard++ < 4000)
    {
        if(g_session.state() == PlayState::AI_THINKING) { g_session.update(none(), 64); continue; }
        if(g_session.state() == PlayState::MARK_DEAD) { g_session.update(press(BTN_START), 0); continue; }
        // walk to the first legal point and play it, else pass
        const go::Board& b = g_session.game().board();
        Point target = PASS;
        for(int i = 0; i < b.empty_count(); ++i)
        {
            const Point p = b.empty_at(i);
            if(g_session.game().is_legal(p) && !b.is_eye_like(p, g_session.game().to_move())) { target = p; break; }
        }
        g_session.force_move(target);
    }
    CHECK(g_session.state() == PlayState::OVER);
    CHECK(g_session.game().phase() == go::Phase::OVER);
}
