# Localisation and game copy (`src/game/strings*`, `mission_text*`)

Area: strings and localisation. Files: `src/game/strings_list.h`, `mission_text_list.h`, `strings_en.h`,
`strings_fr.h`, `mission_text_en.h`, `mission_text_fr.h`, `strings.cpp`, `strings_lines.h`,
`tests/host/test_strings.cpp`. 431 ids x 2 languages; ~25 KB of ROM
(22 KB literals + 2 x 431 pointers).

```
cmake -S tests/host -B build-host -G Ninja && cmake --build build-host --target test_strings
./build-host/test_strings              # 11 cases, ~56 000 assertions
```

## How the table is built

`strings_ids.h` (frozen contract) turns `STRING_LIST(X)` into `enum class Str`. That list lives in
`strings_list.h` and is `UI_STRING_LIST(X)` followed by `MISSION_TEXT_LIST(X)`
(`mission_text_list.h`), so **UI text and campaign text share one enum and one lookup**.

The four text files are X-macro lists of `T(ID, "text")` rows in the same order:
`UI_TEXT_EN` / `UI_TEXT_FR` and `MISSION_TEXT_EN` / `MISSION_TEXT_FR`. `strings.cpp` expands each
list twice - once for the literal, once for `(uint16_t)Str::ID` - and `static_assert`s that row *i*
carries id *i* and that both tables have exactly `Str::COUNT` rows. A missing, extra, duplicated or
displaced row is a **compile error**, not a silently shifted table, and `tr()` stays a single array
index (`O(1)`, no search, no init code).

Adding a string = one line in `strings_list.h` + one line in each of `strings_en.h` /
`strings_fr.h` (or the two `mission_text_*` files), all at the same position. The id must start
with one of the category prefixes below, or `test_strings` fails.

## Layout budgets

Character budgets per category (the test enforces lines *and* characters per line). They come from
the mockup boxes at 240x160 measured with the real `font_8`/`font_16` metrics
(`include/moku_fonts.h`): body text averages 4.3 px per character, so a 120 px card holds ~27.

| prefix | lines x chars | box in the mockups | measured widest (font_8) |
|---|---|---|---|
| `HUDL_` | 1 x 12 | HUD/row label; 52 px when it shares an 84 px HUD row with a value, 80 px on the 132-208 px rows of the score/options/sandbox screens | 63 px `YOUR RECORD` |
| `VAL_` | 1 x 12 | option and setting values | 61 px `TRÈS FACILE` |
| `MENU_` | 1 x 14 | menu items (pause, title, buttons) | 79 px `ESTIMATE SCORE` |
| `TITLE_` | 1 x 20 | screen titles (`MISSION CLEAR` and friends are drawn in font_16) | 108 px `CHOISIS L’ADVERSAIRE` |
| `HINT_` | 1 x 20 | button hints inside the 84 px HUD column | 81 px `SEL HINT · SHOWN` |
| `NAV_` | 1 x 40 | full-width navigation line (240 px) | 159 px `← → CHANGER · A JOUER · B RETOUR` |
| `BAN_` | 1 x 12 | event banner, font_16, over the 144 px board | 82 px `UH-OH! ATARI` (font_16) |
| `MNAME_` | 1 x 16 | mission name: font_16 on the briefing, font_8 on the map | 113 px `DERNIER SOUFFLE` (font_16) |
| `CHAP_` | 1 x 14 | chapter label on the map info line | 72 px |
| `OBJ_`, `RANKH_` | 2 x 26 | briefing cards, 120 px wide | 119 px |
| `DLG_` | 3 x 26 | dialogue box, 176 px right of the 48 px portrait | 120 px |
| `MSG_`, `BLURB_` | 2 x 34 | full-width panels (200 px) and the opponent panel (150 px) | 145 px |
| `SAY_`, `INTJ_` | 2 x 24 / 2 x 22 | speech bubbles (~120 px) | 111 px |
| `LVLB_` | 3 x 18 | sandbox AI column, 72 px | 72 px |
| `OPP_` / `OPPT_` / `WHO_` / `VOICE_` | 1 x 8 / 20 / 12 / 10 | opponent name, title, speaker name, helper voice prefix | 100 px `LA TORTUE ENDORMIE` |

