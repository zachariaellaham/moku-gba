// Unit tests for go scoring: area / territory rules, Bouzy influence, Benson pass-alive and the
// dead stone proposal. Every fixture below is counted by hand in its comment.
#include "doctest.h"
#include "go_fixture.h"
#include "go/score.h"

using namespace go;

namespace {

struct Expect {
    int black_stones = 0, white_stones = 0;   // live stones (dead ones are removed first)
    int black_terr = 0, white_terr = 0;       // surrounded empty points
    int black_dead = 0, white_dead = 0;       // marked dead stones of each colour
    int dame = 0;                             // empty points that belong to nobody
};

Color expect_winner(int b_x2, int w_x2)
{
    return b_x2 > w_x2 ? BLACK : (b_x2 < w_x2 ? WHITE : EMPTY);
}

// Checks both rulesets on the same position, plus the owner map and the point totals.
void verify(const Board& b, const bool* dead, int komi_x2, const Expect& e)
{
    const int b_pris = b.captures(BLACK) + e.white_dead;   // prisoners held by black
    const int w_pris = b.captures(WHITE) + e.black_dead;

    const ScoreResult a = score_area(b, dead, komi_x2);
    CHECK(a.black_stones == e.black_stones);
    CHECK(a.white_stones == e.white_stones);
    CHECK(a.black_territory == e.black_terr);
    CHECK(a.white_territory == e.white_terr);
    CHECK(a.black_dead == e.black_dead);
    CHECK(a.white_dead == e.white_dead);
    CHECK(a.black_captures == b_pris);
    CHECK(a.white_captures == w_pris);
    CHECK(a.black_x2 == 2 * (e.black_stones + e.black_terr));
    CHECK(a.white_x2 == 2 * (e.white_stones + e.white_terr) + komi_x2);
    CHECK(a.margin_x2 == a.black_x2 - a.white_x2);
    CHECK(a.winner == expect_winner(a.black_x2, a.white_x2));

    const ScoreResult t = score_territory(b, dead, komi_x2);
    CHECK(t.black_stones == e.black_stones);
    CHECK(t.white_stones == e.white_stones);
    CHECK(t.black_territory == e.black_terr);
    CHECK(t.white_territory == e.white_terr);
    CHECK(t.black_x2 == 2 * (e.black_terr + b_pris));
    CHECK(t.white_x2 == 2 * (e.white_terr + w_pris) + komi_x2);
    CHECK(t.margin_x2 == t.black_x2 - t.white_x2);
    CHECK(t.winner == expect_winner(t.black_x2, t.white_x2));

    // score_game dispatches to the right one
    CHECK(score_game(b, dead, komi_x2, Ruleset::AREA).black_x2 == a.black_x2);
    CHECK(score_game(b, dead, komi_x2, Ruleset::TERRITORY).black_x2 == t.black_x2);

    // the owner map must cover the board exactly once
    int own_b = 0, own_w = 0, own_dame = 0, own_none = 0;
    for (int y = 0; y < b.size(); ++y)
        for (int x = 0; x < b.size(); ++x) {
            switch (a.owner[b.point(x, y)]) {
                case OWN_BLACK: ++own_b; break;
                case OWN_WHITE: ++own_w; break;
                case OWN_DAME: ++own_dame; break;
                default: ++own_none; break;
            }
        }
    CHECK(own_none == 0);
    CHECK(own_b == e.black_stones + e.black_terr);
    CHECK(own_w == e.white_stones + e.white_terr);
    CHECK(own_dame == e.dame);
    CHECK(own_b + own_w + own_dame == b.size() * b.size());
    // dead stones are removed before scoring, so their points are already inside the territory
    // or dame counts: live stones + territory + dame must cover the board exactly.
    CHECK(e.black_stones + e.white_stones + e.black_terr + e.white_terr + e.dame ==
          b.size() * b.size());
    // the territory map is identical under both rulesets
    for (int p = 0; p < b.num_points(); ++p) CHECK(a.owner[p] == t.owner[p]);
}

}  // namespace

// =============================================================================================
// area / territory fixtures

TEST_CASE("fixture 1: empty boards")
{
    Board b;
    SUBCASE("empty 9x9, komi 7.5")
    {
        b.init(9);
        Expect e;
        e.dame = 81;                       // one region that touches neither colour
        verify(b, nullptr, 15, e);
        CHECK(score_area(b, nullptr, 15).winner == WHITE);
        CHECK(score_area(b, nullptr, 15).white_x2 == 15);
    }
    SUBCASE("empty 7x7, komi 0 is a draw")
    {
        b.init(7);
        Expect e;
        e.dame = 49;
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).winner == EMPTY);
        CHECK(score_territory(b, nullptr, 0).winner == EMPTY);
    }
    SUBCASE("empty 19x19")
    {
        b.init(19);
        Expect e;
        e.dame = 361;
        verify(b, nullptr, 13, e);
    }
}

