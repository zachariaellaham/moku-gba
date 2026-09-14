// MOKU rules engine - depth-limited ladder reader (see ladder.h).
//
// The search plays moves on a private lightweight board (a colour array plus an undo stack) instead
// of copying go::Board at every node: a Board copy is ~11 KB, a ladder is up to 60 plies deep and the
// reader runs inside the AI's move evaluation, so copying is out of the question. The rules the
// little board implements are exactly the ones a ladder needs: capture, suicide is illegal, simple ko.
// The state is one file-scope object in EWRAM (GO_EWRAM_BSS); the search is not re-entrant.
#include "go/ladder.h"
#include <cstring>

namespace go {

namespace {

constexpr int MAX_DEPTH = LADDER_MAX_PLIES + 2;
constexpr int CAP_STACK = 512;          // captured stones recorded along the current line
constexpr int BOARD_POINTS = MAX_SIZE * MAX_SIZE;
constexpr int MAX_DEFENDER_MOVES = 8;   // extend + captures of adjacent attacker groups in atari

struct LadderState {
    uint8_t col[MAX_POINTS];
    int stride = 11;
    int num_points = 121;
    Point ko = NO_POINT;

    // undo stack (one entry per ply played)
    Point placed[MAX_DEPTH];
    Color placed_col[MAX_DEPTH];
    Point ko_before[MAX_DEPTH];
    int16_t cap_start[MAX_DEPTH];
    int16_t cap_n[MAX_DEPTH];
    Point cap_pts[CAP_STACK];
    int16_t cap_top = 0;
    int16_t depth = 0;

    // flood scratch
    uint16_t stamp[MAX_POINTS];
    uint16_t gen = 0;
    Point stack[BOARD_POINTS];
    Point group_pts[BOARD_POINTS];

