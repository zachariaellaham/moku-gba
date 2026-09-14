// MOKU rules engine - scoring, influence, Benson pass-alive, dead-stone proposal (see score.h).
//
// Memory policy: every buffer here is either a small stack array (<= 1 KB) or part of the one
// file-scope scratch object (GO_EWRAM_BSS: EWRAM on the GBA). Nothing allocates and nothing
// recurses. The scratch is single threaded and never live across a call boundary, except inside
// propose_dead, which deliberately keeps the influence map in g.work while it walks the groups.
#include "go/score.h"
#include "go/game.h"   // for the Ruleset enumerators (score.h only forward-declares them)
#include <cstring>

namespace go {

namespace {

// Max chains of one colour, and max regions (components of "not that colour") on a 19x19 board is
// 181 (checkerboard); the caps below leave headroom and every loop guards against overflow.
constexpr int MAX_GROUPS = 192;
constexpr int GROUP_WORDS = (MAX_GROUPS + 31) / 32;
constexpr int BOARD_POINTS = MAX_SIZE * MAX_SIZE;

// Bouzy influence: a stone seeds +-BOUZY_STONE; 5 dilations add at most 4 per pass, so the internal
// range is [-148, 148] and the int8 output saturates at +-127 (only points within 5 steps of a stone
// can saturate, and those are never in doubt).
constexpr int BOUZY_STONE = 128;

// A group that is not provably alive but owns an enclosed space of at least this many points is
// assumed to be able to make two eyes there, so it is never proposed dead. Four points or fewer is
// a dead eye space against a player who moves first (square four, bent three, ...); five or more is
// shape dependent, and proposing a live group dead is the worse mistake of the two.
constexpr int EYE_SPACE_ALIVE = 5;

struct Scratch {
    uint16_t stamp[MAX_POINTS];
    uint16_t gen;
    Point stack[BOARD_POINTS];
    Point region_pts[BOARD_POINTS];
    int16_t work[MAX_POINTS];      // Bouzy accumulator
    int16_t prev[MAX_POINTS];      // Bouzy snapshot of the previous pass
    // Benson
    int16_t chain_of[MAX_POINTS];
    int16_t region_of[MAX_POINTS];
    uint32_t vital[MAX_GROUPS][GROUP_WORDS];    // region -> chains it is vital to
    uint32_t border[MAX_GROUPS][GROUP_WORDS];   // region -> chains bordering it
    uint32_t chain_alive[GROUP_WORDS];
    uint32_t region_ok[GROUP_WORDS];
    int16_t vital_count[MAX_GROUPS];
    // propose_dead
    bool alive[MAX_POINTS];
    Point group_pts[BOARD_POINTS];
    Point group_rep[MAX_GROUPS * 2];
    int16_t group_libs[MAX_GROUPS * 2];
    bool in_group[MAX_POINTS];
};

GO_EWRAM_BSS Scratch g;

inline uint16_t next_gen()
{
    if (++g.gen == 0) {
        std::memset(g.stamp, 0, sizeof(g.stamp));
        g.gen = 1;
    }
    return g.gen;
}

inline void bits_clear(uint32_t* w) { for (int i = 0; i < GROUP_WORDS; ++i) w[i] = 0; }
inline void bits_set(uint32_t* w, int i) { w[i >> 5] |= uint32_t(1) << (i & 31); }
inline bool bits_test(const uint32_t* w, int i) { return (w[i >> 5] >> (i & 31)) & 1u; }
inline void bits_clear_bit(uint32_t* w, int i) { w[i >> 5] &= ~(uint32_t(1) << (i & 31)); }
inline void bits_and(uint32_t* a, const uint32_t* b) { for (int i = 0; i < GROUP_WORDS; ++i) a[i] &= b[i]; }
inline void bits_or(uint32_t* a, const uint32_t* b) { for (int i = 0; i < GROUP_WORDS; ++i) a[i] |= b[i]; }
inline void bits_fill(uint32_t* w, int n)
{
    bits_clear(w);
    for (int i = 0; i < n; ++i) bits_set(w, i);
}
// Is every bit of `sub` also set in `sup`?
inline bool bits_subset(const uint32_t* sub, const uint32_t* sup)
{
    for (int i = 0; i < GROUP_WORDS; ++i)
        if (sub[i] & ~sup[i]) return false;
    return true;
}

// Colour accessors, so the flood fill works both on a Board and on a private colour array.
struct BoardCol {
    const Board* b;
    Color operator()(Point p) const { return b->at(p); }
};
struct ArrayCol {
    const uint8_t* a;
    Color operator()(Point p) const { return a[p]; }
};

// Floods the empty region containing p and reports who surrounds it.
template <class C>
uint8_t flood_region(const C& col, int stride, Point p, uint16_t* stamp, uint16_t gen,
                     Point* out, int* out_count)
{
    int top = 0, n = 0;
    bool touch_b = false, touch_w = false;
    g.stack[top++] = p;
    stamp[p] = gen;
    while (top > 0) {
        const Point q = g.stack[--top];
        if (out != nullptr) out[n] = q;
        ++n;
        const Point nb[4] = {Point(q - stride), Point(q - 1), Point(q + 1), Point(q + stride)};
        for (int i = 0; i < 4; ++i) {
            const Point e = nb[i];
            const Color c = col(e);
            if (c == EMPTY) {
                if (stamp[e] == gen) continue;
                stamp[e] = gen;
                g.stack[top++] = e;
            } else if (c == BLACK) {
                touch_b = true;
            } else if (c == WHITE) {
                touch_w = true;
            }
        }
    }
    if (out_count != nullptr) *out_count = n;
    if (touch_b == touch_w) return OWN_DAME;   // both or neither
    return touch_b ? OWN_BLACK : OWN_WHITE;
}

// Shared body of score_area / score_territory.
ScoreResult score_common(const Board& b, const bool* dead, int komi_x2, bool area)
{
    ScoreResult r;
    std::memset(r.owner, OWN_NONE, sizeof(r.owner));
    const int stride = b.stride();
    const int n_points = b.num_points();

    uint8_t col[MAX_POINTS];
    for (int p = 0; p < n_points; ++p) {
        Color c = b.at(Point(p));
        if ((c == BLACK || c == WHITE) && dead != nullptr && dead[p]) {
            if (c == BLACK) ++r.black_dead; else ++r.white_dead;
            c = EMPTY;
        }
        col[p] = c;
    }
    for (int p = 0; p < n_points; ++p) {
        if (col[p] == BLACK) { ++r.black_stones; r.owner[p] = OWN_BLACK; }
        else if (col[p] == WHITE) { ++r.white_stones; r.owner[p] = OWN_WHITE; }
    }

    const ArrayCol acc{col};
    const uint16_t gen = next_gen();
    for (int p = 0; p < n_points; ++p) {
        if (col[p] != EMPTY || g.stamp[p] == gen) continue;
        int count = 0;
        const uint8_t ow = flood_region(acc, stride, Point(p), g.stamp, gen, g.region_pts, &count);
        for (int i = 0; i < count; ++i) r.owner[g.region_pts[i]] = ow;
        if (ow == OWN_BLACK) r.black_territory = int16_t(r.black_territory + count);
        else if (ow == OWN_WHITE) r.white_territory = int16_t(r.white_territory + count);
    }

    // Prisoners held by each side: stones captured during play plus the opponent's dead stones.
    r.black_captures = int16_t(b.captures(BLACK) + r.white_dead);
    r.white_captures = int16_t(b.captures(WHITE) + r.black_dead);

    if (area) {
        r.black_x2 = int16_t(2 * (r.black_stones + r.black_territory));
        r.white_x2 = int16_t(2 * (r.white_stones + r.white_territory) + komi_x2);
    } else {
        r.black_x2 = int16_t(2 * (r.black_territory + r.black_captures));
        r.white_x2 = int16_t(2 * (r.white_territory + r.white_captures) + komi_x2);
    }
    r.margin_x2 = int16_t(r.black_x2 - r.white_x2);
    r.winner = r.margin_x2 > 0 ? BLACK : (r.margin_x2 < 0 ? WHITE : EMPTY);
    return r;
}

// Bouzy 5/21 on g.work. `ignore` (may be null) suppresses the seeds of the marked stones, which is
// how propose_dead asks "whose area would this be if that group were not there?".
void bouzy_map(const Board& b, const bool* ignore, int dilations, int erosions)
{
    const int stride = b.stride();
    const int n_points = b.num_points();
    std::memset(g.work, 0, size_t(n_points) * sizeof(g.work[0]));
    for (int p = 0; p < n_points; ++p) {
        if (ignore != nullptr && ignore[p]) continue;
        const Color c = b.at(Point(p));
        if (c == BLACK) g.work[p] = BOUZY_STONE;
        else if (c == WHITE) g.work[p] = -BOUZY_STONE;
    }
    const int y0 = stride + 1, y1 = n_points - stride - 1;   // first/last on-board index
    for (int d = 0; d < dilations; ++d) {
        std::memcpy(g.prev, g.work, size_t(n_points) * sizeof(g.work[0]));
        for (int p = y0; p < y1; ++p) {
            if (b.at(Point(p)) == BORDER) continue;
            const int nb[4] = {p - stride, p - 1, p + 1, p + stride};
            int pos = 0, neg = 0;
            for (int i = 0; i < 4; ++i) {
                if (b.at(Point(nb[i])) == BORDER) continue;
                if (g.prev[nb[i]] > 0) ++pos;
                else if (g.prev[nb[i]] < 0) ++neg;
            }
            const int v = g.prev[p];
            if (v >= 0 && neg == 0) g.work[p] = int16_t(v + pos);
            else if (v <= 0 && pos == 0) g.work[p] = int16_t(v - neg);
        }
    }
    for (int e = 0; e < erosions; ++e) {
        std::memcpy(g.prev, g.work, size_t(n_points) * sizeof(g.work[0]));
        for (int p = y0; p < y1; ++p) {
            if (b.at(Point(p)) == BORDER) continue;
            const int v = g.prev[p];
            if (v == 0) continue;
            const int nb[4] = {p - stride, p - 1, p + 1, p + stride};
            int against = 0;
            for (int i = 0; i < 4; ++i) {
                if (b.at(Point(nb[i])) == BORDER) continue;   // off-board never erodes
                if (v > 0 ? g.prev[nb[i]] <= 0 : g.prev[nb[i]] >= 0) ++against;
            }
            if (v > 0) g.work[p] = int16_t(v - against < 0 ? 0 : v - against);
            else g.work[p] = int16_t(v + against > 0 ? 0 : v + against);
        }
    }
}

// Benson's algorithm for one colour; sets out[p] for its unconditionally alive stones.
void benson_colour(const Board& b, Color c, bool* out)
{
    const int stride = b.stride();
    const int n_points = b.num_points();

    // 1. Index the chains of colour c (union-find roots -> dense ids).
    for (int p = 0; p < n_points; ++p) { g.chain_of[p] = -1; g.region_of[p] = -1; }
    int n_chains = 0;
    for (int p = 0; p < n_points; ++p) {
        if (b.at(Point(p)) != c) continue;
        const Point root = b.group_root(Point(p));
        if (g.chain_of[root] < 0) {
            if (n_chains >= MAX_GROUPS) return;      // impossible on <= 19x19; stay conservative
            g.chain_of[root] = int16_t(n_chains++);
        }
        g.chain_of[p] = g.chain_of[root];
    }
    if (n_chains == 0) return;

    // 2. Regions = connected components of the points that are not colour c. For each region record
    //    the chains that border it and the chains it is vital to (every empty point of the region is
    //    a liberty of that chain; a region with no empty point is vital to all its border chains).
    int n_regions = 0;
    for (int p0 = 0; p0 < n_points; ++p0) {
        const Color c0 = b.at(Point(p0));
        if (c0 == BORDER || c0 == c || g.region_of[p0] >= 0) continue;
        if (n_regions >= MAX_GROUPS) return;
        const int r = n_regions++;
        uint32_t* vital = g.vital[r];
        uint32_t* border = g.border[r];
        bits_clear(border);
        bits_fill(vital, n_chains);      // intersection accumulator
        int empties = 0;
        int top = 0;
        g.stack[top++] = Point(p0);
        g.region_of[p0] = int16_t(r);
        while (top > 0) {
            const Point q = g.stack[--top];
            const Point nb[4] = {Point(q - stride), Point(q - 1), Point(q + 1), Point(q + stride)};
            uint32_t touch[GROUP_WORDS];
            bits_clear(touch);
            for (int i = 0; i < 4; ++i) {
                const Point e = nb[i];
                const Color ce = b.at(e);
                if (ce == BORDER) continue;
                if (ce == c) {
                    bits_set(touch, g.chain_of[e]);
                    continue;
                }
                if (g.region_of[e] >= 0) continue;
                g.region_of[e] = int16_t(r);
                g.stack[top++] = e;
            }
            bits_or(border, touch);
            if (b.at(q) == EMPTY) {
                ++empties;
                bits_and(vital, touch);   // only chains adjacent to *every* empty point stay vital
            }
        }
        if (empties == 0) {
            for (int i = 0; i < GROUP_WORDS; ++i) vital[i] = border[i];
        }
    }

    // 3. Fixed point: drop regions whose border leaves the alive set, then chains with < 2 vital
    //    regions, until nothing changes.
    bits_fill(g.chain_alive, n_chains);
    bits_fill(g.region_ok, n_regions);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = 0; r < n_regions; ++r) {
            if (!bits_test(g.region_ok, r)) continue;
            if (!bits_subset(g.border[r], g.chain_alive)) {
                bits_clear_bit(g.region_ok, r);
                changed = true;
            }
        }
        for (int x = 0; x < n_chains; ++x) g.vital_count[x] = 0;
        for (int r = 0; r < n_regions; ++r) {
            if (!bits_test(g.region_ok, r)) continue;
            for (int x = 0; x < n_chains; ++x)
                if (bits_test(g.vital[r], x)) ++g.vital_count[x];
        }
        for (int x = 0; x < n_chains; ++x) {
            if (!bits_test(g.chain_alive, x)) continue;
            if (g.vital_count[x] < 2) {
                bits_clear_bit(g.chain_alive, x);
                changed = true;
            }
        }
    }