TEST_CASE("fixture 2: a single stone owns the whole board")
{
    Board b;
    SUBCASE("one black stone on 9x9")
    {
        b.init(9);
        b.set_stone(b.point(4, 4), BLACK);
        Expect e;
        e.black_stones = 1;
        e.black_terr = 80;
        verify(b, nullptr, 15, e);
        CHECK(score_area(b, nullptr, 15).black_x2 == 162);
        CHECK(score_territory(b, nullptr, 15).black_x2 == 160);
    }
    SUBCASE("one white stone on 7x7, komi 0")
    {
        b.init(7);
        b.set_stone(b.point(0, 0), WHITE);
        Expect e;
        e.white_stones = 1;
        e.white_terr = 48;
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).winner == WHITE);
    }
}

TEST_CASE("fixture 3: split boards with and without dame")
{
    Board b;
    SUBCASE("9x9 split by an empty column: 36 stones each, the column is dame")
    {
        fx::set_board(b, {
            "XXXX.OOOO", "XXXX.OOOO", "XXXX.OOOO", "XXXX.OOOO", "XXXX.OOOO",
            "XXXX.OOOO", "XXXX.OOOO", "XXXX.OOOO", "XXXX.OOOO",
        });
        Expect e;
        e.black_stones = 36;
        e.white_stones = 36;
        e.dame = 9;
        verify(b, nullptr, 15, e);
        CHECK(score_area(b, nullptr, 15).winner == WHITE);        // komi decides
        CHECK(score_area(b, nullptr, 15).margin_x2 == -15);
    }
    SUBCASE("7x7 completely filled: 21 black, 28 white")
    {
        fx::set_board(b, {
            "XXXOOOO", "XXXOOOO", "XXXOOOO", "XXXOOOO", "XXXOOOO", "XXXOOOO", "XXXOOOO",
        });
        Expect e;
        e.black_stones = 21;
        e.white_stones = 28;
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).black_x2 == 42);
        CHECK(score_area(b, nullptr, 0).white_x2 == 56);
        CHECK(score_territory(b, nullptr, 0).black_x2 == 0);      // nothing but stones
        CHECK(score_territory(b, nullptr, 0).white_x2 == 0);
        CHECK(score_territory(b, nullptr, 0).winner == EMPTY);
    }
}

TEST_CASE("fixture 4: two walls, the same position under both rulesets")
{
    // black wall on row 2 owns rows 0-1 (14 points), white wall on row 3 owns rows 4-6 (21 points)
    Board b;
    fx::set_board(b, {
        ".......",
        ".......",
        "XXXXXXX",
        "OOOOOOO",
        ".......",
        ".......",
        ".......",
    });
    Expect e;
    e.black_stones = 7;
    e.white_stones = 7;
    e.black_terr = 14;
    e.white_terr = 21;

    SUBCASE("komi 0")
    {
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).black_x2 == 42);          // 7 stones + 14 territory
        CHECK(score_area(b, nullptr, 0).white_x2 == 56);          // 7 stones + 21 territory
        CHECK(score_area(b, nullptr, 0).margin_x2 == -14);        // white by 7
        CHECK(score_territory(b, nullptr, 0).black_x2 == 28);     // territory only
        CHECK(score_territory(b, nullptr, 0).white_x2 == 42);
        CHECK(score_territory(b, nullptr, 0).margin_x2 == -14);   // the same margin here
    }
    SUBCASE("komi 7.5")
    {
        verify(b, nullptr, 15, e);
        CHECK(score_area(b, nullptr, 15).white_x2 == 71);
    }
    SUBCASE("komi 6.5")
    {
        verify(b, nullptr, 13, e);
        CHECK(score_territory(b, nullptr, 13).white_x2 == 55);
    }
    SUBCASE("komi 0 and komi 9 (18 half points)")
    {
        verify(b, nullptr, 18, e);
        CHECK(score_area(b, nullptr, 18).margin_x2 == -32);
    }
}

TEST_CASE("fixture 5: a row of dame between the walls is a draw")
{
    Board b;
    fx::set_board(b, {
        ".......",
        ".......",
        "XXXXXXX",
        ".......",
        "OOOOOOO",
        ".......",
        ".......",
    });
    Expect e;
    e.black_stones = 7;
    e.white_stones = 7;
    e.black_terr = 14;
    e.white_terr = 14;
    e.dame = 7;
    verify(b, nullptr, 0, e);
    CHECK(score_area(b, nullptr, 0).winner == EMPTY);
    CHECK(score_territory(b, nullptr, 0).winner == EMPTY);
}

