# Go rules engine (`src/go`) — decisions and measurements

Scope: `board.cpp`, `game.cpp`, `score.cpp`, `ladder.cpp`. The public headers in `src/go` are the
contract and were implemented as written; everything below either explains a choice the headers
leave open or flags a deliberate deviation.

## Board

- **Layout** is the one described in `board.h`: padded flat arrays (stride = size + 2, border points
  hold `BORDER`), union-find with iterative path halving, a circular `next_stone` list per group, and
  **pseudo-liberties** per root (count, sum, sum of squares). `in_atari()` is
  `n > 0 && n <= 4 && sq_sum * n == sum * sum` — all pseudo-liberties coincide iff Cauchy-Schwarz is
  tight, and a point has at most four neighbours so the products stay exact in 32 bits.
- **Exact liberties** use a stamp array (`mark_` + `mark_gen_`) instead of clearing a bitmap. The
  generation counter wraps once every 65536 calls and then clears the array; the only private member
  added to `board.h` is `uint16_t next_mark_gen() const` (allowed: private members only).
- **`copy_from` copies the used prefix only** — `(size+2)^2` entries of each array plus the scalars —
  and *not* the liberty stamps (private per board, generation-based, never observable). The capture
  list of the last move *is* copied (its prefix), because `last_captured()` is observable state.
- **Ko**: the simple ko point is set only when a single stone captures exactly one stone and the
  capturing stone then has exactly one liberty. `is_legal()` refuses the ko point for *both* colours,
  as the header specifies; only the side to move ever asks, and `Game` guarantees that.
- **`is_self_atari`** is exact when the move captures nothing and in the snapback case (one stone
  captured); it returns false as soon as two or more stones would be captured, which is what the
  header documents. The fuzz test checks the exact cases against "play it and count".
- **`remove_stone`** (scoring-time removal) lifts the whole group and puts every stone but one back:
  union-find cannot delete, and this keeps the pseudo-liberty bookkeeping exact. It is O(group²) in
  the worst case but only runs at scoring time.
- **Zobrist**: `constexpr ZobristTable ZOBRIST = make_zobrist();` in `board.cpp`. Verified to be
  constant-initialised: `arm-none-eabi-nm` reports it in `.rodata` (10584 bytes) and `board.o` has no
  `.init_array` entry, so there is no start-up cost. The hash covers stones only (positional superko).

## Game

- **Undo is replay** (`rebuild()`): `board_.init()`, replay the setup stones, then every recorded
  move. 225k undo+replay per second at move 60 on the host (≈ 9k/s on the GBA) — far more than a
  human can press B.
- **Handicap stones are setup stones.** They are placed by `start()` into `setup_[]`, so `rebuild()`
  restores them for free and `setup_stones()` shows them to the mission code. White moves first when
  the handicap is 2..5; an unsupported size/handicap combination (7x7, 1, 6+) silently becomes an even
  game and `settings().handicap` is normalised to 0.
- **Handicap and area scoring**: no compensation is added for the handicap stones (Chinese rules
  normally give White the handicap count back). Keeping it simple was the instruction; the campaign
  never mixes handicap with a scored finish, and the sandbox shows the same number both players see.
- **Positional superko**: only a capturing move can recreate a position, so `would_repeat()` first
  asks `is_capture()`, then simulates the move on a shared scratch `Board` and compares the resulting
  hash against the whole history. Cost is one `copy_from` (8 M/s on the host for 9x9) plus a linear
  scan of at most 1024 `uint64_t`.
- **Two passes → `MARK_DEAD`**; the marks are *not* proposed automatically (the play scene calls
  `propose_dead_marks()` when it opens the marking UI, so the player sees the proposal appear).
  `finish_marking()` computes the final score and switches to `OVER`; `resume_play()` clears the marks
  because they would go stale as soon as another stone is played.
- **`score()` while playing** returns `score_game(board, nullptr, komi, rules)` — the exact
  region-based score with no dead stones, as `game.h` documents. The Bouzy guess is `estimate_score()`
  and is the caller's choice.
- **Resign** sets the winner immediately; `final_score()` still carries the point counts (useful for
  the result screen) with `winner` forced to the resignee's opponent.

## Scoring

- **Area**: live stones + surrounded empty regions, dead stones removed first. **Territory**:
  surrounded empty regions + prisoners, where prisoners = stones captured during play + the
  opponent's dead stones. `black_captures` / `white_captures` in `ScoreResult` always carry that full
  prisoner count (informational under area rules).