    for (int p = 0; p < n_points; ++p)
        if (b.at(Point(p)) == c && bits_test(g.chain_alive, g.chain_of[p])) out[p] = true;
}

// True when the group at p is one half of a textbook seki: it and one opposite-colour neighbour
// group share every liberty they have (>= 2 of them), so neither side can fill without dying.
bool in_shared_liberty_seki(const Board& b, Point p)
{
    Point libs[4];
    const int n = b.liberties(p, libs, 4, 3);
    if (n != 2) return false;
    const Color c = b.at(p);
    const Color opp = opponent(c);
    const int stride = b.stride();
    for (int i = 0; i < n; ++i) {
        const Point nb[4] = {Point(libs[i] - stride), Point(libs[i] - 1), Point(libs[i] + 1),
                             Point(libs[i] + stride)};
        for (int k = 0; k < 4; ++k) {
            if (b.at(nb[k]) != opp) continue;
            Point olibs[4];
            const int on = b.liberties(nb[k], olibs, 4, 3);
            if (on != 2) continue;
            const bool same = (olibs[0] == libs[0] && olibs[1] == libs[1]) ||
                              (olibs[0] == libs[1] && olibs[1] == libs[0]);
            if (same) return true;
        }
    }
    return false;
}

}  // namespace

// -------------------------------------------------------------------------------------------
// public API

