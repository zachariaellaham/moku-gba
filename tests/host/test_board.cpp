// Unit tests for go::Board - the incremental board (groups, liberties, captures, ko, hash).
#include "doctest.h"
#include "go_fixture.h"

using namespace go;

namespace {
Point P(const Board& b, int x, int y) { return b.point(x, y); }
}  // namespace

TEST_CASE("board geometry and init")
{
    Board b;
    for (int size : {7, 9, 13, 19}) {
        b.init(size);
        CHECK(b.size() == size);
        CHECK(b.stride() == size + 2);
        CHECK(b.num_points() == (size + 2) * (size + 2));
        CHECK(b.empty_count() == size * size);
        CHECK(b.hash() == 0u);
        CHECK(b.stones(BLACK) == 0);
        CHECK(b.stones(WHITE) == 0);
        CHECK(b.captures(BLACK) == 0);
        CHECK(b.captures(WHITE) == 0);
        CHECK(b.last_move() == NO_POINT);
        CHECK(b.last_color() == EMPTY);
        CHECK(b.ko_point() == NO_POINT);
        CHECK(b.last_captured_count() == 0);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                const Point p = b.point(x, y);
                CHECK(b.on_board(p));
                CHECK(b.at(p) == EMPTY);
                CHECK(b.x_of(p) == x);
                CHECK(b.y_of(p) == y);
            }
        // the padding ring around the board is BORDER on all four sides
        CHECK(b.at(Point(b.point(0, 0) - 1)) == BORDER);
        CHECK(b.at(Point(b.point(size - 1, 0) + 1)) == BORDER);
        CHECK(b.at(Point(b.point(0, 0) + b.up_offset())) == BORDER);
        CHECK(b.at(Point(b.point(0, size - 1) - b.up_offset())) == BORDER);
        CHECK(!b.on_board(Point(b.point(0, 0) - 1)));
        CHECK(b.up_offset() == -(size + 2));
        CHECK(fx::empties_consistent(b));
    }
}

TEST_CASE("placing stones updates colour, counts, last move and hash")
{
    Board b;
    b.init(9);
    const Point p = P(b, 4, 4);
    CHECK(b.play(p, BLACK) == 0);
    CHECK(b.at(p) == BLACK);
    CHECK(b.at(4, 4) == BLACK);
    CHECK(b.stones(BLACK) == 1);
    CHECK(b.stones(WHITE) == 0);
    CHECK(b.empty_count() == 80);
    CHECK(b.last_move() == p);
    CHECK(b.last_color() == BLACK);
    CHECK(b.hash() != 0u);
    CHECK(b.liberties(p) == 4);
    CHECK(b.group_size(p) == 1);
    CHECK(b.pseudo_liberties(p) == 4);
    CHECK(!b.in_atari(p));
    CHECK(b.atari_point(p) == NO_POINT);
    CHECK(b.group_root(p) == p);
    CHECK(fx::empties_consistent(b));
    CHECK(fx::groups_consistent(b));

    b.play(P(b, 4, 5), BLACK);
    CHECK(b.group_size(p) == 2);
    CHECK(b.liberties(p) == 6);
    CHECK(b.group_root(p) == b.group_root(P(b, 4, 5)));
    Point stones[4];
    CHECK(b.group_stones(p, stones, 4) == 2);
    CHECK(b.next_stone(stones[0]) == stones[1]);
    CHECK(b.next_stone(stones[1]) == stones[0]);
    CHECK(((stones[0] == p && stones[1] == P(b, 4, 5)) ||
           (stones[1] == p && stones[0] == P(b, 4, 5))));
}