- **Regions** are plain flood fills: a region touching both colours (or neither) is dame. That gives
  the right answer for seki with shared liberties — the classic no-eye seki scores 0 for both sides
  under territory rules and both groups keep counting under area rules (fixture 8 in `test_score`) —
  and it does not special-case an eye inside a seki, which the Japanese rules would also deny. Noted
  as a known simplification.
- **Bouzy 5/21 scaling**: a stone seeds ±128 in an `int16` work map, five dilations can add at most
  4 per pass, so the internal range is [-148, 148]; the `int8_t` output saturates at ±127. Only points
  within five steps of a stone can saturate and those are never in doubt. Off-board neighbours are
  ignored by both operators, so edge points do not erode faster than the middle.
- **`estimate_score`** follows the header literally: stones always count for their own colour, an
  empty point counts for the side whose influence is non-zero there, influence 0 is `OWN_DAME`.
  Under territory rules it counts regions + captures instead of stones + regions.
- **Benson** is the real algorithm: chains of one colour, regions = connected components of the
  points that are *not* that colour, a region is vital to a chain when every **empty** point of the
  region is a liberty of that chain (a region with no empty points is vacuously vital, which is what
  makes an eye filled with a dead enemy stone still count), then iterate: drop regions whose border
  contains a dropped chain, drop chains with fewer than two vital regions, until stable. Chains and
  regions are indexed into bitmaps (192 slots, 6 words each); both are bounded by 181 on a 19x19
  board and the code bails out conservatively if that were ever exceeded.
- **`propose_dead` — deliberate deviation.** The rule was "not pass-alive and majority of stones
  inside opponent influence", but a stone always dominates its own point in a Bouzy map (±128 minus
  at most 21×4 of erosion stays positive), so the literal rule can never fire. Removing the group from
  the influence seeds instead — the first implementation — condemns any wall that faces a strong
  opponent group (two facing walls each look "dead" once the other side is the only source left).
  What is implemented instead, and what the tests pin down:
  1. never propose a **pass-alive** group (Benson), and never propose a group in a textbook seki
     (it and one opposite-colour neighbour share exactly the same two liberties);
  2. walk the empty regions the group touches. A region bordered only by this group is its **own eye
     space**; a region where no point favours the group's colour and at least one favours the
     opponent is the **opponent's area**; anything else means the group has somewhere else to live,
     and it is left alone;
  3. a group that only touches its own eye space and enemy area is dead unless that eye space is
     ≥ 5 points (`EYE_SPACE_ALIVE`): four points or fewer is a dead shape against a player who moves
     first, five or more is shape-dependent and proposing a live group dead is the worse mistake.
  Groups are processed **fewest liberties first** and the influence map is recomputed once a group is
  given up, so the stones inside an enemy area are decided before the wall that surrounds them.
  The player can always toggle marks; this is a proposal, not a ruling.

## Ladder

- The search runs on a **private lightweight board** (a colour array plus a move/undo stack with the
  captured stones and the previous ko point), not on copies of `go::Board`: a `Board` is 10.9 KB and
  a ladder is up to 60 plies deep. It implements exactly the rules a ladder needs — capture, suicide
  illegal, simple ko.
- **AND/OR search**: at an attacker node any capturing child wins, otherwise UNKNOWN dominates
  ESCAPED; at a defender node any escaping child wins, otherwise UNKNOWN dominates CAPTURED. The
  attacker tries both liberties when the target has two (only one of them usually keeps the ladder
  going, and the other dies within two plies). The defender extends on the atari point and may
  capture any adjacent attacker group that is in atari.
- **Limits**: `max_plies` is a true depth limit (a 14-ply ladder is UNKNOWN at 13 and CAPTURED at 14),
  plus a node budget of `8 * max_plies + 16` so pathological branching cannot hang the AI. Exhausting
  either gives UNKNOWN.
- **Defender to move with two or more liberties is ESCAPED**: the attacker failed to keep it in
  atari, which is the ladder question.
- **`last_ladder_line`** is strictly alternating, starting with the side that was to move, so an
  animation can just play it out. When the defender has no legal answer left (every extension would
  be suicide) the line stops at the attacker's atari — the capture that follows needs no reading.
  It is only filled when the result is CAPTURED. `ladder_after_move` puts the atari move first.
- Recursion depth is bounded by `max_plies`; measured frames are 20 bytes (attacker) and 72 bytes
  (defender) of thumb stack, i.e. ≈ 2.8 KB worst case at 60 plies — fine for the IWRAM stack, but
  callers inside deep stacks should pass a smaller `max_plies`.

## Memory placement on the GBA (affects every area)

