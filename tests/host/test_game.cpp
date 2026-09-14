// Unit tests for go::Game - rules layer: komi, handicap, superko, passes, resign, undo, marking.
#include "doctest.h"
#include "go_fixture.h"

using namespace go;

namespace {

Point P(const Game& g, int x, int y) { return g.board().point(x, y); }

GameSettings settings(int size, Ruleset r = Ruleset::AREA, int handicap = 0)
{
    GameSettings s;
    s.size = uint8_t(size);
    s.rules = r;
    s.komi_x2 = GameSettings::default_komi_x2(r);
    s.handicap = uint8_t(handicap);
    return s;
}

}  // namespace

TEST_CASE("start: defaults and komi")
{
    CHECK(GameSettings::default_komi_x2(Ruleset::AREA) == 15);      // 7.5
    CHECK(GameSettings::default_komi_x2(Ruleset::TERRITORY) == 13); // 6.5
    GameSettings d;
    CHECK(d.size == 9);
    CHECK(d.komi_x2 == 15);
    CHECK(d.handicap == 0);
    CHECK(d.rules == Ruleset::AREA);

    Game g;
    g.start(settings(9));
    CHECK(g.board().size() == 9);
    CHECK(g.to_move() == BLACK);
    CHECK(g.phase() == Phase::PLAYING);
    CHECK(g.end_reason() == EndReason::NONE);
    CHECK(g.winner() == EMPTY);
    CHECK(g.move_count() == 0);
    CHECK(g.setup_count() == 0);
    CHECK(g.consecutive_passes() == 0);
    CHECK(!g.can_undo());
    CHECK(g.history_count() == 1);
    CHECK(g.history_hash(0) == 0u);
    CHECK(g.settings().komi_x2 == 15);

    g.start(settings(19, Ruleset::TERRITORY));
    CHECK(g.board().size() == 19);
    CHECK(g.settings().rules == Ruleset::TERRITORY);
    CHECK(g.settings().komi_x2 == 13);
}

TEST_CASE("handicap points")
{
    Point p[5];
    SUBCASE("unsupported combinations")
    {
        CHECK(handicap_points(7, 2, p) == 0);
        CHECK(handicap_points(9, 0, p) == 0);
        CHECK(handicap_points(9, 1, p) == 0);
        CHECK(handicap_points(9, 6, p) == 0);
        CHECK(handicap_points(11, 3, p) == 0);
    }
    SUBCASE("9x9 hoshi")
    {
        Board b;
        b.init(9);
        const Point ur = b.point(6, 2), ll = b.point(2, 6), lr = b.point(6, 6),
                    ul = b.point(2, 2), ce = b.point(4, 4);
        CHECK(handicap_points(9, 2, p) == 2);
        CHECK(p[0] == ur);
        CHECK(p[1] == ll);
        CHECK(handicap_points(9, 3, p) == 3);
        CHECK(p[2] == lr);
        CHECK(handicap_points(9, 4, p) == 4);
        CHECK(p[3] == ul);
        CHECK(handicap_points(9, 5, p) == 5);
        CHECK(p[4] == ce);
    }
    SUBCASE("13x13 hoshi")
    {
        Board b;
        b.init(13);
        CHECK(handicap_points(13, 2, p) == 2);
        CHECK(p[0] == b.point(9, 3));
        CHECK(p[1] == b.point(3, 9));
        CHECK(handicap_points(13, 5, p) == 5);
        CHECK(p[2] == b.point(9, 9));
        CHECK(p[3] == b.point(3, 3));
        CHECK(p[4] == b.point(6, 6));
    }
    SUBCASE("19x19 hoshi")
    {
        Board b;
        b.init(19);
        CHECK(handicap_points(19, 2, p) == 2);
        CHECK(p[0] == b.point(15, 3));
        CHECK(p[1] == b.point(3, 15));
        CHECK(handicap_points(19, 5, p) == 5);
        CHECK(p[2] == b.point(15, 15));
        CHECK(p[3] == b.point(3, 3));
        CHECK(p[4] == b.point(9, 9));
    }
    SUBCASE("all points are distinct and on the third/fourth line")
    {
        for (int size : {9, 13, 19})
            for (int h = 2; h <= 5; ++h) {
                CHECK(handicap_points(size, h, p) == h);
                for (int i = 0; i < h; ++i)
                    for (int j = i + 1; j < h; ++j) CHECK(p[i] != p[j]);
            }
    }
}