Deviations from the planned budgets, all deliberate:

- **Mission names 16, not 14 characters**: the mockup's own French name `DERNIER SOUFFLE` is 15,
  and it measures 113 px in font_16 - it fits the 144 px board area and the 120 px briefing card.
- **Banners stay at 12 characters, screen titles move to 20**: `MISSION CLEAR` (13) /
  `MISSION RÉUSSIE` (15) are `TITLE_`, not `BAN_`; the in-play banners are the short ones.
- **Option values 12, not 10**: `TRÈS FACILE`, `OBSIDIENNE`, `VERROUILLÉ` need it and sit in
  132-208 px rows.
- `NAV_` (40) exists because the mockups' navigation lines (`← → CHANGE · A START · B BACK`) are
  drawn across the whole 240 px screen, not in a column.

## Line breaks are explicit

Every multi-line text carries its own `'\n'`; **no scene re-wraps them**. That keeps the French and
the English breaking at the same sensible places (`Deux pierres unies mettent / leurs souffles en
commun.`) instead of at whatever the box width happens to allow.

`bn::sprite_text_generator` raises `BN_ERROR("Invalid character")` on any control character, so a
multi-line string must never be handed to it whole. `src/game/strings_lines.h` has the two helpers:

- `text_lines(text, out, max_lines)` - split on `'\n'`, no allocation, spans into the literal.
- `wrap_text(text, max_width, width_fn, ctx, buffer, buffer_size, out, max_lines)` - greedy word
  wrap for a **narrower** box, used by the play HUD: the briefing objective is written for the
  120 px card, and the HUD column is 84 px. The text is copied into the caller's buffer with its
  own breaks turned into spaces (cutting only between whole UTF-8 sequences), so the spans handed
  back never contain a control character. All 40 objectives re-wrap into <= 3 lines at 84 px
  (checked with the real font metrics; the rendered result is in the validation screenshots below).

## Runtime composition

No `printf` in the ROM: the tables never contain format specifiers. Where a number belongs in a
sentence it goes **at the end**, and the scene appends it:

| id | appended by the scene | checked |
|---|---|---|
| `BAN_CAPTURE` | `" +N"` -> `CAPTURE! +2` / `PRISE ! +2` | <= 12 characters with `+99` |
| `MSG_ATARI_BLACK` / `MSG_ATARI_WHITE` | `" "` + coordinate + `"."` -> `White has one breath left at E4.` | <= 34 characters with `T19.` |
| `MSG_TITLE_FOOTER` | `" 1/3"` | |
| `MSG_BEST_WIN_BY` | `" 12.5"` / `" 12,5"` | |
| `MSG_BLACK_WINS_BY` / `MSG_WHITE_WINS_BY` | `" 6.5 "` + `MSG_POINTS` | |
| `HINT_HINTS_SHOWN` | `" 1/3"` | |
| `HUDL_LIBERTIES` | `" 1"` (the colon is part of the label: `LIBERTÉS :` with the French space) | |
| `VOICE_INDI` / `VOICE_REX` | `" "` + the tutor line, for helper 1 / 2 (SEN speaks unprefixed) | |

Numbers are the scene's job, including the decimal separator: **`7.5` in English, `7,5` in
French** (komi, margins, estimates - all stored as halves, `x2`).

## Ids other tables index arithmetically

Guaranteed by `test_strings` (`tables indexed by an enum value are contiguous`):

- **Missions**: 20 blocks of 7 in mission order - `MNAME_nn`, `OBJ_nn`, `RANKH_nn`,
  `DLG_nn_BEFORE`, `_HINT`, `_WIN`, `_FAIL`. So for `MissionDef` of mission *m*:
  `name_id = (uint16_t)Str::MNAME_01 + 7*(m-1)`, `objective_id = +1`, `rank_hint_id = +2`,
  `text_before = +3`, `text_hint = +4`, `text_win = +5`, `text_fail = +6`.
