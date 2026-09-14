// Unit tests for the ladder reader (go/ladder.h).
#include "doctest.h"
#include "go_fixture.h"
#include "go/ladder.h"

using namespace go;

namespace {

Point P(const Board& b, int x, int y) { return b.point(x, y); }

// The ladder shape used by most tests: a two stone white group in atari on (5,4), which can only run
// up and to the right until the top-right corner takes its last liberty.
void atari_ladder(Board& b)
{
    fx::set_board(b, {
        ".........",
        ".........",
        ".........",
        "....X....",
        "...XO....",
        "...XOX...",
        "....X....",
        ".........",
        ".........",
    });
}

// A two stone white group with two liberties, black to play: both ataris start a working ladder.
void open_ladder(Board& b)
{
    fx::set_board(b, {
        ".........",
        ".........",
        ".........",
        "....X....",
        "...XO....",
        "...XO....",
        "....X....",
        ".........",
        ".........",
    });
}

// Replays the recorded line on a copy of the board: every move must be legal and the colours
// alternate, starting with the side to move. The line ends either with the stones coming off the
// board, or - when the defender has no legal answer left - with the group in atari and the attacker
// to play, which is the same thing one move later.
bool line_captures(const Board& start, Point target, bool attacker_to_move)
{
    Point line[LADDER_MAX_PLIES];
    const int n = last_ladder_line(line, LADDER_MAX_PLIES);   // may be 0: dead with nothing to play
    const Color defender = start.at(target);
    const Color attacker = opponent(defender);
    Board b;
    b.copy_from(start);
    Color c = attacker_to_move ? attacker : defender;
    for (int i = 0; i < n; ++i) {
        if (!b.is_legal(line[i], c)) return false;
        b.play(line[i], c);
        c = opponent(c);
    }
    if (b.at(target) != defender) return true;                   // captured outright
    // otherwise the line must end with the defender to play, in atari, and with nothing legal left:
    // neither running out nor taking one of the surrounding groups.
    if (c != defender || !b.in_atari(target)) return false;
    if (b.is_legal(b.atari_point(target), defender)) return false;
    Point stones[MAX_SIZE * MAX_SIZE];
    const int n_stones = b.group_stones(target, stones, MAX_SIZE * MAX_SIZE);
    const int stride = b.stride();
    for (int i = 0; i < n_stones; ++i) {
        const Point q = stones[i];
        const Point nb[4] = {Point(q - stride), Point(q - 1), Point(q + 1), Point(q + stride)};
        for (int k = 0; k < 4; ++k)
            if (b.at(nb[k]) == attacker && b.in_atari(nb[k]) &&
                b.is_legal(b.atari_point(nb[k]), defender)) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("a working ladder is read to the end")
{
    Board b;
    atari_ladder(b);
    REQUIRE(b.liberties(P(b, 4, 4)) == 1);
    CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::CAPTURED);
    CHECK(line_captures(b, P(b, 4, 4), false));

    Point line[LADDER_MAX_PLIES];
    const int n = last_ladder_line(line, LADDER_MAX_PLIES);
    CHECK(n == 14);                                  // 7 defender moves, 7 attacker moves
    CHECK(line[0] == P(b, 5, 4));                    // the defender runs out of the atari
    CHECK(line[1] == P(b, 6, 4));                    // the attacker keeps it in atari
    CHECK(line[n - 1] == P(b, 8, 4));                // the last stone is taken at the right edge

    SUBCASE("reading twice gives the same answer")
    {
        CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::CAPTURED);
        Point again[LADDER_MAX_PLIES];
        CHECK(last_ladder_line(again, LADDER_MAX_PLIES) == n);
        for (int i = 0; i < n; ++i) CHECK(again[i] == line[i]);
    }
    SUBCASE("the board is not modified by the read")
    {
        const uint64_t h = b.hash();
        read_ladder(b, P(b, 4, 4), false);
        CHECK(b.hash() == h);
        CHECK(fx::groups_consistent(b));
        CHECK(fx::empties_consistent(b));
    }
    SUBCASE("any stone of the group is a valid handle")
    {
        CHECK(read_ladder(b, P(b, 4, 5), false) == LadderResult::CAPTURED);
    }
    SUBCASE("the same shape for the other colour")
    {
        Board m;
        fx::set_board(m, {
            ".........",
            ".........",
            ".........",
            "....O....",
            "...OX....",
            "...OXO...",
            "....O....",
            ".........",
            ".........",
        });
        CHECK(read_ladder(m, P(m, 4, 4), false) == LadderResult::CAPTURED);
        CHECK(line_captures(m, P(m, 4, 4), false));
    }
}

TEST_CASE("a two liberty group: the attacker must pick the right side, and both work here")
{
    Board b;
    open_ladder(b);
    REQUIRE(b.liberties(P(b, 4, 4)) == 2);
    CHECK(read_ladder(b, P(b, 4, 4), true) == LadderResult::CAPTURED);
    CHECK(line_captures(b, P(b, 4, 4), true));
    Point line[LADDER_MAX_PLIES];
    const int n = last_ladder_line(line, LADDER_MAX_PLIES);
    CHECK(n == 13);
    CHECK(line[0] == P(b, 5, 4));                    // the attacker ataris from above

    SUBCASE("with the defender to move it simply runs away")
    {
        CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::ESCAPED);
        Point none[4];
        CHECK(last_ladder_line(none, 4) == 0);       // no line is recorded unless it is captured
    }
}