TEST_CASE("fixture 6: a stone inside enemy territory, alive or dead")
{
    Board b;
    fx::set_board(b, {
        ".O.....",
        ".......",
        "XXXXXXX",
        "OOOOOOO",
        ".......",
        ".......",
        ".......",
    });
    SUBCASE("not marked: the whole region is dame because it touches both colours")
    {
        Expect e;
        e.black_stones = 7;
        e.white_stones = 8;
        e.white_terr = 21;
        e.dame = 13;
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).black_x2 == 14);
    }
    SUBCASE("marked dead: black gets the region and one prisoner")
    {
        bool dead[MAX_POINTS] = {};
        dead[b.point(1, 0)] = true;
        Expect e;
        e.black_stones = 7;
        e.white_stones = 7;
        e.black_terr = 14;
        e.white_terr = 21;
        e.white_dead = 1;
        verify(b, dead, 0, e);
        CHECK(score_area(b, dead, 0).black_x2 == 42);             // dead stones just disappear
        CHECK(score_territory(b, dead, 0).black_x2 == 30);        // 14 territory + 1 prisoner
        CHECK(score_territory(b, dead, 0).white_x2 == 42);
    }
}

TEST_CASE("fixture 7: dead stones on both sides")
{
    Board b;
    fx::set_board(b, {
        ".O.....",
        ".......",
        "XXXXXXX",
        "OOOOOOO",
        "....X..",
        ".......",
        ".......",
    });
    bool dead[MAX_POINTS] = {};
    dead[b.point(1, 0)] = true;
    dead[b.point(4, 4)] = true;
    Expect e;
    e.black_stones = 7;
    e.white_stones = 7;
    e.black_terr = 14;
    e.white_terr = 21;
    e.black_dead = 1;
    e.white_dead = 1;
    verify(b, dead, 0, e);
    CHECK(score_area(b, dead, 0).black_x2 == 42);
    CHECK(score_area(b, dead, 0).white_x2 == 56);
    CHECK(score_territory(b, dead, 0).black_x2 == 30);            // 14 + 1 prisoner
    CHECK(score_territory(b, dead, 0).white_x2 == 44);            // 21 + 1 prisoner
}

TEST_CASE("fixture 8: seki - no territory under territory rules, stones still count for area")
{
    // The two stone black group and the five stone white group share exactly the two liberties
    // (1,0) and (1,1): neither can fill without dying, so both live and the shared points are dame.
    Board b;
    fx::set_board(b, {
        "X.OX...",
        "X.OX...",
        "OOOX...",
        "XXXX...",
        ".......",
        ".......",
        ".......",
    });
    REQUIRE(b.liberties(b.point(0, 0)) == 2);
    REQUIRE(b.liberties(b.point(2, 0)) == 2);
    REQUIRE(b.group_size(b.point(0, 0)) == 2);
    REQUIRE(b.group_size(b.point(2, 0)) == 5);

    Expect e;
    e.black_stones = 9;        // 2 in the seki + 3 in the right wall + 4 along row 3
    e.white_stones = 5;
    e.black_terr = 33;         // everything right of and below the walls
    e.dame = 2;                // the two shared liberties
    verify(b, nullptr, 0, e);
    CHECK(score_area(b, nullptr, 0).black_x2 == 84);       // 9 stones + 33 points
    CHECK(score_area(b, nullptr, 0).white_x2 == 10);       // the seki stones still count
    CHECK(score_territory(b, nullptr, 0).black_x2 == 66);  // territory only
    CHECK(score_territory(b, nullptr, 0).white_x2 == 0);   // no eyes in the seki
    CHECK(score_area(b, nullptr, 0).owner[b.point(1, 0)] == OWN_DAME);
    CHECK(score_area(b, nullptr, 0).owner[b.point(1, 1)] == OWN_DAME);
}

TEST_CASE("fixture 9: a corner enclosure and a wall, with dame in the middle")
{
    Board b;
    fx::set_board(b, {
        "..X...O..",
        "..X...O..",
        "XXX...O..",
        "......O..",
        "......O..",
        "......O..",
        "......O..",
        "......O..",
        "......O..",
    });
    Expect e;
    e.black_stones = 5;
    e.white_stones = 9;
    e.black_terr = 4;          // the 2x2 corner
    e.white_terr = 18;         // the two columns right of the wall
    e.dame = 45;
    verify(b, nullptr, 15, e);
    CHECK(score_area(b, nullptr, 15).black_x2 == 18);
    CHECK(score_area(b, nullptr, 15).white_x2 == 69);
    CHECK(score_territory(b, nullptr, 15).black_x2 == 8);
    CHECK(score_territory(b, nullptr, 15).white_x2 == 51);
}

TEST_CASE("fixture 10: a three point eye counts as territory")
{
    Board b;
    fx::set_board(b, {
        "XXXXX..",
        "X...X..",
        "XXXXX..",
        ".......",
        ".......",
        ".......",
        ".......",
    });
    Expect e;
    e.black_stones = 12;
    e.black_terr = 37;         // 3 eye points + 34 outside points, all touching black only
    verify(b, nullptr, 0, e);
    CHECK(score_area(b, nullptr, 0).black_x2 == 98);       // the whole board
    CHECK(score_area(b, nullptr, 0).owner[b.point(1, 1)] == OWN_BLACK);
}