- **Mission 18 stages**: 5 blocks of 5 - `MNAME_T18_k`, `OBJ_T18_k`, `DLG_T18_k_BEFORE`, `_HINT`,
  `_WIN`; every stage shares `DLG_T18_FAIL`.
- **Rival (EMBER, from mission 15)**: `DLG_RIVAL_M15 + (m - 15)`, one line per mission 15..20,
  shown after the tutor line on the briefing. Speaker name: `OPP_EMBER`.
- **Unlocks**: `MSG_UNLOCK_SANDBOX_9_EASY + (UNLOCK_x - 1)`, in `missions.h` order.
- **AI levels** (`ai::Level` 0..4): `VAL_LVL_VERY_EASY + level`, blurb `LVLB_VERY_EASY + level`.
- **Opponents** (0..4 = PEBBLE, SPROUT, KOAN, EMBER, TENGEN): `OPP_PEBBLE + i`, `OPPT_PEBBLE + i`,
  `BLURB_PEBBLE + i`, and 6 lines each: `SAY_PEBBLE_INTRO + 6*i + event`, event = intro, on
  capturing, on being captured, win, lose, taunt on atari.
- **Helpers** (`save::Settings::helper` 0..2 = SEN, INDI, REX): name `WHO_SEN + h`, description
  `MSG_HELPER_SEN + h`, and 5 interjections `INTJ_SEN_ATARI + 5*h + event`, event = atari,
  capture, mission clear, fail, thinking. Voice prefix `VOICE_INDI + (h-1)` for h = 1, 2.
- **Skins** (0..2): `VAL_SKIN_CLASSIC + skin`. Stone colour names per skin:
  `VAL_BLACK`/`VAL_WHITE` (classic), `VAL_BLUE`/`VAL_ORANGE` (pup), `VAL_OBSIDIAN`/`VAL_AMBER`
  (dino), and the one-letter HUD forms `VAL_LTR_*` (`B 2 · W 0` / `N 2 · B 0` in French).
- **Chapters** on the map: `CHAP_1 + (chapter - 1)` with missions 1-4 = ch.1, 5-9 = ch.2,
  10-14 = ch.3, 15-18 = ch.4, 19-20 = ch.5 (the mockup shows mission 07 as `9×9 · CH.2 CAPTURE`).
- **Fail reasons** for `MissionRun::fail_reason_id`: `MSG_FAIL_MOVES`, `_ESCAPED`, `_CAPTURED`,
  `_LADDER`, `_LIVED`, `_DIED`, `_INSIDE`, `_MARGIN`, `_LOST`, `_RESIGNED`.

## Writing rules

- **Character set**: printable ASCII plus the 46 glyphs both fonts carry
  (`À Â Ç É È Ê Ë Î Ï Ô Ù Û Ü Œ à â ç é è ê ë î ï ô ù û ü œ ’ « » … · ★ ☆ ▲ ▼ ▶ ◀ × ← ↑ → ↓ ▮ ▯ ©`).
  Anything else is a runtime `BN_ERROR`; the test rejects it and so does the validation ROM at
  compile time. The last seven were added for the mockups' arrows, meters and
  `©2026`, and are used here.
- **Typography**: `’` never `'`, `«` `»` never `"`, `…` never `...`. French keeps its space before
  `! ? :` (`ATARI !`, `LIBERTÉS :`, `Ko : joue ailleurs d’abord.`).
- **Voice**: Master Sen is warm, concrete and short - never more than three lines, no jargon
  without an image ("An eye is a house with no door"). The same lines are spoken by INDI and REX;
  only the interjections change. The tutor uses *tu* in French throughout.
- **Names stay names**: PEBBLE, SPROUT, KOAN, EMBER, TENGEN, SEN, INDI, REX, MOKU, KOMI, ATARI,
  SEKI, KO are identical in both languages (34 of 431 entries match; every text longer than 20
  characters differs). Skin names are translated (`CLASSIQUE`, `TOUTOU`, `DINO`), because they are
  descriptions in the options list, not characters.
