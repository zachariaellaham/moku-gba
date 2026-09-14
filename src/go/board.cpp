// MOKU rules engine - incremental board (see board.h for the design notes).
#include "go/board.h"
#include "go/zobrist.h"
#include <cstring>

namespace go {

// Constant-initialised (constexpr) so the table lives in .rodata with no start-up code.
constexpr ZobristTable ZOBRIST = make_zobrist();

namespace {
inline uint32_t sq(Point p) { return uint32_t(p) * uint32_t(p); }
}  // namespace

// ---------------------------------------------------------------------------------------------
// setup

void Board::init(int size)
{
    size_ = size;
    stride_ = size + 2;
    num_points_ = stride_ * stride_;
    hash_ = 0;
    ko_ = NO_POINT;
    last_move_ = NO_POINT;
    last_color_ = EMPTY;
    captures_[0] = captures_[1] = captures_[2] = 0;
    stone_count_[0] = stone_count_[1] = stone_count_[2] = 0;
    empty_count_ = 0;
    last_captured_count_ = 0;
    mark_gen_ = 0;
    std::memset(mark_, 0, sizeof(mark_));
    for (int p = 0; p < num_points_; ++p) {
        color_[p] = BORDER;
        parent_[p] = uint16_t(p);
        next_stone_[p] = uint16_t(p);
        lib_count_[p] = 0;
        lib_sum_[p] = 0;
        lib_sq_sum_[p] = 0;
        group_size_[p] = 0;
        empty_index_[p] = -1;
    }
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            Point p = point(x, y);
            color_[p] = EMPTY;
            add_empty(p);
        }
}

void Board::copy_from(const Board& o)
{
    const int n = o.num_points_;
    size_ = o.size_;
    stride_ = o.stride_;
    num_points_ = n;
    hash_ = o.hash_;
    ko_ = o.ko_;
    last_move_ = o.last_move_;
    last_color_ = o.last_color_;
    std::memcpy(captures_, o.captures_, sizeof(captures_));
    std::memcpy(stone_count_, o.stone_count_, sizeof(stone_count_));
    empty_count_ = o.empty_count_;
    std::memcpy(color_, o.color_, size_t(n) * sizeof(color_[0]));
    std::memcpy(parent_, o.parent_, size_t(n) * sizeof(parent_[0]));
    std::memcpy(next_stone_, o.next_stone_, size_t(n) * sizeof(next_stone_[0]));
    std::memcpy(lib_count_, o.lib_count_, size_t(n) * sizeof(lib_count_[0]));
    std::memcpy(lib_sum_, o.lib_sum_, size_t(n) * sizeof(lib_sum_[0]));
    std::memcpy(lib_sq_sum_, o.lib_sq_sum_, size_t(n) * sizeof(lib_sq_sum_[0]));
    std::memcpy(group_size_, o.group_size_, size_t(n) * sizeof(group_size_[0]));
    std::memcpy(empties_, o.empties_, size_t(o.empty_count_) * sizeof(empties_[0]));
    std::memcpy(empty_index_, o.empty_index_, size_t(n) * sizeof(empty_index_[0]));
    // The capture list of the last move is tiny and part of the observable state: copy the used
    // prefix only. The liberty stamps stay private to each board.
    last_captured_count_ = o.last_captured_count_;
    if (last_captured_count_ > 0)
        std::memcpy(last_captured_, o.last_captured_, size_t(last_captured_count_) * sizeof(Point));
}

// ---------------------------------------------------------------------------------------------
// private helpers

Point Board::find(Point p)
{
    while (parent_[p] != p) {
        parent_[p] = parent_[parent_[p]];   // path halving
        p = Point(parent_[p]);
    }
    return p;
}

Point Board::find_const(Point p) const
{
    while (parent_[p] != p) p = Point(parent_[p]);
    return p;
}

