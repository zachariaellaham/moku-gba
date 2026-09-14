// Every campaign mission must be winnable by its scripted solution against the opponent it ships
// with, and must refuse the naive answer. These tests run the real rules engine and the real AI.
#include "doctest.h"
#include <initializer_list>
#include "game/missions.h"
#include "go/ladder.h"
#include "ai/ai.h"

using namespace campaign;
using go::BLACK;
using go::Board;
using go::PASS;
using go::Point;

namespace {

// Lets the mission's opponent answer, exactly as the play scene does.
void opponent_reply(MissionRun& run, uint32_t seed)
{
    const MissionDef& d = *run.stage;
    if(run.status != MissionStatus::PLAYING) return;
    if(run.game.phase() != go::Phase::PLAYING) return;
    if(run.game.to_move() == d.player) return;
    if(d.opponent == Opponent::NONE) { mission_opponent_played(run, PASS); return; }
    const Point m = ai::pick_move(run.game, opponent_level(d.opponent), seed);
    mission_opponent_played(run, m);
}

// A ladder mission is scripted only by its first atari; the rest is read out move by move.
Point next_ladder_atari(const MissionRun& run)
{
    const MissionDef& d = *run.stage;
    const Board& b = run.game.board();
    Point target = go::NO_POINT;
    for(int i = 0; i < d.marked_count; ++i)
    {
        const Point p = b.point(d.marked[i].x, d.marked[i].y);
        if(b.at(p) != go::EMPTY) { target = p; break; }
    }
    if(target == go::NO_POINT) return PASS;
    Point libs[4];
    const int n = b.liberties(target, libs, 4, 4);
    for(int i = 0; i < n; ++i)
    {
        if(!run.game.is_legal(libs[i])) continue;
        // does this atari capture straight away, or hold as a ladder?
        static Board scratch;
        scratch.copy_from(b);
        scratch.play(libs[i], d.player);
        if(scratch.at(target) == go::EMPTY) return libs[i];
        if(go::read_ladder(scratch, target, /*attacker_to_move=*/false) == go::LadderResult::CAPTURED) return libs[i];
    }
    return n > 0 ? libs[0] : PASS;
}

// Plays a mission's scripted solution and returns the final status.
MissionStatus solve(MissionRun& run, const MissionDef& def, int stage = 0, int* moves_out = nullptr)
{
    mission_start(run, def, stage);
    const MissionDef& d = *run.stage;
    int guard = 0;
    int scripted = 0;
    while(run.status == MissionStatus::PLAYING && guard++ < 60)
    {
        Point m;
        if(d.objective == Objective::CAPTURE_LADDER && scripted >= 1) m = next_ladder_atari(run);
        else if(scripted < d.solution_len)
            m = (d.solution[scripted].x < 0) ? PASS
                                             : run.game.board().point(d.solution[scripted].x, d.solution[scripted].y);
        else break;
        ++scripted;
        const go::PlayResult r = mission_player_move(run, m);
        if(!r.ok) { INFO("illegal scripted move at step ", scripted); return MissionStatus::FAIL; }
        opponent_reply(run, uint32_t(def.id) * 7919u + uint32_t(guard) * 104729u + 3u);
    }
    if(moves_out) *moves_out = run.player_moves;
    return run.status;
}

MissionRun g_run;   // 25 KB: too big for the stack

}  // namespace

TEST_CASE("the mission table is consistent")
{
    for(int i = 0; i < MISSION_COUNT; ++i)
    {
        const MissionDef& d = mission(i);
        CAPTURE(i);
        CHECK(d.id == i + 1);
        CHECK((d.board == 7 || d.board == 9 || d.board == 13 || d.board == 19));
        CHECK(d.player == BLACK);
        CHECK(d.name_id != 0);
        CHECK(d.objective_id != 0);
        CHECK(d.text_before != 0);
        // board sizes must never shrink as the campaign goes on
        if(i > 0) CHECK(d.board >= mission(i - 1).board);
    }
    CHECK(mission(17).stage_count == 5);       // mission 18 is the five-problem set
}

