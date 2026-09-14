// Engine benchmark: random games through go::Board (play + is_legal), plus the scoring, ladder and
// superko paths the AI leans on. Host tool only - it is not a test, it prints numbers.
//
//   build-host/tool_bench [games9] [games19]
//
// The GBA runs roughly 20-30x slower than this machine per clock, so divide by ~25 for a rough
// ARM7TDMI estimate (the ROM tests measure the real thing).
#include "go/board.h"
#include "go/game.h"
#include "go/ladder.h"
#include "go/score.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace go;
using Clock = std::chrono::steady_clock;

namespace {

double seconds_since(Clock::time_point t0)
{
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

struct Counters {
    long long games = 0, moves = 0, legal_checks = 0, passes = 0, captures = 0;
};

// One random game: at every move the whole empty list is scanned with is_legal (that is what a
// playout move generator does), then a legal non eye filling move is played.
void random_game(Board& b, Rng& rng, Counters& c)
{
    Color col = BLACK;
    int passes = 0, moves = 0;
    const int limit = b.size() * b.size() * 3;
    while (passes < 2 && moves < limit) {
        const int n = b.empty_count();
        Point choice = PASS;
        const int start = n > 0 ? int(rng.below(uint32_t(n))) : 0;
        for (int i = 0; i < n; ++i) {
            const Point p = b.empty_at((start + i) % n);
            ++c.legal_checks;
            if (b.is_eye_like(p, col) || !b.is_legal(p, col)) continue;
            choice = p;
            break;
        }
        if (choice == PASS) {
            b.play_pass(col);
            ++passes;
            ++c.passes;
        } else {
            c.captures += b.play(choice, col);
            passes = 0;
            ++moves;
            ++c.moves;
        }
        col = opponent(col);
    }
    ++c.games;
}

void bench_board(int size, int games)
{
    Board b;
    Rng rng(uint32_t(0x9E3779B9u ^ uint32_t(size)));
    Counters c;
    const Clock::time_point t0 = Clock::now();
    for (int i = 0; i < games; ++i) {
        b.init(size);
        random_game(b, rng, c);
    }
    const double dt = seconds_since(t0);
    std::printf("board %2dx%-2d : %6d games in %7.3f s -> %9.0f games/s, %10.0f point-plays/s, "
                "%10.0f is_legal/s  (avg %5.1f moves, %5.1f captures per game)\n",
                size, size, games, dt, games / dt, double(c.moves) / dt,
                double(c.legal_checks) / dt, double(c.moves) / double(c.games),
                double(c.captures) / double(c.games));
}

void bench_copy(int size, int iterations)
{
    Board a, b;
    Rng rng(12345);
    Counters c;
    a.init(size);
    random_game(a, rng, c);
    const Clock::time_point t0 = Clock::now();
    for (int i = 0; i < iterations; ++i) b.copy_from(a);
    const double dt = seconds_since(t0);
    std::printf("copy  %2dx%-2d : %9.0f copy_from/s\n", size, size, iterations / dt);
}

void bench_score(int size, int iterations)
{
    Board b;
    Rng rng(777);
    Counters c;
    b.init(size);
    random_game(b, rng, c);
    bool dead[MAX_POINTS];
    int8_t inf[MAX_POINTS];
    bool alive[MAX_POINTS];

    Clock::time_point t0 = Clock::now();
    for (int i = 0; i < iterations; ++i) score_area(b, nullptr, 15);
    double dt = seconds_since(t0);
    std::printf("score %2dx%-2d : %9.0f score_area/s", size, size, iterations / dt);

    t0 = Clock::now();
    for (int i = 0; i < iterations; ++i) bouzy_influence(b, inf);
    dt = seconds_since(t0);
    std::printf(", %8.0f bouzy/s", iterations / dt);

    t0 = Clock::now();
    for (int i = 0; i < iterations; ++i) benson_pass_alive(b, alive);
    dt = seconds_since(t0);
    std::printf(", %8.0f benson/s", iterations / dt);

    t0 = Clock::now();
    for (int i = 0; i < iterations / 10 + 1; ++i) propose_dead(b, dead);
    dt = seconds_since(t0);
    std::printf(", %8.0f propose_dead/s\n", (iterations / 10 + 1) / dt);
}

void bench_ladder(int iterations)
{
    Board b;
    b.init(19);
    // the standard running ladder: a two stone group in atari that dies in the far corner
    b.set_stone(b.point(4, 4), WHITE);
    b.set_stone(b.point(4, 5), WHITE);
    b.set_stone(b.point(4, 3), BLACK);
    b.set_stone(b.point(3, 4), BLACK);
    b.set_stone(b.point(3, 5), BLACK);
    b.set_stone(b.point(4, 6), BLACK);
    b.set_stone(b.point(5, 5), BLACK);
    const Clock::time_point t0 = Clock::now();
    int captured = 0;
    for (int i = 0; i < iterations; ++i)
        if (read_ladder(b, b.point(4, 4), false) == LadderResult::CAPTURED) ++captured;
    const double dt = seconds_since(t0);
    Point line[LADDER_MAX_PLIES];
    const int n = last_ladder_line(line, LADDER_MAX_PLIES);
    std::printf("ladder 19x19: %9.0f read_ladder/s (%d plies, %d/%d captured)\n",
                iterations / dt, n, captured, iterations);
}

void bench_game(int size, int games)
{
    // full Game play: legality with positional superko, move record, undo by replay
    Rng rng(4242);
    Game g;
    GameSettings s;
    s.size = uint8_t(size);
    long long moves = 0;
    const Clock::time_point t0 = Clock::now();
    for (int i = 0; i < games; ++i) {
        g.start(s);
        while (g.phase() == Phase::PLAYING && g.move_count() < size * size * 3) {
            const int n = g.board().empty_count();
            bool played = false;
            if (n > 0) {
                const int start = int(rng.below(uint32_t(n)));
                for (int k = 0; k < n; ++k) {
                    const Point p = g.board().empty_at((start + k) % n);
                    if (g.board().is_eye_like(p, g.to_move())) continue;
                    if (!g.is_legal(p)) continue;
                    if (g.play(p).ok) { played = true; ++moves; break; }
                }
            }
            if (!played) g.play(PASS);
        }
    }
    const double dt = seconds_since(t0);
    std::printf("game  %2dx%-2d : %9.0f games/s, %10.0f moves/s (superko checked)\n",
                size, size, games / dt, double(moves) / dt);

    // undo cost: rebuild by replay
    g.start(s);
    for (int i = 0; i < 60 && g.phase() == Phase::PLAYING; ++i) {
        const int n = g.board().empty_count();
        const Point p = g.board().empty_at(int(rng.below(uint32_t(n))));
        if (!g.play(p).ok) --i;
    }
    const int undo_iterations = 20000;
    const Clock::time_point t1 = Clock::now();
    int done = 0;
    for (int i = 0; i < undo_iterations; ++i) {
        const Move last = g.moves()[g.move_count() - 1];
        if (!g.undo()) break;
        g.play(last.p);
        ++done;
    }
    const double dt1 = seconds_since(t1);
    std::printf("undo  %2dx%-2d : %9.0f undo+replay/s at %d moves\n", size, size, done / dt1,
                g.move_count());
}

}  // namespace

int main(int argc, char** argv)
{
    const int games9 = argc > 1 ? std::atoi(argv[1]) : 20000;
    const int games19 = argc > 2 ? std::atoi(argv[2]) : 2000;

    std::printf("MOKU engine benchmark\n");
    std::printf("sizeof(go::Board)       = %zu bytes\n", sizeof(Board));
    std::printf("sizeof(go::Game)        = %zu bytes\n", sizeof(Game));
    std::printf("sizeof(go::ScoreResult) = %zu bytes\n", sizeof(ScoreResult));
    std::printf("\n");

    bench_board(9, games9);
    bench_board(13, games9 / 4);
    bench_board(19, games19);
    bench_copy(9, 2000000);
    bench_copy(19, 500000);
    bench_game(9, games9 / 10);
    bench_score(9, 200000);
    bench_score(19, 20000);
    bench_ladder(200000);
    return 0;
}
