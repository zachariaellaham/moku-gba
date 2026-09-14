# AI decisions and measurements

## Structure
`ai::Thinker` is resumable: `start()` then `step(work)` until it returns true, so the play scene can
spend ~12 ms per frame thinking and never drops a frame. One unit of work is one candidate
evaluation (NORMAL) or one MCTS playout (HARD/MASTER); the two search-free levels finish in one unit.
All level state lives in a single `.sbss` (EWRAM) block: only one Thinker may be searching at a time,
which is the case in the game and in the arena.

## Levels
- **VERY EASY** - uniform random over legal moves, never fills an eye, never self-ataris, passes when
  nothing sensible is left.
- **EASY** - greedy one ply: largest capture > rescue a group in atari (verified by replaying the
  move and counting liberties) > extend a two-liberty group > random near existing stones > random.
- **NORMAL** - full evaluation of the best 12-20 candidates (by board size). Each candidate is played
  on a scratch board and scored in centipoints: captures and rescues (220/200 per stone), atari with
  a ladder check (60 + 120 per stone when the ladder works), self-atari (-240 per stone), the 3x3
  pattern prior, first/second/third/fourth line bonuses, contact and locality, and the delta of a
  Bouzy 5/21 influence sketch.
- **HARD / MASTER** - UCT + RAVE over a fixed node pool (16-byte nodes; 2560 for HARD, 5120 for
  MASTER), light Mogo-style playouts, 1600 / 4200 playouts.

## Why the influence term is small
A Bouzy 5/21 map is a territory *sketch*, not a score. On a sparse board one stone in open space
"claims" about twenty points, purely as an artefact of the dilation, and the first version of the
evaluator happily played into the void instead of capturing three stones that were in atari. The
delta is therefore clamped to +-18 points, weighted at 26 centipoints per point, and scaled up as the
board fills. Material now dominates the opening, and the sketch decides quiet endgame moves.

## The pass bug (worth remembering)
The first MCTS offered PASS as a root child in every position. It made the search *non-monotonic*:
more playouts made it weaker (24 games vs NORMAL: 58% at 800 playouts, 62% at 1600, 37% at 3200),
because a lucky run of playouts could elect the pass and throw the game away, and the longer the
search ran the more chances it had. Passing is now a candidate only when the opponent has just passed,
when the board is more than four fifths full, or when there is no other legal move. The same test
became 87% / 96% / 100%.

## Measured tuning (9x9, 16-24 games per point, arena tool)
| knob | values tried | chosen |
|---|---|---|
| UCT constant (x1024) | 0, 80, 150, 250, 420 | **60** - 0 and 80 both scored 95.8%, 420 scored 66.7%. RAVE already provides the exploration. |
| RAVE bias K | 300, 900, 2000, 5000 | **2000** |
| expand threshold | 2, 8, 20 | **16** - sized so the node pool lasts the whole search rather than filling in the first third of it |
| children per node | 10, 16, 28 | **16** (root keeps 48 on 7x7/9x9, 20 on 13x13, 12 on 19x19) |

Self-play scaling check (the cleanest signal that the search is sound): a four-fold playout advantage
wins 85-95% of 20 games at three different budgets.

## Playout speed (host, one core, -O2)
| board | playouts/s | average length |
|---|---|---|
| 9x9 | 13 200 | 110 moves |
| 13x13 | 6 600 | 219 moves |
| 19x19 | 3 200 | 460 moves |

From an empty 9x9 board black wins 43.1% of light playouts with 7.5 komi, so the policy is close to
balanced rather than favouring one colour.

## Pattern table
46 hand-written 3x3 shapes (hane, cut, connect, tiger mouth, block, eye shapes, throw-in, crosscut,
edge shapes), expanded over the eight symmetries into a 65536-entry byte table in ROM
(`tools/gen_patterns.py`). Orthogonal neighbours are always pinned; diagonals are wildcards unless the
shape depends on them. The playout policy only accepts a match of weight >= 70; the evaluator and the
MCTS priors use the full graded weight.

## Strength ladder (50 games each, 9x9, komi 7.5, colours alternating)
Run with `tests/host/tool_arena`; each level uses the settings it ships with.

| match | win rate | average margin | game length |
|---|---|---|---|
| EASY vs VERY EASY | 86.0% | +26.9 | 105 moves |
| NORMAL vs EASY | 100.0% | +61.0 | 119 moves |
| HARD vs NORMAL | 98.0% | +5.5 | 96 moves |
| MASTER vs HARD | 76.0% | +14.7 | 106 moves |

The bar is 65%; every rung clears it. HARD beats NORMAL by a wide margin on win rate but
only +5.5 points on average, which is what a Monte-Carlo player looks like: it plays for the win,
not for the size of it, and gives points away once it is comfortably ahead.
