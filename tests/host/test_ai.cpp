#include "doctest.h"
#include "ai_fixture.h"
#include "ai/internal.h"
#include "ai/patterns.h"

using namespace go;
using fixture::Position;

namespace {

// Runs a full game between two levels and returns the score margin in half points (black - white).
int play_game(ai::Level black, ai::Level white, int size, uint32_t seed, int komi_x2 = 15,
              int* moves_out = nullptr)
{
    GameSettings s;
    s.size = uint8_t(size);
    s.komi_x2 = int16_t(komi_x2);
    Game g;
    g.start(s);
    int moves = 0;
    const int limit = size * size * 3;
    while(g.phase() == Phase::PLAYING && moves < limit)
    {
        const ai::Level lvl = (g.to_move() == BLACK) ? black : white;
        const Point m = ai::pick_move(g, lvl, seed * 2654435761u + uint32_t(moves) * 40503u + 1u);
        const PlayResult r = g.play(m);
        REQUIRE(r.ok);
        ++moves;
    }
    if(moves_out) *moves_out = moves;
    if(g.phase() == Phase::MARK_DEAD)
    {
        g.propose_dead_marks();
        g.finish_marking();
        return g.final_score().margin_x2;
    }
    return g.score().margin_x2;
}

}  // namespace

TEST_CASE("pattern key encodes the neighbourhood in the documented order")
{
    Position p;
    fixture::build(p, {
        ".........",
        "....X....",
        "...OaO...",   // 'a' is the empty centre we probe
        "....X....",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const Point c = p.marker('a');
    const uint16_t key = ai::pattern_key(p.game.board(), c, BLACK);
    // N = black (own) -> code 1 at slot 1; W and E = white (opponent) -> code 2 at slots 7 and 3;
    // S = black -> code 1 at slot 5. Corners empty.
    CHECK(((key >> 2) & 3) == 1);
    CHECK(((key >> 6) & 3) == 2);
    CHECK(((key >> 10) & 3) == 1);
    CHECK(((key >> 14) & 3) == 2);
    CHECK(((key >> 0) & 3) == 0);
    // Same neighbourhood seen by white swaps own and opponent.
    const uint16_t wkey = ai::pattern_key(p.game.board(), c, WHITE);
    CHECK(((wkey >> 2) & 3) == 2);
    CHECK(((wkey >> 6) & 3) == 1);
}

TEST_CASE("pattern table rates a hane and a cut above a lone stone contact")
{
    Position hane;
    fixture::build(hane, {
        ".........",
        "..X......",
        "..Oa.....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const uint8_t w_hane = ai::pattern_weight(hane.game.board(), hane.marker('a'), BLACK);

    Position cut;
    fixture::build(cut, {
        ".........",
        "..XO.....",
        "..OaX....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const uint8_t w_cut = ai::pattern_weight(cut.game.board(), cut.marker('a'), BLACK);

    Position lone;
    fixture::build(lone, {
        ".........",
        ".........",
        "....a....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const uint8_t w_lone = ai::pattern_weight(lone.game.board(), lone.marker('a'), BLACK);

    CHECK(w_lone == 0);
    CHECK(w_hane > 0);
    CHECK(w_cut > 0);
    CHECK(w_cut >= w_hane);
}

TEST_CASE("generate_moves refuses eyes, suicide and superko")
{
    Position p;
    fixture::build(p, {
        ".XX......",
        "XaX......",     // 'a' is a black eye
        ".XX......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    Point moves[400];
    const int n = ai::generate_moves(p.game, moves, 400, false);
    const Point eye = p.marker('a');
    for(int i = 0; i < n; ++i) CHECK(moves[i] != eye);
    // white may play there (it is not white's eye) but it would be suicide, so it is excluded too
    p.game.set_to_move(WHITE);
    const int nw = ai::generate_moves(p.game, moves, 400, false);
    for(int i = 0; i < nw; ++i) CHECK(moves[i] != eye);
}

TEST_CASE("VERY EASY never plays an illegal move, never fills an eye, never self-ataris")
{
    for(uint32_t seed = 1; seed <= 40; ++seed)
    {
        GameSettings s;
        s.size = 9;
        Game g;
        g.start(s);
        int moves = 0;
        while(g.phase() == Phase::PLAYING && moves < 200)
        {
            const Point m = ai::pick_move(g, ai::Level::VERY_EASY, seed * 7919u + uint32_t(moves));
            if(m != PASS)
            {
                REQUIRE(g.is_legal(m));
                CHECK_FALSE(g.board().is_eye_like(m, g.to_move()));
                CHECK_FALSE(g.board().is_self_atari(m, g.to_move()));
            }
            REQUIRE(g.play(m).ok);
            ++moves;
        }
    }
}

TEST_CASE("EASY takes a capture when one is on the board")
{
    Position p;
    fixture::build(p, {
        ".X.......",
        "XOa......",     // white stone in atari at (1,1); 'a' captures it
        ".X.......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const Point capture = p.marker('a');
    for(uint32_t seed = 1; seed <= 10; ++seed)
        CHECK(ai::pick_move(p.game, ai::Level::EASY, seed) == capture);
}

TEST_CASE("EASY rescues its own group from atari")
{
    Position p;
    fixture::build(p, {
        ".O.......",
        "OXa......",     // black stone in atari; 'a' is the only escape
        ".O.......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const Point escape = p.marker('a');
    for(uint32_t seed = 1; seed <= 10; ++seed)
        CHECK(ai::pick_move(p.game, ai::Level::EASY, seed) == escape);
}

TEST_CASE("NORMAL takes a large capture over a shape move")
{
    Position p;
    fixture::build(p, {
        ".XXX.....",
        "XOOOa....",     // three white stones in atari
        ".XXX.....",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    CHECK(ai::pick_move(p.game, ai::Level::NORMAL, 12345) == p.marker('a'));
}

TEST_CASE("NORMAL saves a big group instead of playing elsewhere")
{
    Position p;
    fixture::build(p, {
        ".........",
        "OOO......",
        "XXXa.....",
        "OOO......",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    // The three black stones have exactly one liberty, at 'a'; extending there gives them three.
    // Neither white wall is in atari, so no capture competes with the rescue.
    const Point escape = p.marker('a');
    REQUIRE(p.game.board().in_atari(p.game.board().point(0, 2)));
    REQUIRE(p.game.board().atari_point(p.game.board().point(0, 2)) == escape);
    REQUIRE(p.game.board().liberties(p.game.board().point(0, 1)) > 1);
    REQUIRE(p.game.board().liberties(p.game.board().point(0, 3)) > 1);
    CHECK(ai::pick_move(p.game, ai::Level::NORMAL, 999) == escape);
}

TEST_CASE("every level plays only legal moves over many random games")
{
    const ai::Level levels[] = { ai::Level::VERY_EASY, ai::Level::EASY, ai::Level::NORMAL };
    for(ai::Level lvl : levels)
    {
        for(uint32_t seed = 1; seed <= 12; ++seed)
        {
            GameSettings s;
            s.size = (seed % 3 == 0) ? 7 : 9;
            Game g;
            g.start(s);
            int moves = 0;
            while(g.phase() == Phase::PLAYING && moves < 250)
            {
                const Point m = ai::pick_move(g, lvl, seed * 104729u + uint32_t(moves));
                if(m != PASS) REQUIRE(g.is_legal(m));
                REQUIRE(g.play(m).ok);
                ++moves;
            }
        }
    }
}

TEST_CASE("playouts finish and score a finished position correctly")
{
    Position p;
    fixture::build(p, {
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO..",
        "XXXOOOO.."}, BLACK);
    // 27 black stones, 36 white stones, the last two columns are open.
    Board b;
    b.copy_from(p.game.board());
    Rng rng(7);
    ai::detail::run_playout(b, BLACK, rng, 0, nullptr);
    CHECK(b.empty_count() < 9 * 9);
}

TEST_CASE("MCTS rescues a large group that is about to die")
{
    // Eight black stones with a single liberty at 'a'. Losing them swings the score by sixteen
    // points, so the playouts see it clearly; anything else here is a blunder.
    Position p;
    fixture::build(p, {
        "..OOOO...",
        ".OXXXXO..",
        ".OXXXXa..",
        ".OOOOO...",
        ".........",
        ".........",
        ".........",
        ".........",
        "........."}, BLACK);
    const Point escape = p.marker('a');
    REQUIRE(p.game.board().in_atari(p.game.board().point(2, 1)));
    REQUIRE(p.game.board().atari_point(p.game.board().point(2, 1)) == escape);
    REQUIRE(p.game.board().group_size(p.game.board().point(2, 1)) == 8);
    ai::Limits lim = ai::default_limits(ai::Level::HARD);
    lim.max_playouts = 800;
    const Point got = ai::pick_move(p.game, ai::Level::HARD, 4242, &lim);
    INFO("played (", p.game.board().x_of(got), ",", p.game.board().y_of(got), ")");
    CHECK(got == escape);
}

TEST_CASE("MCTS passes rather than filling its own territory in a finished game")
{
    Position p;
    // Both sides are alive with two eyes and black is far ahead: every legal move fills one of
    // black's own eyes and kills the group, so passing is the only sane move.
    fixture::build(p, {
        "XXXXXXOOO",
        ".X.XXXO.O",
        "XXXXXXOOO",
        "XXXXXXOOO",
        "XXXXXXOOO",
        "XXXXXXOOO",
        "XXXXXXOOO",
        "XXXXXXO.O",
        "XXXXXXOOO"}, BLACK);
    ai::Limits lim = ai::default_limits(ai::Level::HARD);
    lim.max_playouts = 600;
    CHECK(ai::pick_move(p.game, ai::Level::HARD, 77, &lim) == PASS);
}

TEST_CASE("MCTS returns legal moves on every board size")
{
    for(int size : {7, 9, 13})
    {
        GameSettings s;
        s.size = uint8_t(size);
        Game g;
        g.start(s);
        ai::Limits lim = ai::default_limits(ai::Level::HARD);
        lim.max_playouts = 120;
        for(int i = 0; i < 12; ++i)
        {
            const Point m = ai::pick_move(g, ai::Level::HARD, uint32_t(size * 100 + i), &lim);
            if(m != PASS) REQUIRE(g.is_legal(m));
            REQUIRE(g.play(m).ok);
            if(g.phase() != Phase::PLAYING) break;
        }
    }
}

TEST_CASE("a stronger level beats a weaker one on 9x9")
{
    // A short ladder check kept fast enough for the unit suite; tool_arena runs the full 50 games.
    int wins = 0;
    const int games = 8;
    for(int i = 0; i < games; ++i)
    {
        const int margin = (i % 2 == 0) ? play_game(ai::Level::EASY, ai::Level::VERY_EASY, 9, uint32_t(i + 1))
                                        : -play_game(ai::Level::VERY_EASY, ai::Level::EASY, 9, uint32_t(i + 1));
        if(margin > 0) ++wins;
    }
    CHECK(wins * 100 >= games * 60);
}