TEST_CASE("fixture 11: one point eyes for both colours")
{
    Board b;
    fx::set_board(b, {
        "XXX.OOO",
        "X.X.O.O",
        "XXX.OOO",
        ".......",
        ".......",
        ".......",
        ".......",
    });
    Expect e;
    e.black_stones = 8;
    e.white_stones = 8;
    e.black_terr = 1;
    e.white_terr = 1;
    e.dame = 31;               // column 3 plus everything below row 2
    verify(b, nullptr, 0, e);
    CHECK(score_area(b, nullptr, 0).winner == EMPTY);
    CHECK(score_area(b, nullptr, 0).owner[b.point(1, 1)] == OWN_BLACK);
    CHECK(score_area(b, nullptr, 0).owner[b.point(5, 1)] == OWN_WHITE);
    CHECK(score_area(b, nullptr, 0).owner[b.point(3, 0)] == OWN_DAME);
}

TEST_CASE("fixture 12: prisoners taken during play count under territory rules")
{
    Board b;
    b.init(7);
    b.set_stone(b.point(0, 0), WHITE);
    b.set_stone(b.point(1, 0), BLACK);
    REQUIRE(b.liberties(b.point(0, 0)) == 1);
    REQUIRE(b.play(b.point(0, 1), BLACK) == 1);          // black captures the corner stone
    REQUIRE(b.captures(BLACK) == 1);

    Expect e;
    e.black_stones = 2;
    e.black_terr = 47;
    verify(b, nullptr, 0, e);
    CHECK(score_area(b, nullptr, 0).black_x2 == 98);      // 2 stones + 47 points = the whole board
    CHECK(score_territory(b, nullptr, 0).black_x2 == 96); // 47 territory + 1 prisoner
    CHECK(score_territory(b, nullptr, 0).black_captures == 1);
}

TEST_CASE("fixture 13: captures and dead stones add up as prisoners")
{
    Board b;
    fx::set_board(b, {
        ".O.....",
        ".......",
        "XXXXXXX",
        "OOOOOOO",
        ".......",
        ".......",
        ".......",
    });
    // pretend three stones were captured by each side earlier in the game
    b.play(b.point(6, 6), WHITE);      // just to have a legal last move; no captures
    bool dead[MAX_POINTS] = {};
    dead[b.point(1, 0)] = true;
    Expect e;
    e.black_stones = 7;
    e.white_stones = 8;                // the wall plus the new stone at (6,6)
    e.black_terr = 14;
    e.white_terr = 20;                 // one point of the lower region is now occupied
    e.white_dead = 1;
    verify(b, dead, 0, e);
    CHECK(score_territory(b, dead, 0).black_captures == 1);
    CHECK(score_territory(b, dead, 0).white_captures == 0);
}

TEST_CASE("fixture 14: a 19x19 position")
{
    Board b;
    b.init(19);
    for (int y = 0; y < 19; ++y) {
        b.set_stone(b.point(8, y), BLACK);
        b.set_stone(b.point(10, y), WHITE);
    }
    Expect e;
    e.black_stones = 19;
    e.white_stones = 19;
    e.black_terr = 19 * 8;             // columns 0..7
    e.white_terr = 19 * 8;             // columns 11..18
    e.dame = 19;                       // column 9
    verify(b, nullptr, 15, e);
    CHECK(score_area(b, nullptr, 15).black_x2 == 2 * (19 + 152));
    CHECK(score_area(b, nullptr, 15).winner == WHITE);   // komi
}

TEST_CASE("fixture 15: whole board 7x7 games")
{
    Board b;
    SUBCASE("black wins a filled 7x7 by one point")
    {
        // 25 black stones, 24 white stones, no empty points: black by 1 with komi 0
        fx::set_board(b, {
            "XXXXOOO",
            "XXXXOOO",
            "XXXXOOO",
            "XXXOOOO",
            "XXXOOOO",
            "XXXOOOO",
            "XXXXOOO",
        });
        Expect e;
        e.black_stones = 25;
        e.white_stones = 24;
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).margin_x2 == 2);
        CHECK(score_area(b, nullptr, 0).winner == BLACK);
        CHECK(score_area(b, nullptr, 1).winner == BLACK);    // komi 0.5 still loses for white
        CHECK(score_area(b, nullptr, 2).winner == EMPTY);    // komi 1 makes it a jigo
        CHECK(score_area(b, nullptr, 3).winner == WHITE);
    }
    SUBCASE("a finished 7x7 with eyes and territory")
    {
        fx::set_board(b, {
            "..XOO..",
            "..XXO..",
            "...XO..",
            "..XXO..",
            "..XOO..",
            "..XO...",
            "..XO...",
        });
        // 9 stones each; black owns columns 0-1 plus (2,2) = 15 points, white owns the right
        // side, 14 points in columns 5-6 plus (4,5) and (4,6) = 16 points
        Expect e;
        e.black_stones = 9;
        e.white_stones = 9;
        e.black_terr = 15;
        e.white_terr = 16;
        verify(b, nullptr, 15, e);
        CHECK(score_area(b, nullptr, 15).black_x2 == 48);
        CHECK(score_area(b, nullptr, 15).white_x2 == 65);
        CHECK(score_area(b, nullptr, 15).winner == WHITE);
    }
}