void Board::merge(Point a, Point b)
{
    // a, b are distinct roots. Union by size; splice the circular stone lists.
    if (group_size_[a] < group_size_[b]) { Point t = a; a = b; b = t; }
    parent_[b] = uint16_t(a);
    group_size_[a] = uint16_t(group_size_[a] + group_size_[b]);
    lib_count_[a] = uint16_t(lib_count_[a] + lib_count_[b]);
    lib_sum_[a] += lib_sum_[b];
    lib_sq_sum_[a] += lib_sq_sum_[b];
    uint16_t t = next_stone_[a];
    next_stone_[a] = next_stone_[b];
    next_stone_[b] = t;
}

// Places a stone of colour c at empty point p: pseudo-liberties, neighbours' liberty loss, merges.
// No capture, ko or last-move bookkeeping.
void Board::add_stone_libs(Point p)
{
    const Color c = color_[p];
    const int s = stride_;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
    parent_[p] = uint16_t(p);
    next_stone_[p] = uint16_t(p);
    group_size_[p] = 1;
    lib_count_[p] = 0;
    lib_sum_[p] = 0;
    lib_sq_sum_[p] = 0;
    for (int i = 0; i < 4; ++i) {
        const Point n = nb[i];
        const Color cn = color_[n];
        if (cn == EMPTY) {
            ++lib_count_[p];
            lib_sum_[p] += uint32_t(n);
            lib_sq_sum_[p] += sq(n);
        } else if (cn != BORDER) {
            const Point r = find(n);
            --lib_count_[r];
            lib_sum_[r] -= uint32_t(p);
            lib_sq_sum_[r] -= sq(p);
        }
    }
    for (int i = 0; i < 4; ++i) {
        const Point n = nb[i];
        if (color_[n] != c) continue;
        const Point r = find(n);
        const Point rp = find(p);
        if (r != rp) merge(rp, r);
    }
}

int Board::remove_group(Point root)
{
    const Color c = color_[root];
    const Color opp = opponent(c);
    const int s = stride_;
    int n = 0;
    Point p = root;
    do {
        const Point next = Point(next_stone_[p]);
        color_[p] = EMPTY;
        hash_ ^= ZOBRIST.stone[c][p];
        add_empty(p);
        last_captured_[last_captured_count_++] = p;
        const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
        for (int i = 0; i < 4; ++i) {
            const Point q = nb[i];
            if (color_[q] != opp) continue;
            const Point r = find(q);
            ++lib_count_[r];
            lib_sum_[r] += uint32_t(p);
            lib_sq_sum_[r] += sq(p);
        }
        parent_[p] = uint16_t(p);
        next_stone_[p] = uint16_t(p);
        group_size_[p] = 0;
        lib_count_[p] = 0;
        lib_sum_[p] = 0;
        lib_sq_sum_[p] = 0;
        ++n;
        p = next;
    } while (p != root);
    stone_count_[c] = int16_t(stone_count_[c] - n);
    return n;
}

void Board::add_empty(Point p)
{
    empty_index_[p] = empty_count_;
    empties_[empty_count_++] = p;
}

void Board::remove_empty(Point p)
{
    const int i = empty_index_[p];
    const Point last = empties_[--empty_count_];
    empties_[i] = last;
    empty_index_[last] = int16_t(i);
    empty_index_[p] = -1;
}

// ---------------------------------------------------------------------------------------------
// legality

bool Board::is_legal(Point p, Color c) const
{
    if (p == PASS) return true;
    if (p < 0 || p >= num_points_) return false;
    if (color_[p] != EMPTY || p == ko_) return false;
    return !is_suicide(p, c);
}

bool Board::is_suicide(Point p, Color c) const
{
    const int s = stride_;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
    for (int i = 0; i < 4; ++i) {
        const Point n = nb[i];
        const Color cn = color_[n];
        if (cn == EMPTY) return false;
        if (cn == BORDER) continue;
        const bool atari = in_atari(n);
        if (cn == c) { if (!atari) return false; }   // keeps a liberty other than p
        else if (atari) return false;                // captures
    }
    return true;
}

bool Board::is_capture(Point p, Color c) const
{
    const Color opp = opponent(c);
    const int s = stride_;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
    for (int i = 0; i < 4; ++i)
        if (color_[nb[i]] == opp && in_atari(nb[i])) return true;
    return false;
}