TEST_CASE("every setup position is legal")
{
    for(int i = 0; i < MISSION_COUNT; ++i)
    {
        const MissionDef& d = mission(i);
        const int stages = d.stage_count > 0 ? d.stage_count : 1;
        for(int s = 0; s < stages; ++s)
        {
            mission_start(g_run, d, s);
            const Board& b = g_run.game.board();
            for(int y = 0; y < b.size(); ++y)
                for(int x = 0; x < b.size(); ++x)
                {
                    const Point p = b.point(x, y);
                    if(b.at(p) == go::EMPTY) continue;
                    CAPTURE(d.id);
                    CAPTURE(x);
                    CAPTURE(y);
                    CHECK(b.liberties(p, nullptr, 0, 1) > 0);
                }
        }
    }
}

TEST_CASE("the tactical missions are solved by their scripted solution")
{
    // Missions 15, 19 and 20 are whole games; they are covered by their own test below.
    const int tactical[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17 };
    for(int id : tactical)
    {
        const MissionDef& d = mission(id - 1);
        int moves = 0;
        const MissionStatus st = solve(g_run, d, 0, &moves);
        CAPTURE(id);
        CHECK(st == MissionStatus::CLEAR);
        CHECK(moves <= (d.move_limit ? d.move_limit : 60));
        if(st == MissionStatus::CLEAR)
        {
            CHECK(mission_rank(g_run) == Rank::S);
            CHECK(mission_stars(g_run));
        }
    }
}

TEST_CASE("mission 18 chains five problems")
{
    const MissionDef& d = mission(17);
    REQUIRE(d.stage_count == 5);
    for(int s = 0; s < d.stage_count; ++s)
    {
        CAPTURE(s);
        CHECK(solve(g_run, d, s) == MissionStatus::CLEAR);
    }
}

TEST_CASE("mission 6 refuses the ladder played from the wrong side")
{
    const MissionDef& d = mission(5);
    mission_start(g_run, d, 0);
    const Board& b = g_run.game.board();
    const Point target = b.point(d.marked[0].x, d.marked[0].y);
    Point libs[4];
    const int n = b.liberties(target, libs, 4, 4);
    REQUIRE(n == 2);
    const Point right = b.point(d.solution[0].x, d.solution[0].y);
    const Point wrong = (libs[0] == right) ? libs[1] : libs[0];

    mission_player_move(g_run, wrong);
    int guard = 0;
    while(g_run.status == MissionStatus::PLAYING && guard++ < 20)
    {
        opponent_reply(g_run, 1234u + uint32_t(guard));
        if(g_run.status != MissionStatus::PLAYING) break;
        mission_player_move(g_run, next_ladder_atari(g_run));
    }
    CHECK(g_run.status == MissionStatus::FAIL);
}

TEST_CASE("mission 8 accepts only the net, not the ladder")
{
    const MissionDef& d = mission(7);
    const Board* b = nullptr;

    mission_start(g_run, d, 0);
    b = &g_run.game.board();
    const Point net = b->point(d.solution[0].x, d.solution[0].y);
    mission_player_move(g_run, net);
    CHECK(g_run.status == MissionStatus::CLEAR);

    for(int i = 0; i < d.marked2_count; ++i)
    {
        mission_start(g_run, d, 0);
        b = &g_run.game.board();
        const Point ladder_point = b->point(d.marked2[i].x, d.marked2[i].y);
        mission_player_move(g_run, ladder_point);
        CAPTURE(i);
        CHECK(g_run.status != MissionStatus::CLEAR);
        int guard = 0;
        while(g_run.status == MissionStatus::PLAYING && guard++ < 12)
        {
            opponent_reply(g_run, 777u + uint32_t(guard));
            if(g_run.status != MissionStatus::PLAYING) break;
            mission_player_move(g_run, next_ladder_atari(g_run));
        }
        CHECK(g_run.status == MissionStatus::FAIL);
    }
}

TEST_CASE("mission 14 refuses every move except the one that makes seki")
{
    const MissionDef& d = mission(13);
    mission_start(g_run, d, 0);
    const Board& b = g_run.game.board();
    const Point right = b.point(d.solution[0].x, d.solution[0].y);
    const Point marked = b.point(d.marked[0].x, d.marked[0].y);
    Point libs[4];
    const int n = b.liberties(marked, libs, 4, 4);
    for(int i = 0; i < n; ++i)
    {
        if(libs[i] == right) continue;
        mission_start(g_run, d, 0);
        mission_player_move(g_run, libs[i]);
        int guard = 0;
        while(g_run.status == MissionStatus::PLAYING && guard++ < 10)
        {
            opponent_reply(g_run, 42u + uint32_t(guard));
            if(g_run.status != MissionStatus::PLAYING) break;
            mission_player_move(g_run, PASS);
        }
        CAPTURE(i);
        CHECK(g_run.status != MissionStatus::CLEAR);
    }
}