ScoreResult score_area(const Board& b, const bool* dead, int komi_x2)
{
    return score_common(b, dead, komi_x2, true);
}

ScoreResult score_territory(const Board& b, const bool* dead, int komi_x2)
{
    return score_common(b, dead, komi_x2, false);
}

ScoreResult score_game(const Board& b, const bool* dead, int komi_x2, Ruleset rules)
{
    return score_common(b, dead, komi_x2, rules == Ruleset::AREA);
}

void bouzy_influence(const Board& b, int8_t* out, int dilations, int erosions)
{
    bouzy_map(b, nullptr, dilations, erosions);
    std::memset(out, 0, MAX_POINTS * sizeof(int8_t));
    const int n_points = b.num_points();
    for (int p = 0; p < n_points; ++p) {
        if (b.at(Point(p)) == BORDER) continue;
        const int v = g.work[p];
        out[p] = int8_t(v > 127 ? 127 : (v < -127 ? -127 : v));
    }
}

ScoreResult estimate_score(const Board& b, int komi_x2, Ruleset rules)
{
    ScoreResult r;
    std::memset(r.owner, OWN_NONE, sizeof(r.owner));
    bouzy_map(b, nullptr, 5, 21);
    const int n_points = b.num_points();
    for (int p = 0; p < n_points; ++p) {
        const Color c = b.at(Point(p));
        if (c == BORDER) continue;
        if (c == BLACK) { ++r.black_stones; r.owner[p] = OWN_BLACK; continue; }
        if (c == WHITE) { ++r.white_stones; r.owner[p] = OWN_WHITE; continue; }
        const int v = g.work[p];
        if (v > 0) { ++r.black_territory; r.owner[p] = OWN_BLACK; }
        else if (v < 0) { ++r.white_territory; r.owner[p] = OWN_WHITE; }
        else r.owner[p] = OWN_DAME;
    }
    r.black_captures = int16_t(b.captures(BLACK));
    r.white_captures = int16_t(b.captures(WHITE));
    if (rules == Ruleset::AREA) {
        r.black_x2 = int16_t(2 * (r.black_stones + r.black_territory));
        r.white_x2 = int16_t(2 * (r.white_stones + r.white_territory) + komi_x2);
    } else {
        r.black_x2 = int16_t(2 * (r.black_territory + r.black_captures));
        r.white_x2 = int16_t(2 * (r.white_territory + r.white_captures) + komi_x2);
    }
    r.margin_x2 = int16_t(r.black_x2 - r.white_x2);
    r.winner = r.margin_x2 > 0 ? BLACK : (r.margin_x2 < 0 ? WHITE : EMPTY);
    return r;
}

