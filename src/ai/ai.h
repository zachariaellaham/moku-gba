// MOKU AI - five levels sharing the move generator, resumable (time-sliced) thinking.
//
// Usage (both on GBA and host):
//   ai::Thinker th;                      // keep one per game (resign tracking needs history)
//   th.start(game, ai::Level::HARD, seed);
//   while (!th.step(4)) { /* yield to the frame loop */ }
//   go::Point mv = th.result();          // board point or go::PASS; th.wants_resign() for HARD/MASTER
//
// step(work) performs at most `work` units then returns whether the move is ready:
//   VERY_EASY / EASY : 1 unit finishes the move.
//   NORMAL           : 1 unit = full evaluation of one candidate (Bouzy delta, patterns, ladders).
//   HARD / MASTER    : 1 unit = one MCTS playout (tree descent + light playout + backup).
// The caller decides how many units fit in a frame (the play scene measures ~12 ms with bn::timer).
// finish() forces the best move so far (used when the frame budget is exhausted).
#pragma once
#include "go/game.h"

namespace ai {

enum class Level : uint8_t { VERY_EASY = 0, EASY = 1, NORMAL = 2, HARD = 3, MASTER = 4 };
constexpr int LEVEL_COUNT = 5;

struct Limits {
    int max_playouts = 0;      // MCTS: stop after this many playouts (0 = level default)
    int max_nodes = 0;         // MCTS: node pool cap (0 = level default; HARD 4096, MASTER 10240)
    int frame_budget = 0;      // informational: frames the scene allows (HARD 120, MASTER 300)
    bool allow_resign = true;  // HARD/MASTER may report wants_resign()
};

Limits default_limits(Level level);

struct Stats {
    uint8_t phase = 0;         // 1 prepare, 2 evaluate, 3 deep search, 4 playouts (debug overlay)
    int playouts = 0;          // MCTS playouts of the last think
    int nodes = 0;             // tree nodes used
    int winrate_permille = 500;// root win rate for the side to move (MCTS) / heuristic estimate
    int candidates = 0;        // candidates evaluated (NORMAL)
    int ladders_read = 0;
};

// Runs `count` complete playouts from `board` and returns their summed score, so nothing can be
// optimised away. The ROM times this to report playouts per second per board size: the search
// itself never runs playouts back to back like this, and the two numbers answer different
// questions - how fast a playout is, and how many of them the tree is given.
// One move of the playout policy: legal, never a self-atari, never an own eye, and PASS when only
// those are left. It is what a simulation plays, exposed so a test can drive a whole game quickly.
go::Point quick_move(const go::Board& board, go::Color to_move, uint32_t& seed);

int bench_playouts(const go::Board& board, go::Color to_move, int komi_x2, uint32_t seed, int count);

class Thinker {
public:
    Thinker();
    void start(const go::Game& game, Level level, uint32_t seed, const Limits* limits = nullptr);
    bool step(int work);                 // true when done
    void finish();                       // stop now; result() = best move so far
    bool done() const { return done_; }
    go::Point result() const { return result_; }      // valid when done()
    bool wants_resign() const { return resign_; }     // HARD/MASTER: win rate < 10% for 3 consecutive moves
    const Stats& stats() const { return stats_; }
    Level level() const { return level_; }
    void reset_history();                // forget the resign streak (new game)

    // Best-first ordering of candidate moves for the current root (after start()). Used for hints,
    // the debug overlay and mission tutoring. Returns count written (<= max_out).
    int ranked_moves(go::Point* out, int max_out) const;

private:
    Level level_ = Level::VERY_EASY;
    bool done_ = true, resign_ = false;
    go::Point result_ = go::PASS;
    Stats stats_;
    Limits limits_;
    uint32_t seed_ = 1;
    int low_winrate_streak_ = 0;
    // Level-specific state lives in ai::detail (mcts.h / levels.cpp) and is referenced via an
    // opaque handle so that this header stays small.
    void* state_ = nullptr;
};

// Convenience for tests and missions: think to completion.
go::Point pick_move(const go::Game& game, Level level, uint32_t seed, const Limits* limits = nullptr);

// Move generation shared by all levels: legal moves for the side to move, excluding eye-filling
// (is_eye_like) and, when `no_self_atari` is set, moves that put the played group in atari.
// PASS is never included. Returns the count.
int generate_moves(const go::Game& game, go::Point* out, int max_out, bool no_self_atari);

// Mission tutoring helper: does `move` capture a stone of the group at `target` right now?
bool move_captures_group(const go::Board& b, go::Point move, go::Color c, go::Point target);

}  // namespace ai