TEST_CASE("a ladder breaker on the path saves the group")
{
    Board b;
    SUBCASE("white stone right on the diagonal")
    {
        atari_ladder(b);
        b.set_stone(P(b, 7, 1), WHITE);              // sits exactly where the ladder must pass
        CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::ESCAPED);
        Point none[4];
        CHECK(last_ladder_line(none, 4) == 0);
    }
    SUBCASE("a stone next to the path does not break it")
    {
        atari_ladder(b);
        b.set_stone(P(b, 8, 0), WHITE);
        CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::CAPTURED);
        CHECK(line_captures(b, P(b, 4, 4), false));
    }
    SUBCASE("a stone far away does not break it")
    {
        atari_ladder(b);
        b.set_stone(P(b, 0, 8), WHITE);
        CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::CAPTURED);
    }
    SUBCASE("the same stone breaks it for the other colour too")
    {
        fx::set_board(b, {
            ".........",
            ".......X.",
            ".........",
            "....O....",
            "...OX....",
            "...OXO...",
            "....O....",
            ".........",
            ".........",
        });
        CHECK(read_ladder(b, P(b, 4, 4), false) == LadderResult::ESCAPED);
    }
}

TEST_CASE("a ladder that hits the edge is captured")
{
    Board b;
    fx::set_board(b, {
        ".........",
        "..OX.....",
        "..X......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    REQUIRE(b.liberties(P(b, 2, 1)) == 2);
    CHECK(read_ladder(b, P(b, 2, 1), true) == LadderResult::CAPTURED);
    CHECK(line_captures(b, P(b, 2, 1), true));
    Point line[LADDER_MAX_PLIES];
    const int n = last_ladder_line(line, LADDER_MAX_PLIES);
    CHECK(n == 5);
    CHECK(line[0] == P(b, 1, 1));       // the atari that pushes the stone against the top edge
    CHECK(line[1] == P(b, 2, 0));
    CHECK(b.y_of(line[n - 1]) == 0);    // the capture happens on the first line
}

TEST_CASE("the defender escapes by capturing the surrounding stones")
{
    Board b;
    // The white group (3,2)(4,2) is in atari on (4,3), but the black stones above it are in atari
    // too: taking them at (5,1) gives white three liberties.
    fx::set_board(b, {
        "...OO....",
        "..OXX....",
        "..XOOX...",
        "...X.....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    });
    REQUIRE(b.liberties(P(b, 3, 2)) == 1);
    REQUIRE(b.liberties(P(b, 3, 1)) == 1);
    CHECK(read_ladder(b, P(b, 3, 2), false) == LadderResult::ESCAPED);

    SUBCASE("without that resource the same shape is a ladder")
    {
        b.set_stone(P(b, 5, 1), BLACK);          // the black group is no longer capturable
        REQUIRE(b.liberties(P(b, 3, 1)) > 1);
        CHECK(read_ladder(b, P(b, 3, 2), false) == LadderResult::CAPTURED);
        CHECK(line_captures(b, P(b, 3, 2), false));
    }
}

TEST_CASE("depth limit")
{
    Board b;
    atari_ladder(b);
    // the real line is 14 plies long, so anything shorter can only answer "do not know"
    for (int d : {1, 2, 3, 5, 8, 13}) {
        CHECK(read_ladder(b, P(b, 4, 4), false, d) == LadderResult::UNKNOWN);
        Point none[4];
        CHECK(last_ladder_line(none, 4) == 0);
    }
    CHECK(read_ladder(b, P(b, 4, 4), false, 14) == LadderResult::CAPTURED);
    CHECK(read_ladder(b, P(b, 4, 4), false, 60) == LadderResult::CAPTURED);
    CHECK(read_ladder(b, P(b, 4, 4), false, 1000) == LadderResult::CAPTURED);   // clamped
    SUBCASE("a short limit still reports an escape when the group is simply free")
    {
        Board f;
        f.init(9);
        f.set_stone(P(f, 4, 4), WHITE);
        CHECK(read_ladder(f, P(f, 4, 4), true, 1) == LadderResult::ESCAPED);
    }
}

TEST_CASE("groups that are not ladder targets")
{
    Board b;
    b.init(9);
    SUBCASE("three or more liberties escape at once")
    {
        b.set_stone(P(b, 4, 4), WHITE);
        CHECK(b.liberties(P(b, 4, 4)) == 4);
        CHECK(read_ladder(b, P(b, 4, 4), true) == LadderResult::ESCAPED);
        b.set_stone(P(b, 4, 3), BLACK);
        CHECK(b.liberties(P(b, 4, 4)) == 3);
        CHECK(read_ladder(b, P(b, 4, 4), true) == LadderResult::ESCAPED);
    }
    SUBCASE("empty points and off board points are unknown")
    {
        CHECK(read_ladder(b, P(b, 0, 0), true) == LadderResult::UNKNOWN);
        CHECK(read_ladder(b, PASS, true) == LadderResult::UNKNOWN);
        CHECK(read_ladder(b, NO_POINT, true) == LadderResult::UNKNOWN);
        CHECK(read_ladder(b, Point(0), true) == LadderResult::UNKNOWN);          // border point
        CHECK(read_ladder(b, Point(b.num_points() + 5), true) == LadderResult::UNKNOWN);
    }
    SUBCASE("a group the attacker cannot keep in atari escapes")
    {
        // the white group has two liberties but they are far apart, so every atari leaves three
        fx::set_board(b, {
            ".........",
            ".........",
            "...XXX...",
            "...OOO...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        CHECK(b.liberties(P(b, 3, 3)) == 5);
        CHECK(read_ladder(b, P(b, 3, 3), true) == LadderResult::ESCAPED);
    }
}

TEST_CASE("the ko point is respected")
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
    b.play(P(b, 3, 2), WHITE);                       // white captures in the ko
    REQUIRE(b.ko_point() == P(b, 4, 2));
    REQUIRE(b.liberties(P(b, 3, 2)) == 1);
    // black would love to take the white stone back, but the ko rule forbids it right now
    CHECK(read_ladder(b, P(b, 3, 2), true) == LadderResult::ESCAPED);
}

TEST_CASE("ladder_after_move")
{
    Board b;
    open_ladder(b);
    SUBCASE("either atari starts a working ladder")
    {
        CHECK(ladder_after_move(b, P(b, 5, 4), BLACK) == LadderResult::CAPTURED);
        Point line[LADDER_MAX_PLIES];
        int n = last_ladder_line(line, LADDER_MAX_PLIES);
        CHECK(n == 13);
        CHECK(line[0] == P(b, 5, 4));                // the atari itself opens the line
        CHECK(ladder_after_move(b, P(b, 5, 5), BLACK) == LadderResult::CAPTURED);
        n = last_ladder_line(line, LADDER_MAX_PLIES);
        CHECK(line[0] == P(b, 5, 5));
    }
    SUBCASE("a move that ataris nothing is not a ladder")
    {
        CHECK(ladder_after_move(b, P(b, 0, 0), BLACK) == LadderResult::ESCAPED);
        Point none[4];
        CHECK(last_ladder_line(none, 4) == 0);
    }
    SUBCASE("illegal or occupied moves are unknown")
    {
        CHECK(ladder_after_move(b, P(b, 4, 4), BLACK) == LadderResult::UNKNOWN);
        CHECK(ladder_after_move(b, PASS, BLACK) == LadderResult::UNKNOWN);
        CHECK(ladder_after_move(b, P(b, 0, 0), EMPTY) == LadderResult::UNKNOWN);
    }
    SUBCASE("an atari that does not work is reported as escaped")
    {
        b.set_stone(P(b, 6, 6), WHITE);              // breaks the ladder that runs down and right
        b.set_stone(P(b, 7, 1), WHITE);              // ... and the one that runs up and right
        CHECK(ladder_after_move(b, P(b, 5, 4), BLACK) == LadderResult::ESCAPED);
        CHECK(ladder_after_move(b, P(b, 5, 5), BLACK) == LadderResult::ESCAPED);
    }
    SUBCASE("the board is untouched")
    {
        const uint64_t h = b.hash();
        ladder_after_move(b, P(b, 5, 4), BLACK);
        CHECK(b.hash() == h);
        CHECK(fx::groups_consistent(b));
    }
}

TEST_CASE("last_ladder_line truncates to the caller's buffer")
{
    Board b;
    atari_ladder(b);
    REQUIRE(read_ladder(b, P(b, 4, 4), false) == LadderResult::CAPTURED);
    Point four[4];
    CHECK(last_ladder_line(four, 4) == 4);
    Point full[LADDER_MAX_PLIES];
    const int n = last_ladder_line(full, LADDER_MAX_PLIES);
    CHECK(n == 14);
    for (int i = 0; i < 4; ++i) CHECK(four[i] == full[i]);
    CHECK(last_ladder_line(full, 0) == 0);
}

TEST_CASE("fuzz: the reader never lies about random positions")
{
    Rng rng(0xADDE7);
    int captured = 0, escaped = 0, unknown = 0, bad_line = 0, bad_free = 0;
    for (int game = 0; game < 300; ++game) {
        Board b;
        b.init(9);
        fx::random_game(b, rng, 60);
        for (int p = 0; p < b.num_points(); ++p) {
            const Color c = b.at(Point(p));
            if (c != BLACK && c != WHITE) continue;
            const int libs = b.liberties(Point(p), nullptr, 0, 4);
            const bool attacker_first = (p & 1) != 0;
            const LadderResult r = read_ladder(b, Point(p), attacker_first);
            if (r == LadderResult::CAPTURED) {
                ++captured;
                if (!line_captures(b, Point(p), attacker_first)) ++bad_line;
            } else if (r == LadderResult::ESCAPED) {
                ++escaped;
            } else {
                ++unknown;
            }
            // a group with three or more liberties is never captured by a ladder
            if (libs >= 3 && r != LadderResult::ESCAPED) ++bad_free;
        }
        CHECK(fx::groups_consistent(b));       // the reader must not touch the board
        CHECK(fx::empties_consistent(b));
    }
    CHECK(bad_line == 0);
    CHECK(bad_free == 0);
    CHECK(captured > 0);
    CHECK(escaped > 0);
    MESSAGE("ladder fuzz: captured=" << captured << " escaped=" << escaped << " unknown=" << unknown);
}
