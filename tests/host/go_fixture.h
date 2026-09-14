// Shared helpers for the go engine unit tests: ASCII board fixtures, reference (slow but obviously
// correct) implementations to check the fast incremental ones against, and consistency checks.
#pragma once
#include "go/board.h"
#include "go/game.h"
#include <initializer_list>

namespace fx {

using go::Board;
using go::Color;
using go::Point;
using go::BLACK;
using go::WHITE;
using go::EMPTY;
using go::BORDER;

// '.' or '+' empty, 'X'/'x'/'#' black, 'O'/'o'/'0' white; any other character is skipped, so rows may
// carry coordinate labels or spaces between stones.
inline Color char_color(char c)
{
    if (c == 'X' || c == 'x' || c == '#') return BLACK;
    if (c == 'O' || c == 'o' || c == '0') return WHITE;
    return EMPTY;
}

inline bool is_point_char(char c)
{
    return c == '.' || c == '+' || char_color(c) != EMPTY;
}

// Fills `b` from ASCII rows (row 0 = y 0 = top). The board is (re)initialised to rows.size().
inline void set_board(Board& b, std::initializer_list<const char*> rows)
{
    const int size = int(rows.size());
    b.init(size);
    int y = 0;
    for (const char* row : rows) {
        int x = 0;
        for (const char* c = row; *c != '\0'; ++c) {
            if (!is_point_char(*c)) continue;
            const Color col = char_color(*c);
            if (col != EMPTY && x < size && y < size) b.set_stone(b.point(x, y), col);
            ++x;
        }
        ++y;
    }
}

// Same, for a Game: the stones become setup stones (so undo/rebuild keeps them).
inline void setup_game(go::Game& g, std::initializer_list<const char*> rows)
{
    const int size = int(rows.size());
    go::GameSettings s;
    s.size = uint8_t(size);
    g.start(s);
    int y = 0;
    for (const char* row : rows) {
        int x = 0;
        for (const char* c = row; *c != '\0'; ++c) {
            if (!is_point_char(*c)) continue;
            const Color col = char_color(*c);
            if (col != EMPTY && x < size && y < size) g.add_setup_stone(g.board().point(x, y), col);
            ++x;
        }
        ++y;
    }
}

// ---------------------------------------------------------------------------------------------
// reference implementations (flood fill, no incremental state)

// Exact liberties of the group containing p, by flood fill over Board::at() only.
// (The visited marks use a generation counter so the fuzz tests can call this millions of times.)
inline int ref_liberties(const Board& b, Point p)
{
    static uint32_t seen[go::MAX_POINTS] = {}, lib[go::MAX_POINTS] = {};
    static uint32_t gen = 0;
    ++gen;
    const int stride = b.stride();
    Point stack[go::MAX_SIZE * go::MAX_SIZE];
    int top = 0, libs = 0;
    const Color c = b.at(p);
    if (c != BLACK && c != WHITE) return 0;
    stack[top++] = p;
    seen[p] = gen;
    while (top > 0) {
        const Point q = stack[--top];
        const Point nb[4] = {Point(q - stride), Point(q - 1), Point(q + 1), Point(q + stride)};
        for (int i = 0; i < 4; ++i) {
            const Point e = nb[i];
            const Color ce = b.at(e);
            if (ce == EMPTY) {
                if (lib[e] != gen) { lib[e] = gen; ++libs; }
            } else if (ce == c && seen[e] != gen) {
                seen[e] = gen;
                stack[top++] = e;
            }
        }
    }
    return libs;
}

inline int ref_group_size(const Board& b, Point p)
{
    static uint32_t seen[go::MAX_POINTS] = {};
    static uint32_t gen = 0;
    ++gen;
    const int stride = b.stride();
    Point stack[go::MAX_SIZE * go::MAX_SIZE];
    int top = 0, n = 0;
    const Color c = b.at(p);
    if (c != BLACK && c != WHITE) return 0;
    stack[top++] = p;
    seen[p] = gen;
    while (top > 0) {
        const Point q = stack[--top];
        ++n;
        const Point nb[4] = {Point(q - stride), Point(q - 1), Point(q + 1), Point(q + stride)};
        for (int i = 0; i < 4; ++i)
            if (b.at(nb[i]) == c && seen[nb[i]] != gen) { seen[nb[i]] = gen; stack[top++] = nb[i]; }
    }
    return n;
}

// A move is suicide unless it leaves a liberty: an empty neighbour, a friendly group with another
// liberty, or an opponent group that it captures.
inline bool ref_suicide(const Board& b, Point p, Color c)
{
    const int stride = b.stride();
    const Point nb[4] = {Point(p - stride), Point(p - 1), Point(p + 1), Point(p + stride)};
    for (int i = 0; i < 4; ++i) {
        const Color ce = b.at(nb[i]);
        if (ce == EMPTY) return false;
        if (ce == BORDER) continue;
        const int l = ref_liberties(b, nb[i]);
        if (ce == c) { if (l >= 2) return false; }
        else if (l == 1) return false;
    }
    return true;
}

inline bool ref_legal(const Board& b, Point p, Color c)
{
    if (p == go::PASS) return true;
    if (!b.on_board(p) || b.at(p) != EMPTY || p == b.ko_point()) return false;
    return !ref_suicide(b, p, c);
}

inline bool ref_captures(const Board& b, Point p, Color c)
{
    const int stride = b.stride();
    const Point nb[4] = {Point(p - stride), Point(p - 1), Point(p + 1), Point(p + stride)};
    for (int i = 0; i < 4; ++i)
        if (b.at(nb[i]) == go::opponent(c) && ref_liberties(b, nb[i]) == 1) return true;
    return false;
}

// ---------------------------------------------------------------------------------------------
// invariant checks (return true when the board is self-consistent)

// Every empty point appears exactly once in the empties list and nothing else does.
inline bool empties_consistent(const Board& b)
{
    int seen[go::MAX_POINTS] = {};
    for (int i = 0; i < b.empty_count(); ++i) {
        const Point p = b.empty_at(i);
        if (!b.on_board(p) || b.at(p) != EMPTY) return false;
        if (++seen[p] > 1) return false;
    }
    int empties = 0;
    for (int p = 0; p < b.num_points(); ++p) {
        if (b.at(Point(p)) != EMPTY) continue;
        ++empties;
        if (seen[p] != 1) return false;
    }
    return empties == b.empty_count();
}

// group/liberty bookkeeping against the reference flood fill, for every stone on the board.
inline bool groups_consistent(const Board& b)
{
    for (int p = 0; p < b.num_points(); ++p) {
        const Color c = b.at(Point(p));
        if (c != BLACK && c != WHITE) continue;
        const int ref = ref_liberties(b, Point(p));
        if (b.liberties(Point(p)) != ref) return false;
        if (b.group_size(Point(p)) != ref_group_size(b, Point(p))) return false;
        if (b.in_atari(Point(p)) != (ref == 1)) return false;
        if (b.pseudo_liberties(Point(p)) < ref) return false;
        if (ref == 1) {
            Point lib[1];
            b.liberties(Point(p), lib, 1, 1);
            if (b.atari_point(Point(p)) != lib[0]) return false;
        } else if (b.atari_point(Point(p)) != go::NO_POINT) {
            return false;
        }
        // walking next_stone must enumerate the group exactly once
        int walked = 0;
        Point q = Point(p);
        do { ++walked; q = b.next_stone(q); } while (q != Point(p) && walked <= b.num_points());
        if (walked != b.group_size(Point(p))) return false;
    }
    return true;
}

inline bool stone_counts_consistent(const Board& b)
{
    int black = 0, white = 0;
    for (int p = 0; p < b.num_points(); ++p) {
        if (b.at(Point(p)) == BLACK) ++black;
        else if (b.at(Point(p)) == WHITE) ++white;
    }
    return black == b.stones(BLACK) && white == b.stones(WHITE);
}

// Rebuilds the position with set_stone on a fresh board: the Zobrist hash must match.
inline uint64_t rebuilt_hash(const Board& b)
{
    Board fresh;
    fresh.init(b.size());
    for (int p = 0; p < b.num_points(); ++p) {
        const Color c = b.at(Point(p));
        if (c == BLACK || c == WHITE) fresh.set_stone(Point(p), c);
    }
    return fresh.hash();
}

inline bool same_position(const Board& a, const Board& b)
{
    if (a.size() != b.size() || a.hash() != b.hash()) return false;
    for (int p = 0; p < a.num_points(); ++p)
        if (a.at(Point(p)) != b.at(Point(p))) return false;
    return true;
}

// ---------------------------------------------------------------------------------------------
// random games

// Plays up to max_moves random legal moves (never filling own eyes, so games terminate), alternating
// colours. Returns the number of moves played.
inline int random_game(Board& b, go::Rng& rng, int max_moves, Color first = BLACK)
{
    Color c = first;
    int played = 0, passes = 0;
    while (played < max_moves && passes < 2) {
        const int n = b.empty_count();
        bool moved = false;
        if (n > 0) {
            const int start = int(rng.below(uint32_t(n)));
            for (int i = 0; i < n; ++i) {
                const Point p = b.empty_at((start + i) % n);
                if (b.is_eye_like(p, c) || !b.is_legal(p, c)) continue;
                b.play(p, c);
                moved = true;
                break;
            }
        }
        if (moved) { ++played; passes = 0; } else { ++passes; b.play_pass(c); }
        c = go::opponent(c);
    }
    return played;
}

}  // namespace fx
