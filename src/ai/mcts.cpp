// UCT + RAVE search. Integer arithmetic only; win rates live in 1/1024 units.
#include "ai/mcts.h"
#include <cstring>

namespace ai {
namespace detail {

using go::BLACK;
using go::PASS;
using go::Point;

namespace {

inline uint32_t isqrt32(uint32_t x)
{
    if(x == 0) return 0;
    uint32_t r = x, last;
    // three Newton iterations from a power-of-two seed converge for everything we feed it
    uint32_t seed = 1u << ((32 - __builtin_clz(x) + 1) >> 1);
    r = seed;
    for(int i = 0; i < 6; ++i)
    {
        last = r;
        r = (r + x / r) >> 1;
        if(r == last) break;
    }
    while(r * r > x) --r;
    while((r + 1) * (r + 1) <= x) ++r;
    return r;
}

// 256 * log2(1 + i/16), i = 0..15
const uint8_t LOG2_FRAC[16] = { 0, 22, 44, 64, 84, 103, 121, 139, 156, 172, 188, 203, 218, 232, 246, 255 };

// ln(n) in 1/1024 units.
inline int ln_x1024(uint32_t n)
{
    if(n < 2) return 0;
    const int hi = 31 - __builtin_clz(n);
    const uint32_t m = (hi >= 4) ? (n >> (hi - 4)) : (n << (4 - hi));
    const int log2_x256 = (hi << 8) + LOG2_FRAC[m & 15];
    return (log2_x256 * 2839) >> 10;      // 2839/1024 = 4 * ln(2)
}

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Scored { Point move; int32_t prior; };

// Plain .bss, which devkitARM's linker script puts in IWRAM - unlike GO_EWRAM_BSS. Eleven
// kilobytes of the thirty-two, spent where it buys the most: every playout move reads and writes
// this board dozens of times.
go::Board g_play_board;

// insertion of one candidate into a descending-by-prior top-K list
void insert_top(Scored* list, int& count, int max_count, Point move, int prior)
{
    if(count < max_count)
    {
        int i = count++;
        while(i > 0 && list[i - 1].prior < prior) { list[i] = list[i - 1]; --i; }
        list[i].move = move;
        list[i].prior = prior;
        return;
    }
    if(prior <= list[count - 1].prior) return;
    int i = count - 1;
    while(i > 0 && list[i - 1].prior < prior) { list[i] = list[i - 1]; --i; }
    list[i].move = move;
    list[i].prior = prior;
}

}  // namespace

Mcts::Mcts() :
    play_board_(g_play_board)
{
}

void Mcts::start(const go::Game& game, uint32_t seed, int max_nodes, int max_root_children)
{
    game_ = &game;
    const Board& src = game.board();
    if(root_board_.size() != src.size()) root_board_.init(src.size());
    root_board_.copy_from(src);
    if(play_board_.size() != src.size()) play_board_.init(src.size());
    root_color_ = game.to_move();
    komi_x2_ = game.settings().komi_x2;
    max_nodes_ = clampi(max_nodes > 0 ? max_nodes : MCTS_MAX_NODES, 64, MCTS_MAX_NODES);
    max_root_children_ = clampi(max_root_children, 1, MCTS_MAX_CHILDREN);
    rng_ = Rng(seed ? seed : 1u);
    node_count_ = 1;
    playouts_ = 0;
    pool_full_ = false;
    path_len_ = 0;
    in_playout_ = false;
    Node& root = nodes_[0];
    root.first_child = 0;
    root.child_count = 0;
    root.flags = 0;
    root.move = PASS;
    root.prior = 0;
    root.visits = 0;
    root.wins = 0;
    root.rave_visits = 0;
    root.rave_wins = 0;
    expand(0, root_board_, root_color_, true);
}

bool Mcts::expand(int node_index, const Board& b, Color c, bool is_root)
{
    Node& n = nodes_[node_index];
    if(n.flags & 1) return n.child_count > 0;
    n.flags |= 1;

    const int tuned = tuning().max_children;
    const int max_children = is_root ? max_root_children_ : (tuned < MCTS_MAX_CHILDREN ? tuned : MCTS_MAX_CHILDREN);
    Scored list[MCTS_MAX_CHILDREN];
    int count = 0;
    for(int i = 0, ne = b.empty_count(); i < ne; ++i)
    {
        const Point p = b.empty_at(i);
        if(b.at(p) != go::EMPTY) continue;
        if(b.is_eye_like(p, c)) continue;
        if(!b.is_legal(p, c)) continue;
        if(is_bad_self_atari(b, p, c)) continue;
        if(is_root && game_ && !game_->is_legal(p)) continue;    // positional superko, root only
        insert_top(list, count, max_children, p, is_root ? refined_prior(b, p, c) : heuristic_prior(b, p, c, b.last_move()));
    }

    // Passing is only a candidate when it can actually be right: the opponent has just passed, or
    // the board is nearly full. Offering it from move one lets a lucky streak of playouts elect it,
    // which throws the game away - and gets *more* likely the longer the search runs.
    const bool allow_pass = count == 0 ||
        (is_root && (b.last_move() == go::PASS || b.empty_count() * 5 < b.size() * b.size()));
    int wanted = count + (allow_pass ? 1 : 0);
    if(wanted == 0) return false;
    if(node_count_ + wanted > max_nodes_)
    {
        wanted = max_nodes_ - node_count_;
        pool_full_ = true;
        if(wanted <= 0) { n.child_count = 0; return false; }
        if(allow_pass && wanted > 1) count = wanted - 1;
        else { count = wanted; }
    }

    n.first_child = uint16_t(node_count_);
    n.child_count = uint8_t(wanted);
    for(int i = 0; i < count; ++i)
    {
        Node& ch = nodes_[node_count_ + i];
        ch.first_child = 0;
        ch.child_count = 0;
        ch.flags = 0;
        ch.move = int16_t(list[i].move);
        ch.prior = uint16_t(clampi(list[i].prior, 1, 1000));
        ch.visits = 0;
        ch.wins = 0;
        const Tuning& t = tuning();
        const int seed_permille = t.prior_rave_floor + (t.prior_rave_span * ch.prior) / 1000;
        ch.rave_visits = uint16_t(t.prior_rave_visits);
        ch.rave_wins = uint16_t((t.prior_rave_visits * seed_permille) / 1000);
    }
    if(wanted > count)
    {
        Node& ch = nodes_[node_count_ + count];
        ch.first_child = 0;
        ch.child_count = 0;
        ch.flags = 0;
        ch.move = PASS;
        ch.prior = 1;
        ch.visits = 0;
        ch.wins = 0;
        ch.rave_visits = 0;
        ch.rave_wins = 0;
    }
    node_count_ += wanted;
    return true;
}

void Mcts::start_with_candidates(const go::Game& game, uint32_t seed, int max_nodes,
                                 const Point* moves, const int* priors, int count)
{
    game_ = &game;
    const Board& src = game.board();
    if(root_board_.size() != src.size()) root_board_.init(src.size());
    root_board_.copy_from(src);
    if(play_board_.size() != src.size()) play_board_.init(src.size());
    root_color_ = game.to_move();
    komi_x2_ = game.settings().komi_x2;
    max_nodes_ = clampi(max_nodes > 0 ? max_nodes : MCTS_MAX_NODES, 64, MCTS_MAX_NODES);
    max_root_children_ = MCTS_MAX_CHILDREN;
    rng_ = Rng(seed ? seed : 1u);
    playouts_ = 0;
    pool_full_ = false;
    path_len_ = 0;
    in_playout_ = false;

    Node& root = nodes_[0];
    root.first_child = 1;
    root.flags = 1;
    root.move = PASS;
    root.prior = 0;
    root.visits = 0;
    root.wins = 0;
    root.rave_visits = 0;
    root.rave_wins = 0;

    if(count > MCTS_MAX_CHILDREN) count = MCTS_MAX_CHILDREN;
    if(count > max_nodes_ - 1) count = max_nodes_ - 1;
    root.child_count = uint8_t(count > 0 ? count : 0);
    node_count_ = 1 + (count > 0 ? count : 0);

    const Tuning& t = tuning();

    for(int i = 0; i < count; ++i)
    {
        Node& ch = nodes_[1 + i];
        ch.first_child = 0;
        ch.child_count = 0;
        ch.flags = 0;
        ch.move = int16_t(moves[i]);
        ch.prior = uint16_t(clampi(priors[i], 1, 1000));
        ch.visits = 0;
        ch.wins = 0;
        const int seed_permille = t.prior_rave_floor + (t.prior_rave_span * ch.prior) / 1000;
        ch.rave_visits = uint16_t(t.prior_rave_visits);
        ch.rave_wins = uint16_t((t.prior_rave_visits * seed_permille) / 1000);
    }
}

int Mcts::confident_visits() const
{
    // Measured: with fewer than about forty visits on the best child, the search is noise and
    // overrules the evaluator for the worse (30 games on 9x9: 100% for the evaluator alone, 40%
    // once a forty-playout search was allowed to choose). Forty visits is where it starts paying.
    return 40;
}

int Mcts::select_child(int node_index, int parent_visits) const
{
    const Node& n = nodes_[node_index];
    const Tuning& t = tuning();
    const int log_term = ln_x1024(uint32_t(parent_visits < 1 ? 1 : parent_visits));
    int best = 0, best_value = -(1 << 30);
    for(int i = 0; i < n.child_count; ++i)
    {
        const Node& ch = nodes_[n.first_child + i];
        const int q = ch.visits ? ((int(ch.wins) << 10) / ch.visits) : 512;
        int value;
        if(ch.rave_visits)
        {
            const uint32_t rv = ch.rave_visits, v = ch.visits;
            const uint32_t denom = rv + v + (rv * v) / uint32_t(t.rave_bias);
            const int beta = int((rv << 10) / (denom ? denom : 1));
            const int qr = (int(ch.rave_wins) << 10) / ch.rave_visits;
            value = ((1024 - beta) * q + beta * qr) >> 10;
        }
        else
        {
            value = q;
        }
        const int ratio = log_term / (ch.visits + 1);
        value += (t.uct_c * int(isqrt32(uint32_t(ratio) << 10))) >> 10;
        value += (t.progressive_bias * int(ch.prior)) / (1000 * (int(ch.visits) + 1));
        if(value > best_value) { best_value = value; best = i; }
    }
    return n.first_child + best;
}

void Mcts::backup(int black_win)
{
    for(int i = 0; i < path_len_; ++i)
    {
        Node& n = nodes_[path_[i]];
        if(n.visits < 65500) ++n.visits;
        if(i > 0)
        {
            const Color played = Color(path_color_[i - 1]);
            n.wins = uint16_t(n.wins + ((played == BLACK) ? black_win : (1 - black_win)));
        }
        const Color pc = Color(path_color_[i]);
        const int after = path_entry_idx_[i];
        const int win = (pc == BLACK) ? black_win : (1 - black_win);
        for(int k = 0; k < n.child_count; ++k)
        {
            Node& ch = nodes_[n.first_child + k];
            if(ch.move < 0) continue;
            const uint16_t fi = first_idx_[ch.move * 2 + (pc - 1)];
            if(fi == 0xFFFF || int(fi) <= after) continue;
            if(ch.rave_visits < 65000) { ++ch.rave_visits; ch.rave_wins = uint16_t(ch.rave_wins + win); }
        }
    }
}

void Mcts::begin_simulation()
{
    const int points = root_board_.num_points();
    std::memset(first_idx_, 0xFF, sizeof(uint16_t) * size_t(points) * 2);
    play_board_.copy_from(root_board_);
    Color c = root_color_;
    sim_idx_ = 0;
    path_len_ = 0;
    path_[path_len_] = 0;
    path_entry_idx_[path_len_] = -1;
    path_color_[path_len_] = uint8_t(c);
    ++path_len_;

    int cur = 0;

    while((nodes_[cur].flags & 1) && nodes_[cur].child_count > 0 && path_len_ < MCTS_MAX_PATH - 1)
    {
        const int ci = select_child(cur, nodes_[cur].visits);
        const Point mv = Point(nodes_[ci].move);

        if(mv == PASS)
        {
            play_board_.play_pass(c);
        }
        else
        {
            uint16_t& slot = first_idx_[mv * 2 + (c - 1)];
            if(slot == 0xFFFF) slot = uint16_t(sim_idx_);
            play_board_.play(mv, c);
        }

        ++sim_idx_;
        c = opponent(c);
        path_[path_len_] = uint16_t(ci);
        path_entry_idx_[path_len_] = int16_t(sim_idx_ - 1);
        path_color_[path_len_] = uint8_t(c);
        ++path_len_;
        cur = ci;
    }

    if(!(nodes_[cur].flags & 1) && nodes_[cur].visits >= tuning().expand_threshold && !pool_full_)
    {
        if(expand(cur, play_board_, c, cur == 0) && path_len_ < MCTS_MAX_PATH - 1)
        {
            const int ci = select_child(cur, nodes_[cur].visits);
            const Point mv = Point(nodes_[ci].move);

            if(mv == PASS)
            {
                play_board_.play_pass(c);
            }
            else
            {
                uint16_t& slot = first_idx_[mv * 2 + (c - 1)];
                if(slot == 0xFFFF) slot = uint16_t(sim_idx_);
                play_board_.play(mv, c);
            }

            ++sim_idx_;
            c = opponent(c);
            path_[path_len_] = uint16_t(ci);
            path_entry_idx_[path_len_] = int16_t(sim_idx_ - 1);
            path_color_[path_len_] = uint8_t(c);
            ++path_len_;
        }
    }

    sim_color_ = c;
    record_.first_idx = first_idx_;
    record_.moves = sim_idx_;
    playout_begin(playout_, play_board_, c, sim_idx_);
    in_playout_ = true;
}

void Mcts::finish_simulation()
{
    const int margin = playout_score_x2(play_board_, komi_x2_);
    backup(margin > 0 ? 1 : 0);
    ++playouts_;
    in_playout_ = false;
}

void Mcts::run(int work)
{
    if(work < 1)
    {
        work = 1;
    }

    while(work > 0)
    {
        if(! in_playout_)
        {
            begin_simulation();
            work -= path_len_;            // the tree descent is board moves too
        }

        const int slice = work > 0 ? work : 1;

        if(playout_advance(playout_, play_board_, rng_, &record_, slice))
        {
            finish_simulation();
            work -= playout_.played;
            if(work <= 0) return;
        }
        else
        {
            return;                       // stopped mid-playout: the rest belongs to the next frame
        }
    }
}

void Mcts::run_playouts(int playouts)
{
    for(int i = 0; i < playouts; ++i)
    {
        if(! in_playout_)
        {
            begin_simulation();
        }

        while(! playout_advance(playout_, play_board_, rng_, &record_, 64)) {}

        finish_simulation();
    }
}

Point Mcts::best_move() const
{
    const Node& r = nodes_[0];
    // Two gates, both measured: the search needs a few hundred playouts before its shape means
    // anything, and the child it picks needs enough of them to stand out from its neighbours.
    const int confident = (playouts_ >= 800) ? confident_visits() : (1 << 20);
    int best = -1, best_visits = -1, best_wins = -1;
    int best_prior = -1, prior_choice = -1;

    for(int i = 0; i < r.child_count; ++i)
    {
        const Node& ch = nodes_[r.first_child + i];

        if(int(ch.prior) > best_prior)
        {
            best_prior = ch.prior;
            prior_choice = i;
        }

        if(ch.visits < confident) continue;

        // Visit count, not win rate: the most visited child is the one the search kept coming back
        // to, and it does not swing on a lucky run of playouts the way a win rate does.
        const int w = (int(ch.wins) * 1000) / ch.visits;

        if(ch.visits > best_visits || (ch.visits == best_visits && w > best_wins))
        {
            best_visits = ch.visits;
            best_wins = w;
            best = i;
        }
    }

    // Nothing was searched deeply enough to be trusted: keep what the evaluator chose.
    if(best < 0) best = prior_choice;
    if(best < 0) return PASS;
    return Point(nodes_[r.first_child + best].move);
}

int Mcts::winrate_permille() const
{
    const Node& r = nodes_[0];
    int best = -1, best_visits = -1;
    for(int i = 0; i < r.child_count; ++i)
    {
        const Node& ch = nodes_[r.first_child + i];
        if(ch.visits > best_visits) { best_visits = ch.visits; best = i; }
    }
    if(best < 0 || best_visits <= 0) return 500;
    const Node& ch = nodes_[r.first_child + best];
    return (int(ch.wins) * 1000) / ch.visits;
}

int Mcts::ranked(Point* out, int max_out) const
{
    const Node& r = nodes_[0];
    int order[MCTS_MAX_CHILDREN];
    int count = 0;
    for(int i = 0; i < r.child_count; ++i)
    {
        const int visits = nodes_[r.first_child + i].visits;
        int pos = count;
        while(pos > 0 && nodes_[r.first_child + order[pos - 1]].visits < visits) { order[pos] = order[pos - 1]; --pos; }
        order[pos] = i;
        ++count;
    }
    if(count > max_out) count = max_out;
    for(int i = 0; i < count; ++i) out[i] = Point(nodes_[r.first_child + order[i]].move);
    return count;
}

}  // namespace detail
go::Point quick_move(const go::Board& board, go::Color to_move, uint32_t& seed)
{
    go::Rng rng{ seed ? seed : 1u };
    const go::Point move = detail::playout_move(board, to_move, rng);
    seed = rng.s;
    return move;
}

int bench_playouts(const go::Board& board, go::Color to_move, int komi_x2, uint32_t seed, int count)
{
    go::Rng rng{ seed ? seed : 1u };
    int total = 0;

    for(int i = 0; i < count; ++i)
    {
        detail::g_play_board = board;
        total += detail::run_playout(detail::g_play_board, to_move, rng, komi_x2, nullptr);
    }

    return total;
}

}  // namespace ai
