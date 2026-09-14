// Prints the player moves that solve a mission against the opponent it ships with, so the ROM test
// can replay exactly that line. Used by tools/gen_rom_tests.py.
//
//   tool_mission_line 6      ->  7 2
//                                6 4
//                                ...
#include <cstdio>
#include <cstdlib>

#include "ai/ai.h"
#include "game/missions.h"
#include "go/ladder.h"

using namespace campaign;
using go::Board;
using go::PASS;
using go::Point;

namespace {

MissionRun g_run;
Board g_scratch;

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
        g_scratch.copy_from(b);
        g_scratch.play(libs[i], d.player);
        if(g_scratch.at(target) == go::EMPTY) return libs[i];
        if(go::read_ladder(g_scratch, target, false) == go::LadderResult::CAPTURED) return libs[i];
    }

    return n > 0 ? libs[0] : PASS;
}

}  // namespace

int main(int argc, char** argv)
{
    const int id = argc > 1 ? atoi(argv[1]) : 6;
    const MissionDef& d = mission(id - 1);
    mission_start(g_run, d, 0);

    int scripted = 0;
    int guard = 0;

    while(g_run.status == MissionStatus::PLAYING && guard++ < 40)
    {
        Point m;

        if(d.objective == Objective::CAPTURE_LADDER && scripted >= 1)
        {
            m = next_ladder_atari(g_run);
        }
        else if(scripted < d.solution_len)
        {
            m = (d.solution[scripted].x < 0) ? PASS
                                             : g_run.game.board().point(d.solution[scripted].x, d.solution[scripted].y);
        }
        else if(d.move_limit > 0 && g_run.player_moves < d.move_limit)
        {
            // The scripted moves have run out but the mission is not won yet - missions like the
            // 3-3 invasion are a sequence, not a single answer. Let the strongest level finish it,
            // and print what it played: that is the line the ROM test replays.
            m = ai::pick_move(g_run.game, ai::Level::MASTER, uint32_t(guard) * 2654435761u + 11u);
        }
        else
        {
            break;
        }

        ++scripted;

        if(m == PASS) printf("pass\n");
        else printf("%d %d\n", g_run.game.board().x_of(m), g_run.game.board().y_of(m));

        if(!mission_player_move(g_run, m).ok)
        {
            fprintf(stderr, "illegal scripted move\n");
            return 2;
        }

        if(g_run.status != MissionStatus::PLAYING) break;
        if(g_run.game.to_move() == d.player) continue;

        const Point reply = (d.opponent == Opponent::NONE)
            ? PASS
            : ai::pick_move(g_run.game, opponent_level(d.opponent), uint32_t(d.id) * 7919u + uint32_t(guard) * 104729u + 3u);
        mission_opponent_played(g_run, reply);
    }

    fprintf(stderr, "mission %d final status %d after %d player moves\n", id, int(g_run.status), g_run.player_moves);
    return g_run.status == MissionStatus::CLEAR ? 0 : 1;
}