    // search state
    Color defender = BLACK, attacker = WHITE;
    Point target = NO_POINT;
    int max_plies = LADDER_MAX_PLIES;
    int nodes = 0;
    int node_budget = 0;
    bool overflow = false;
    Point cand[MAX_DEPTH][MAX_DEFENDER_MOVES];   // per-ply move list (keeps the recursion frames tiny)
    Point line[MAX_DEPTH];
    int line_len = 0;
};

GO_EWRAM_BSS LadderState lb;

inline uint16_t next_gen()
{
    if (++lb.gen == 0) {
        std::memset(lb.stamp, 0, sizeof(lb.stamp));
        lb.gen = 1;
    }
    return lb.gen;
}

// Exact liberties of the group at p (which must hold a stone). `cap` stops the count early.
int libs(Point p, Point* out, int max_out, int cap)
{
    const Color c = lb.col[p];
    const int s = lb.stride;
    const uint16_t gen = next_gen();
    int top = 0, n = 0;
    lb.stack[top++] = p;
    lb.stamp[p] = gen;
    while (top > 0) {
        const Point q = lb.stack[--top];
        const Point nb[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
        for (int i = 0; i < 4; ++i) {
            const Point e = nb[i];
            if (lb.stamp[e] == gen) continue;
            const Color ce = lb.col[e];
            if (ce == EMPTY) {
                lb.stamp[e] = gen;
                if (out != nullptr && n < max_out) out[n] = e;
                ++n;
                if (cap > 0 && n >= cap) return n;
            } else if (ce == c) {
                lb.stamp[e] = gen;
                lb.stack[top++] = e;
            }
        }
    }
    return n;
}

// Collects the stones of the group at p; returns the count (<= BOARD_POINTS).
int group_points(Point p, Point* out)
{
    const Color c = lb.col[p];
    const int s = lb.stride;
    const uint16_t gen = next_gen();
    int top = 0, n = 0;
    lb.stack[top++] = p;
    lb.stamp[p] = gen;
    while (top > 0) {
        const Point q = lb.stack[--top];
        out[n++] = q;
        const Point nb[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
        for (int i = 0; i < 4; ++i) {
            const Point e = nb[i];
            if (lb.stamp[e] == gen || lb.col[e] != c) continue;
            lb.stamp[e] = gen;
            lb.stack[top++] = e;
        }
    }
    return n;
}

// Smallest point index of the group at p: a stable identity for de-duplication.
Point group_rep(Point p)
{
    const int n = group_points(p, lb.group_pts);
    Point rep = lb.group_pts[0];
    for (int i = 1; i < n; ++i)
        if (lb.group_pts[i] < rep) rep = lb.group_pts[i];
    return rep;
}

// Removes the group at p, recording the stones so the move can be undone. Returns the count.
int remove_group(Point p)
{
    const Color c = lb.col[p];
    const int s = lb.stride;
    const uint16_t gen = next_gen();
    int top = 0, n = 0;
    lb.stack[top++] = p;
    lb.stamp[p] = gen;
    while (top > 0) {
        const Point q = lb.stack[--top];
        lb.col[q] = EMPTY;
        if (lb.cap_top >= CAP_STACK) { lb.overflow = true; return n; }
        lb.cap_pts[lb.cap_top++] = q;
        ++n;
        const Point nb[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
        for (int i = 0; i < 4; ++i) {
            const Point e = nb[i];
            if (lb.stamp[e] == gen || lb.col[e] != c) continue;
            lb.stamp[e] = gen;
            lb.stack[top++] = e;
        }
    }
    return n;
}

// Plays c at p. Returns false and leaves the position untouched when the move is illegal
// (occupied, ko, or suicide).
bool play(Point p, Color c)
{
    if (lb.col[p] != EMPTY || p == lb.ko || lb.depth >= MAX_DEPTH - 1) return false;
    const int d = lb.depth;
    const int s = lb.stride;
    const Color opp = opponent(c);
    lb.ko_before[d] = lb.ko;
    lb.cap_start[d] = lb.cap_top;
    lb.placed[d] = p;
    lb.placed_col[d] = c;
    lb.col[p] = c;

    int captured = 0;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
    for (int i = 0; i < 4; ++i) {
        const Point e = nb[i];
        if (lb.col[e] != opp) continue;
        if (libs(e, nullptr, 0, 1) == 0) captured += remove_group(e);
    }
    if (captured == 0 && libs(p, nullptr, 0, 1) == 0) {   // suicide: undo the placement
        lb.col[p] = EMPTY;
        return false;
    }
    lb.cap_n[d] = int16_t(lb.cap_top - lb.cap_start[d]);

    lb.ko = NO_POINT;
    if (captured == 1) {
        bool alone = true;
        for (int i = 0; i < 4; ++i) if (lb.col[nb[i]] == c) alone = false;
        if (alone && libs(p, nullptr, 0, 2) == 1) lb.ko = lb.cap_pts[lb.cap_start[d]];
    }
    ++lb.depth;
    return true;
}

void undo()
{
    const int d = --lb.depth;
    lb.col[lb.placed[d]] = EMPTY;
    const Color opp = opponent(lb.placed_col[d]);
    const int start = lb.cap_start[d];
    const int end = start + lb.cap_n[d];
    for (int i = start; i < end; ++i) lb.col[lb.cap_pts[i]] = opp;
    lb.cap_top = int16_t(start);
    lb.ko = lb.ko_before[d];
}

LadderResult search_defender(int depth);

// The attacker is to move: fill one of the target's liberties and keep it in atari.
LadderResult search_attacker(int depth)
{
    if (lb.overflow || depth >= lb.max_plies || ++lb.nodes > lb.node_budget)
        return LadderResult::UNKNOWN;
    Point ls[4];
    const int n = libs(lb.target, ls, 4, 3);
    if (n >= 3) return LadderResult::ESCAPED;
    if (n == 0) { lb.line_len = depth; return LadderResult::CAPTURED; }

    bool unknown = false;
    for (int i = 0; i < n; ++i) {
        if (!play(ls[i], lb.attacker)) continue;
        LadderResult r;
        if (lb.col[lb.target] != lb.defender) {      // the move took the whole group off the board
            r = LadderResult::CAPTURED;
            lb.line_len = depth + 1;
        } else {
            r = search_defender(depth + 1);
        }
        undo();
        if (r == LadderResult::CAPTURED) {
            lb.line[depth] = ls[i];
            return LadderResult::CAPTURED;
        }
        if (r == LadderResult::UNKNOWN) unknown = true;
    }
    return unknown ? LadderResult::UNKNOWN : LadderResult::ESCAPED;
}

// The defender is to move: run out of the atari, or capture one of the surrounding stones.
LadderResult search_defender(int depth)
{
    if (lb.overflow || depth >= lb.max_plies || ++lb.nodes > lb.node_budget)
        return LadderResult::UNKNOWN;
    Point ls[4];
    const int n = libs(lb.target, ls, 4, 3);
    if (n >= 2) return LadderResult::ESCAPED;    // not in atari on its own move: the ladder failed
    if (n == 0) { lb.line_len = depth; return LadderResult::CAPTURED; }

    Point* const cand = lb.cand[depth];
    int nc = 0;
    cand[nc++] = ls[0];                          // extend on the atari point
    const int s = lb.stride;
    const int ng = group_points(lb.target, lb.group_pts);
    for (int i = 0; i < ng && nc < MAX_DEFENDER_MOVES; ++i) {
        const Point q = lb.group_pts[i];
        const Point nb[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
        for (int k = 0; k < 4 && nc < MAX_DEFENDER_MOVES; ++k) {
            if (lb.col[nb[k]] != lb.attacker) continue;
            Point alib[1];
            if (libs(nb[k], alib, 1, 2) != 1) continue;
            bool seen = false;
            for (int j = 0; j < nc; ++j) if (cand[j] == alib[0]) seen = true;
            if (!seen) cand[nc++] = alib[0];     // capture the attacking group
        }
    }

    bool unknown = false, played_any = false;
    Point best = NO_POINT;
    for (int i = 0; i < nc; ++i) {
        if (!play(cand[i], lb.defender)) continue;
        played_any = true;
        LadderResult r;
        const int after = libs(lb.target, nullptr, 0, 3);
        if (after >= 3) r = LadderResult::ESCAPED;
        else r = search_attacker(depth + 1);
        undo();
        if (r == LadderResult::ESCAPED) return LadderResult::ESCAPED;
        if (r == LadderResult::UNKNOWN) unknown = true;
        else best = cand[i];
    }
    // The defender has no legal answer at all (every extension would be suicide): the group is lost
    // and the line stops here - the attacker just takes it next move. The line stays strictly
    // alternating, which is what the animation and the mission hints rely on.
    if (!played_any) { lb.line_len = depth; return LadderResult::CAPTURED; }
    if (unknown) return LadderResult::UNKNOWN;
    lb.line[depth] = best;
    return LadderResult::CAPTURED;
}

void setup(const Board& b, Color defender, Point target, int max_plies)
{
    lb.stride = b.stride();
    lb.num_points = b.num_points();
    for (int p = 0; p < lb.num_points; ++p) lb.col[p] = b.at(Point(p));
    lb.ko = b.ko_point();
    lb.cap_top = 0;
    lb.depth = 0;
    lb.defender = defender;
    lb.attacker = opponent(defender);
    lb.target = target;
    lb.max_plies = max_plies < 1 ? 1 : (max_plies > LADDER_MAX_PLIES ? LADDER_MAX_PLIES : max_plies);
    lb.nodes = 0;
    lb.node_budget = lb.max_plies * 8 + 16;
    lb.overflow = false;
    lb.line_len = 0;
}

}  // namespace

LadderResult read_ladder(const Board& b, Point p, bool attacker_to_move, int max_plies)
{
    lb.line_len = 0;
    if (!b.on_board(p)) return LadderResult::UNKNOWN;
    const Color d = b.at(p);
    if (d != BLACK && d != WHITE) return LadderResult::UNKNOWN;
    if (b.liberties(p, nullptr, 0, 3) >= 3) return LadderResult::ESCAPED;

    setup(b, d, p, max_plies);
    const LadderResult r = attacker_to_move ? search_attacker(0) : search_defender(0);
    if (r != LadderResult::CAPTURED) lb.line_len = 0;
    return r;
}

LadderResult ladder_after_move(const Board& b, Point atari_move, Color c, int max_plies)
{
    lb.line_len = 0;
    if (!b.on_board(atari_move) || b.at(atari_move) != EMPTY) return LadderResult::UNKNOWN;
    if (c != BLACK && c != WHITE) return LadderResult::UNKNOWN;
    if (!b.is_legal(atari_move, c)) return LadderResult::UNKNOWN;

    const Color defender = opponent(c);
    setup(b, defender, NO_POINT, max_plies);
    if (!play(atari_move, c)) return LadderResult::UNKNOWN;

    // Every opposite-colour group next to the played stone that is now in atari is a ladder target.
    const int s = lb.stride;
    const Point nb[4] = {Point(atari_move - s), Point(atari_move - 1), Point(atari_move + 1),
                         Point(atari_move + s)};
    Point reps[4];
    int n_rep = 0;
    bool captured = false, escaped = false, unknown = false;
    for (int i = 0; i < 4; ++i) {
        if (lb.col[nb[i]] != defender) continue;
        if (libs(nb[i], nullptr, 0, 2) != 1) continue;
        const Point rep = group_rep(nb[i]);
        bool seen = false;
        for (int k = 0; k < n_rep; ++k) if (reps[k] == rep) seen = true;
        if (seen) continue;
        reps[n_rep++] = rep;

        lb.target = nb[i];
        lb.nodes = 0;
        const int saved_len = lb.line_len;
        const LadderResult r = search_defender(1);
        if (r == LadderResult::CAPTURED) { captured = true; break; }
        lb.line_len = saved_len;
        if (r == LadderResult::ESCAPED) escaped = true; else unknown = true;
    }
    undo();

    if (captured) {
        lb.line[0] = atari_move;
        return LadderResult::CAPTURED;
    }
    lb.line_len = 0;
    if (n_rep == 0) return LadderResult::ESCAPED;    // nothing was put in atari: no ladder at all
    if (escaped && !unknown) return LadderResult::ESCAPED;
    return unknown ? LadderResult::UNKNOWN : LadderResult::ESCAPED;
}

int last_ladder_line(Point* out, int max_out)
{
    int n = lb.line_len;
    if (n > max_out) n = max_out;
    for (int i = 0; i < n; ++i) out[i] = lb.line[i];
    return n;
}

}  // namespace go
