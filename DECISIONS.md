# DECISIONS

Every non-obvious choice, with the reason. Newest at the bottom of each section.

## Toolchain
- **Butano 21 + devkitARM r68 (gcc 16.1)**, installed from the devkitPro package tarballs directly
  (the pacman .deb is no longer published and pkg.devkitpro.org refuses non-browser user agents, so the
  tarballs were fetched with a browser UA and extracted into `$DEVKITPRO`). Butano lives next to
  the repo (`LIBBUTANO ?= ../butano/butano`).
- **ROM tests use a custom libmgba harness** (`tests/rom/harness/mokurun.c`) instead of mGBA's Lua
  scripting: Debian's `mgba-sdl` 0.10.5 has no `--script` option, and a libmgba program runs ~4000 fps
  headless, so a 30-minute soak is a 30-second test. It reads/writes GBA memory by ELF symbol.
- Host unit tests use **doctest** (single header, vendored). Test framework choice is irrelevant to the
  ROM; doctest gives subcases and readable failures.

## Engine
- Board is a padded flat array (stride = size + 2) with union-find groups and **pseudo-liberties**
  (count, sum, sum of squares): capture and atari detection are O(1) per neighbour, and exact liberty
  counts are computed on demand with a stamp array. This is the standard "libego" design and is the
  fastest known layout for a CPU without cache.
- **Undo is replay**: the game keeps the setup stones and the move list and rebuilds the board. It
  costs ~1 ms per 100 moves on the GBA and cannot desynchronise captures/ko/superko state.
- Positional superko: only capturing moves can repeat a position, so the superko check simulates
  captures on a scratch board and compares the hash with the history (at most a few hundred u64).
- Handicap stones use fixed hoshi placement (2: two opposite corners, 3: + third corner,
  4: four corners, 5: + centre). 7×7 has no handicap (no hoshi convention).
- No SGF parser in the ROM: missions are code-defined tables (`src/game/missions.cpp`). A tiny SGF
  writer exists on the host only (`tools/`) for debugging games.

## AI: what the hardware changed
The design called for MCTS with UCT and RAVE at HARD and MASTER, with one to two thousand
playouts per move on 9x9. Both parts are implemented and measured. The count is not what a 16.8 MHz
ARM7 with no cache gives: **about forty playouts in a two second think**, and forty playouts is
inside the part of the MCTS curve where the search is noise.

Measured on 9x9, 30 games each, at the budgets the ROM delivers:

| what chooses the move | vs NORMAL |
|---|---|
| a 40-playout UCT+RAVE search | 50% |
| a 100-playout search | 50% |
| the evaluator searched two plies deep | **100%** |

So the move is chosen by a depth-limited search over the same evaluator NORMAL uses, and the
Monte-Carlo tree only overrules it once it has 800 playouts **and** 40 visits on its choice. On this
hardware that gate never opens; on a faster one it does, and the search then takes over cleanly
(measured 96.7% against NORMAL at 1600 playouts). The playout engine, the tree, RAVE, the node pool
and the playouts/second overlay all remain, and the win rate it produces is what the resign rule
reads - gated on 200 playouts so the AI cannot resign on noise.

Two more measured findings are worth recording because they are counter-intuitive:

* **Odd search depths are worse than even ones.** A three-ply search lost every game to a two-ply
  one: the line ends on our own move, so it flatters it. MASTER searches four.
* **A textbook negamax with a positional leaf evaluation was weaker than scoring the difference of
  move evaluations.** 50% against NORMAL instead of 100%. The move evaluator knows about shape,
  ladders and contact; a stone count at the leaf throws that away.

MASTER's advantage over HARD is knowledge, not depth or width: width made no difference at all
(50%), and what separated them was giving MASTER a ladder audit - after the move it reads the ladder
on every group left with two liberties or fewer, so it sees the move that leaves its own group in a
lost ladder. With that, 100%.

## Everything the AI does is sliced to fit a frame
"Never freeze for more than one frame" is a hard constraint, and almost every part of the AI broke
it at first. What it took:

* **Playouts resume mid-simulation.** A whole 19x19 playout is a quarter of a second. `run()` is
  measured in board moves, not simulations, and the search can stop between two moves of a playout
  and continue next frame.