TEST_CASE("single stone capture")
{
    Board b;
    fx::set_board(b, {
        ".........",
        "....X....",
        "...XOX...",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    const Point w = P(b, 4, 2);
    CHECK(b.at(w) == WHITE);
    CHECK(b.liberties(w) == 1);
    CHECK(b.in_atari(w));
    CHECK(b.atari_point(w) == P(b, 4, 3));
    CHECK(b.is_capture(P(b, 4, 3), BLACK));
    CHECK(!b.is_capture(P(b, 4, 3), WHITE));
    CHECK(b.play(P(b, 4, 3), BLACK) == 1);
    CHECK(b.at(w) == EMPTY);
    CHECK(b.captures(BLACK) == 1);
    CHECK(b.captures(WHITE) == 0);
    CHECK(b.stones(WHITE) == 0);
    CHECK(b.stones(BLACK) == 4);
    CHECK(b.last_captured_count() == 1);
    CHECK(b.last_captured()[0] == w);
    CHECK(b.empty_count() == 81 - 4);
    CHECK(b.ko_point() == NO_POINT);      // the capturing stone has more than one liberty
    CHECK(fx::empties_consistent(b));
    CHECK(fx::groups_consistent(b));
    CHECK(fx::stone_counts_consistent(b));
    CHECK(b.hash() == fx::rebuilt_hash(b));
}

TEST_CASE("multi stone captures, corner and edge")
{
    Board b;
    SUBCASE("three stone chain")
    {
        fx::set_board(b, {
            "..XXX....",
            ".XOOO....",
            "..XX.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point w = P(b, 2, 1);
        CHECK(b.group_size(w) == 3);
        CHECK(b.liberties(w) == 2);                       // (5,1) and (4,2)
        CHECK(b.play(P(b, 5, 1), BLACK) == 0);
        CHECK(b.in_atari(w));
        CHECK(b.atari_point(w) == P(b, 4, 2));
        CHECK(b.play(P(b, 4, 2), BLACK) == 3);
        CHECK(b.captures(BLACK) == 3);
        CHECK(b.at(P(b, 2, 1)) == EMPTY);
        CHECK(b.at(P(b, 3, 1)) == EMPTY);
        CHECK(b.at(P(b, 4, 1)) == EMPTY);
        CHECK(b.last_captured_count() == 3);
        CHECK(b.ko_point() == NO_POINT);
        CHECK(fx::empties_consistent(b));
        CHECK(fx::groups_consistent(b));
        CHECK(b.hash() == fx::rebuilt_hash(b));
    }
    SUBCASE("corner stone")
    {
        fx::set_board(b, {
            "O........",
            "X........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point w = P(b, 0, 0);
        CHECK(b.liberties(w) == 1);
        CHECK(b.in_atari(w));
        CHECK(b.atari_point(w) == P(b, 1, 0));
        CHECK(b.play(P(b, 1, 0), BLACK) == 1);
        CHECK(b.at(w) == EMPTY);
        CHECK(fx::empties_consistent(b));
        CHECK(fx::groups_consistent(b));
    }
    SUBCASE("edge chain")
    {
        fx::set_board(b, {
            ".XOOX....",
            "..X......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point w = P(b, 2, 0);
        CHECK(b.group_size(w) == 2);
        CHECK(b.liberties(w) == 1);                       // only (3,1)
        CHECK(b.in_atari(w));
        CHECK(b.play(P(b, 3, 1), BLACK) == 2);
        CHECK(b.at(P(b, 2, 0)) == EMPTY);
        CHECK(b.at(P(b, 3, 0)) == EMPTY);
        CHECK(fx::empties_consistent(b));
        CHECK(fx::groups_consistent(b));
    }
    SUBCASE("one move captures two separate groups")
    {
        fx::set_board(b, {
            ".X.X.....",
            "XO.OX....",
            ".X.X.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point a = P(b, 1, 1), c = P(b, 3, 1);
        CHECK(b.in_atari(a));
        CHECK(b.in_atari(c));
        CHECK(b.atari_point(a) == P(b, 2, 1));
        CHECK(b.atari_point(c) == P(b, 2, 1));
        CHECK(b.play(P(b, 2, 1), BLACK) == 2);
        CHECK(b.at(a) == EMPTY);
        CHECK(b.at(c) == EMPTY);
        CHECK(b.captures(BLACK) == 2);
        CHECK(b.last_captured_count() == 2);
        CHECK(b.ko_point() == NO_POINT);                  // two stones taken: never a ko
        CHECK(fx::empties_consistent(b));
        CHECK(fx::groups_consistent(b));
    }
    SUBCASE("four stone block")
    {
        fx::set_board(b, {
            ".XX......",
            "XOOX.....",
            "XOOX.....",
            ".X.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point w = P(b, 1, 1);
        CHECK(b.group_size(w) == 4);
        CHECK(b.liberties(w) == 1);
        CHECK(b.atari_point(w) == P(b, 2, 3));
        CHECK(b.play(P(b, 2, 3), BLACK) == 4);
        CHECK(b.captures(BLACK) == 4);
        CHECK(b.ko_point() == NO_POINT);
        CHECK(fx::groups_consistent(b));
        CHECK(fx::empties_consistent(b));
        CHECK(b.hash() == fx::rebuilt_hash(b));
    }
}

TEST_CASE("suicide is illegal, capture into suicide is legal")
{
    Board b;
    SUBCASE("single stone suicide")
    {
        fx::set_board(b, {
            ".O.......",
            "O.O......",
            ".O.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point p = P(b, 1, 1);
        CHECK(b.is_suicide(p, BLACK));
        CHECK(!b.is_legal(p, BLACK));
        CHECK(fx::ref_suicide(b, p, BLACK));
        CHECK(!b.is_suicide(p, WHITE));       // white just connects
        CHECK(b.is_legal(p, WHITE));
    }
    SUBCASE("two stone suicide")
    {
        fx::set_board(b, {
            "..OO.....",
            ".OX.O....",
            "..OO.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point x = P(b, 2, 1);
        CHECK(b.liberties(x) == 1);
        CHECK(b.atari_point(x) == P(b, 3, 1));
        CHECK(b.is_suicide(P(b, 3, 1), BLACK));           // the pair would have no liberty
        CHECK(!b.is_legal(P(b, 3, 1), BLACK));
        CHECK(fx::ref_suicide(b, P(b, 3, 1), BLACK));
        CHECK(!b.is_suicide(P(b, 3, 1), WHITE));          // white captures the black stone instead
        CHECK(b.play(P(b, 3, 1), WHITE) == 1);
        CHECK(b.at(x) == EMPTY);
        CHECK(b.captures(WHITE) == 1);
        CHECK(fx::groups_consistent(b));
    }
    SUBCASE("capturing removes the suicide")
    {
        fx::set_board(b, {
            ".XOX.....",
            ".O.O.....",
            "..O......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        const Point sacrifice = P(b, 2, 0);
        const Point target = P(b, 2, 1);
        CHECK(b.at(sacrifice) == WHITE);
        CHECK(b.liberties(sacrifice) == 1);               // only (2,1)
        CHECK(b.atari_point(sacrifice) == target);
        CHECK(!b.is_suicide(target, BLACK));              // it captures the stone above
        CHECK(b.is_legal(target, BLACK));
        CHECK(b.is_self_atari(target, BLACK));            // ... and then has one liberty
        CHECK(b.play(target, BLACK) == 1);
        CHECK(b.at(sacrifice) == EMPTY);
        CHECK(b.in_atari(target));
        CHECK(b.ko_point() == sacrifice);                 // single stone taking a single stone: ko
        CHECK(fx::groups_consistent(b));
        CHECK(fx::empties_consistent(b));
    }
}

TEST_CASE("simple ko")
{
    Board b;
    fx::set_board(b, {
        ".........",
        "...XO....",
        "..X.XO...",
        "...XO....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    const Point bx = P(b, 4, 2);      // black stone in the ko, one liberty at (3,2)
    const Point ko = P(b, 3, 2);
    CHECK(b.at(bx) == BLACK);
    CHECK(b.in_atari(bx));
    CHECK(b.atari_point(bx) == ko);
    CHECK(b.play(ko, WHITE) == 1);
    CHECK(b.at(bx) == EMPTY);
    CHECK(b.ko_point() == bx);
    CHECK(!b.is_legal(bx, BLACK));                     // immediate recapture is forbidden
    CHECK(!b.is_legal(bx, WHITE));                     // Board bans the ko point for both colours:
                                                       // only the side to move ever asks (see game.h)
    CHECK(b.is_legal(P(b, 7, 7), BLACK));
    b.play(P(b, 7, 7), BLACK);
    CHECK(b.ko_point() == NO_POINT);                   // the ban lasts exactly one move
    b.play(P(b, 0, 8), WHITE);
    CHECK(b.is_legal(bx, BLACK));
    CHECK(b.play(bx, BLACK) == 1);
    CHECK(b.ko_point() == ko);
    CHECK(fx::groups_consistent(b));
    CHECK(fx::empties_consistent(b));
    CHECK(b.hash() == fx::rebuilt_hash(b));
}

TEST_CASE("a pass clears the ko point and the capture list")
{
    Board b;
    fx::set_board(b, {
        ".........",
        "...XO....",
        "..X.XO...",
        "...XO....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    b.play(P(b, 3, 2), WHITE);
    CHECK(b.ko_point() != NO_POINT);
    CHECK(b.last_captured_count() == 1);
    b.play_pass(BLACK);
    CHECK(b.ko_point() == NO_POINT);
    CHECK(b.last_move() == PASS);
    CHECK(b.last_color() == BLACK);
    CHECK(b.last_captured_count() == 0);
}

TEST_CASE("snapback: sacrifice one stone, take five")
{
    Board b;
    fx::set_board(b, {
        "..OX.....",
        "OOOX.....",
        "XXX......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    const Point a = P(b, 0, 0), bpt = P(b, 1, 0);
    const Point wgrp = P(b, 0, 1);
    CHECK(b.group_size(wgrp) == 4);
    CHECK(b.liberties(wgrp) == 2);                      // exactly (0,0) and (1,0)

    CHECK(b.is_self_atari(a, BLACK));                   // the sacrifice stone has one liberty
    CHECK(!b.is_capture(a, BLACK));
    CHECK(b.is_legal(a, BLACK));
    CHECK(b.play(a, BLACK) == 0);
    CHECK(b.in_atari(a));
    CHECK(b.in_atari(wgrp));
    CHECK(b.atari_point(wgrp) == bpt);

    // White takes the stone; its group is then in atari on the freed point (that is the snapback).
    CHECK(b.is_self_atari(bpt, WHITE));                 // captures one, keeps one liberty
    CHECK(b.play(bpt, WHITE) == 1);
    CHECK(b.at(a) == EMPTY);
    CHECK(b.captures(WHITE) == 1);
    CHECK(b.ko_point() == NO_POINT);                    // the capturing group is not a single stone
    CHECK(b.in_atari(bpt));
    CHECK(b.atari_point(bpt) == a);
    CHECK(b.group_size(bpt) == 5);

    CHECK(b.is_legal(a, BLACK));
    CHECK(b.play(a, BLACK) == 5);
    CHECK(b.captures(BLACK) == 5);
    CHECK(b.stones(WHITE) == 0);
    CHECK(fx::groups_consistent(b));
    CHECK(fx::empties_consistent(b));
    CHECK(b.hash() == fx::rebuilt_hash(b));
}

TEST_CASE("hash: incremental play equals a rebuilt position")
{
    Rng rng(12345);
    for (int game = 0; game < 50; ++game) {
        Board b;
        b.init(9);
        fx::random_game(b, rng, 120);
        CHECK(b.hash() == fx::rebuilt_hash(b));
    }
    SUBCASE("after captures")
    {
        Board b;
        fx::set_board(b, {
            ".........",
            "....X....",
            "...XOX...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        b.play(P(b, 4, 3), BLACK);
        CHECK(b.hash() == fx::rebuilt_hash(b));
    }
    SUBCASE("colour and emptiness")
    {
        Board b1, b2;
        b1.init(9);
        b2.init(9);
        b1.set_stone(P(b1, 3, 3), BLACK);
        b2.set_stone(P(b2, 3, 3), WHITE);
        CHECK(b1.hash() != b2.hash());
        CHECK(b1.hash() != 0u);
    }
    SUBCASE("order independence")
    {
        Board b3, b4;
        b3.init(9);
        b4.init(9);
        b3.play(P(b3, 0, 0), BLACK);
        b3.play(P(b3, 8, 8), WHITE);
        b4.play(P(b4, 8, 8), WHITE);
        b4.play(P(b4, 0, 0), BLACK);
        CHECK(b3.hash() == b4.hash());
    }
    SUBCASE("removing every stone returns to zero")
    {
        Board b5;
        b5.init(9);
        b5.set_stone(P(b5, 2, 2), BLACK);
        b5.set_stone(P(b5, 5, 5), WHITE);
        CHECK(b5.hash() != 0u);
        b5.remove_stone(P(b5, 2, 2));
        b5.remove_stone(P(b5, 5, 5));
        CHECK(b5.hash() == 0u);
    }
}

TEST_CASE("copy_from reproduces behaviour exactly")
{
    Rng rng(777);
    for (int game = 0; game < 40; ++game) {
        Board a;
        a.init(game % 2 ? 13 : 9);
        fx::random_game(a, rng, 60);
        Board c;
        c.copy_from(a);
        CHECK(fx::same_position(a, c));
        CHECK(c.size() == a.size());
        CHECK(c.empty_count() == a.empty_count());
        CHECK(c.captures(BLACK) == a.captures(BLACK));
        CHECK(c.captures(WHITE) == a.captures(WHITE));
        CHECK(c.stones(BLACK) == a.stones(BLACK));
        CHECK(c.ko_point() == a.ko_point());
        CHECK(c.last_move() == a.last_move());
        CHECK(c.last_color() == a.last_color());
        CHECK(c.last_captured_count() == a.last_captured_count());
        CHECK(fx::empties_consistent(c));
        CHECK(fx::groups_consistent(c));

        // identical random continuations must end in identical positions
        Rng r1(uint32_t(game) + 1), r2(uint32_t(game) + 1);
        fx::random_game(a, r1, 40);
        fx::random_game(c, r2, 40);
        CHECK(fx::same_position(a, c));
        CHECK(a.captures(BLACK) == c.captures(BLACK));
        CHECK(a.captures(WHITE) == c.captures(WHITE));

        // and the copy is independent of the original
        Board d;
        d.copy_from(a);
        const uint64_t h = a.hash();
        for (int i = 0; i < d.empty_count(); ++i) {
            const Point p = d.empty_at(i);
            if (d.is_legal(p, BLACK)) { d.play(p, BLACK); break; }
        }
        CHECK(a.hash() == h);
        CHECK(fx::empties_consistent(a));
    }
}

TEST_CASE("liberties: out buffer, cap and exactness")
{
    Board b;
    fx::set_board(b, {
        ".........",
        "....X....",
        "...XXX...",
        "....X....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    const Point p = P(b, 4, 2);
    CHECK(b.group_size(p) == 5);
    CHECK(b.liberties(p) == 8);
    CHECK(fx::ref_liberties(b, p) == 8);
    Point out[8];
    CHECK(b.liberties(p, out, 8) == 8);
    for (int i = 0; i < 8; ++i) {
        CHECK(b.at(out[i]) == EMPTY);
        for (int j = i + 1; j < 8; ++j) CHECK(out[i] != out[j]);
    }
    CHECK(b.liberties(p, nullptr, 0, 3) == 3);        // cap stops the count early
    Point two[2];
    CHECK(b.liberties(p, two, 2, 2) == 2);
    CHECK(b.pseudo_liberties(p) >= 8);
    // every group member reports the same group
    CHECK(b.liberties(P(b, 3, 2)) == 8);
    CHECK(b.group_root(P(b, 3, 2)) == b.group_root(P(b, 5, 2)));
}

TEST_CASE("is_self_atari")
{
    Board b;
    SUBCASE("plain self atari and safe moves")
    {
        fx::set_board(b, {
            ".O.......",
            "O..O.....",
            ".O.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_self_atari(P(b, 1, 1), BLACK));       // only (2,1) would be left
        CHECK(!b.is_self_atari(P(b, 5, 5), BLACK));      // four liberties
        CHECK(!b.is_self_atari(P(b, 2, 1), BLACK));      // three liberties
    }
    SUBCASE("connecting to a group with liberties is not self atari")
    {
        fx::set_board(b, {
            ".........",
            "..XX.....",
            "..O......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(!b.is_self_atari(P(b, 4, 1), BLACK));
    }
    SUBCASE("filling the last liberty of a friendly group is self atari")
    {
        fx::set_board(b, {
            "OOO......",
            "XX.O.....",
            "OO.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        // black pair (0,1)(1,1) has a single liberty at (2,1); extending there keeps one liberty
        CHECK(b.in_atari(P(b, 0, 1)));
        CHECK(b.is_self_atari(P(b, 2, 1), BLACK));
        CHECK(b.liberties(P(b, 0, 1)) == 1);
    }
    SUBCASE("a capture of two stones is not self atari")
    {
        fx::set_board(b, {
            "XX.......",
            "OO.......",
            "XX.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        // white pair (0,1)(1,1) has one liberty at (2,1); black there captures two stones
        CHECK(b.in_atari(P(b, 0, 1)));
        CHECK(b.atari_point(P(b, 0, 1)) == P(b, 2, 1));
        CHECK(b.is_capture(P(b, 2, 1), BLACK));
        CHECK(!b.is_self_atari(P(b, 2, 1), BLACK));
    }
    SUBCASE("snapback shape: capture one stone and keep one liberty")
    {
        fx::set_board(b, {
            ".XOX.....",
            ".O.O.....",
            "..O......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_capture(P(b, 2, 1), BLACK));
        CHECK(b.is_self_atari(P(b, 2, 1), BLACK));
    }
}

TEST_CASE("is_eye_like")
{
    Board b;
    SUBCASE("true eye in the centre")
    {
        fx::set_board(b, {
            ".........",
            "...XXX...",
            "...X.X...",
            "...XXX...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_eye_like(P(b, 4, 2), BLACK));
        CHECK(!b.is_eye_like(P(b, 4, 2), WHITE));
    }
    SUBCASE("one opponent diagonal in the centre is still eye like")
    {
        fx::set_board(b, {
            ".........",
            "...XXO...",
            "...X.X...",
            "...XXX...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_eye_like(P(b, 4, 2), BLACK));
    }
    SUBCASE("false eye: two opponent diagonals")
    {
        fx::set_board(b, {
            ".........",
            "...XXO...",
            "...X.X...",
            "...OXX...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(!b.is_eye_like(P(b, 4, 2), BLACK));
    }
    SUBCASE("corner and edge eyes")
    {
        fx::set_board(b, {
            ".X.......",
            "XX.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_eye_like(P(b, 0, 0), BLACK));
        fx::set_board(b, {
            ".X.......",
            "XO.......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(!b.is_eye_like(P(b, 0, 0), BLACK));     // the only diagonal is white
        fx::set_board(b, {
            "..X......",
            ".X.X.....",
            "..X......",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_eye_like(P(b, 2, 1), BLACK));
        CHECK(!b.is_eye_like(P(b, 2, 1), WHITE));
        fx::set_board(b, {
            "..X......",
            ".X.X.....",
            "..XO.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_eye_like(P(b, 2, 1), BLACK));      // a single opponent diagonal is allowed
        fx::set_board(b, {
            "..XO.....",
            ".X.X.....",
            "..XO.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(!b.is_eye_like(P(b, 2, 1), BLACK));     // two white diagonals: false eye
    }
    SUBCASE("an empty or occupied neighbour disqualifies")
    {
        b.init(9);
        CHECK(!b.is_eye_like(P(b, 4, 4), BLACK));
        fx::set_board(b, {
            ".........",
            "...XXX...",
            "...X.X...",
            "...XX....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.is_eye_like(P(b, 4, 2), BLACK));      // still surrounded on all four sides
        fx::set_board(b, {
            ".........",
            "...XXX...",
            "...X.X...",
            "...X.X...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(!b.is_eye_like(P(b, 4, 2), BLACK));     // (4,3) is empty
    }
}

TEST_CASE("set_stone and remove_stone")
{
    Board b;
    b.init(9);
    b.set_stone(P(b, 3, 3), BLACK);
    b.set_stone(P(b, 3, 4), BLACK);
    b.set_stone(P(b, 4, 3), WHITE);
    CHECK(b.stones(BLACK) == 2);
    CHECK(b.stones(WHITE) == 1);
    CHECK(b.group_size(P(b, 3, 3)) == 2);
    CHECK(b.liberties(P(b, 4, 3)) == 3);
    CHECK(b.ko_point() == NO_POINT);
    CHECK(b.last_move() == NO_POINT);            // setup stones are not moves
    CHECK(fx::empties_consistent(b));
    CHECK(fx::groups_consistent(b));

    b.remove_stone(P(b, 3, 3));
    CHECK(b.at(P(b, 3, 3)) == EMPTY);
    CHECK(b.stones(BLACK) == 1);
    CHECK(b.group_size(P(b, 3, 4)) == 1);
    CHECK(b.liberties(P(b, 4, 3)) == 4);
    CHECK(fx::empties_consistent(b));
    CHECK(fx::groups_consistent(b));
    CHECK(b.hash() == fx::rebuilt_hash(b));

    SUBCASE("removing a stone from the middle of a chain splits it")
    {
        fx::set_board(b, {
            ".........",
            "..XXXXX..",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        b.remove_stone(P(b, 4, 1));
        CHECK(b.group_size(P(b, 2, 1)) == 2);
        CHECK(b.group_size(P(b, 5, 1)) == 2);
        CHECK(b.stones(BLACK) == 4);
        CHECK(fx::groups_consistent(b));
        CHECK(fx::empties_consistent(b));
        CHECK(b.hash() == fx::rebuilt_hash(b));
    }
}

TEST_CASE("fuzz: 2000 random 9x9 games agree with the reference implementation")
{
    Rng rng(0xC0FFEE);
    Board b;
    int moves_total = 0;
    int bad_groups = 0, bad_atari = 0, bad_empties = 0, bad_legal = 0, bad_hash = 0;
    int bad_self_atari = 0, bad_counts = 0, bad_capture = 0;

    for (int game = 0; game < 2000; ++game) {
        b.init(9);
        Color c = BLACK;
        int passes = 0, move = 0;
        while (move < 200 && passes < 2) {
            if ((move & 7) == 0) {
                for (int i = 0; i < b.empty_count(); ++i) {
                    const Point p = b.empty_at(i);
                    if (b.is_legal(p, c) != fx::ref_legal(b, p, c)) ++bad_legal;
                    if (b.is_capture(p, c) != fx::ref_captures(b, p, c)) ++bad_capture;
                }
                for (int k = 0; k < 6 && b.empty_count() > 0; ++k) {
                    const Point p = b.empty_at(int(rng.below(uint32_t(b.empty_count()))));
                    if (!b.is_legal(p, c) || b.is_capture(p, c)) continue;
                    Board t;
                    t.copy_from(b);
                    t.play(p, c);
                    if (b.is_self_atari(p, c) != (fx::ref_liberties(t, p) == 1)) ++bad_self_atari;
                }
            }

            bool moved = false;
            const int n = b.empty_count();
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
            if (moved) { ++move; ++moves_total; passes = 0; } else { b.play_pass(c); ++passes; }
            c = opponent(c);

            if ((move & 15) == 0) {
                if (!fx::groups_consistent(b)) ++bad_groups;
                if (!fx::empties_consistent(b)) ++bad_empties;
                if (!fx::stone_counts_consistent(b)) ++bad_counts;
            }
        }
        if (!fx::groups_consistent(b)) ++bad_groups;
        if (!fx::empties_consistent(b)) ++bad_empties;
        if (!fx::stone_counts_consistent(b)) ++bad_counts;
        if (b.hash() != fx::rebuilt_hash(b)) ++bad_hash;
        for (int p = 0; p < b.num_points(); ++p) {
            const Color cc = b.at(Point(p));
            if (cc != BLACK && cc != WHITE) continue;
            if (b.in_atari(Point(p)) != (fx::ref_liberties(b, Point(p)) == 1)) ++bad_atari;
        }
    }
    CHECK(moves_total > 50000);
    CHECK(bad_groups == 0);
    CHECK(bad_atari == 0);
    CHECK(bad_empties == 0);
    CHECK(bad_legal == 0);
    CHECK(bad_capture == 0);
    CHECK(bad_hash == 0);
    CHECK(bad_self_atari == 0);
    CHECK(bad_counts == 0);
}

TEST_CASE("fuzz: 19x19 and 7x7 games stay consistent")
{
    Rng rng(4242);
    int bad = 0;
    for (int game = 0; game < 40; ++game) {
        Board b;
        b.init(game % 2 ? 19 : 7);
        fx::random_game(b, rng, 500);
        if (!fx::groups_consistent(b)) ++bad;
        if (!fx::empties_consistent(b)) ++bad;
        if (!fx::stone_counts_consistent(b)) ++bad;
        if (b.hash() != fx::rebuilt_hash(b)) ++bad;
        Board c;
        c.copy_from(b);
        if (!fx::same_position(b, c)) ++bad;
    }
    CHECK(bad == 0);
}
