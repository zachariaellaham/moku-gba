// Scoring (area / territory), Bouzy 5/21 influence estimate, dead-stone proposal, Benson pass-alive.
#pragma once
#include "board.h"

namespace go {

enum class Ruleset : uint8_t;

// Owner map values.
enum : uint8_t { OWN_NONE = 0, OWN_BLACK = 1, OWN_WHITE = 2, OWN_DAME = 3 };

struct ScoreResult {
    int16_t black_x2 = 0, white_x2 = 0;   // final totals in half points (komi included in white)
    int16_t margin_x2 = 0;                // black_x2 - white_x2
    Color winner = EMPTY;                 // EMPTY = draw
    int16_t black_stones = 0, white_stones = 0;
    int16_t black_territory = 0, white_territory = 0;
    int16_t black_captures = 0, white_captures = 0;   // prisoners held BY black / BY white (territory rules)
    int16_t black_dead = 0, white_dead = 0;           // dead stones removed (black_dead = black stones)
    uint8_t owner[MAX_POINTS];            // OWN_* per point (stones count as their colour)
};

// Area scoring: stones + surrounded empty regions; dead stones (dead[p] true) are removed first.
// dead may be nullptr. komi_x2 is added to white.
ScoreResult score_area(const Board& b, const bool* dead, int komi_x2);
// Territory scoring: territory + prisoners (captures + dead stones); dead may be nullptr.
ScoreResult score_territory(const Board& b, const bool* dead, int komi_x2);
ScoreResult score_game(const Board& b, const bool* dead, int komi_x2, Ruleset rules);

// Bouzy 5/21 influence map: dilate 5 times, erode 21 times. out[p] in [-128, 127] (positive = black).
// Cost ~ 26 passes over the board (fine for a pause-menu estimate; AI uses it on candidate moves).
void bouzy_influence(const Board& b, int8_t* out, int dilations = 5, int erosions = 21);
// Score estimate from influence: every point with |influence| > 0 counts for its side (stones always
// count for their colour). Returns an area-style ScoreResult.
ScoreResult estimate_score(const Board& b, int komi_x2, Ruleset rules);

// Benson's algorithm: marks pass-alive stones (out[p] = true for unconditionally alive stones).
void benson_pass_alive(const Board& b, bool* out);

// Proposes dead stones: groups that are not pass-alive and whose stones lie in the opponent's
// Bouzy influence (majority of the group). Writes dead[p] for every point (false elsewhere).
void propose_dead(const Board& b, bool* dead);

// Empty-region ownership helper: floods the empty region containing p, returns OWN_BLACK/OWN_WHITE/
// OWN_DAME (touches both or none) and writes region points if out != nullptr.
uint8_t region_owner(const Board& b, Point p, uint16_t* stamp, uint16_t gen, Point* out, int* out_count);

}  // namespace go
