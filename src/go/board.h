// Fast incremental Go board (host + GBA). This is the AI's inner loop: keep it allocation-free.
//
// Design (libego style):
//  * padded flat arrays, stride = size + 2, border points are BORDER;
//  * groups tracked with union-find (iterative find with path halving) + a circular linked list of
//    stones per group (next_stone) so groups can be walked without recursion;
//  * pseudo-liberties per group root: count, sum of liberty points, sum of squares. A group is in atari
//    iff count > 0 && sq_sum * count == sum * sum (all pseudo-liberties are the same point), and that
//    point is sum / count. Exact liberty counts use a stamp array (liberties()).
//  * simple ko point (single-stone capture by a single stone); positional superko is handled by Game.
//  * an empty-point list (empties / empty_index) for fast random move generation.
//
// Legality: is_legal(p, c) == at(p) == EMPTY && p != ko_point() && !is_suicide(p, c).
// Suicide is illegal. Playing into a capture is fine.
#pragma once
#include "go_types.h"

namespace go {

class Board {
public:
    Board() { init(9); }

    void init(int size);                       // clears the board, sets stride/borders
    void copy_from(const Board& other);        // copies only the (size+2)^2 prefix of each array + scalars

    int size() const { return size_; }
    int stride() const { return stride_; }
    int num_points() const { return num_points_; }        // (size+2)^2
    Point point(int x, int y) const { return Point((y + 1) * stride_ + (x + 1)); }   // 0-based, x = column
    int x_of(Point p) const { return p % stride_ - 1; }
    int y_of(Point p) const { return p / stride_ - 1; }
    bool on_board(Point p) const { return p >= 0 && p < num_points_ && color_[p] != BORDER; }
    Color at(Point p) const { return color_[p]; }
    Color at(int x, int y) const { return color_[point(x, y)]; }

    // --- move legality (fast, no allocation) ---
    bool is_legal(Point p, Color c) const;     // p must be a board point or PASS (PASS -> true)
    bool is_suicide(Point p, Color c) const;   // p empty assumed
    bool is_capture(Point p, Color c) const;   // would capture at least one stone
    bool is_self_atari(Point p, Color c) const;// after playing, the stone's group has exactly 1 liberty
                                               // (exact when the move captures nothing; if it captures,
                                               // returns true only in the snapback case: 1 stone captured
                                               // and the resulting group has 1 liberty)
    bool is_eye_like(Point p, Color c) const;  // all 4 neighbours are c/BORDER and diagonals: on the edge
                                               // no opponent diagonal, elsewhere at most one
    Point ko_point() const { return ko_; }

    // --- play ---
    // Precondition: is_legal(p, c). Places the stone, removes captured groups, updates ko/hash.
    // Returns the number of captured stones. Captured points are available in last_captured().
    int play(Point p, Color c);
    void play_pass(Color c);                   // clears the ko point, records last move = PASS
    // Setup stone placement without capture/ko side effects (missions, handicap). Asserts p is empty.
    void set_stone(Point p, Color c);
    void remove_stone(Point p);                // for dead-stone removal at scoring time

    // --- state ---
    uint64_t hash() const { return hash_; }    // Zobrist of stones only
    Point last_move() const { return last_move_; }
    Color last_color() const { return last_color_; }
    int captures(Color by) const { return captures_[by]; }     // prisoners taken BY color
    int stones(Color c) const { return stone_count_[c]; }
    int empty_count() const { return empty_count_; }
    Point empty_at(int i) const { return empties_[i]; }        // 0 <= i < empty_count()
    int last_captured_count() const { return last_captured_count_; }
    const Point* last_captured() const { return last_captured_; }

    // --- groups (p must hold a stone) ---
    Point group_root(Point p) const;           // representative (const: no path compression)
    int group_size(Point p) const;
    bool in_atari(Point p) const;
    Point atari_point(Point p) const;          // the single liberty when in_atari(), else NO_POINT
    int pseudo_liberties(Point p) const;
    // Exact liberties of the group containing p. If out != nullptr, up to max_out liberty points are
    // written. cap stops the count early (cap <= 0 means no cap). Cost O(group size).
    int liberties(Point p, Point* out = nullptr, int max_out = 0, int cap = 0) const;
    // Walk the stones of a group: first = p, then next_stone(first) ... until back to p.
    Point next_stone(Point p) const { return next_stone_[p]; }
    int group_stones(Point p, Point* out, int max_out) const;

    // Neighbour offsets, in order: -stride (up), -1 (left), +1 (right), +stride (down).
    int up_offset() const { return -stride_; }

private:
    Point find(Point p);                       // mutable path-halving find
    Point find_const(Point p) const;
    void merge(Point a, Point b);              // union two roots
    void add_stone_libs(Point p);              // pseudo-lib bookkeeping helpers
    int remove_group(Point root);              // captures; returns stones removed
    void add_empty(Point p);
    void remove_empty(Point p);
    uint16_t next_mark_gen() const;            // bumps the exact-liberty stamp generation

    int size_ = 9, stride_ = 11, num_points_ = 121;
    uint64_t hash_ = 0;
    Point ko_ = NO_POINT, last_move_ = NO_POINT;
    Color last_color_ = EMPTY;
    int16_t captures_[3] = {0, 0, 0};
    int16_t stone_count_[3] = {0, 0, 0};
    int16_t empty_count_ = 0;

    uint8_t color_[MAX_POINTS];
    uint16_t parent_[MAX_POINTS];
    uint16_t next_stone_[MAX_POINTS];
    uint16_t lib_count_[MAX_POINTS];           // valid at roots only
    uint32_t lib_sum_[MAX_POINTS];
    uint32_t lib_sq_sum_[MAX_POINTS];
    uint16_t group_size_[MAX_POINTS];
    Point empties_[MAX_POINTS];
    int16_t empty_index_[MAX_POINTS];

    // Scratch (not copied by copy_from): exact-liberty stamps and capture list of the last move.
    mutable uint16_t mark_[MAX_POINTS];
    mutable uint16_t mark_gen_ = 0;
    Point last_captured_[MAX_SIZE * MAX_SIZE];
    int16_t last_captured_count_ = 0;
};

}  // namespace go