TEST_CASE("handicap game: stones placed, white moves first")
{
    for (int size : {9, 13, 19})
        for (int h = 2; h <= 5; ++h) {
            Game g;
            g.start(settings(size, Ruleset::AREA, h));
            CHECK(g.settings().handicap == h);
            CHECK(g.to_move() == WHITE);
            CHECK(g.board().stones(BLACK) == h);
            CHECK(g.board().stones(WHITE) == 0);
            CHECK(g.setup_count() == h);
            CHECK(g.history_hash(0) == g.board().hash());
            Point p[5];
            handicap_points(size, h, p);
            for (int i = 0; i < h; ++i) CHECK(g.board().at(p[i]) == BLACK);
            // the handicap stones survive an undo round trip
            g.play(g.board().point(0, 0));
            CHECK(g.undo());
            CHECK(g.board().stones(BLACK) == h);
            CHECK(g.to_move() == WHITE);
        }
    // an unsupported handicap falls back to an even game
    Game g;
    g.start(settings(7, Ruleset::AREA, 3));
    CHECK(g.settings().handicap == 0);
    CHECK(g.to_move() == BLACK);
    CHECK(g.board().stones(BLACK) == 0);
}

TEST_CASE("play: alternation, legality and the play result")
{
    Game g;
    g.start(settings(9));
    const Point p = P(g, 4, 4);
    PlayResult r = g.play(p);
    CHECK(r.ok);
    CHECK(r.captured == 0);
    CHECK(!r.atari_created);
    CHECK(!r.self_in_atari);
    CHECK(!r.game_ended);
    CHECK(g.to_move() == WHITE);
    CHECK(g.move_count() == 1);
    CHECK(g.moves()[0].p == p);
    CHECK(g.moves()[0].c == BLACK);
    CHECK(g.history_count() == 2);
    CHECK(g.history_hash(1) == g.board().hash());
    CHECK(g.can_undo());

    CHECK(!g.play(p).ok);                    // occupied
    CHECK(g.board().at(p) == BLACK);
    CHECK(g.move_count() == 1);

    SUBCASE("atari and capture flags")
    {
        Game h;
        fx::setup_game(h, {
            ".........",
            "....X....",
            "...X.X...",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        h.set_to_move(WHITE);
        PlayResult w = h.play(P(h, 4, 2));   // white plays into the hole: one liberty left
        CHECK(w.ok);
        CHECK(w.self_in_atari);
        CHECK(!w.atari_created);
        PlayResult b = h.play(P(h, 4, 3));   // black fills it: capture
        CHECK(b.ok);
        CHECK(b.captured == 1);
        CHECK(!b.atari_created);
        CHECK(h.board().captures(BLACK) == 1);
    }
    SUBCASE("atari_created")
    {
        Game h;
        fx::setup_game(h, {
            ".........",
            "....X....",
            "....O....",
            "...X.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        PlayResult r2 = h.play(P(h, 5, 2));
        CHECK(r2.ok);
        CHECK(!r2.atari_created);            // still two liberties: (3,2) and (4,3)
        h.play(P(h, 0, 0));                  // white elsewhere
        PlayResult r3 = h.play(P(h, 3, 2));
        CHECK(r3.ok);
        CHECK(r3.atari_created);
    }
}

TEST_CASE("passes end the game and move to the marking phase")
{
    Game g;
    g.start(settings(9));
    g.play(P(g, 2, 2));
    PlayResult p1 = g.play(PASS);
    CHECK(p1.ok);
    CHECK(!p1.game_ended);
    CHECK(g.consecutive_passes() == 1);
    CHECK(g.phase() == Phase::PLAYING);
    CHECK(g.to_move() == BLACK);
    CHECK(g.moves()[1].p == PASS);

    g.play(P(g, 5, 5));                       // a real move resets the pass counter
    CHECK(g.consecutive_passes() == 0);
    g.play(PASS);
    PlayResult p2 = g.play(PASS);
    CHECK(p2.ok);
    CHECK(p2.game_ended);
    CHECK(g.phase() == Phase::MARK_DEAD);
    CHECK(g.end_reason() == EndReason::TWO_PASSES);
    CHECK(g.consecutive_passes() == 2);
    CHECK(!g.is_legal(PASS));                 // no more moves while marking
    CHECK(!g.is_legal(P(g, 8, 8)));
    CHECK(!g.play(P(g, 8, 8)).ok);

    SUBCASE("resume_play goes back to playing")
    {
        g.resume_play();
        CHECK(g.phase() == Phase::PLAYING);
        CHECK(g.consecutive_passes() == 0);
        CHECK(g.end_reason() == EndReason::NONE);
        CHECK(g.is_legal(P(g, 8, 8)));
    }
    SUBCASE("finish_marking ends the game with a score")
    {
        g.finish_marking();
        CHECK(g.phase() == Phase::OVER);
        CHECK(g.end_reason() == EndReason::TWO_PASSES);
        const ScoreResult& s = g.final_score();
        CHECK(s.winner == g.winner());
        CHECK(s.margin_x2 == s.black_x2 - s.white_x2);
        CHECK(s.white_x2 >= 15);              // komi at least
        CHECK(!g.is_legal(P(g, 8, 8)));
    }
}

TEST_CASE("simple ko is refused by the game as well")
{
    Game g;
    fx::setup_game(g, {
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
    g.set_to_move(WHITE);
    const Point bx = P(g, 4, 2), ko = P(g, 3, 2);
    CHECK(g.play(ko).ok);
    CHECK(g.board().captures(WHITE) == 1);
    CHECK(!g.is_legal(bx));                    // simple ko
    CHECK(g.would_repeat(bx));                 // ... and it is also a repetition
    CHECK(!g.play(bx).ok);
    CHECK(g.move_count() == 1);
    g.play(P(g, 8, 8));
    g.play(P(g, 0, 8));
    CHECK(g.is_legal(bx));                     // the ko ban has expired and the position is new
    CHECK(g.play(bx).ok);
}

TEST_CASE("positional superko: send two, return one")
{
    // Found by exhaustive replay of random 7x7 games (scratch tool find_superko.cpp): black puts two
    // stones in atari, white captures them, and black's recapture recreates the position three plies
    // back. White's capture takes two stones, so no simple ko point is set and the board rules allow
    // the recapture - catching it is exactly what positional superko is for.
    Game g;
    fx::setup_game(g, {
        "OOOOOOO",
        "OOOXOX.",
        "XOOXOOO",
        ".XXXXXO",
        "XXXOXXO",
        "XXOOOOO",
        "X.X.O.O",
    });
    const uint64_t start_hash = g.board().hash();
    CHECK(g.history_hash(0) == start_hash);
    CHECK(g.to_move() == BLACK);

    CHECK(g.play(P(g, 3, 6)).ok);                       // black extends into a self atari
    PlayResult cap = g.play(P(g, 1, 6));                // white takes two stones
    CHECK(cap.ok);
    CHECK(cap.captured == 2);
    CHECK(g.board().ko_point() == NO_POINT);            // two stones: no simple ko
    CHECK(g.to_move() == BLACK);

    const Point back = P(g, 2, 6);
    CHECK(g.board().is_legal(back, BLACK));             // the board rules allow it
    CHECK(g.would_repeat(back));                        // but it repeats the starting position
    CHECK(!g.is_legal(back));
    PlayResult bad = g.play(back);
    CHECK(!bad.ok);
    CHECK(g.move_count() == 2);
    CHECK(g.board().hash() != start_hash);

    // another legal move is still accepted, and the repetition stays banned
    CHECK(g.play(P(g, 3, 6)).ok);
    CHECK(g.move_count() == 3);

    SUBCASE("undo brings the position back and the ban with it")
    {
        CHECK(g.undo());
        CHECK(g.move_count() == 2);
        CHECK(g.would_repeat(back));
        CHECK(!g.is_legal(back));
    }
}

TEST_CASE("would_repeat only considers capturing moves")
{
    Game g;
    g.start(settings(9));
    g.play(P(g, 3, 3));
    CHECK(!g.would_repeat(P(g, 5, 5)));
    CHECK(!g.would_repeat(PASS));
    CHECK(!g.would_repeat(P(g, 3, 3)));       // occupied
    const uint64_t h = g.board().hash();
    CHECK(g.would_repeat(P(g, 4, 4)) == false);
    CHECK(g.board().hash() == h);             // the check leaves the board untouched
}

TEST_CASE("resign")
{
    Game g;
    g.start(settings(9));
    g.play(P(g, 4, 4));
    g.resign(WHITE);
    CHECK(g.phase() == Phase::OVER);
    CHECK(g.end_reason() == EndReason::RESIGN);
    CHECK(g.winner() == BLACK);
    CHECK(g.final_score().winner == BLACK);
    CHECK(!g.is_legal(P(g, 0, 0)));
    CHECK(!g.is_legal(PASS));
    CHECK(!g.play(P(g, 0, 0)).ok);

    g.start(settings(9));
    g.resign(BLACK);
    CHECK(g.winner() == WHITE);
    CHECK(g.end_reason() == EndReason::RESIGN);
}

TEST_CASE("undo round trips")
{
    SUBCASE("play N moves, undo them all")
    {
        Rng rng(9182);
        for (int game = 0; game < 30; ++game) {
            Game g;
            g.start(settings(9));
            const uint64_t start = g.board().hash();
            uint64_t hashes[64];
            int n = 0;
            while (n < 40) {
                const int e = g.board().empty_count();
                if (e == 0) break;
                const Point p = g.board().empty_at(int(rng.below(uint32_t(e))));
                if (!g.is_legal(p)) continue;
                if (!g.play(p).ok) continue;
                hashes[n++] = g.board().hash();
            }
            CHECK(n == 40);
            CHECK(g.move_count() == 40);
            for (int i = n - 1; i >= 0; --i) {
                CHECK(g.board().hash() == hashes[i]);
                CHECK(g.undo());
                CHECK(g.history_count() == i + 1);
            }
            CHECK(g.board().hash() == start);
            CHECK(g.move_count() == 0);
            CHECK(!g.can_undo());
            CHECK(!g.undo());
            CHECK(g.to_move() == BLACK);
            CHECK(g.board().stones(BLACK) == 0);
            CHECK(g.board().stones(WHITE) == 0);
            CHECK(g.board().captures(BLACK) == 0);
            CHECK(fx::empties_consistent(g.board()));
            CHECK(fx::groups_consistent(g.board()));
        }
    }
    SUBCASE("undo then replay the same move is a no-op")
    {
        Rng rng(5150);
        Game g;
        g.start(settings(9));
        for (int i = 0; i < 60; ++i) {
            const int e = g.board().empty_count();
            const Point p = g.board().empty_at(int(rng.below(uint32_t(e))));
            if (!g.is_legal(p)) continue;
            const Color mover = g.to_move();
            g.play(p);
            const uint64_t h = g.board().hash();
            const int captures_b = g.board().captures(BLACK);
            const int captures_w = g.board().captures(WHITE);
            const Point ko = g.board().ko_point();
            const int count = g.move_count();
            CHECK(g.undo());
            CHECK(g.to_move() == mover);
            CHECK(g.play(p).ok);
            CHECK(g.board().hash() == h);
            CHECK(g.board().captures(BLACK) == captures_b);
            CHECK(g.board().captures(WHITE) == captures_w);
            CHECK(g.board().ko_point() == ko);
            CHECK(g.move_count() == count);
            CHECK(g.history_hash(count) == h);
        }
    }
    SUBCASE("undo restores captured stones and the capture counters")
    {
        Game g;
        fx::setup_game(g, {
            ".........",
            "..XXX....",
            ".XOOO....",
            "..XX.....",
            ".........",
            ".........",
            ".........",
            ".........",
            ".........",
        });
        g.play(P(g, 5, 2));                      // black fills one liberty
        g.play(P(g, 8, 8));                      // white elsewhere
        CHECK(g.play(P(g, 4, 3)).captured == 3);
        CHECK(g.board().stones(WHITE) == 1);
        CHECK(g.board().captures(BLACK) == 3);
        CHECK(g.undo());
        CHECK(g.board().stones(WHITE) == 4);
        CHECK(g.board().captures(BLACK) == 0);
        CHECK(g.board().at(P(g, 2, 2)) == WHITE);
        CHECK(fx::groups_consistent(g.board()));
        CHECK(fx::empties_consistent(g.board()));
    }
    SUBCASE("undo out of the marking phase")
    {
        Game g;
        g.start(settings(9));
        g.play(P(g, 4, 4));
        g.play(PASS);
        g.play(PASS);
        CHECK(g.phase() == Phase::MARK_DEAD);
        CHECK(g.undo());
        CHECK(g.phase() == Phase::PLAYING);
        CHECK(g.end_reason() == EndReason::NONE);
        CHECK(g.consecutive_passes() == 1);
        CHECK(g.move_count() == 2);
        CHECK(g.is_legal(P(g, 0, 0)));
    }
}

TEST_CASE("setup stones")
{
    Game g;
    g.start(settings(9));
    g.add_setup_stone(P(g, 2, 2), BLACK);
    g.add_setup_stone(P(g, 3, 3), WHITE);
    CHECK(g.setup_count() == 2);
    CHECK(g.board().stones(BLACK) == 1);
    CHECK(g.board().stones(WHITE) == 1);
    CHECK(g.setup_stones()[0].p == P(g, 2, 2));
    CHECK(g.setup_stones()[0].c == BLACK);
    CHECK(g.history_hash(0) == g.board().hash());
    CHECK(g.move_count() == 0);

    g.add_setup_stone(P(g, 2, 2), WHITE);        // occupied: ignored
    CHECK(g.setup_count() == 2);

    g.set_to_move(WHITE);
    CHECK(g.to_move() == WHITE);
    g.play(P(g, 5, 5));
    CHECK(g.to_move() == BLACK);
    g.add_setup_stone(P(g, 7, 7), BLACK);        // too late: ignored
    CHECK(g.setup_count() == 2);
    CHECK(g.undo());
    CHECK(g.to_move() == WHITE);                 // the initial colour survives the rebuild
    CHECK(g.board().stones(BLACK) == 1);
}

TEST_CASE("dead stone marking")
{
    Game g;
    fx::setup_game(g, {
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
    g.play(PASS);
    g.play(PASS);
    REQUIRE(g.phase() == Phase::MARK_DEAD);
    REQUIRE(g.board().group_size(P(g, 1, 1)) == 10);    // white ring, a two point eye: dead shape
    REQUIRE(g.board().liberties(P(g, 1, 1)) == 2);

    SUBCASE("toggle marks the whole group")
    {
        g.toggle_dead(P(g, 1, 1));
        CHECK(g.is_dead(P(g, 1, 1)));
        CHECK(g.is_dead(P(g, 2, 1)));
        CHECK(g.is_dead(P(g, 3, 1)));
        CHECK(g.is_dead(P(g, 1, 2)));
        CHECK(g.is_dead(P(g, 4, 2)));
        CHECK(g.is_dead(P(g, 1, 3)));
        CHECK(g.is_dead(P(g, 3, 3)));
        CHECK(!g.is_dead(P(g, 0, 0)));
        g.toggle_dead(P(g, 3, 3));                // toggling any stone clears the whole group
        CHECK(!g.is_dead(P(g, 1, 1)));
        CHECK(!g.is_dead(P(g, 2, 1)));
    }
    SUBCASE("empty points and the clear function")
    {
        g.toggle_dead(P(g, 1, 1));
        g.toggle_dead(P(g, 8, 8));                // empty: no effect
        CHECK(!g.is_dead(P(g, 8, 8)));
        g.clear_dead_marks();
        CHECK(!g.is_dead(P(g, 1, 1)));
    }
    SUBCASE("the score follows the marks")
    {
        const ScoreResult before = g.score();
        g.toggle_dead(P(g, 1, 1));
        const ScoreResult after = g.score();
        CHECK(before.white_territory == 2);            // the two eye points
        CHECK(after.white_stones == before.white_stones - 10);
        CHECK(after.white_territory == 0);
        CHECK(after.black_territory == before.black_territory + 12);  // 10 stones + the eye space
        CHECK(after.white_dead == 10);
        CHECK(after.black_captures == 10);            // dead stones are prisoners
        CHECK(after.black_x2 > before.black_x2);
    }
    SUBCASE("propose_dead_marks proposes the enclosed white group")
    {
        g.propose_dead_marks();
        CHECK(g.is_dead(P(g, 1, 1)));
        CHECK(g.is_dead(P(g, 3, 3)));
        CHECK(!g.is_dead(P(g, 0, 0)));
        CHECK(!g.is_dead(P(g, 4, 4)));
        g.finish_marking();
        CHECK(g.phase() == Phase::OVER);
        CHECK(g.final_score().white_dead == 10);
        CHECK(g.final_score().black_dead == 0);
    }
    SUBCASE("marks are dropped when play resumes")
    {
        g.toggle_dead(P(g, 1, 1));
        g.resume_play();
        CHECK(!g.is_dead(P(g, 1, 1)));
        CHECK(g.phase() == Phase::PLAYING);
    }
}

TEST_CASE("score() ignores dead marks while playing")
{
    Game g;
    fx::setup_game(g, {
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
    const ScoreResult playing = g.score();
    CHECK(playing.white_stones == 10);
    CHECK(playing.white_dead == 0);
    g.play(PASS);
    g.play(PASS);
    g.toggle_dead(P(g, 1, 1));
    CHECK(g.score().white_dead == 10);
    CHECK(g.score().white_stones == 0);
}

TEST_CASE("the game record survives long random games")
{
    Rng rng(31337);
    Game g;
    g.start(settings(9));
    int moves = 0;
    while (g.phase() == Phase::PLAYING && moves < 300) {
        const int e = g.board().empty_count();
        bool played = false;
        if (e > 0) {
            const int start = int(rng.below(uint32_t(e)));
            for (int i = 0; i < e; ++i) {
                const Point p = g.board().empty_at((start + i) % e);
                if (g.board().is_eye_like(p, g.to_move())) continue;
                if (!g.is_legal(p)) continue;
                if (g.play(p).ok) { played = true; break; }
            }
        }
        if (!played) g.play(PASS);
        ++moves;
    }
    CHECK(g.phase() == Phase::MARK_DEAD);
    CHECK(g.history_count() == g.move_count() + 1);
    // every recorded hash must match a replay of the record
    Game replay;
    replay.start(settings(9));
    for (int i = 0; i < g.move_count(); ++i) {
        CHECK(replay.board().hash() == g.history_hash(i));
        CHECK(replay.play(g.moves()[i].p).ok);
    }
    CHECK(replay.board().hash() == g.board().hash());
    CHECK(fx::groups_consistent(g.board()));
    CHECK(fx::empties_consistent(g.board()));
}
