// Internal helpers shared by the AI levels. Not part of the public contract (ai/ai.h).
#pragma once
#include "ai/ai.h"
#include "ai/patterns.h"
#include "go/ladder.h"
#include "go/score.h"

namespace ai {
namespace detail {

using go::Board;
using go::Color;
using go::Game;
using go::opponent;
using go::Point;
using go::Rng;

// --------------------------------------------------------------------------------------------
// tunables (a single place so the arena can reason about them)
// --------------------------------------------------------------------------------------------
struct Tuning {
    // NORMAL evaluator, in centipoints (100 = one point of territory). A captured stone is worth
    // two points under area scoring (the stone plus the point it sat on), hence the 220.
    int capture_value = 220;         // per stone captured
    int save_value = 200;            // per own stone rescued from atari
    // The Bouzy map is a rough territory sketch, not a score: on a sparse board a single stone in
    // open space "claims" twenty points, which is an artefact of the dilation. It is therefore a
    // secondary term - clamped, and weighted up as the board fills.
    int influence_value = 26;        // per point of Bouzy territory swing
    int influence_clamp = 18;        // maximum swing counted, in points
    int atari_value = 60;            // per opponent stone put in atari
    int atari_ladder_bonus = 120;    // extra per stone when the ladder works
    int self_atari_penalty = 240;    // per own stone put in atari
    int pattern_value = 3;           // per unit of pattern weight (max 124 -> 372)
    int line_bonus[4] = {-160, -20, 110, 80}; // 1st, 2nd, 3rd, 4th line, early game only
    int near_last_bonus = 120;       // move within a knight's move of the last move
    int contact_bonus = 50;          // move touching any stone
    int eye_shape_bonus = 45;        // move that creates a second eye for a weak group
    int pass_threshold = -120;       // pass rather than play a move worse than this
    // MCTS
    int rave_bias = 2000;            // K in beta = rv / (rv + v + rv*v/K)
    int uct_c = 60;                  // exploration constant x1024. RAVE already explores:
                                     // measured, anything above ~0.1 costs strength here.
    int progressive_bias = 90;       // prior * pb / (visits + 1), prior is 0..1000
    int expand_threshold = 16;       // visits before a leaf grows children (sized so the node
                                     // pool lasts the whole search)
    int max_children = 16;           // children kept at a non-root node (top ones by prior)
    int prior_rave_visits = 12;      // virtual RAVE visits seeded from the heuristic prior
    int prior_rave_floor = 300;      // prior 0..1000 maps to this..(floor + span) win rate, permille
    int prior_rave_span = 400;
    int playout_pattern_min = 70;    // pattern weight needed to be picked by the playout policy
    int playout_atari_permille = 900;// chance of answering an atari in a playout
    int playout_scan_limit = 48;     // empty points examined before a playout gives up and passes
    int resign_permille = 100;       // resign under 10% win rate
    int resign_streak = 3;
};
const Tuning& tuning();
void set_tuning(const Tuning& t);    // host-side tuning experiments only

// --------------------------------------------------------------------------------------------
// board helpers
// --------------------------------------------------------------------------------------------
// Distance to the nearest edge: 0 on the first line, 1 on the second...
inline int line_of(const Board& b, Point p) {
    int x = b.x_of(p), y = b.y_of(p), n = b.size() - 1;
    int dx = x < n - x ? x : n - x;
    int dy = y < n - y ? y : n - y;
    return dx < dy ? dx : dy;
}
// Chebyshev distance between two points of the same board.
inline int chebyshev(const Board& b, Point a, Point c) {
    int dx = b.x_of(a) - b.x_of(c); if (dx < 0) dx = -dx;
    int dy = b.y_of(a) - b.y_of(c); if (dy < 0) dy = -dy;
    return dx > dy ? dx : dy;
}
// A self-atari that is almost always a blunder: leaves more than one stone in atari and captures
// nothing. One-stone self-atari (throw-in, ko threat, snapback bait) stays allowed.
bool is_bad_self_atari(const Board& b, Point p, Color c);
// True when the point is surrounded by `c` stones and diagonally safe (a real eye we must not fill).
bool fills_own_eye(const Board& b, Point p, Color c);
// Stones of `c` currently in atari next to p; writes the roots. Returns the count.
int adjacent_atari_groups(const Board& b, Point p, Color c, Point* roots, int max_roots);

// The Bouzy map costs 26 passes over the board at its full setting, which is 25 ms on a 19x19
// board on this hardware - more than a frame. Big boards get a shorter map; the sketch is coarser
// but the frame survives.
void influence_map(const Board& b, int8_t* out);

// Cheap 0..1000 move prior used to order candidates and seed MCTS children.
int heuristic_prior(const Board& b, Point p, Color c, Point last_move);

// A cheaper ranking used to pick which moves are worth looking at: shape, contact and locality,
// with none of the liberty walking. Sorting a hundred points with the full prior is a frame's work
// on this hardware; with this one it is a fraction of one.
int quick_prior(const Board& b, Point p, Color c, Point last_move);
// The same prior plus ladder verification of any atari the move creates or answers: used for the
// MCTS root, where picking the right dozen candidates on a big board decides the whole search.
int refined_prior(const Board& b, Point p, Color c);

// One scratch board and one influence buffer shared by every level. Never re-entrant: a level owns
// them for the duration of one evaluation.
constexpr int REPLY_WIDTH = 6;      // opponent answers examined by the two-ply refinement

Board& shared_scratch();
Board& shared_scratch2();
int8_t* shared_influence();

// Full NORMAL evaluation of one candidate in centipoints. `after` must already contain the position
// after the move (the caller keeps the scratch board), `influence_before` the root influence sum.
struct MoveEval {
    Point move = go::PASS;
    int score = 0;
    int tactical = 0;        // the part of the score that is material: captures, rescues, ataris
    int captures = 0;
    int atari_stones = 0;
    bool self_atari = false;
    bool ladder = false;
    uint8_t pattern = 0;
};
MoveEval evaluate_move(const Game& g, Board& scratch, Point p, int influence_before, int8_t* influence_buf);
// `with_influence` false skips the Bouzy map, which is most of the cost. The deep search runs that
// way: territory decides the direction of play at the root, tactics decide the lines below it.
MoveEval evaluate_move_board(const Board& root, Color c, Board& scratch, Point p, int influence_before,
                             int8_t* influence_buf, bool with_influence = true,
                             bool with_deep_tactics = false);

// Reads every group left with two liberties or fewer and scores what the ladders say: a move that
// leaves one of our groups in a lost ladder is worth much less than it looks. Only MASTER pays for
// this; it is what that level knows and HARD does not.
int ladder_audit(const Board& b, Color c);

// How good a position is for `c`, in the same centipoints the evaluator uses. `with_influence`
// buys a Bouzy map, which is worth it near the root and far too slow at the leaves of a deep
// search; without it the value is stones and prisoners.
int position_value(const Board& b, Color c, int komi_x2, int8_t* influence_buf, bool with_influence);

// Depth-limited search over the same evaluator.
//
//   value(board, c, d) = max over m of [ eval(board, c, m) - value(board + m, opponent, d - 1) ]
//   value(board, c, 0) = 0
//
// Depth 1 is exactly what NORMAL plays. Depth 2 and 3 are what HARD and MASTER play: on a 16.8 MHz
// ARM7 a two second think buys about forty Monte-Carlo playouts, which is inside the noisy part of
// MCTS, while the same time runs a three-ply search over the evaluator several times over. Measured
// on 9x9, 30 games: depth 2 beats NORMAL 100%, a forty-playout search beats it 50%.
constexpr int MAX_SEARCH_DEPTH = 4;

struct SearchWidths {
    uint8_t width[MAX_SEARCH_DEPTH] = { 5, 6, 6, 4 };
};

// Returns the value of the best move and, when `best_out` is given, the move itself.
int move_search(const Board& root, Color c, int komi_x2, int depth, const SearchWidths& widths,
                int8_t* influence_buf, Point* best_out);

// The same search for a single candidate, so the caller can spend one candidate per frame and
// never stall the display.
int move_value(const Board& root, Color c, int komi_x2, Point m, int depth, const SearchWidths& widths,
               int influence_before, int8_t* influence_buf);

// The deep search, taken apart so it can be run one opponent answer at a time: a whole candidate is
// more than a frame's worth of work on this machine, one answer is not.
struct DeepSearch {
    Point move = go::PASS;
    MoveEval mine;
    Point replies[REPLY_WIDTH];
    int reply_count = 0;
    int reply_index = 0;
    int best_reply_value = 0;
    int influence_after = 0;
    bool replies_listed = false;
};

// Plays `m` and leaves the position in shared_scratch2(). On a 19x19 board this is a frame's work
// on its own, so listing the opponent's answers is a second step.
void deep_begin(DeepSearch& d, const Board& root, Color c, Point m, int influence_before,
                int8_t* influence_buf, bool deep_tactics = false);
void deep_list_replies(DeepSearch& d, Color c, const SearchWidths& widths);
// Scores one answer. Returns true when every answer has been scored.
bool deep_step(DeepSearch& d, Color c, int komi_x2, int depth, const SearchWidths& widths,
               int8_t* influence_buf, bool deep_tactics = false);
// The candidate's value: what we gain, less the best the opponent can take back.
[[nodiscard]] int deep_value(const DeepSearch& d);

// --------------------------------------------------------------------------------------------
// light playouts (implemented in playout.bn_iwram.cpp so the code runs from IWRAM)
// --------------------------------------------------------------------------------------------
struct PlayoutRecord {
    // first_idx[point * 2 + (colour - 1)] = simulation move index of the first play there, else 0xFFFF.
    uint16_t* first_idx;
    int moves = 0;
};

// A playout in progress. On a 16.8 MHz CPU a whole 19x19 playout is a quarter of a second, so the
// search runs them a few moves at a time and the frame never stalls.
struct PlayoutState {
    Color to_move = go::BLACK;
    int16_t passes = 0;
    int16_t played = 0;
    int16_t limit = 0;
    int16_t idx = 0;
    bool finished = false;
};

void playout_begin(PlayoutState& st, const Board& b, Color to_move, int start_index);
// Plays at most `max_moves` more moves. Returns true when the playout has finished.
bool playout_advance(PlayoutState& st, Board& b, Rng& rng, PlayoutRecord* rec, int max_moves);

// Plays `b` to the end in one go (tests and the host arena; the ROM uses the sliced form).
int run_playout(Board& b, Color to_move, Rng& rng, int komi_x2, PlayoutRecord* rec);
// Fast area score of a finished playout position (all stones alive), in half points, black minus white.
int playout_score_x2(const Board& b, int komi_x2);

// One policy step: returns the move to play (or go::PASS). Exposed for unit tests.
Point playout_move(const Board& b, Color c, Rng& rng);

// --------------------------------------------------------------------------------------------
// the search-free levels
// --------------------------------------------------------------------------------------------
Point pick_very_easy(const Game& g, Rng& rng);
Point pick_easy(const Game& g, Board& scratch, Rng& rng);

}  // namespace detail
}  // namespace ai