- **Go vocabulary (FR)**: souffle/liberté, atari, capture/prise, échelle, filet, ko, œil/yeux,
  faux œil, coup de rappel (snapback), seki, coupe, invasion 3-3, vie et mort, territoire, komi,
  hane, point vital, tengen. "Coup de rappel" is used for snapback because the mission has to name
  the shape in a mission title (`COUP DE RAPPEL`, 14 characters).

Rank hints (`RANKH_*`) state the real S threshold from `missions_data.cpp`
(`player_moves + 2*undos + 2*hints <= rank_s`, passes not counted). Missions 15, 19 and 20 are
ranked by the winning margin and currently have `rank_s = 0`, i.e. any win is an S - which is what
their rank hints say. **If those thresholds change, these three strings must change with them.**

## Campaign copy

20 missions, each `before` 2-3 lines, `hint` 1-2, `win` 1-2, `fail` 1-2, plus an objective card
line and a rank hint - written to teach one idea per mission and to be read in a couple of seconds:
1 First Stone (crossings), 2 Breath (liberties), 3 Take It (capture), 4 Run! (escape), 5 Connect,
6 Ladder, 7 Last Breath (group liberties, the mockup's briefing), 8 Net, 9 Ko, 10 One Eye,
11 Two Eyes, 12 False Eye, 13 Snapback, 14 Seki, 15 Count (scoring), 16 Cut, 17 Invade 3-3,
18 Life & Death (five chained tsumego, each written against the generated position in
`missions_data.cpp`: a plain capture, the three-space eye, a group with one false eye, a corner
snapback, a ladder),
19 Whole Board, 20 Tengen. EMBER, the fox general, taunts the player from mission 15 on and is the
opponent of the last mission; TENGEN is the master kept for the sandbox.

## Keeping the copy true to the missions

The campaign copy is written against the positions in `missions_data.cpp`, not against the plan:
mission 9 teaches "take the ko, then close it" because that is what its position and its two-move
solution ask for, the five tsumego describe their own shapes (a plain capture, the three-space eye,
a group whose second space is not an eye, a corner snapback, a ladder), and every rank hint states
that mission's real S threshold. **A changed position or threshold needs its strings changed too** -
tell the strings owner rather than editing the tables by hand: the two occasions someone did, the
text ended up with raw newlines inside the literals, which does not compile for the ROM.

## Validation

1. `test_strings` (11 cases, ~56 000 assertions): both tables complete and non-empty, every id has
   a category and fits its box, character set, EN != FR for 92 % of entries and for every text
   longer than 20 characters, `tr`/`set_language`/`current_language`, out-of-range id -> `"?"`,
   the index arithmetic above, `text_lines` and `wrap_text` (including UTF-8-safe truncation).
2. Host and GBA compilers: `g++ -std=c++20 -Wall -Wextra` and
   `arm-none-eabi-g++ -std=c++20 -Wall -Wextra -mthumb -O2 -fno-exceptions -fno-rtti`, both clean.
3. **Validation ROM** (scratch copy of the Butano template, built with devkitARM and run headless
   in `tests/rom/harness/mokurun`): `static_assert(fonts::text_width(font, text) > 0)` for all 862
   texts in *both* fonts - a missing glyph would not compile - then every line of every string
   measured at runtime through `bn::sprite_text_generator::width()` (no assert, widest font_8 line
   159 px, matching the offline measurement exactly), and six mockup screens rendered in EN and FR
   (briefing 1c, play HUD + DINO banner 2c, options 2e, mission clear 1f, choose opponent 2a) and
   inspected at 3x. 26-33 sprites per screen.

Two things the screenshots taught, worth repeating for every scene:

- A `font_16` line needs its whole 16 px cell on screen or the accent row is clipped
  (`MISSION RÉUSSIE` at y = 6 lost its É; at y = 14 it is perfect).
- The play HUD **must** use `wrap_text` for `OBJ_*`: the authored 26-character lines are 105 px and
  the column is 84 px.