TEST_CASE("fixture 16: scattered stones leave the whole board neutral")
{
    Board b;
    fx::set_board(b, {
        "X.O.X.O",
        ".......",
        "O.X.O.X",
        ".......",
        "X.O.X.O",
        ".......",
        "O.X.O.X",
    });
    Expect e;
    e.black_stones = 8;
    e.white_stones = 8;
    e.dame = 33;               // one connected empty region that touches both colours
    verify(b, nullptr, 0, e);
    CHECK(score_area(b, nullptr, 0).winner == EMPTY);
    CHECK(score_territory(b, nullptr, 0).black_x2 == 0);
}

TEST_CASE("fixture 17: two enclosed corners and a wall")
{
    Board b;
    fx::set_board(b, {
        "..X...X..",
        "..X...X..",
        "XXX...XXX",
        ".........",
        "OOOOOOOOO",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    Expect e;
    e.black_stones = 10;
    e.white_stones = 9;
    e.black_terr = 8;          // the two 2x2 corners
    e.white_terr = 36;         // everything below the wall
    e.dame = 18;               // the open middle, which touches both
    verify(b, nullptr, 15, e);
    CHECK(score_area(b, nullptr, 15).black_x2 == 36);
    CHECK(score_area(b, nullptr, 15).white_x2 == 105);
    CHECK(score_territory(b, nullptr, 15).black_x2 == 16);
    CHECK(score_territory(b, nullptr, 15).white_x2 == 87);
}

TEST_CASE("fixture 18: a 13x13 split")
{
    Board b;
    fx::set_board(b, {
        "XXXXXX.OOOOOO", "XXXXXX.OOOOOO", "XXXXXX.OOOOOO", "XXXXXX.OOOOOO",
        "XXXXXX.OOOOOO", "XXXXXX.OOOOOO", "XXXXXX.OOOOOO", "XXXXXX.OOOOOO",
        "XXXXXX.OOOOOO", "XXXXXX.OOOOOO", "XXXXXX.OOOOOO", "XXXXXX.OOOOOO",
        "XXXXXX.OOOOOO",
    });
    Expect e;
    e.black_stones = 78;
    e.white_stones = 78;
    e.dame = 13;
    verify(b, nullptr, 13, e);
    CHECK(score_area(b, nullptr, 13).black_x2 == 156);
    CHECK(score_area(b, nullptr, 13).white_x2 == 169);
    CHECK(score_area(b, nullptr, 13).winner == WHITE);
}

TEST_CASE("fixture 19: a big dead group changes both scores")
{
    Board b;
    fx::set_board(b, {
        "XXXXXXX",
        "XOOOOOX",
        "XO...OX",
        "XOOOOOX",
        "XXXXXXX",
        ".......",
        ".......",
    });
    REQUIRE(b.group_size(b.point(1, 1)) == 12);
    SUBCASE("alive: white keeps its stones and its three point eye")
    {
        Expect e;
        e.black_stones = 20;
        e.white_stones = 12;
        e.black_terr = 14;
        e.white_terr = 3;
        verify(b, nullptr, 0, e);
        CHECK(score_area(b, nullptr, 0).black_x2 == 68);
        CHECK(score_area(b, nullptr, 0).white_x2 == 30);
    }
    SUBCASE("dead: black takes the whole board and twelve prisoners")
    {
        bool dead[MAX_POINTS] = {};
        for (int p = 0; p < b.num_points(); ++p)
            if (b.at(Point(p)) == WHITE) dead[p] = true;
        Expect e;
        e.black_stones = 20;
        e.black_terr = 29;         // 14 outside + the 15 points the white ring occupied
        e.white_dead = 12;
        verify(b, dead, 0, e);
        CHECK(score_area(b, dead, 0).black_x2 == 98);
        CHECK(score_area(b, dead, 0).white_x2 == 0);
        CHECK(score_territory(b, dead, 0).black_x2 == 82);   // 29 territory + 12 prisoners
        CHECK(score_territory(b, dead, 0).white_x2 == 0);
    }
    SUBCASE("propose_dead finds exactly that group")
    {
        bool dead[MAX_POINTS];
        propose_dead(b, dead);
        for (int p = 0; p < b.num_points(); ++p)
            CHECK(dead[p] == (b.at(Point(p)) == WHITE));
    }
}

TEST_CASE("fixture 20: five handicap stones own an empty board")
{
    Board b;
    b.init(9);
    Point hp[5];
    REQUIRE(handicap_points(9, 5, hp) == 5);
    for (int i = 0; i < 5; ++i) b.set_stone(hp[i], BLACK);
    Expect e;
    e.black_stones = 5;
    e.black_terr = 76;
    verify(b, nullptr, 15, e);
    CHECK(score_area(b, nullptr, 15).black_x2 == 162);
    CHECK(score_area(b, nullptr, 15).white_x2 == 15);
}

// =============================================================================================
// influence / estimate

TEST_CASE("bouzy influence on a clear position")
{
    Board b;
    fx::set_board(b, {
        ".......",
        ".......",
        "XXXXXXX",
        "OOOOOOO",
        ".......",
        ".......",
        ".......",
    });
    int8_t inf[MAX_POINTS];
    bouzy_influence(b, inf);
    for (int x = 0; x < 7; ++x) {
        CHECK(inf[b.point(x, 0)] > 0);
        CHECK(inf[b.point(x, 1)] > 0);
        CHECK(inf[b.point(x, 2)] > 0);        // the stones themselves
        CHECK(inf[b.point(x, 3)] < 0);
        CHECK(inf[b.point(x, 4)] < 0);
        CHECK(inf[b.point(x, 5)] < 0);
        CHECK(inf[b.point(x, 6)] < 0);
    }
    SUBCASE("an empty board has no influence at all")
    {
        Board e;
        e.init(9);
        int8_t zero[MAX_POINTS];
        bouzy_influence(e, zero);
        for (int p = 0; p < e.num_points(); ++p) CHECK(zero[p] == 0);
    }
    SUBCASE("mirroring the colours negates the map")
    {
        Board m;
        fx::set_board(m, {
            ".......",
            ".......",
            "OOOOOOO",
            "XXXXXXX",
            ".......",
            ".......",
            ".......",
        });
        int8_t inf2[MAX_POINTS];
        bouzy_influence(m, inf2);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 7; ++x)
                CHECK(inf2[m.point(x, y)] == -inf[b.point(x, y)]);
    }
    SUBCASE("borders stay at zero and values fit int8")
    {
        for (int p = 0; p < b.num_points(); ++p)
            if (b.at(Point(p)) == BORDER) CHECK(inf[p] == 0);
    }
}

TEST_CASE("estimate_score matches the obvious owner map")
{
    Board b;
    fx::set_board(b, {
        ".......",
        ".......",
        "XXXXXXX",
        "OOOOOOO",
        ".......",
        ".......",
        ".......",
    });
    const ScoreResult e = estimate_score(b, 0, Ruleset::AREA);
    for (int x = 0; x < 7; ++x) {
        CHECK(e.owner[b.point(x, 0)] == OWN_BLACK);
        CHECK(e.owner[b.point(x, 1)] == OWN_BLACK);
        CHECK(e.owner[b.point(x, 2)] == OWN_BLACK);
        CHECK(e.owner[b.point(x, 4)] == OWN_WHITE);
        CHECK(e.owner[b.point(x, 6)] == OWN_WHITE);
    }
    CHECK(e.black_stones == 7);
    CHECK(e.white_stones == 7);
    CHECK(e.black_territory == 14);
    CHECK(e.white_territory == 21);
    // on a clear position the estimate agrees with the exact area score
    const ScoreResult exact = score_area(b, nullptr, 0);
    CHECK(e.black_x2 == exact.black_x2);
    CHECK(e.white_x2 == exact.white_x2);
    CHECK(e.winner == exact.winner);

    SUBCASE("territory rules drop the stones from the estimate")
    {
        const ScoreResult t = estimate_score(b, 0, Ruleset::TERRITORY);
        CHECK(t.black_x2 == 28);
        CHECK(t.white_x2 == 42);
    }
    SUBCASE("an empty board estimates to komi only")
    {
        Board z;
        z.init(9);
        const ScoreResult t = estimate_score(z, 15, Ruleset::AREA);
        CHECK(t.black_x2 == 0);
        CHECK(t.white_x2 == 15);
        CHECK(t.winner == WHITE);
    }
}

// =============================================================================================
// Benson pass-alive

TEST_CASE("benson: two eyes are alive, one eye is not")
{
    Board b;
    bool alive[MAX_POINTS];
    SUBCASE("two separate eyes")
    {
        fx::set_board(b, {
            "XXXXX..",
            "X.X.X..",
            "XXXXX..",
            ".......",
            ".......",
            ".......",
            ".......",
        });
        benson_pass_alive(b, alive);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 5; ++x)
                if (b.at(b.point(x, y)) == BLACK) CHECK(alive[b.point(x, y)]);
        CHECK(!alive[b.point(1, 1)]);      // the eye point itself is empty, never marked
    }
    SUBCASE("a single eye is not pass alive")
    {
        fx::set_board(b, {
            "XXX....",
            "X.X....",
            "XXX....",
            ".......",
            ".......",
            ".......",
            ".......",
        });
        benson_pass_alive(b, alive);
        for (int p = 0; p < b.num_points(); ++p) CHECK(!alive[p]);
    }
    SUBCASE("an eye filled with a dead enemy stone still counts (vacuously vital)")
    {
        fx::set_board(b, {
            "XXXXX..",
            "X.XOX..",
            "XXXXX..",
            ".......",
            ".......",
            ".......",
            ".......",
        });
        benson_pass_alive(b, alive);
        CHECK(alive[b.point(0, 0)]);
        CHECK(!alive[b.point(3, 1)]);      // the captured white stone is not alive
    }
    SUBCASE("both colours are handled in one pass")
    {
        fx::set_board(b, {
            "XXXXX....",
            "X.X.X....",
            "XXXXX....",
            ".........",
            ".........",
            "....OOOOO",
            "....O.O.O",
            "....OOOOO",
            ".........",
        });
        benson_pass_alive(b, alive);
        CHECK(alive[b.point(0, 0)]);
        CHECK(alive[b.point(4, 5)]);
        CHECK(alive[b.point(8, 7)]);
    }
}

