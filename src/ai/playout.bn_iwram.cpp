// Light (Mogo-style) playouts. The file name puts this code in IWRAM as ARM instructions: it is the
// hottest loop in the game, run thousands of times per AI move.
#include "ai/internal.h"

namespace ai {
namespace detail {

// devkitARM's linker script keeps `.iwram` sections, and excludes only files whose name contains
// ".iwram" from ROM .text - "playout.bn_iwram.o" does not match that, so the section attribute is
// what actually moves this code into IWRAM. Verified with arm-none-eabi-nm: these symbols must
// live at 0x03xxxxxx.
#define MOKU_IWRAM __attribute__((section(".iwram")))

using go::BLACK;
using go::EMPTY;
using go::PASS;
using go::WHITE;

namespace {

// Moves that are sane in a playout: legal, not filling our own eye, not a multi-stone self-atari.
MOKU_IWRAM bool playable(const Board& b, Point p, Color c)
{
    return b.is_legal(p, c) && !b.is_eye_like(p, c) && !is_bad_self_atari(b, p, c);
}

// Our groups next to `last` that the opponent just put in atari, and the ways out.
int escape_moves(const Board& b, Point last, Color c, Point* out, int max_out)
{
    int count = 0;
    Point roots[4];
    const int n = adjacent_atari_groups(b, last, c, roots, 4);
    for(int i = 0; i < n && count < max_out; ++i)
    {
        const Point esc = b.atari_point(roots[i]);
        if(esc >= 0 && b.is_legal(esc, c) && !b.is_self_atari(esc, c)) out[count++] = esc;
    }
    // capturing the stone that did the atari is just as good a way out
    if(count < max_out && b.at(last) == opponent(c) && b.in_atari(last))
    {
        const Point cap = b.atari_point(last);
        if(cap >= 0 && b.is_legal(cap, c)) out[count++] = cap;
    }
    return count;
}

}  // namespace

MOKU_IWRAM Point playout_move(const Board& b, Color c, Rng& rng)
{
    const Tuning& t = tuning();
    const Point last = b.last_move();

    if(last >= 0)
    {
        // 1. answer an atari created by the last move
        Point esc[6];
        const int en = escape_moves(b, last, c, esc, 6);
        if(en > 0 && int(rng.below(1000)) < t.playout_atari_permille) return esc[rng.below(uint32_t(en))];

        // 2. a shape move next to the last stone played
        const int s = b.stride();
        const Point nb[8] = {
            Point(last - s - 1), Point(last - s), Point(last - s + 1), Point(last + 1),
            Point(last + s + 1), Point(last + s), Point(last + s - 1), Point(last - 1)
        };
        Point cand[8];
        int weight[8];
        int n = 0, total = 0;
        for(int i = 0; i < 8; ++i)
        {
            const Point p = nb[i];
            if(b.at(p) != EMPTY) continue;
            const int w = int(pattern_weight(b, p, c));
            if(w < t.playout_pattern_min) continue;
            if(!playable(b, p, c)) continue;
            cand[n] = p;
            weight[n] = w;
            total += w;
            ++n;
        }
        if(n > 0)
        {
            int pick = int(rng.below(uint32_t(total)));
            for(int i = 0; i < n; ++i)
            {
                pick -= weight[i];
                if(pick < 0) return cand[i];
            }
            return cand[n - 1];
        }
    }

    // 3. uniform random over the empty points
    const int n = b.empty_count();
    if(n == 0) return PASS;
    for(int tries = 0; tries < 6; ++tries)
    {
        const Point p = b.empty_at(int(rng.below(uint32_t(n))));
        if(playable(b, p, c)) return p;
    }
    const int start = int(rng.below(uint32_t(n)));
    const int scan = n < tuning().playout_scan_limit ? n : tuning().playout_scan_limit;
    for(int i = 0; i < scan; ++i)
    {
        const Point p = b.empty_at(start + i < n ? start + i : start + i - n);
        if(playable(b, p, c)) return p;
    }
    return PASS;                          // passing with dame left costs nothing: the fast area
                                          // score only counts points surrounded by one colour
}

MOKU_IWRAM int playout_score_x2(const Board& b, int komi_x2)
{
    const int s = b.stride();
    int black = 0, white = 0;
    for(int y = 0, size = b.size(); y < size; ++y)
    {
        for(int x = 0; x < size; ++x)
        {
            const Point p = b.point(x, y);
            const Color v = b.at(p);
            if(v == BLACK) { ++black; continue; }
            if(v == WHITE) { ++white; continue; }
            bool hb = false, hw = false;
            const Point nb[4] = { Point(p - s), Point(p - 1), Point(p + 1), Point(p + s) };
            for(int i = 0; i < 4; ++i)
            {
                const Color nv = b.at(nb[i]);
                if(nv == BLACK) hb = true;
                else if(nv == WHITE) hw = true;
            }
            if(hb && !hw) ++black;
            else if(hw && !hb) ++white;
        }
    }
    return (black - white) * 2 - komi_x2;
}

void playout_begin(PlayoutState& st, const Board& b, Color to_move, int start_index)
{
    st.to_move = to_move;
    st.passes = 0;
    st.played = 0;
    st.limit = int16_t(b.size() * b.size() * 2 + 24);
    st.idx = int16_t(start_index);
    st.finished = false;
}

MOKU_IWRAM bool playout_advance(PlayoutState& st, Board& b, Rng& rng, PlayoutRecord* rec, int max_moves)
{
    for(int i = 0; i < max_moves; ++i)
    {
        if(st.finished || st.played >= st.limit || st.passes >= 2)
        {
            st.finished = true;
            if(rec) rec->moves = st.idx;
            return true;
        }

        const Point m = playout_move(b, st.to_move, rng);

        if(m == PASS)
        {
            b.play_pass(st.to_move);
            ++st.passes;
        }
        else
        {
            if(rec && rec->first_idx)
            {
                uint16_t& slot = rec->first_idx[m * 2 + (st.to_move - 1)];
                if(slot == 0xFFFF) slot = uint16_t(st.idx);
            }

            b.play(m, st.to_move);
            st.passes = 0;
        }

        st.to_move = opponent(st.to_move);
        ++st.played;
        ++st.idx;
    }

    return false;
}

int run_playout(Board& b, Color to_move, Rng& rng, int komi_x2, PlayoutRecord* rec)
{
    PlayoutState st;
    playout_begin(st, b, to_move, rec ? rec->moves : 0);
    while(! playout_advance(st, b, rng, rec, 64)) {}
    return playout_score_x2(b, komi_x2);
}

}  // namespace detail
}  // namespace ai