bool Board::is_self_atari(Point p, Color c) const
{
    const Color opp = opponent(c);
    const int s = stride_;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};

    // Captured stones (distinct opponent groups in atari).
    int captured = 0;
    Point cap_roots[4];
    int cap_n = 0;
    for (int i = 0; i < 4; ++i) {
        const Point n = nb[i];
        if (color_[n] != opp || !in_atari(n)) continue;
        const Point r = find_const(n);
        bool seen = false;
        for (int k = 0; k < cap_n; ++k) if (cap_roots[k] == r) { seen = true; break; }
        if (seen) continue;
        cap_roots[cap_n++] = r;
        captured += group_size_[r];
    }
    if (captured >= 2) return false;

    // Distinct liberties of the merged group, p excluded, counted up to 2.
    const uint16_t gen = next_mark_gen();
    mark_[p] = gen;
    int count = 0;
    Point seen_roots[4];
    int seen_n = 0;
    for (int i = 0; i < 4; ++i) {
        const Point n = nb[i];
        const Color cn = color_[n];
        if (cn == EMPTY) {
            if (mark_[n] == gen) continue;
            mark_[n] = gen;
            if (++count >= 2) return false;
        } else if (cn == c) {
            const Point r = find_const(n);
            bool seen = false;
            for (int k = 0; k < seen_n; ++k) if (seen_roots[k] == r) { seen = true; break; }
            if (seen) continue;
            seen_roots[seen_n++] = r;
            if (in_atari(n)) continue;   // its only liberty is p
            Point q = r;
            do {
                const Point qn[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
                for (int k = 0; k < 4; ++k) {
                    const Point e = qn[k];
                    if (color_[e] != EMPTY || mark_[e] == gen) continue;
                    mark_[e] = gen;
                    if (++count >= 2) return false;
                }
                q = Point(next_stone_[q]);
            } while (q != r);
        }
    }
    count += captured;   // a single captured stone becomes a liberty (snapback shape)
    return count == 1;
}

bool Board::is_eye_like(Point p, Color c) const
{
    const Color opp = opponent(c);
    const int s = stride_;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
    for (int i = 0; i < 4; ++i) {
        const Color cn = color_[nb[i]];
        if (cn != c && cn != BORDER) return false;
    }
    const Point dg[4] = {Point(p - s - 1), Point(p - s + 1), Point(p + s - 1), Point(p + s + 1)};
    int opp_count = 0, border = 0;
    for (int i = 0; i < 4; ++i) {
        const Color cd = color_[dg[i]];
        if (cd == BORDER) ++border;
        else if (cd == opp) ++opp_count;
    }
    if (border > 0) return opp_count == 0;
    return opp_count <= 1;
}

// ---------------------------------------------------------------------------------------------
// play

int Board::play(Point p, Color c)
{
    const Color opp = opponent(c);
    const int s = stride_;
    color_[p] = c;
    remove_empty(p);
    hash_ ^= ZOBRIST.stone[c][p];
    ++stone_count_[c];
    add_stone_libs(p);

    last_captured_count_ = 0;
    int captured = 0;
    const Point nb[4] = {Point(p - s), Point(p - 1), Point(p + 1), Point(p + s)};
    for (int i = 0; i < 4; ++i) {
        const Point n = nb[i];
        if (color_[n] != opp) continue;
        const Point r = find(n);
        if (lib_count_[r] == 0) captured += remove_group(r);
    }
    captures_[c] = int16_t(captures_[c] + captured);

    const Point root = find(p);
    if (captured == 1 && group_size_[root] == 1 && lib_count_[root] == 1) ko_ = last_captured_[0];
    else ko_ = NO_POINT;
    last_move_ = p;
    last_color_ = c;
    return captured;
}

void Board::play_pass(Color c)
{
    ko_ = NO_POINT;
    last_move_ = PASS;
    last_color_ = c;
    last_captured_count_ = 0;
}