TEST_CASE("benson: a false eye does not keep a group alive")
{
    Board b;
    bool alive[MAX_POINTS];
    SUBCASE("corner eye shared by two chains")
    {
        // (0,0) looks like an eye but (1,0) and (0,1) are different chains and white sits on the
        // diagonal, so neither chain has two vital regions.
        fx::set_board(b, {
            ".XX....",
            "XO.....",
            "X......",
            ".......",
            ".......",
            ".......",
            ".......",
        });
        benson_pass_alive(b, alive);
        for (int p = 0; p < b.num_points(); ++p) CHECK(!alive[p]);
    }
    SUBCASE("a real eye plus an eye shared with three loose stones")
    {
        // The ring has a real eye at (1,1). Its second "eye" at (2,4) is shared with three single
        // stones; those have only one vital region each, so they drop out, the shared region then
        // has a dead border chain, and the ring loses its second eye as well.
        fx::set_board(b, {
            "XXX....",
            "X.X....",
            "XXX....",
            "..X....",
            ".X.X...",
            "..X....",
            ".......",
        });
        benson_pass_alive(b, alive);
        for (int p = 0; p < b.num_points(); ++p) CHECK(!alive[p]);
    }
    SUBCASE("closing the shape makes it alive")
    {
        // the same group with the second eye properly enclosed by one chain
        fx::set_board(b, {
            "XXX....",
            "X.X....",
            "XXX....",
            "XXX....",
            "X.X....",
            "XXX....",
            ".......",
        });
        benson_pass_alive(b, alive);
        CHECK(alive[b.point(0, 0)]);
        CHECK(alive[b.point(2, 5)]);
    }
}

