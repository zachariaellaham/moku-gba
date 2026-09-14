// Depth-limited ladder reader (both sides read), used by missions, the NORMAL evaluator and MASTER playouts.
#pragma once
#include "board.h"

namespace go {

enum class LadderResult : uint8_t { CAPTURED = 0, ESCAPED = 1, UNKNOWN = 2 };

constexpr int LADDER_MAX_PLIES = 60;

// Reads whether the group at `p` (which must have 1 or 2 liberties) can be captured by the attacker
// (= opponent of the group's colour) when it is the attacker's turn (attacker_to_move = true) or the
// defender's turn. Defender tries: extending along every liberty, and capturing any adjacent attacker
// group in atari. Attacker tries: every liberty of the target (when 2 libs) / the atari point.
// Returns CAPTURED if the attacker captures within max_plies, ESCAPED if the group reaches >= 3
// liberties or captures an attacking stone that frees it, UNKNOWN on depth exhaustion.
LadderResult read_ladder(const Board& b, Point p, bool attacker_to_move, int max_plies = LADDER_MAX_PLIES);

// Convenience: does playing `atari_move` by `c` start a working ladder against the adjacent group?
// Returns CAPTURED if after atari_move the defending group (opponent of c, in atari) cannot escape.
LadderResult ladder_after_move(const Board& b, Point atari_move, Color c, int max_plies = LADDER_MAX_PLIES);

// The sequence of the last read (for animation / campaign hints): attacker and defender moves in
// order. Filled by read_ladder / ladder_after_move when result is CAPTURED. Max LADDER_MAX_PLIES.
int last_ladder_line(Point* out, int max_out);

}  // namespace go
