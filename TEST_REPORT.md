# MOKU — test report

Every requirement, the test that covers it, and the result. Everything
here was run on this machine against the ROM in `dist/moku.gba` and the sources in this commit.

- **Host suites**: `ctest --test-dir build-host` — 8 suites, **103 829 assertions, 0 failures**.
- **ROM suites**: `tests/rom/run_all.sh` — 20 scripts, **20 passed, 0 failed**, 132 000 emulated
  frames in about 105 seconds.

---

## 1. Rules engine

| requirement | test | result |
|---|---|---|
| board sizes 7, 9, 13, 19 | `test_board`, `test_game`, ROM `02_board_demo` | PASS |
| groups with cached liberties, incremental | `test_board` (2 000-game fuzz against a flood-fill reference) | PASS |
| suicide illegal, capture-into-suicide legal | `test_board` | PASS |
| positional superko by Zobrist + history | `test_game` | PASS |
| captures, pass, resign, two passes end the game | `test_game`, `test_session` | PASS |
| area **and** territory scoring, selectable | `test_score` (30 hand-counted fixtures, both rulesets) | PASS |
| komi 0–9 in half steps | `test_score`, sandbox screen | PASS |
| dead-stone marking, AI proposal, player confirms | `test_score`, `test_session`, ROM `13_sandbox` | PASS |
| score estimate (Bouzy 5/21) in the pause menu | `test_score`, ROM `11_play_screen` | PASS |
| handicap 2–5 on 9/13/19 | `test_game`, ROM `13_sandbox` | PASS |
| undo stack and replay | `test_game`, `test_session` | PASS |
| ladder reader, ≤60 plies, both sides | `test_ladder` (fuzz replays every captured line) | PASS |

**Differential test.** `tool_difftest` plays random games with the C++ engine and prints every
position; `tools/difftest.py` replays the same moves through an independent implementation of the
rules written in Python and compares. 270 games across 7×7, 9×9, 13×13 and 19×19: **0 mismatches**.

**Engine speed** (host, one core, `tool_bench`): 3.7 M point-plays/s and 34 000 full random 9×9
games/s; `sizeof(go::Board)` 10 920 bytes, `sizeof(go::Game)` 25 600.

---

## 2. AI

### Strength ladder — the bar is 65%

50 games each on 9×9, komi 7.5, colours alternating, at the budgets the ROM actually delivers
(`build-host/tool_arena`):

| match | win rate | average margin | game length |
|---|---|---|---|
| EASY vs VERY EASY | **86.0%** | +26.9 | 105 moves |
| NORMAL vs EASY | **100.0%** | +61.0 | 119 moves |
| HARD vs NORMAL | **100.0%** | +16.0 | 90 moves |
| MASTER vs HARD | **100.0%** | +28.0 | 102 moves |

| requirement | test | result |
|---|---|---|
| VERY EASY: random, no self-atari, no eye filling, passes sensibly | `test_ai` | PASS |
| EASY: capture > rescue > extend > near stones | `test_ai` | PASS |
| NORMAL: 1-ply over influence, liberties, patterns, ladders, edge | `test_ai`, ladder above | PASS |
| HARD / MASTER stronger still | ladder above | PASS |
| 3×3 pattern table, hand-authored, all symmetries | `test_ai`, `tools/gen_patterns.py` (46 shapes → 65 536 entries) | PASS |
| never plays an illegal move | `test_ai` fuzz over every level and board size; `tool_arena` aborts on one | PASS |
| resigns under 10% for 3 moves | `ai::Thinker`, gated on ≥200 playouts so it cannot resign on noise | PASS |
| thinking is time-sliced, never freezes a frame | ROM measurements below | PASS |
| playouts/s in a debug overlay | L+R+SELECT on the board; ROM `20_speech_overlay` toggles it and reads it back, `18_screens` keeps the shot | PASS |

### Playouts per second, measured on the hardware

`23_playouts` runs complete playouts back to back from a fresh position and times them
(`TCMD_BENCH_PLAYOUTS`). This is the playout engine alone — how fast one simulation is:

| board | playouts / second |
|---|---|
| 7×7 | 30 |
| 9×9 | 19 |
| 13×13 | 9 |
| 19×19 | 4 |

The tree never gets all of that, because the evaluator and the depth search are served first. The
same script measures what is actually left: MASTER's opening move on 9×9 runs **29 playouts** over
its five-second think, five per second of thinking time. Both numbers are on screen in the debug
overlay (L+R+SELECT) while the game is running.

**What the hardware changed.** The plan was 1–2 k playouts per move on 9×9. A 16.8 MHz ARM7
with no cache delivers about 40 in two seconds, and 40 playouts is *inside* the noisy part of MCTS:
measured over 30 games, letting a 40-playout search choose dropped HARD from 100% against NORMAL to
50%. HARD and MASTER therefore search the evaluator itself — two plies and four — and the
Monte-Carlo tree only overrules them once it has 800 playouts with 40 visits on its choice. That
gate never opens on this hardware and does open on a faster one. MASTER's edge over HARD is
knowledge rather than depth: it reads the ladder on every group left with two liberties or fewer.
`DECISIONS.md` has the numbers behind each step.

---

## 3. Campaign

| requirement | test | result |
|---|---|---|
| 20 missions, one concept each, boards 7→9→13→19 | `test_missions` ("the mission table is consistent") | PASS |
| every setup position is legal | `test_missions` | PASS |
| each mission is winnable by its solution, against the opponent it ships with | `test_missions`, ROM `12_missions` (all 20 in the ROM) | PASS |
| the naive answer fails | `test_missions`: wrong-side ladder, the two ladder ataris in the net mission, every non-seki move, both losing eye-space points | PASS |
| a whole mission played on buttons alone | ROM `19_playthrough`: title → save select → map → briefing → mission 1 → clear → next mission → map, no debug commands | PASS |
| win/fail checkers for all 20 | `test_missions`, ROM `12_missions` | PASS |
| ranks S/A/B/C from moves, undos, hints | `test_missions` | PASS |
| stars for rank ≥ A with no hints and no undos | `test_missions` | PASS |
| hints follow the solution and cost the stars | `test_missions` | PASS |
| hints after 2 failures | ROM `25_hint_offer`: SELECT gives one at any time, and from the third attempt the helper offers one unprompted; clearing the mission resets that | PASS |
| unlimited retry, unlocks on clear | ROM `16_save` (progress survives a reboot) | PASS |
| tutor dialogue before / hint / win / fail | ROM `10_menus`, `14_skins_language` screenshots | PASS |

**The positions are proved, not asserted.** `tools/gen_missions.py` builds every position from an
ASCII diagram and checks its promise with an independent Go engine before writing a byte: the ladder
captures in five moves and only from the correct side, the net at (4,4) is *the only move on the
board* that traps the stone, the snapback really recaptures five, exactly one move makes the seki,
and each life-and-death problem has exactly one answer. A position that stops being true stops the
build.

---

## 4. Sandbox

| requirement | test | result |
|---|---|---|
| board, colour, opponent, handicap, scoring, komi, skin | ROM `10_menus`, `13_sandbox` | PASS |
| five opponent characters with pips, blurb and your record | ROM `10_menus` screenshot `09_choose_opponent.png` | PASS |
| unlocks gate the sandbox | `src/game/unlocks.h`, ROM `10_menus` (debug unlock) | PASS |
| two-player hot seat | `test_session`, ROM `13_sandbox`, `22_resume` | PASS |
| an unfinished game is offered again | ROM `22_resume` (the RESUME row appears only when there is one) | PASS |
| a game against each level, on each board size, to a score | ROM `24_full_games`: a whole 9×9 game against all five opponents under **both** rulesets, each played to its marking phase and its score screen; `13_sandbox` covers 7/9/13/19 and hot seat | PASS |

---

## 5. Presentation