void benson_pass_alive(const Board& b, bool* out)
{
    std::memset(out, 0, MAX_POINTS * sizeof(bool));
    benson_colour(b, BLACK, out);
    benson_colour(b, WHITE, out);
}

// Looks at every empty region touching the group. Returns false as soon as the group touches a
// region that is neither its own enclosed eye space nor an area the opponent controls; `own_space`
// accumulates the size of the regions that only this group borders.
bool enclosed_by_opponent(const Board& b, const Point* stones, int n, Color c, int& own_space)
{
    const int stride = b.stride();
    const Color opp = opponent(c);
    own_space = 0;
    for (int i = 0; i < n; ++i) g.in_group[stones[i]] = true;
    const uint16_t gen = next_gen();
    bool enclosed = true;
    for (int i = 0; i < n && enclosed; ++i) {
        const Point q = stones[i];
        const Point nb[4] = {Point(q - stride), Point(q - 1), Point(q + 1), Point(q + stride)};
        for (int k = 0; k < 4; ++k) {
            const Point start = nb[k];
            if (b.at(start) != EMPTY || g.stamp[start] == gen) continue;
            // flood this empty region
            int top = 0, count = 0, own_inf = 0, opp_inf = 0;
            bool touch_opp = false, touch_friend = false;
            g.stack[top++] = start;
            g.stamp[start] = gen;
            while (top > 0) {
                const Point r = g.stack[--top];
                ++count;
                const int v = g.work[r];
                if (c == BLACK ? v > 0 : v < 0) ++own_inf;
                else if (c == BLACK ? v < 0 : v > 0) ++opp_inf;
                const Point rn[4] = {Point(r - stride), Point(r - 1), Point(r + 1), Point(r + stride)};
                for (int j = 0; j < 4; ++j) {
                    const Point e = rn[j];
                    const Color ce = b.at(e);
                    if (ce == EMPTY) {
                        if (g.stamp[e] == gen) continue;
                        g.stamp[e] = gen;
                        g.stack[top++] = e;
                    } else if (ce == opp) {
                        touch_opp = true;
                    } else if (ce == c && !g.in_group[e]) {
                        touch_friend = true;
                    }
                }
            }
            if (!touch_opp && !touch_friend) own_space += count;      // the group's own eye space
            else if (opp_inf > own_inf && opp_inf > 0) continue;      // the opponent owns this area
            else { enclosed = false; break; }                         // somewhere else to live
        }
    }
    for (int i = 0; i < n; ++i) g.in_group[stones[i]] = false;
    return enclosed;
}

