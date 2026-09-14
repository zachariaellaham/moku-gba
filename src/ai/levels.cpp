// Move generation, board features and the three search-free levels (VERY EASY, EASY, NORMAL).
#include "ai/internal.h"

namespace ai {

using namespace detail;
using go::BLACK;
using go::Board;
using go::Color;
using go::EMPTY;
using go::Game;
using go::PASS;
using go::Point;
using go::WHITE;

namespace detail {

namespace {
Tuning g_tuning;
go::Board g_scratch GO_EWRAM_BSS;
go::Board g_scratch2 GO_EWRAM_BSS;
int8_t g_influence[go::MAX_POINTS] GO_EWRAM_BSS;
}

go::Board& shared_scratch() { return g_scratch; }
go::Board& shared_scratch2() { return g_scratch2; }
int8_t* shared_influence() { return g_influence; }

const Tuning& tuning() { return g_tuning; }
void set_tuning(const Tuning& t) { g_tuning = t; }

// --------------------------------------------------------------------------------------------
// board features
// --------------------------------------------------------------------------------------------

// Unique roots of the groups of colour `c` touching p. Returns the count (<= 4).
static int adjacent_groups(const Board& b, Point p, Color c, Point* roots)
{
    const int s = b.stride();
    const Point n[4] = { Point(p - s), Point(p - 1), Point(p + 1), Point(p + s) };
    int count = 0;
    for(int i = 0; i < 4; ++i)
    {
        if(b.at(n[i]) != c) continue;
        const Point root = b.group_root(n[i]);
        bool seen = false;
        for(int j = 0; j < count; ++j) if(roots[j] == root) { seen = true; break; }
        if(!seen) roots[count++] = root;
    }
    return count;
}

int adjacent_atari_groups(const Board& b, Point p, Color c, Point* roots, int max_roots)
{
    Point all[4];
    const int n = adjacent_groups(b, p, c, all);
    int count = 0;
    for(int i = 0; i < n && count < max_roots; ++i) if(b.in_atari(all[i])) roots[count++] = all[i];
    return count;
}

// Stones captured by playing p with colour c (0 if the move captures nothing).
static int captured_stones(const Board& b, Point p, Color c)
{
    Point roots[4];
    const int n = adjacent_groups(b, p, opponent(c), roots);
    int total = 0;
    for(int i = 0; i < n; ++i) if(b.in_atari(roots[i]) && b.atari_point(roots[i]) == p) total += b.group_size(roots[i]);
    return total;
}

// Own stones sitting in atari that the move at p would extend (an upper bound on what it rescues).
static int rescued_stones(const Board& b, Point p, Color c)
{
    Point roots[4];
    const int n = adjacent_groups(b, p, c, roots);
    int total = 0;
    for(int i = 0; i < n; ++i) if(b.in_atari(roots[i]) && b.atari_point(roots[i]) == p) total += b.group_size(roots[i]);
    return total;
}

// Opponent stones put in atari by playing p (counted after the move).
static int atari_stones_after(const Board& after, Point p, Color c)
{
    Point roots[4];
    const int n = adjacent_groups(after, p, opponent(c), roots);
    int total = 0;
    for(int i = 0; i < n; ++i) if(after.in_atari(roots[i])) total += after.group_size(roots[i]);
    return total;
}

bool is_bad_self_atari(const Board& b, Point p, Color c)
{
    if(!b.is_self_atari(p, c)) return false;
    if(captured_stones(b, p, c) > 0) return false;       // a capture is never a plain blunder
    Point roots[4];
    const int n = adjacent_groups(b, p, c, roots);
    int size = 1;
    for(int i = 0; i < n; ++i) size += b.group_size(roots[i]);
    return size > 1;                                      // one-stone sacrifices stay legal moves
}

bool fills_own_eye(const Board& b, Point p, Color c) { return b.is_eye_like(p, c); }

int quick_prior(const Board& b, Point p, Color c, Point last_move)
{
    const Tuning& t = tuning();
    int v = 120 + 3 * int(pattern_weight(b, p, c));

    const int s = b.stride();
    const Point nb[4] = { Point(p - s), Point(p - 1), Point(p + 1), Point(p + s) };
    int own = 0, enemy = 0, enemy_atari = 0;

    for(int i = 0; i < 4; ++i)
    {
        const Color v2 = b.at(nb[i]);
        if(v2 == c) ++own;
        else if(v2 == opponent(c))
        {
            ++enemy;
            if(b.in_atari(nb[i])) ++enemy_atari;     // pseudo-liberty test: no group walk
        }
    }

    if(enemy_atari) v += 300;
    if(own) v += 40;
    if(enemy) v += 60;

    const bool early = b.empty_count() * 10 > b.size() * b.size() * 6;
    if(early)
    {
        const int line = line_of(b, p);
        v += t.line_bonus[line < 4 ? line : 3];
    }

    if(last_move >= 0)
    {
        const int d = chebyshev(b, p, last_move);
        if(d <= 1) v += 160;
        else if(d <= 2) v += 90;
        else if(d <= 4) v += 25;
    }

    if(v < 1) v = 1;
    if(v > 1000) v = 1000;
    return v;
}

int heuristic_prior(const Board& b, Point p, Color c, Point last_move)
{
    const Tuning& t = tuning();
    int v = 120;
    const int caps = captured_stones(b, p, c);
    if(caps > 0) v += 260 + 25 * (caps > 8 ? 8 : caps);
    const int rescue = rescued_stones(b, p, c);
    if(rescue > 0) v += 190 + 20 * (rescue > 8 ? 8 : rescue);
    v += 3 * int(pattern_weight(b, p, c));
    if(is_bad_self_atari(b, p, c)) v -= 400;
    // opponent groups left with two liberties next to p: playing here threatens them
    {
        Point roots[4];
        const int n = adjacent_groups(b, p, opponent(c), roots);
        for(int i = 0; i < n; ++i) if(b.pseudo_liberties(roots[i]) <= 8 && b.liberties(roots[i], nullptr, 0, 3) == 2) v += 110;
    }
    const bool early = b.empty_count() * 10 > b.size() * b.size() * 6;
    if(early)
    {
        const int line = line_of(b, p);
        v += t.line_bonus[line < 4 ? line : 3];
    }
    if(last_move >= 0)
    {
        const int d = chebyshev(b, p, last_move);
        if(d <= 1) v += 160;
        else if(d <= 2) v += 90;
        else if(d <= 4) v += 25;
    }
    if(v < 1) v = 1;
    if(v > 1000) v = 1000;
    return v;
}

void influence_map(const Board& b, int8_t* out)
{
    if(b.size() >= 19)      go::bouzy_influence(b, out, 2, 6);
    else if(b.size() >= 13) go::bouzy_influence(b, out, 4, 13);
    else                    go::bouzy_influence(b, out, 5, 21);
}

int refined_prior(const Board& b, Point p, Color c)
{
    int v = heuristic_prior(b, p, c, b.last_move());
    const int caps = captured_stones(b, p, c);
    const int rescue = rescued_stones(b, p, c);
    Point opp_roots[4];
    const int opp_atari_now = adjacent_atari_groups(b, p, opponent(c), opp_roots, 4);
    Point own_roots[4];
    const int own_low = adjacent_groups(b, p, c, own_roots);
    bool interesting = caps > 0 || rescue > 0 || opp_atari_now > 0;
    if(!interesting)
    {
        for(int i = 0; i < own_low; ++i)
            if(b.liberties(own_roots[i], nullptr, 0, 3) == 2) { interesting = true; break; }
    }
    if(!interesting) return v;

    Board& s = shared_scratch();
    s.copy_from(b);
    s.play(p, c);
    const int lib_after = s.liberties(p, nullptr, 0, 4);

    // an escape that stays in atari, or that the opponent can ladder, is worthless
    if(rescue > 0)
    {
        if(lib_after <= 1) v -= 320;
        else if(lib_after == 2 && go::read_ladder(s, p, /*attacker_to_move=*/true) == go::LadderResult::CAPTURED) v -= 240;
        else v += 60;
    }
    // an atari that works as a ladder is worth as much as the capture itself
    Point roots[4];
    const int n = adjacent_atari_groups(s, p, opponent(c), roots, 4);
    for(int i = 0; i < n; ++i)
    {
        const int size = s.group_size(roots[i]);
        if(go::read_ladder(s, roots[i], /*attacker_to_move=*/false) == go::LadderResult::CAPTURED) v += 150 + 20 * (size > 6 ? 6 : size);
        else v += 40;
    }
    if(v < 1) v = 1;
    if(v > 1000) v = 1000;
    return v;
}

// Sum of the signs of a Bouzy influence map: black territory minus white territory, in points.
static int influence_total(const Board& b, const int8_t* inf)
{
    int total = 0;
    for(int y = 0; y < b.size(); ++y)
        for(int x = 0; x < b.size(); ++x)
        {
            const int v = inf[b.point(x, y)];
            total += (v > 0) - (v < 0);
        }
    return total;
}

int position_value(const Board& b, Color c, int komi_x2, int8_t* influence_buf, bool with_influence)
{
    const int prisoners = b.captures(BLACK) - b.captures(WHITE);
    const int stones = b.stones(BLACK) - b.stones(WHITE);
    int value = 90 * prisoners + 30 * stones - 50 * komi_x2 / 2;

    if(with_influence)
    {
        influence_map(b, influence_buf);
        value += 100 * influence_total(b, influence_buf);
    }

    return (c == BLACK) ? value : -value;
}

namespace {

// One board per ply, so a recursive search never copies more than it must.
go::Board g_search_boards[MAX_SEARCH_DEPTH] GO_EWRAM_BSS;

int negamax(const Board& node, Color c, int komi_x2, int depth, const SearchWidths& widths, int ply,
            int alpha, int beta, int8_t* influence_buf, Point* best_out)
{
    if(depth <= 0) return 0;

    // candidates: the best few by the cheap prior
    const int width = widths.width[ply < MAX_SEARCH_DEPTH ? ply : MAX_SEARCH_DEPTH - 1];
    Point moves[16];
    int priors[16];
    int count = 0;
    const int keep = width < 16 ? width : 16;

    for(int i = 0, n = node.empty_count(); i < n; ++i)
    {
        const Point p = node.empty_at(i);
        if(node.is_eye_like(p, c)) continue;
        if(!node.is_legal(p, c)) continue;
        if(is_bad_self_atari(node, p, c)) continue;
        const int prior = quick_prior(node, p, c, node.last_move());

        if(count < keep)
        {
            moves[count] = p;
            priors[count] = prior;
            ++count;
        }
        else
        {
            int worst = 0;
            for(int k = 1; k < keep; ++k) if(priors[k] < priors[worst]) worst = k;
            if(prior > priors[worst]) { moves[worst] = p; priors[worst] = prior; }
        }
    }

    if(count == 0)
    {
        if(best_out) *best_out = go::PASS;
        return 0;
    }

    // best prior first: alpha-beta cuts much sooner
    for(int i = 0; i < count; ++i)
    {
        int best = i;
        for(int j = i + 1; j < count; ++j) if(priors[j] > priors[best]) best = j;
        if(best != i)
        {
            const Point mp = moves[i]; moves[i] = moves[best]; moves[best] = mp;
            const int pp = priors[i]; priors[i] = priors[best]; priors[best] = pp;
        }
    }

    // Territory decides the direction of play, so the map is worth its cost for our move and the
    // opponent's answer. Below that the lines are tactical and the map is only a tax.
    const bool with_influence = ply <= 1;
    int influence_before = 0;

    if(with_influence)
    {
        influence_map(node, influence_buf);
        influence_before = influence_total(node, influence_buf);
        if(c != BLACK) influence_before = -influence_before;
    }

    Board& child = g_search_boards[ply < MAX_SEARCH_DEPTH ? ply : MAX_SEARCH_DEPTH - 1];
    int best_value = -(1 << 28);
    Point best_move = moves[0];

    for(int i = 0; i < count; ++i)
    {
        const MoveEval e = evaluate_move_board(node, c, child, moves[i],
                                               (c == BLACK) ? influence_before : -influence_before,
                                               influence_buf, with_influence);
        // `child` now holds the position after our move (evaluate_move_board left it there).
        // The value of a move is what it is worth, less what the opponent takes back: measured
        // over 50 games this beats a textbook negamax with a positional leaf, because the move
        // evaluator knows about shape and a stone count does not.
        int value = e.score;

        if(depth > 1)
        {
            value -= negamax(child, opponent(c), komi_x2, depth - 1, widths, ply + 1,
                             -beta, -alpha, influence_buf, nullptr);
        }

        if(value > best_value)
        {
            best_value = value;
            best_move = moves[i];
        }

        if(value > alpha) alpha = value;
        if(alpha >= beta) break;                 // the opponent will not allow this line
    }

    if(best_out) *best_out = best_move;
    return best_value;
}

}  // namespace

int move_value(const Board& root, Color c, int komi_x2, Point m, int depth, const SearchWidths& widths,
               int influence_before, int8_t* influence_buf)
{
    Board& child = g_search_boards[0];
    const MoveEval e = evaluate_move_board(root, c, child, m, influence_before, influence_buf);
    int value = e.score;

    if(depth > 1)
    {
        value -= negamax(child, opponent(c), komi_x2, depth - 1, widths, 1,
                         -(1 << 28), (1 << 28), influence_buf, nullptr);
    }

    return value;
}

void deep_begin(DeepSearch& d, const Board& root, Color c, Point m, int influence_before,
                int8_t* influence_buf, bool deep_tactics)
{
    const Color them = opponent(c);
    d.move = m;
    d.reply_count = 0;
    d.reply_index = 0;
    d.best_reply_value = 0;
    d.replies_listed = false;
    d.mine = evaluate_move_board(root, c, shared_scratch2(), m, influence_before, influence_buf, true,
                                 deep_tactics);

    // evaluate_move_board left the position in shared_scratch2() and the influence map of that
    // very position in influence_buf, so the total costs nothing more than a sum.
    Board& after = shared_scratch2();
    d.influence_after = influence_total(after, influence_buf);
    if(them != BLACK) d.influence_after = -d.influence_after;
}

void deep_list_replies(DeepSearch& d, Color c, const SearchWidths& widths)
{
    const Color them = opponent(c);
    Board& after = shared_scratch2();
    const Point m = d.move;
    d.replies_listed = true;
    const int keep = widths.width[1] < REPLY_WIDTH ? widths.width[1] : REPLY_WIDTH;
    int priors[REPLY_WIDTH];

    // Answers are local. Walking all 361 points of a 19x19 board and asking the prior about each
    // one costs two hundred milliseconds here; the answer is almost always within a few lines of
    // the move being answered, and the radius shrinks the scan to about eighty points.
    const int radius = after.size() > 13 ? 4 : after.size();

    for(int i = 0, n = after.empty_count(); i < n; ++i)
    {
        const Point q = after.empty_at(i);
        if(chebyshev(after, q, m) > radius) continue;
        if(after.is_eye_like(q, them)) continue;
        if(!after.is_legal(q, them)) continue;
        if(is_bad_self_atari(after, q, them)) continue;
        const int prior = quick_prior(after, q, them, after.last_move());

        if(d.reply_count < keep)
        {
            d.replies[d.reply_count] = q;
            priors[d.reply_count] = prior;
            ++d.reply_count;
        }
        else
        {
            int worst = 0;
            for(int k = 1; k < keep; ++k) if(priors[k] < priors[worst]) worst = k;
            if(prior > priors[worst]) { d.replies[worst] = q; priors[worst] = prior; }
        }
    }
}

bool deep_step(DeepSearch& d, Color c, int komi_x2, int depth, const SearchWidths& widths,
               int8_t* influence_buf, bool deep_tactics)
{
    if(d.reply_index >= d.reply_count)
    {
        return true;
    }

    const Color them = opponent(c);
    Board& after = shared_scratch2();
    Board& after_reply = g_search_boards[1];
    const Point q = d.replies[d.reply_index];
    const MoveEval theirs = evaluate_move_board(after, them, after_reply, q,
                                                (them == BLACK) ? d.influence_after : -d.influence_after,
                                                influence_buf, true, deep_tactics);
    // Same formulation as negamax: what the opponent gains, less what we get back.
    int value = theirs.score;

    if(depth > 2)
    {
        value -= negamax(after_reply, c, komi_x2, depth - 2, widths, 2,
                         -(1 << 28), (1 << 28), influence_buf, nullptr);
    }

    if(d.reply_index == 0 || value > d.best_reply_value)
    {
        d.best_reply_value = value;
    }

    ++d.reply_index;
    return d.reply_index >= d.reply_count;
}

int deep_value(const DeepSearch& d)
{
    return d.mine.score - (d.reply_count > 0 ? d.best_reply_value : 0);
}

int move_search(const Board& root, Color c, int komi_x2, int depth, const SearchWidths& widths,
                int8_t* influence_buf, Point* best_out)
{
    if(depth > MAX_SEARCH_DEPTH) depth = MAX_SEARCH_DEPTH;
    return negamax(root, c, komi_x2, depth, widths, 0, -(1 << 28), (1 << 28), influence_buf, best_out);
}

MoveEval evaluate_move(const Game& g, Board& scratch, Point p, int influence_before, int8_t* influence_buf)
{
    return evaluate_move_board(g.board(), g.to_move(), scratch, p, influence_before, influence_buf);
}

int ladder_audit(const Board& b, Color c)
{
    const Color them = opponent(c);
    Point seen[16];
    int seen_count = 0;
    int score = 0;

    for(int y = 0; y < b.size(); ++y)
    {
        for(int x = 0; x < b.size(); ++x)
        {
            const Point p = b.point(x, y);
            const Color v = b.at(p);
            if(v != c && v != them) continue;

            const Point root = b.group_root(p);
            bool dup = false;
            for(int i = 0; i < seen_count; ++i) if(seen[i] == root) { dup = true; break; }
            if(dup) continue;
            if(seen_count < 16) seen[seen_count++] = root; else continue;

            const int libs = b.liberties(p, nullptr, 0, 3);
            if(libs > 2) continue;

            // whoever is not the owner is the attacker; the owner moves first here
            const bool ours = v == c;
            if(go::read_ladder(b, p, /*attacker_to_move=*/!ours) != go::LadderResult::CAPTURED) continue;

            const int size = b.group_size(p);
            score += (ours ? -120 : 110) * (size > 6 ? 6 : size);
        }
    }

    return score;
}

MoveEval evaluate_move_board(const Board& root, Color c, Board& scratch, Point p, int influence_before,
                             int8_t* influence_buf, bool with_influence, bool with_deep_tactics)
{
    const Tuning& t = tuning();
    MoveEval e;
    e.move = p;
    e.pattern = pattern_weight(root, p, c);

    const int rescue_before = rescued_stones(root, p, c);
    scratch.copy_from(root);
    e.captures = scratch.play(p, c);

    const int lib_after = scratch.liberties(p, nullptr, 0, 3);
    e.self_atari = lib_after <= 1;
    e.atari_stones = atari_stones_after(scratch, p, c);

    int score = 0;
    score += t.capture_value * e.captures;
    if(rescue_before > 0 && lib_after >= 2) score += t.save_value * rescue_before;
    if(e.atari_stones > 0)
    {
        score += t.atari_value * e.atari_stones;
        Point roots[4];
        const int n = adjacent_atari_groups(scratch, p, opponent(c), roots, 4);
        for(int i = 0; i < n; ++i)
        {
            if(go::read_ladder(scratch, roots[i], /*attacker_to_move=*/false) == go::LadderResult::CAPTURED)
            {
                e.ladder = true;
                score += t.atari_ladder_bonus * scratch.group_size(roots[i]);
            }
        }
    }
    if(e.self_atari)
    {
        const int size = scratch.group_size(p);
        if(e.captures < size) score -= t.self_atari_penalty * size;
    }

    e.tactical = score;              // everything above is material; what follows is shape

    if(with_influence)
    {
        influence_map(scratch, influence_buf);
        const int after = influence_total(scratch, influence_buf);
        int delta = (c == BLACK) ? (after - influence_before) : (influence_before - after);
        if(delta > t.influence_clamp) delta = t.influence_clamp;
        if(delta < -t.influence_clamp) delta = -t.influence_clamp;
        // weight the sketch up as the board fills: on an empty board it is mostly noise
        const int filled = root.size() * root.size() - root.empty_count();
        const int weight = t.influence_value * (32 + (96 * filled) / (root.size() * root.size())) / 64;
        score += weight * delta;
    }

    score += t.pattern_value * int(e.pattern);
    const bool early = root.empty_count() * 10 > root.size() * root.size() * 6;
    if(early)
    {
        const int line = line_of(root, p);
        score += t.line_bonus[line < 4 ? line : 3];
    }
    const Point last = root.last_move();
    if(last >= 0 && chebyshev(root, p, last) <= 2) score += t.near_last_bonus;
    {
        const int s = root.stride();
        const Point nb[4] = { Point(p - s), Point(p - 1), Point(p + 1), Point(p + s) };
        for(int i = 0; i < 4; ++i)
        {
            const Color v = root.at(nb[i]);
            if(v == BLACK || v == WHITE) { score += t.contact_bonus; break; }
        }
    }
    if(with_deep_tactics) score += ladder_audit(scratch, c);

    e.score = score;
    return e;
}

}  // namespace detail

// --------------------------------------------------------------------------------------------
// public helpers
// --------------------------------------------------------------------------------------------

int generate_moves(const Game& g, Point* out, int max_out, bool no_self_atari)
{
    const Board& b = g.board();
    const Color c = g.to_move();
    int count = 0;
    for(int i = 0, n = b.empty_count(); i < n && count < max_out; ++i)
    {
        const Point p = b.empty_at(i);
        if(b.is_eye_like(p, c)) continue;
        if(no_self_atari ? b.is_self_atari(p, c) : is_bad_self_atari(b, p, c)) continue;
        if(!g.is_legal(p)) continue;
        out[count++] = p;
    }
    return count;
}

bool move_captures_group(const Board& b, Point move, Color c, Point target)
{
    if(b.at(move) != EMPTY || b.at(target) == EMPTY || b.at(target) == go::BORDER) return false;
    if(b.at(target) == c) return false;
    if(!b.in_atari(target)) return false;
    return b.atari_point(target) == move;
}

Limits default_limits(Level level)
{
    Limits l;
    switch(level)
    {
    case Level::VERY_EASY: l.max_playouts = 0;    l.max_nodes = 0;    l.frame_budget = 20;  l.allow_resign = false; break;
    case Level::EASY:      l.max_playouts = 0;    l.max_nodes = 0;    l.frame_budget = 24;  l.allow_resign = false; break;
    case Level::NORMAL:    l.max_playouts = 0;    l.max_nodes = 0;    l.frame_budget = 60;  l.allow_resign = false; break;
    case Level::HARD:      l.max_playouts = 1600; l.max_nodes = 1024; l.frame_budget = 120; l.allow_resign = true;  break;
    case Level::MASTER:    l.max_playouts = 4200; l.max_nodes = 1024; l.frame_budget = 300; l.allow_resign = true;  break;
    }
    return l;
}

namespace detail {

// --------------------------------------------------------------------------------------------
// VERY EASY: a random legal move that is never a self-atari and never fills an eye.
// --------------------------------------------------------------------------------------------
Point pick_very_easy(const Game& g, Rng& rng)
{
    const Board& b = g.board();
    const Color c = g.to_move();
    const int n = b.empty_count();
    if(n == 0) return PASS;
    const int start = int(rng.below(uint32_t(n)));
    for(int i = 0; i < n; ++i)
    {
        const Point p = b.empty_at((start + i) % n);
        if(b.is_eye_like(p, c)) continue;
        if(b.is_self_atari(p, c)) continue;
        if(!g.is_legal(p)) continue;
        return p;
    }
    return PASS;
}

// --------------------------------------------------------------------------------------------
// EASY: greedy one-ply priorities - capture, rescue, extend, play near stones.
// --------------------------------------------------------------------------------------------
Point pick_easy(const Game& g, Board& scratch, Rng& rng)
{
    const Board& b = g.board();
    const Color c = g.to_move();
    const int n = b.empty_count();
    Point best_capture = PASS, best_rescue = PASS, best_extend = PASS;
    int best_capture_size = 0, best_rescue_size = 0, best_extend_gain = 0;
    Point near_stones[go::MAX_SIZE * go::MAX_SIZE];
    int near_count = 0;
    Point any[go::MAX_SIZE * go::MAX_SIZE];
    int any_count = 0;

    for(int i = 0; i < n; ++i)
    {
        const Point p = b.empty_at(i);
        if(b.is_eye_like(p, c)) continue;

        const int caps = captured_stones(b, p, c);
        if(caps > best_capture_size && g.is_legal(p)) { best_capture_size = caps; best_capture = p; }
        if(caps > 0) continue;
        if(is_bad_self_atari(b, p, c)) continue;
        if(!g.is_legal(p)) continue;

        const int rescue = rescued_stones(b, p, c);
        if(rescue > 0)
        {
            scratch.copy_from(b);
            scratch.play(p, c);
            if(scratch.liberties(p, nullptr, 0, 3) >= 2 && rescue > best_rescue_size)
            {
                best_rescue_size = rescue;
                best_rescue = p;
            }
        }
        else
        {
            // extending a group that is down to two liberties
            Point roots[4];
            const int gn = adjacent_groups(b, p, c, roots);
            int gain = 0;
            for(int k = 0; k < gn; ++k)
            {
                const int libs = b.liberties(roots[k], nullptr, 0, 4);
                if(libs == 2) gain += b.group_size(roots[k]);
            }
            if(gain > 0 && !b.is_self_atari(p, c))
            {
                scratch.copy_from(b);
                scratch.play(p, c);
                if(scratch.liberties(p, nullptr, 0, 4) >= 3 && gain > best_extend_gain)
                {
                    best_extend_gain = gain;
                    best_extend = p;
                }
            }
        }

        if(b.is_self_atari(p, c)) continue;
        any[any_count++] = p;
        const int s = b.stride();
        bool near = false;
        for(int dy = -2; dy <= 2 && !near; ++dy)
            for(int dx = -2; dx <= 2; ++dx)
            {
                const go::Color v = b.at(Point(p + dy * s + dx));
                if(v == BLACK || v == WHITE) { near = true; break; }
            }
        if(near) near_stones[near_count++] = p;
    }

    if(best_capture != PASS) return best_capture;
    if(best_rescue != PASS) return best_rescue;
    if(best_extend != PASS) return best_extend;
    if(near_count > 0) return near_stones[rng.below(uint32_t(near_count))];
    if(any_count > 0) return any[rng.below(uint32_t(any_count))];
    return PASS;
}

}  // namespace detail
}  // namespace ai