TEST_CASE("mission 11 refuses the two eye-space points that die")
{
    const MissionDef& d = mission(10);
    mission_start(g_run, d, 0);
    const Board& b = g_run.game.board();
    const Point right = b.point(d.solution[0].x, d.solution[0].y);
    const Point marked = b.point(d.marked[0].x, d.marked[0].y);
    Point libs[4];
    const int n = b.liberties(marked, libs, 4, 4);
    REQUIRE(n == 3);
    for(int i = 0; i < n; ++i)
    {
        if(libs[i] == right) continue;
        mission_start(g_run, d, 0);
        mission_player_move(g_run, libs[i]);
        CAPTURE(i);
        CHECK(g_run.status != MissionStatus::CLEAR);
    }
}

TEST_CASE("hints follow the solution and cost rank")
{
    const MissionDef& d = mission(2);        // Take It: a one-move capture
    mission_start(g_run, d, 0);
    const Point hint = mission_hint(g_run);
    CHECK(hint == g_run.game.board().point(d.solution[0].x, d.solution[0].y));
    CHECK(g_run.hints == 1);
    mission_player_move(g_run, hint);
    CHECK(g_run.status == MissionStatus::CLEAR);
    CHECK_FALSE(mission_stars(g_run));       // a hint costs the stars
}

TEST_CASE("undo takes back the player move and the reply")
{
    const MissionDef& d = mission(5);        // Ladder: the opponent always answers
    mission_start(g_run, d, 0);
    const Point first = g_run.game.board().point(d.solution[0].x, d.solution[0].y);
    mission_player_move(g_run, first);
    opponent_reply(g_run, 5u);
    const int moves_before = g_run.game.move_count();
    REQUIRE(moves_before >= 2);
    CHECK(mission_undo(g_run));
    CHECK(g_run.game.move_count() == 0);
    CHECK(g_run.player_moves == 0);
    CHECK(g_run.undos == 1);
    CHECK(g_run.game.to_move() == d.player);
}

TEST_CASE("a mission fails when the move limit runs out")
{
    const MissionDef& d = mission(2);        // Take It, limit 4
    REQUIRE(d.move_limit > 0);
    mission_start(g_run, d, 0);
    const Board& b = g_run.game.board();
    int played = 0;
    for(int y = 6; y >= 0 && g_run.status == MissionStatus::PLAYING; --y)
    {
        const Point p = b.point(0, y);
        if(!g_run.game.is_legal(p)) continue;
        mission_player_move(g_run, p);
        ++played;
        opponent_reply(g_run, 3u);
    }
    CHECK(g_run.status == MissionStatus::FAIL);
    CHECK(played <= d.move_limit + 1);
}

TEST_CASE("the whole-game missions start correctly and are winnable in principle")
{
    for(int id : { 15, 19, 20 })
    {
        const MissionDef& d = mission(id - 1);
        mission_start(g_run, d, 0);
        CAPTURE(id);
        CHECK(g_run.status == MissionStatus::PLAYING);
        CHECK(g_run.game.board().size() == d.board);
        CHECK(d.komi_x2 > 0);
        if(d.handicap > 0)
        {
            CHECK(g_run.game.board().stones(BLACK) == d.handicap);
            CHECK(g_run.game.to_move() == go::WHITE);
        }
    }
}

TEST_CASE("mission 15 can be won against the opponent it ships with")
{
    const MissionDef& d = mission(14);
    mission_start(g_run, d, 0);
    int guard = 0;
    while(g_run.game.phase() == go::Phase::PLAYING && guard++ < 400)
    {
        if(g_run.game.to_move() == d.player)
        {
            // the player is represented by the level one step above the opponent
            const Point m = ai::pick_move(g_run.game, ai::Level::NORMAL, 91u + uint32_t(guard) * 7919u);
            mission_player_move(g_run, m);
        }
        else
        {
            opponent_reply(g_run, 13u + uint32_t(guard) * 104729u);
        }
    }
    if(g_run.game.phase() == go::Phase::MARK_DEAD)
    {
        g_run.game.propose_dead_marks();
        g_run.game.finish_marking();
        mission_check(g_run);
    }
    CHECK(g_run.status == MissionStatus::CLEAR);
}