void propose_dead(const Board& b, bool* dead)
{
    std::memset(dead, 0, MAX_POINTS * sizeof(bool));
    const int n_points = b.num_points();
    benson_pass_alive(b, g.alive);

    // Enumerate the groups, most surrounded first: the order matters because each decision feeds the
    // next (the influence map is recomputed with the stones already found dead taken off the board).
    int n_groups = 0;
    const uint16_t gen = next_gen();
    for (int p = 0; p < n_points; ++p) {
        const Color c = b.at(Point(p));
        if (c != BLACK && c != WHITE) continue;
        const Point root = b.group_root(Point(p));
        if (g.stamp[root] == gen) continue;
        g.stamp[root] = gen;
        if (n_groups >= MAX_GROUPS * 2) break;
        g.group_rep[n_groups] = Point(p);
        g.group_libs[n_groups] = int16_t(b.liberties(Point(p)));
        ++n_groups;
    }
    for (int i = 1; i < n_groups; ++i) {          // insertion sort by liberty count
        const Point rep = g.group_rep[i];
        const int16_t libs = g.group_libs[i];
        int j = i - 1;
        for (; j >= 0 && g.group_libs[j] > libs; --j) {
            g.group_rep[j + 1] = g.group_rep[j];
            g.group_libs[j + 1] = g.group_libs[j];
        }
        g.group_rep[j + 1] = rep;
        g.group_libs[j + 1] = libs;
    }

    bool influence_valid = false;
    for (int i = 0; i < n_groups; ++i) {
        const Point rep = g.group_rep[i];
        const Color c = b.at(rep);
        if (g.alive[rep]) continue;                       // unconditionally alive (Benson)
        if (in_shared_liberty_seki(b, rep)) continue;     // textbook seki: both sides live
        if (!influence_valid) {
            bouzy_map(b, dead, 5, 21);                    // stones already given up are not sources
            influence_valid = true;
        }
        const int n = b.group_stones(rep, g.group_pts, BOARD_POINTS);
        int own_space = 0;
        if (!enclosed_by_opponent(b, g.group_pts, n, c, own_space)) continue;
        if (own_space >= EYE_SPACE_ALIVE) continue;       // room enough for two eyes
        for (int k = 0; k < n; ++k) dead[g.group_pts[k]] = true;
        influence_valid = false;                          // the map changes once a group is removed
    }
}

uint8_t region_owner(const Board& b, Point p, uint16_t* stamp, uint16_t gen, Point* out, int* out_count)
{
    if (out_count != nullptr) *out_count = 0;
    if (!b.on_board(p) || b.at(p) != EMPTY) return OWN_NONE;
    const BoardCol acc{&b};
    return flood_region(acc, b.stride(), p, stamp, gen, out, out_count);
}

}  // namespace go