TEST_CASE("benson: seki groups are not pass alive")
{
    Board b;
    fx::set_board(b, {
        "X.OX...",
        "X.OX...",
        "OOOX...",
        "XXXX...",
        ".......",
        ".......",
        ".......",
    });
    bool alive[MAX_POINTS];
    benson_pass_alive(b, alive);
    CHECK(!alive[b.point(0, 0)]);          // black seki group
    CHECK(!alive[b.point(2, 0)]);          // white seki group
    CHECK(!alive[b.point(3, 0)]);          // the outer black wall has no enclosed eyes either
}

// =============================================================================================
// dead stone proposal

TEST_CASE("propose_dead")
{
    Board b;
    bool dead[MAX_POINTS];
    SUBCASE("a group with a two point eye inside enemy territory is dead")
    {
        fx::set_board(b, {
            "XXXXXX...",
            "XOOOOX...",
            "XO..OX...",
            "XOOOOX...",
            "XXXXXX...",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        propose_dead(b, dead);
        for (int y = 1; y <= 3; ++y)
            for (int x = 1; x <= 4; ++x)
                if (b.at(b.point(x, y)) == WHITE) CHECK(dead[b.point(x, y)]);
        CHECK(!dead[b.point(0, 0)]);       // the enclosing black wall is alive
        CHECK(!dead[b.point(5, 4)]);
    }
    SUBCASE("a stray stone inside enemy territory is dead, the walls are not")
    {
        fx::set_board(b, {
            ".O.....",
            ".......",
            "XXXXXXX",
            "OOOOOOO",
            ".......",
            ".......",
            ".......",
        });
        propose_dead(b, dead);
        CHECK(dead[b.point(1, 0)]);
        for (int x = 0; x < 7; ++x) {
            CHECK(!dead[b.point(x, 2)]);
            CHECK(!dead[b.point(x, 3)]);
        }
    }
    SUBCASE("two facing walls are both alive")
    {
        fx::set_board(b, {
            ".......",
            ".......",
            "XXXXXXX",
            "OOOOOOO",
            ".......",
            ".......",
            ".......",
        });
        propose_dead(b, dead);
        for (int p = 0; p < b.num_points(); ++p) CHECK(!dead[p]);
    }
    SUBCASE("seki groups are left alone")
    {
        fx::set_board(b, {
            "X.OX...",
            "X.OX...",
            "OOOX...",
            "XXXX...",
            ".......",
            ".......",
            ".......",
        });
        propose_dead(b, dead);
        for (int p = 0; p < b.num_points(); ++p) CHECK(!dead[p]);
    }
    SUBCASE("a two eyed group is never proposed dead")
    {
        fx::set_board(b, {
            "OOOOOOO",
            "OXXXXXO",
            "OX.X.XO",
            "OXXXXXO",
            "OOOOOOO",
            ".......",
            ".......",
        });
        propose_dead(b, dead);
        CHECK(!dead[b.point(1, 1)]);
        CHECK(!dead[b.point(3, 2)]);
    }
    SUBCASE("an empty board proposes nothing")
    {
        b.init(9);
        propose_dead(b, dead);
        for (int p = 0; p < b.num_points(); ++p) CHECK(!dead[p]);
    }
}

// =============================================================================================
// region_owner

TEST_CASE("region_owner")
{
    Board b;
    fx::set_board(b, {
        "XXX.OOO",
        "X.X.O.O",
        "XXX.OOO",
        ".......",
        ".......",
        ".......",
        ".......",
    });
    uint16_t stamp[MAX_POINTS] = {};
    Point pts[MAX_SIZE * MAX_SIZE];
    int count = 0;

    CHECK(region_owner(b, b.point(1, 1), stamp, 1, pts, &count) == OWN_BLACK);
    CHECK(count == 1);
    CHECK(pts[0] == b.point(1, 1));

    CHECK(region_owner(b, b.point(5, 1), stamp, 2, pts, &count) == OWN_WHITE);
    CHECK(count == 1);

    CHECK(region_owner(b, b.point(3, 0), stamp, 3, pts, &count) == OWN_DAME);
    CHECK(count == 31);

    CHECK(region_owner(b, b.point(0, 0), stamp, 4, nullptr, &count) == OWN_NONE);   // occupied
    CHECK(count == 0);

    SUBCASE("an empty board is one big neutral region")
    {
        Board e;
        e.init(9);
        CHECK(region_owner(e, e.point(4, 4), stamp, 5, pts, &count) == OWN_DAME);
        CHECK(count == 81);
    }
    SUBCASE("out of range points report nothing")
    {
        CHECK(region_owner(b, NO_POINT, stamp, 6, nullptr, &count) == OWN_NONE);
        CHECK(region_owner(b, PASS, stamp, 7, nullptr, &count) == OWN_NONE);
    }
}

// =============================================================================================
// invariants over random positions

TEST_CASE("scoring invariants hold for random positions")
{
    Rng rng(20240913);
    for (int game = 0; game < 200; ++game) {
        Board b;
        b.init(game % 3 == 0 ? 7 : (game % 3 == 1 ? 9 : 13));
        fx::random_game(b, rng, 200);
        const int points = b.size() * b.size();
        const ScoreResult a = score_area(b, nullptr, 15);
        const ScoreResult t = score_territory(b, nullptr, 15);
        // every point is owned by exactly one of black / white / dame
        int own_b = 0, own_w = 0, own_d = 0;
        for (int y = 0; y < b.size(); ++y)
            for (int x = 0; x < b.size(); ++x) {
                const uint8_t o = a.owner[b.point(x, y)];
                if (o == OWN_BLACK) ++own_b;
                else if (o == OWN_WHITE) ++own_w;
                else if (o == OWN_DAME) ++own_d;
            }
        CHECK(own_b + own_w + own_d == points);
        CHECK(own_b == a.black_stones + a.black_territory);
        CHECK(own_w == a.white_stones + a.white_territory);
        CHECK(a.black_stones == b.stones(BLACK));
        CHECK(a.white_stones == b.stones(WHITE));
        CHECK(a.black_x2 == 2 * (a.black_stones + a.black_territory));
        CHECK(t.black_x2 == 2 * (t.black_territory + b.captures(BLACK)));
        CHECK(a.margin_x2 == a.black_x2 - a.white_x2);
        // area score and territory score differ exactly by stones minus prisoners
        CHECK(a.black_x2 - t.black_x2 == 2 * (a.black_stones - b.captures(BLACK)));
        // the estimate must not be wildly off on a finished position
        const ScoreResult est = estimate_score(b, 15, Ruleset::AREA);
        CHECK(est.black_stones == b.stones(BLACK));
        CHECK(est.white_stones == b.stones(WHITE));
        // marking every stone of one colour dead gives the whole board to the other one
        bool dead[MAX_POINTS] = {};
        for (int p = 0; p < b.num_points(); ++p)
            if (b.at(Point(p)) == WHITE) dead[p] = true;
        const ScoreResult all = score_area(b, dead, 0);
        CHECK(all.white_stones == 0);
        CHECK(all.white_dead == b.stones(WHITE));
        if (b.stones(BLACK) > 0) {                 // black owns every point that is left
            CHECK(all.black_stones + all.black_territory == points);
            CHECK(all.winner == BLACK);
        } else {                                   // random play can wipe black out completely
            CHECK(all.black_stones + all.black_territory == 0);
            CHECK(all.winner == EMPTY);
        }
    }
}
