// Monte-Carlo tree search with UCT + RAVE (AMAF), fixed node pool, no allocation.
// Used by HARD and MASTER. All arithmetic is integer: win rates are in 1/1024 units.
#pragma once
#include "ai/internal.h"

namespace ai {
namespace detail {

// 16 bytes. `wins` counts wins for the player who played `move` (the player to move at the parent).
struct Node {
    uint16_t first_child;
    uint8_t child_count;
    uint8_t flags;          // bit 0: children generated
    int16_t move;           // board point, or go::PASS
    uint16_t prior;         // heuristic prior, 0..1000
    uint16_t visits;
    uint16_t wins;
    uint16_t rave_visits;
    uint16_t rave_wins;
};

constexpr int MCTS_MAX_NODES = 1024;      // 16 KB of EWRAM (see docs/decisions/ai.md)
constexpr int MCTS_MAX_PATH = 192;
constexpr int MCTS_MAX_CHILDREN = 48;

class Mcts {
public:
    Mcts();

    // `game` must stay alive while the search runs. max_nodes/max_root_children clamp the defaults.
    void start(const go::Game& game, uint32_t seed, int max_nodes, int max_root_children);

    // Starts with the root children already chosen and scored by the NORMAL evaluator. On this
    // hardware a two second think buys about forty playouts, which is not enough for the search to
    // find good moves on its own; it is plenty to check the ones the evaluator likes.
    void start_with_candidates(const go::Game& game, uint32_t seed, int max_nodes,
                               const Point* moves, const int* priors, int count);
    // Advances the search by roughly `work` board moves. A whole 19x19 playout is a quarter of a
    // second on this machine, so the unit of work is a move, not a simulation: the caller can stop
    // in the middle of a playout and come back next frame.
    void run(int work);
    void run_playouts(int playouts);              // host helper: runs whole simulations
    int playouts() const { return playouts_; }
    int nodes_used() const { return node_count_; }
    // A child only overrules the evaluator once it has been visited enough to mean something.
    Point best_move() const;
    [[nodiscard]] int confident_visits() const;
    int winrate_permille() const;                 // for the player to move at the root
    int ranked(Point* out, int max_out) const;    // children by visit count, best first

private:
    int select_child(int node_index, int total_visits) const;
    bool expand(int node_index, const Board& b, Color c, bool is_root);
    void backup(int black_win);
    void begin_simulation();
    void finish_simulation();

    Board root_board_;
    // The board the playouts run on lives in IWRAM (see mcts.cpp): it is touched a few thousand
    // times per playout and EWRAM costs three to six cycles per access.
    Board& play_board_;
    const go::Game* game_ = nullptr;
    Color root_color_ = go::BLACK;
    int komi_x2_ = 15;
    int max_nodes_ = MCTS_MAX_NODES;
    int max_root_children_ = MCTS_MAX_CHILDREN;
    int node_count_ = 0;
    int playouts_ = 0;
    bool pool_full_ = false;
    Rng rng_;

    Node nodes_[MCTS_MAX_NODES];
    uint16_t first_idx_[go::MAX_POINTS * 2];
    uint16_t path_[MCTS_MAX_PATH];
    int16_t path_entry_idx_[MCTS_MAX_PATH];
    uint8_t path_color_[MCTS_MAX_PATH];
    int path_len_ = 0;

    // the simulation in progress, so run() can be interrupted between two playout moves
    PlayoutState playout_;
    PlayoutRecord record_;
    Color sim_color_ = go::BLACK;
    int sim_idx_ = 0;
    bool in_playout_ = false;
};

}  // namespace detail
}  // namespace ai