Uninitialised globals land in `.bss`, which the devkitARM GBA linker script maps into **IWRAM — only
32 KB**, shared with the stack, the IRQ handlers and every `*.bn_iwram.cpp` function. The engine's
scratch is 34 KB on its own, so every scratch object is declared `GO_EWRAM_BSS` (`go_types.h`,
`__attribute__((section(".sbss")))` under `MOKU_GBA`): `.sbss` is the EWRAM BSS section of that
script (Butano builds with `BN_EWRAM_BSS_SECTION=".sbss"`), and it is NOLOAD, so unlike
`GO_EWRAM_DATA` (section `.ewram`, for *initialised* EWRAM data) it costs no ROM and no start-up
copy. This was found by linking the engine into a scratch Butano ROM, which failed with
`section .init_array is not within region iwram` until the buffers moved.

Measured with `arm-none-eabi-objdump -h` on the four objects (thumb, -O2):

| object     | .text | .rodata | .sbss (EWRAM) | .bss (IWRAM) |
|------------|-------|---------|---------------|--------------|
| board.o    |  5276 |   10584 |             0 |            0 |
| game.o     |  2104 |       0 |         10920 |            0 |
| score.o    |  5012 |       0 |         18648 |            0 |
| ladder.o   |  3088 |       0 |          5508 |            0 |
| **total**  | 15480 |   10584 |     **35076** |        **0** |

So the engine costs ~25 KB of ROM and ~34 KB of EWRAM scratch, and nothing in IWRAM.

**Any `go::Game` or `go::Board` the game code declares must be a global in EWRAM too** — a `Game` is
25.6 KB, which neither IWRAM nor the stack can hold.

## Sizes and measurements

```
sizeof(go::Board)       = 10920 bytes
sizeof(go::Game)        = 25600 bytes
sizeof(go::ScoreResult) =   466 bytes
```

`build-host/tool_bench` (Release, one core of the 2-CPU dev box; the GBA is roughly 25x slower):

```
board  9x9  :  20000 games in   0.581 s ->     34406 games/s,  3694764 point-plays/s,   7727634 is_legal/s  (avg 107.4 moves, 39.1 captures)
board 13x13 :   5000 games in   0.257 s ->     19466 games/s,  4180984 point-plays/s,   9771557 is_legal/s  (avg 214.8 moves, 69.9 captures)
board 19x19 :   2000 games in   0.225 s ->      8878 games/s,  3994978 point-plays/s,  10962361 is_legal/s  (avg 450.0 moves, 137.7 captures)
copy   9x9  :  8048859 copy_from/s          copy 19x19 : 3854414 copy_from/s
game   9x9  :    20651 games/s,  2213713 moves/s (positional superko checked on every move)
undo   9x9  :   224996 undo+replay/s at 60 moves
score  9x9  :   992788 score_area/s,   52911 bouzy/s,  198222 benson/s,  158231 propose_dead/s
score 19x19 :   223623 score_area/s,    9131 bouzy/s,   39878 benson/s,   36792 propose_dead/s
ladder 19x19:   163013 read_ladder/s (16 ply ladder, always captured)
```

Reading: a light playout that scans the empty list with `is_legal` costs ~29 µs per 9x9 game on the
host, so on the GBA expect ~0.7 ms per random 9x9 playout before any AI logic — about 1400 playouts
per second, or ~17 per frame at 60 fps. `propose_dead` is the most expensive call in the engine
(one Bouzy map per dead group found, ~110 µs each on 19x19 host ≈ 2.7 ms on the GBA); it runs once,
when the marking phase opens.

## Verification

- `cmake -S tests/host -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-host -j2
  && ctest --test-dir build-host --output-on-failure` → 4/4 pass
  (`test_board` 16 cases / 3876 assertions, `test_game` 14 / 5244, `test_score` 28 / 8586,
  `test_ladder` 11 / 732).
- Fuzzing: 2000 random 9x9 games check every liberty count, atari flag, atari point, empties-list
  entry, legality, capture flag, self-atari and hash against an independent flood-fill reference in
  `tests/host/go_fixture.h`; 40 more games do the same on 19x19 and 7x7; 300 games replay every
  CAPTURED ladder line move by move on a real `Board` and verify the group actually dies.
- GBA: each file compiles with
  `arm-none-eabi-g++ -std=c++23 -O2 -mthumb -mcpu=arm7tdmi -fno-exceptions -fno-rtti -DMOKU_GBA=1 -Wall -Wextra`,
  and the four files were linked into a scratch Butano ROM and run in the headless mGBA harness: a
  random 9x9 game + dead-stone proposal + final score + a 19x19 ladder read produce **exactly** the
  same numbers as the same code on the host (moves 141, B 0, W 177, white wins; ladder CAPTURED).