* **The playout loop and the board it runs on live in IWRAM.** Naming a file `*.bn_iwram.cpp` is not
  enough: devkitARM's linker script only collects sections literally named `.iwram`, and excludes
  files whose name contains `.iwram` - which `playout.bn_iwram.o` does not. The functions carry the
  section attribute, and the playout board is plain `.bss` (which is IWRAM) rather than `.sbss`.
* **Candidate preparation runs one stage per frame**: generate, rank cheaply, shortlist, verify the
  shortlist with ladders, then the influence map.
* **Two priors, not one.** Ranking a hundred points with the full prior is a frame's work; the cheap
  one (shape, contact, locality, atari by pseudo-liberties) picks what is worth the full one.
* **The opponent's answers are searched locally.** Scanning all 361 points of a 19x19 board for the
  reply to a move cost 200 ms; answers are almost always within four lines of the move.
* **The influence map is shorter on big boards** (2/6 dilations and erosions on 19x19 against the
  textbook 5/21) and is not computed below the second ply of the search.

The result: no frame is missed while the player has the move, and the longest stall during the
opponent's turn is four frames on 19x19, two on 9x9.

## The 19x19 board: eight pixel cells, no zoom and no scrolling
The choice was between an 8 px grid with a 2x inset around the cursor and a scrolling board.
Neither: nineteen 8 px cells is 152 pixels, and the HUD
column starts at 150, so the whole board fits the screen beside the HUD with a pixel to spare. An
inset would cover the part of the board the player is reading, and scrolling would hide the shape
of a position that Go is entirely about. Stones are drawn 8x8 with a 1 px rim so black and white
stay apart at that size, and the last-move marker is a 2x2 dot rather than a ring.

7x7 and 9x9 use 16 px cells, 13x13 uses 11.

## Butano
* **A scene switch needs a `bn::core::update()` between the old scene and the new one.** Butano only
  releases the VRAM of dropped sprites and backgrounds during its own update, so building the new
  scene in the same frame fails to allocate on blocks that are still marked "to remove".
* **A new sprite has background priority 3**, which puts it behind every panel in this game. Text
  blocks set priority 0 explicitly; this is why the HUD looked empty at first.
* **`bn::format` understands `{}` and nothing else.** `{:02}` asserts, which on hardware is a hang.
* **The text generator has no glyph for a newline.** Multi-line strings are split by the caller;
  `text_block` stops at the first newline rather than asserting.
* **`bn::music::playing()` is the only proof that music plays.** The game publishes the track it
  asked for, but that number is set even when the volume is zero. The ROM tests instead read
  Butano's playing flag and maxmod's sequence position: a position that keeps moving cannot be
  faked by a game that merely intended to start the music. One pattern of the slowest theme is
  76 BPM x 64 rows, about 760 frames, which is what the audio test's patience is set from.
* **A 26 KB scene object cannot come off the heap.** EWRAM is full of static pools, leaving about
  28 KB of heap, so the play session is a singleton rather than a member of its scene.

## Presentation
* **The opponent's speech bubble is not part of the juice.** Banners, shake, particles and the
  helper's pop-in are switched off in CLASSIC and by EFFECTS: OFF, because CLASSIC is meant to
  be sober. The five opponent characters still speak in every skin: their lines are the closest the
  game has to an opponent portrait, and a character who goes silent in one skin reads as a bug.
  Campaign missions stay quiet - there the helper does the talking, and two voices on one board is
  one too many.
* **The bubble is 128 px wide, not 96.** The strings budget 24 characters a line, and 24 characters
  of capitals is 103 px in the 8 px font: a three-part bubble truncated the longest lines. Four
  parts end at x=228, just inside the screen and just above the portrait the tail points at.
* **The debug overlay rebuilds eight times a second, not sixty.** It is most interesting exactly
  when the frame is already busy, and regenerating eight blocks of text every frame pushed the
  longest stall in the 30-minute soak past the point where the test could tell a slow frame from a
  hang. Throttled, it costs nothing measurable: 19x19 against MASTER under random input reports the
  same four-frame worst case and the same peak CPU with the overlay on as with it off.
* **Map nodes carry their mission number only where there is room.** A mission still out of reach
  draws a padlock; a number on top of it reads as neither.

