// AI strength ladder: plays level A against level B and prints the win rate.
//   tool_arena [--size 9] [--games 50] [--komi 15] [--a EASY] [--b VERY_EASY] [--seed 1] [--verbose]
// Colours alternate so neither level keeps the first move advantage.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "ai/ai.h"
#include "ai/internal.h"

using namespace go;

namespace {

const char* LEVEL_NAMES[] = { "VERY_EASY", "EASY", "NORMAL", "HARD", "MASTER" };

ai::Level parse_level(const char* s)
{
    for(int i = 0; i < ai::LEVEL_COUNT; ++i) if(!strcmp(s, LEVEL_NAMES[i])) return ai::Level(i);
    fprintf(stderr, "unknown level %s\n", s);
    exit(2);
}

struct GameResult { int margin_x2; int moves; };

GameResult play(ai::Level black, ai::Level white, int size, int komi_x2, uint32_t seed,
                const ai::Limits* lim_black, const ai::Limits* lim_white)
{
    GameSettings s;
    s.size = uint8_t(size);
    s.komi_x2 = int16_t(komi_x2);
    Game g;
    g.start(s);
    ai::Thinker th;
    int moves = 0;
    const int limit = size * size * 3;
    while(g.phase() == Phase::PLAYING && moves < limit)
    {
        const bool is_black = g.to_move() == BLACK;
        const ai::Level lvl = is_black ? black : white;
        const ai::Limits* lim = is_black ? lim_black : lim_white;
        th.start(g, lvl, seed * 2654435761u + uint32_t(moves) * 40503u + 1u, lim);
        while(!th.step(64)) {}
        Point m = th.result();
        if(th.wants_resign()) { return { is_black ? -1000 : 1000, moves }; }
        if(m != PASS && !g.is_legal(m)) { fprintf(stderr, "ILLEGAL move from %s\n", LEVEL_NAMES[int(lvl)]); exit(3); }
        g.play(m);
        ++moves;
    }
    if(g.phase() == Phase::MARK_DEAD)
    {
        g.propose_dead_marks();
        g.finish_marking();
        return { g.final_score().margin_x2, moves };
    }
    return { g.score().margin_x2, moves };
}

}  // namespace

int main(int argc, char** argv)
{
    int size = 9, games = 50, komi_x2 = 15, verbose = 0;
    uint32_t seed = 1;
    ai::Level a = ai::Level::EASY, b = ai::Level::VERY_EASY;
    int playouts_a = 0, playouts_b = 0;
    for(int i = 1; i < argc; ++i)
    {
        if(!strcmp(argv[i], "--size") && i + 1 < argc) size = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--games") && i + 1 < argc) games = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--komi") && i + 1 < argc) komi_x2 = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--seed") && i + 1 < argc) seed = uint32_t(atoi(argv[++i]));
        else if(!strcmp(argv[i], "--a") && i + 1 < argc) a = parse_level(argv[++i]);
        else if(!strcmp(argv[i], "--b") && i + 1 < argc) b = parse_level(argv[++i]);
        else if(!strcmp(argv[i], "--playouts-a") && i + 1 < argc) playouts_a = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--playouts-b") && i + 1 < argc) playouts_b = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--verbose")) verbose = 1;
        else if(!strcmp(argv[i], "--uct") && i + 1 < argc) { ai::detail::Tuning t = ai::detail::tuning(); t.uct_c = atoi(argv[++i]); ai::detail::set_tuning(t); }
        else if(!strcmp(argv[i], "--rave") && i + 1 < argc) { ai::detail::Tuning t = ai::detail::tuning(); t.rave_bias = atoi(argv[++i]); ai::detail::set_tuning(t); }
        else if(!strcmp(argv[i], "--expand") && i + 1 < argc) { ai::detail::Tuning t = ai::detail::tuning(); t.expand_threshold = atoi(argv[++i]); ai::detail::set_tuning(t); }
        else if(!strcmp(argv[i], "--children") && i + 1 < argc) { ai::detail::Tuning t = ai::detail::tuning(); t.max_children = atoi(argv[++i]); ai::detail::set_tuning(t); }
        else if(!strcmp(argv[i], "--pb") && i + 1 < argc) { ai::detail::Tuning t = ai::detail::tuning(); t.progressive_bias = atoi(argv[++i]); ai::detail::set_tuning(t); }
        else if(!strcmp(argv[i], "--patmin") && i + 1 < argc) { ai::detail::Tuning t = ai::detail::tuning(); t.playout_pattern_min = atoi(argv[++i]); ai::detail::set_tuning(t); }
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
    }

    ai::Limits lim_a = ai::default_limits(a), lim_b = ai::default_limits(b);
    if(playouts_a) lim_a.max_playouts = playouts_a;
    if(playouts_b) lim_b.max_playouts = playouts_b;
    lim_a.allow_resign = false;
    lim_b.allow_resign = false;

    int wins_a = 0, total_moves = 0, margin_sum = 0;
    for(int i = 0; i < games; ++i)
    {
        const bool a_is_black = (i % 2) == 0;
        const GameResult r = a_is_black ? play(a, b, size, komi_x2, seed + uint32_t(i), &lim_a, &lim_b)
                                        : play(b, a, size, komi_x2, seed + uint32_t(i), &lim_b, &lim_a);
        const int margin_for_a = a_is_black ? r.margin_x2 : -r.margin_x2;
        if(margin_for_a > 0) ++wins_a;
        total_moves += r.moves;
        margin_sum += margin_for_a;
        if(verbose)
            printf("game %2d: %s as %s, margin %+.1f, %d moves\n", i + 1, LEVEL_NAMES[int(a)],
                   a_is_black ? "black" : "white", margin_for_a / 2.0, r.moves);
    }
    printf("%s vs %s on %dx%d, komi %.1f, %d games: %s wins %d (%.1f%%), average margin %+.1f, average length %.0f moves\n",
           LEVEL_NAMES[int(a)], LEVEL_NAMES[int(b)], size, size, komi_x2 / 2.0, games,
           LEVEL_NAMES[int(a)], wins_a, 100.0 * wins_a / games, margin_sum / (2.0 * games),
           double(total_moves) / games);
    return wins_a * 100 >= games * 65 ? 0 : 1;
}