void Board::set_stone(Point p, Color c)
{
    if (p < 0 || p >= num_points_ || color_[p] != EMPTY) return;   // asserted precondition
    color_[p] = c;
    remove_empty(p);
    hash_ ^= ZOBRIST.stone[c][p];
    ++stone_count_[c];
    add_stone_libs(p);
}

void Board::remove_stone(Point p)
{
    if (p < 0 || p >= num_points_) return;
    const Color c = color_[p];
    if (c != BLACK && c != WHITE) return;
    const Color opp = opponent(c);
    const int s = stride_;
    // Union-find cannot delete: lift the whole group, then put back every stone but p.
    Point stones[MAX_SIZE * MAX_SIZE];
    const int n = group_stones(p, stones, MAX_SIZE * MAX_SIZE);
    for (int i = 0; i < n; ++i) {
        const Point q = stones[i];
        color_[q] = EMPTY;
        hash_ ^= ZOBRIST.stone[c][q];
        add_empty(q);
        const Point nb[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
        for (int k = 0; k < 4; ++k) {
            const Point o = nb[k];
            if (color_[o] != opp) continue;
            const Point r = find(o);
            ++lib_count_[r];
            lib_sum_[r] += uint32_t(q);
            lib_sq_sum_[r] += sq(q);
        }
        parent_[q] = uint16_t(q);
        next_stone_[q] = uint16_t(q);
        group_size_[q] = 0;
        lib_count_[q] = 0;
        lib_sum_[q] = 0;
        lib_sq_sum_[q] = 0;
    }
    stone_count_[c] = int16_t(stone_count_[c] - n);
    for (int i = 0; i < n; ++i) {
        const Point q = stones[i];
        if (q == p) continue;
        color_[q] = c;
        remove_empty(q);
        hash_ ^= ZOBRIST.stone[c][q];
        ++stone_count_[c];
        add_stone_libs(q);
    }
}

// ---------------------------------------------------------------------------------------------
// groups

Point Board::group_root(Point p) const { return find_const(p); }

int Board::group_size(Point p) const { return group_size_[find_const(p)]; }

int Board::pseudo_liberties(Point p) const { return lib_count_[find_const(p)]; }

bool Board::in_atari(Point p) const
{
    const Point r = find_const(p);
    const uint32_t n = lib_count_[r];
    // All pseudo-liberties coincide iff sq_sum * n == sum^2 (Cauchy-Schwarz); a point has at most
    // four neighbours, so n <= 4 keeps the products exact in 32 bits.
    return n > 0 && n <= 4 && lib_sq_sum_[r] * n == lib_sum_[r] * lib_sum_[r];
}

Point Board::atari_point(Point p) const
{
    const Point r = find_const(p);
    const uint32_t n = lib_count_[r];
    if (n == 0 || n > 4 || lib_sq_sum_[r] * n != lib_sum_[r] * lib_sum_[r]) return NO_POINT;
    return Point(lib_sum_[r] / n);
}

int Board::liberties(Point p, Point* out, int max_out, int cap) const
{
    const uint16_t gen = next_mark_gen();
    const int s = stride_;
    int count = 0;
    Point q = p;
    do {
        const Point nb[4] = {Point(q - s), Point(q - 1), Point(q + 1), Point(q + s)};
        for (int k = 0; k < 4; ++k) {
            const Point e = nb[k];
            if (color_[e] != EMPTY || mark_[e] == gen) continue;
            mark_[e] = gen;
            if (out != nullptr && count < max_out) out[count] = e;
            ++count;
            if (cap > 0 && count >= cap) return count;
        }
        q = Point(next_stone_[q]);
    } while (q != p);
    return count;
}

int Board::group_stones(Point p, Point* out, int max_out) const
{
    int count = 0;
    Point q = p;
    do {
        if (count < max_out) out[count] = q;
        ++count;
        q = Point(next_stone_[q]);
    } while (q != p);
    return count;
}

uint16_t Board::next_mark_gen() const
{
    if (++mark_gen_ == 0) {
        std::memset(mark_, 0, sizeof(mark_));
        mark_gen_ = 1;
    }
    return mark_gen_;
}

}  // namespace go
