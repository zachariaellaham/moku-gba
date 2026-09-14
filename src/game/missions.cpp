// Campaign logic: running a mission, judging its objective, ranking it and hinting.
// The positions themselves are generated and proved in missions_data.cpp (tools/gen_missions.py).
#include "game/missions.h"
#include "go/ladder.h"
#include "go/score.h"
#include "ai/ai.h"
#include "ai/internal.h"

namespace campaign {

using go::BLACK;
using go::Board;
using go::Color;
using go::EMPTY;
using go::PASS;
using go::Point;

namespace {

// The net test needs two scratch boards. Missions are judged between turns, never while the AI is
// searching, so one of them is the AI's own scratch board: EWRAM is the scarcest thing on this
// machine and a Board is eleven kilobytes.
go::Board g_net_b GO_EWRAM_BSS;
bool g_benson[go::MAX_POINTS] GO_EWRAM_BSS;

Point pt(const Board& b, XY xy) { return b.point(xy.x, xy.y); }

const MissionDef& stage_def(const MissionRun& run) { return *run.stage; }

// All marked points are empty: the marked group has been captured.
bool marked_captured(const MissionRun& run)
{
    const MissionDef& d = stage_def(run);
    const Board& b = run.game.board();
    for(int i = 0; i < d.marked_count; ++i)
        if(b.at(pt(b, d.marked[i])) != EMPTY) return false;
    return d.marked_count > 0;
}

// The marked group, if any stone of it is still on the board.
Point marked_stone(const MissionRun& run)
{
    const MissionDef& d = stage_def(run);
    const Board& b = run.game.board();
    for(int i = 0; i < d.marked_count; ++i)
    {
        const Point p = pt(b, d.marked[i]);
        if(b.at(p) != EMPTY) return p;
    }
    return go::NO_POINT;
}

int marked_liberties(const MissionRun& run)
{
    const Point p = marked_stone(run);
    if(p == go::NO_POINT) return 0;
    return run.game.board().liberties(p);
}

// The net test, and the reason mission 8 can be won in one move: after the player's move the
// target has at most two liberties and every escape is answered by a capture or a won ladder.
bool group_is_netted(const Board& board, Point target)
{
    if(board.at(target) == EMPTY) return true;
    const Color defender = board.at(target);
    const Color attacker = go::opponent(defender);
    Point libs[4];
    const int n = board.liberties(target, libs, 4, 3);
    if(n > 2) return false;
    if(n == 0) return true;
    for(int i = 0; i < n; ++i)
    {
        if(!board.is_legal(libs[i], defender)) continue;
        go::Board& g_net_a = ai::detail::shared_scratch();
        g_net_a.copy_from(board);
        g_net_a.play(libs[i], defender);
        if(g_net_a.at(target) == EMPTY) continue;          // the escape killed itself
        Point replies[4];
        const int rn = g_net_a.liberties(target, replies, 4, 4);
        bool answered = false;
        for(int r = 0; r < rn && !answered; ++r)
        {
            if(!g_net_a.is_legal(replies[r], attacker)) continue;
            g_net_b.copy_from(g_net_a);
            g_net_b.play(replies[r], attacker);
            if(g_net_b.at(target) == EMPTY) answered = true;
            else if(go::read_ladder(g_net_b, target, /*attacker_to_move=*/false) == go::LadderResult::CAPTURED) answered = true;
        }
        if(!answered) return false;
    }
    return true;
}

bool group_is_pass_alive(const MissionRun& run, Point seed)
{
    const Board& b = run.game.board();
    if(b.at(seed) == EMPTY) return false;
    go::benson_pass_alive(b, g_benson);
    return g_benson[seed];
}

// A pass-alive group of the player's colour whose stones all sit inside the marked2 area.
bool alive_inside_area(const MissionRun& run)
{
    const MissionDef& d = stage_def(run);
    const Board& b = run.game.board();
    go::benson_pass_alive(b, g_benson);
    for(int i = 0; i < d.marked2_count; ++i)
    {
        const Point p = pt(b, d.marked2[i]);
        if(b.at(p) != d.player || !g_benson[p]) continue;
        // walk the group: every stone must be inside the area
        bool inside = true;
        Point q = p;
        do
        {
            bool found = false;
            for(int k = 0; k < d.marked2_count; ++k)
                if(pt(b, d.marked2[k]) == q) { found = true; break; }
            if(!found) { inside = false; break; }
            q = b.next_stone(q);
        } while(q != p);
        if(inside) return true;
    }
    return false;
}

// Seki: the two marked groups share the same two liberties and have no others.
bool in_seki(const MissionRun& run)
{
    const MissionDef& d = stage_def(run);
    const Board& b = run.game.board();
    if(d.marked_count == 0 || d.marked2_count == 0) return false;
    const Point a = pt(b, d.marked[0]);
    const Point c = pt(b, d.marked2[0]);
    if(b.at(a) == EMPTY || b.at(c) == EMPTY) return false;
    Point la[4], lc[4];
    const int na = b.liberties(a, la, 4, 3);
    const int nc = b.liberties(c, lc, 4, 3);
    if(na != 2 || nc != 2) return false;
    for(int i = 0; i < 2; ++i)
    {
        bool shared = false;
        for(int j = 0; j < 2; ++j) if(la[i] == lc[j]) shared = true;
        if(!shared) return false;
    }
    return true;
}

}  // namespace

ai::Level opponent_level(Opponent o)
{
    switch(o)
    {
    case Opponent::VERY_EASY: return ai::Level::VERY_EASY;
    case Opponent::EASY:      return ai::Level::EASY;
    case Opponent::NORMAL:    return ai::Level::NORMAL;
    case Opponent::HARD:      return ai::Level::HARD;
    case Opponent::MASTER:    return ai::Level::MASTER;
    default:                  return ai::Level::VERY_EASY;
    }
}

void mission_start(MissionRun& run, const MissionDef& def, int stage)
{
    run.def = &def;
    run.stage_index = uint8_t(stage);
    run.stage = (def.stage_count > 0) ? &def.stages[stage] : &def;
    const MissionDef& s = *run.stage;

    go::GameSettings settings;
    settings.size = s.board;
    settings.rules = s.rules;
    settings.komi_x2 = s.komi_x2;
    settings.handicap = s.handicap;
    run.game.start(settings);
    for(int i = 0; i < s.stone_count; ++i)
        run.game.add_setup_stone(run.game.board().point(s.stones[i].x, s.stones[i].y), s.stones[i].c);
    if(s.handicap == 0) run.game.set_to_move(s.player);

    run.status = MissionStatus::PLAYING;
    run.player_moves = 0;
    run.captures = 0;
    run.ko_captures = 0;
    run.survive_moves = 0;
    run.snapback_armed = false;
    run.fail_reason_id = 0;
    // undos and hints are kept across stages so the rank covers the whole mission
    if(stage == 0) { run.undos = 0; run.hints = 0; }
}

MissionStatus mission_check(MissionRun& run)
{
    if(run.status != MissionStatus::PLAYING) return run.status;
    const MissionDef& d = stage_def(run);
    const go::Game& g = run.game;
    const Board& b = g.board();
    bool clear = false, fail = false;

    switch(d.objective)
    {
    case Objective::PLACE_ON_MARKED:
    {
        int placed = 0;
        for(int i = 0; i < d.marked_count; ++i) if(b.at(pt(b, d.marked[i])) == d.player) ++placed;
        clear = placed >= d.param;
        break;
    }
    case Objective::REDUCE_TO_1_LIB:
        if(marked_stone(run) == go::NO_POINT) { clear = true; break; }   // capturing it also works
        clear = marked_liberties(run) == 1;
        break;
    case Objective::CAPTURE_MARKED:
        clear = marked_captured(run);
        break;
    case Objective::SAVE_MARKED:
    {
        const Point p = marked_stone(run);
        if(p == go::NO_POINT) { fail = true; run.fail_reason_id = d.text_fail; break; }
        clear = b.liberties(p, nullptr, 0, d.param + 1) >= d.param;
        break;
    }
    case Objective::CONNECT_MARKED:
    {
        if(d.marked_count == 0 || d.marked2_count == 0) break;
        const Point a = pt(b, d.marked[0]), c = pt(b, d.marked2[0]);
        if(b.at(a) == EMPTY || b.at(c) == EMPTY) { fail = true; break; }
        clear = b.group_root(a) == b.group_root(c);
        break;
    }
    case Objective::CAPTURE_LADDER:
        if(marked_captured(run)) { clear = true; break; }
        if(marked_liberties(run) >= 3) { fail = true; run.fail_reason_id = d.text_fail; }
        break;
    case Objective::CAPTURE_NET:
    {
        if(marked_captured(run)) { clear = true; break; }
        const Point p = marked_stone(run);
        if(p != go::NO_POINT && g.to_move() != d.player && group_is_netted(b, p)) { clear = true; break; }
        if(marked_liberties(run) >= 3) { fail = true; run.fail_reason_id = d.text_fail; }
        break;
    }
    case Objective::KO_WIN:
        // Take the ko, then use the move White cannot answer to close it for good.
        if(run.ko_captures >= d.param && d.marked2_count > 0)
            clear = b.at(pt(b, d.marked[0])) == d.player && b.at(pt(b, d.marked2[0])) == d.player;
        break;
    case Objective::KILL_MARKED:
        clear = marked_captured(run);
        break;
    case Objective::MAKE_TWO_EYES:
    {
        const Point p = marked_stone(run);
        if(p == go::NO_POINT) { fail = true; run.fail_reason_id = d.text_fail; break; }
        clear = group_is_pass_alive(run, p);
        break;
    }
    case Objective::SNAPBACK:
        clear = run.snapback_armed && run.captures >= d.param;
        break;
    case Objective::SEKI_SURVIVE:
        if(in_seki(run))
        {
            clear = run.survive_moves >= d.param;
        }
        else if(run.survive_moves > 0)
        {
            fail = true;                       // the seki existed and the player broke it
            run.fail_reason_id = d.text_fail;
        }
        break;
    case Objective::WIN_BY_MARGIN:
        if(g.phase() == go::Phase::OVER)
        {
            const int margin = (d.player == BLACK) ? g.final_score().margin_x2 : -g.final_score().margin_x2;
            clear = margin >= d.param;
            fail = !clear;
        }
        break;
    case Objective::LIVE_IN_CORNER:
        clear = alive_inside_area(run);
        break;
    case Objective::WIN_GAME:
        if(g.phase() == go::Phase::OVER)
        {
            clear = g.winner() == d.player;
            fail = !clear;
        }
        break;
    case Objective::TSUMEGO:
        break;                                  // the parent mission is judged stage by stage
    }

    if(!clear && !fail && d.move_limit > 0 && run.player_moves >= d.move_limit)
    {
        fail = true;
        run.fail_reason_id = d.text_fail;
    }
    if(clear) run.status = MissionStatus::CLEAR;
    else if(fail) run.status = MissionStatus::FAIL;
    return run.status;
}

go::PlayResult mission_player_move(MissionRun& run, Point p)
{
    const MissionDef& d = stage_def(run);
    const Board& before = run.game.board();
    const bool on_marked = (d.marked_count > 0) && p == before.point(d.marked[0].x, d.marked[0].y);
    const int caps_before = before.captures(d.player);

    go::PlayResult r = run.game.play(p);
    if(!r.ok) return r;
    if(p != PASS) ++run.player_moves;
    const int gained = run.game.board().captures(d.player) - caps_before;
    run.captures += gained;
    if(d.objective == Objective::KO_WIN && on_marked && gained > 0) ++run.ko_captures;
    if(d.objective == Objective::SEKI_SURVIVE && in_seki(run)) ++run.survive_moves;
    mission_check(run);
    return r;
}

void mission_opponent_played(MissionRun& run, Point p)
{
    const MissionDef& d = stage_def(run);
    const Color opp = go::opponent(d.player);
    const int caps_before = run.game.board().captures(opp);
    run.game.play(p);
    if(run.game.board().captures(opp) > caps_before) run.snapback_armed = true;
    mission_check(run);
}

bool mission_undo(MissionRun& run)
{
    if(run.status == MissionStatus::CLEAR) return false;
    bool undone = false;
    // take back the opponent's reply as well, so the player gets their own position back
    if(run.game.move_count() > 0 && run.game.board().last_color() != run.def->player)
        undone = run.game.undo();
    if(run.game.move_count() > 0 && run.game.undo())
    {
        undone = true;
        if(run.player_moves > 0) --run.player_moves;
    }
    if(undone)
    {
        ++run.undos;
        run.status = MissionStatus::PLAYING;
        run.fail_reason_id = 0;
        if(run.survive_moves > 0) --run.survive_moves;
        mission_check(run);
    }
    return undone;
}

Point mission_hint(MissionRun& run)
{
    const MissionDef& d = stage_def(run);
    ++run.hints;
    const Board& b = run.game.board();
    // if the played moves follow the scripted solution, the hint is simply the next one
    if(d.solution && d.solution_len > 0)
    {
        for(int i = 0; i < d.solution_len; ++i)
        {
            if(d.solution[i].x < 0) return PASS;          // "play elsewhere" steps are stored as passes
            const Point p = b.point(d.solution[i].x, d.solution[i].y);
            if(b.at(p) == EMPTY && run.game.is_legal(p)) return p;
        }
    }
    return ai::pick_move(run.game, ai::Level::NORMAL, uint32_t(run.player_moves) * 2654435761u + 7u);
}

bool next_stage(MissionRun& run)
{
    if(!run.def || run.def->stage_count == 0) return false;
    if(run.stage_index + 1 >= run.def->stage_count) return false;
    const int undos = run.undos, hints = run.hints;
    mission_start(run, *run.def, run.stage_index + 1);
    run.undos = undos;
    run.hints = hints;
    return true;
}

Rank mission_rank(const MissionRun& run)
{
    const MissionDef& d = run.def ? *run.def : stage_def(run);
    if(run.status != MissionStatus::CLEAR) return Rank::NONE;

    // Full games are ranked by how big the win is, not by how few moves it took.
    if(d.objective == Objective::WIN_BY_MARGIN || d.objective == Objective::WIN_GAME)
    {
        const int margin = (d.player == BLACK) ? run.game.final_score().margin_x2 : -run.game.final_score().margin_x2;
        if(margin >= d.rank_s) return Rank::S;
        if(margin >= d.rank_a) return Rank::A;
        if(margin >= d.rank_b) return Rank::B;
        return Rank::C;
    }

    int moves = run.player_moves + run.undos * 2 + run.hints * 2;
    if(d.rank_s > 0 && moves <= d.rank_s) return Rank::S;
    if(d.rank_a > 0 && moves <= d.rank_a) return Rank::A;
    if(d.rank_b > 0 && moves <= d.rank_b) return Rank::B;
    return Rank::C;
}

bool mission_stars(const MissionRun& run)
{
    return mission_rank(run) >= Rank::A && run.hints == 0 && run.undos == 0;
}

int mission_marked_points(const MissionRun& run, Point* out, int max_out)
{
    const MissionDef& d = stage_def(run);
    const Board& b = run.game.board();
    int count = 0;
    for(int i = 0; i < d.marked_count && count < max_out; ++i) out[count++] = pt(b, d.marked[i]);
    return count;
}

}  // namespace campaign