## The stack lives in IWRAM, and nothing tells you when it runs out
Adding a playouts-per-second test command cost the game its 19x19 AI. The command computed
`(int64_t(count) * ticks_per_second()) / ticks`; that one 64-bit division pulled libgcc's
`__aeabi_uldivmod` - three kilobytes of it - into IWRAM, where devkitARM keeps its resident
routines. IWRAM static data went from 26 240 bytes to 29 416, the stacks at the top of IWRAM went
from 6.5 KB to 3.3 KB, and MASTER's four-ply search on a 19x19 board began overrunning them. The
symptom was not a crash: the search simply stopped finishing, each frame took three times as long,
and the thirty-minute soak reported the heartbeat stalling. Nothing in the build said a word.

The division is 32-bit now (`count` is at most 500 and the timer runs at 16 384 ticks a second, so
the product cannot overflow), and `tools/check_iwram.sh` runs on every `make dist`: it reads the
IWRAM sections out of the ELF and fails the build if fewer than 5 KB are left for the stacks. Any
64-bit arithmetic, any float, any long long division added anywhere in the game will now stop the
build instead of quietly eating the stack.

## Hints are always there; the offer is what waits for two failures
The rule was "hints after 2 failures". Withholding SELECT until then would mean the HUD's own
"SEL HINT" line is a lie for the first two attempts, and a player who knows they are stuck on the
first try should not have to fail twice to be told. So the hint itself is always available - it
still costs the mission's stars - and what arrives on the third attempt is Master Sen offering one
unprompted, after the mission's opening line. The count is per mission and lives in RAM only:
"twice in a row" is about the sitting you are in, and a mission abandoned yesterday deserves a
fresh start.

## Nothing may fire more than four sounds in a frame
Butano keeps a fixed pool of sound handles and asserts when it runs out, and an assert on a GBA is
a hang with an error screen. A frame can ask for more than a pool's worth without anything being
wrong: the board screen drains its whole event queue at once, so a move that captures a group and
leaves two others in atari, or the end of a game, fires a dozen effects together. `audio::play`
now plays each effect at most once per frame and starts at most four, and the test block reports
what was heard rather than what was asked for. Nobody could hear the difference; the ROM used to
stop dead.

It took a test that plays whole games to find it - `24_full_games` drives both sides with the
playout policy, so a hundred moves of captures and passes land in a handful of frames.

## The saved sandbox game
SRAM has to hold an in-progress sandbox game. It is stored as the move list rather
than the board: undo is already replay, so a list of points is the whole state, it is a quarter of
the size of a 19×19 board plus its group structure, and a save written by an older build either
replays or stops at the first move that no longer makes sense - it can never restore a position
that the rules would not have allowed.

It is written when the board screen is torn down, not on every move: an SRAM write is a byte at a
time and 512 moves is a kilobyte of them. A game that has ended clears the record instead.

The offer to resume lives on the sandbox screen rather than the title, and only when there is
something to resume. The title screen in mockup 1a has three items and no room for a fourth above
its footer; the sandbox screen gains a tenth row and gives back a pixel from each gap to fit it.
Resuming rebuilds the board, komi, ruleset and opponent from the save, not from whatever the setup
screen currently reads.

## Text that fits, proved in pixels
The string tests budgeted characters per line, with a note that the 8 px font averages 4.3 px a
character. An average is not a bound. `tools/gen_font.py` now also writes `include/moku_font_widths.h`
- the same width tables `moku_fonts.h` hands to Butano, with no Butano in them - so the host tests
measure the width the GBA actually draws and wrap exactly the way `render::wrap` does.

It found real truncation the character budgets could not see: seven mission objectives lost their
last word, in one box or the other, in one language or the other. Two French objectives overflowed
only because of the newline the author had chosen, so the briefing screen now wraps the objective
instead of splitting it on newlines, and a translation is free to break where it likes. The other
five were shortened.

## Campaign
Mission 9 teaches the ko rule as *take it, then close it* rather than *capture twice*. Against a
capture-greedy opponent the second capture cannot be forced: whatever the threat, the opponent
connects its ko stone the moment it has retaken, because that connection is the largest rescue on
the board. Taking the ko and using the move the opponent may not answer is the same lesson and is
deterministic. The English and French copy was rewritten to match.

Mission 17 is the shape that lives after a 3-3 invasion rather than the invasion itself: asked to
play the invasion out, even MASTER wandered off into the rest of the board, because "live in this
corner" is not something a global evaluator understands. The position is now the joseki's end, one
move short of two eyes, and the two other eye-space points are proved to die.