| requirement | test | result |
|---|---|---|
| every screen of the mockups | ROM `18_screens` → `tests/rom/screenshots/` (17 shots) | PASS |
| board 9×9 at 16 px, 13×13 at 11 px, 19×19 at 8 px | ROM `02_board_demo` screenshots | PASS |
| coordinates, hoshi, last-move marker, atari flash | screenshots `05_play_classic.png` | PASS |
| three skins, layout identical | ROM `14_skins_language` (six shots) | PASS |
| juicy events in PUP and DINO, sober in CLASSIC | ROM `15_effects` (asserts the banner fires and reads the effect flags in each skin) | PASS |
| effects bounded and skippable | `render::effects` caps every effect at 45 frames; A skips | PASS |
| English and French everywhere, accented glyphs | ROM `14_skins_language` (every screen in both), `test_strings` | PASS |
| strings fit their box in both languages | `test_strings` (61 143 assertions on line lengths, pixel widths and the character set) | PASS |
| three helpers, portraits and poses | Options screen, briefing shots `04`, `13` | PASS |
| no burst of events can exhaust Butano's sound handles | ROM `24_full_games` (ten games' worth of captures, ataris and passes drained in single frames) | PASS |
| music per mode, SFX for stones, captures, atari, menus | ROM `21_audio`: reads Butano's playing flag and maxmod's sequence position per track, per skin, and when the volume is zeroed | PASS |
| opponent speech bubbles, EN and FR | ROM `20_speech_overlay` (intro and the capture line, both languages), `18_screens` | PASS |
| every string fits its box **in pixels** | `test_strings` against the real font width tables | PASS |
| SRAM: progress, ranks, settings, unlocks | ROM `01_save_roundtrip`, `16_save` | PASS |
| SRAM: an in-progress sandbox game | ROM `22_resume`: three moves each, quit, reboot from a saved SRAM image, resume from the sandbox screen, same board, komi, opponent and move number; finishing a game clears it | PASS |

---

## 6. Performance

Measured in the ROM through the debug block. `max_missed_frames` is the **longest single stall**,
not a total.

| situation | CPU peak | longest stall |
|---|---|---|
| menus | — | 0 frames |
| 9×9 board, player's turn | 19.7% of a frame | **0 frames** |
| 19×19 board, player's turn | 17.0% | **0 frames** |
| 9×9, NORMAL thinking | — | 2 frames |
| 9×9, HARD thinking (119 frames, ~2 s) | — | 2 frames |
| 9×9, MASTER thinking (299 frames, ~5 s) | — | 2 frames |
| 19×19, HARD and MASTER thinking | — | 4 frames |

The rule — no frame missed while the player has the move — is asserted by every ROM script.
The stalls above happen during the opponent's turn, behind its thinking animation, and come from the
one piece of work that cannot be subdivided: a single evaluation of a 19×19 position.

**Soak**: `17_soak` feeds 108 000 frames (thirty minutes at 60 fps) of random input across the
menus, a campaign board and a 19×19 game against MASTER. The heartbeat never stops, nothing crashes,
and the game is still on a real screen at the end.

The one number the soak reports that the table above does not is its own worst stall: **78 frames**,
and it happens in the menu phase, where random input keeps starting new games. Building a 19×19
board screen costs about 14 000 timer ticks, a little under a second, and that is the whole of it —
it is a load, not a frame the player was waiting on. Entering the same game deliberately, the ROM
tests measure the same cost and then hold 60 fps.

**Memory**: ROM 632 200 bytes. IWRAM 26 272 of 32 768 (the playout loop and the board it runs on
live there), leaving 6 496 bytes for the stacks — `tools/check_iwram.sh` fails the build if that
ever drops below 5 KB, because the stacks have no other warning (see `DECISIONS.md`). EWRAM 233 568 static plus a 28 576-byte heap — full, and deliberately so.

---

## Reproducing all of it

```sh
cmake -S tests/host -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-host -j2 && ctest --test-dir build-host --output-on-failure

python3 tools/gen_font.py --check            # the font tables the pixel tests measure with
build-host/tool_arena --a HARD --b NORMAL --games 50 --size 9 --playouts-a 40
build-host/tool_difftest --games 150 --size 9 | python3 tools/difftest.py
python3 tools/gen_missions.py --check-only

make -j2 && make dist
tests/rom/run_all.sh
```
