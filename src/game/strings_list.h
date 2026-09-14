// The string table, as an X-macro list of ids. strings_ids.h turns it into enum Str;
// strings_en.h / strings_fr.h (UI) and mission_text_en.h / mission_text_fr.h (missions)
// hold the texts in the same order. Ids are grouped by category and the prefix fixes the
// layout budget: HUDL_ 12 chars, VAL_ 12, MENU_ 14, TITLE_/HINT_ 20, NAV_ 40, BAN_ 12,
// MNAME_ 16, OBJ_/RANKH_ 2x26, DLG_ 3x26, MSG_/BLURB_ 2x34, SAY_ 2x24, INTJ_ 2x22,
// LVLB_ 3x18, OPP_ 8, OPPT_ 20, WHO_ 12, VOICE_ 10, CHAP_ 14 (docs/decisions/strings.md).
// Multi-line texts break lines with an explicit \n; scenes never re-wrap them.
#pragma once
#include "mission_text_list.h"

#define UI_STRING_LIST(X) \
    /* Title screen (mockup 1a) */ \
    X(TITLE_TACTICS) \
    X(MENU_CAMPAIGN) \
    X(MENU_SANDBOX) \
    X(MENU_OPTIONS) \
    X(MENU_CONTINUE) \
    X(MSG_TITLE_FOOTER) \
    X(MSG_DEBUG_UNLOCK) \
    /* Save select */ \
    X(TITLE_SAVE_SELECT) \
    X(HUDL_FILE) \
    X(VAL_EMPTY) \
    X(VAL_NEW_GAME) \
    X(MENU_ERASE) \
    X(HUDL_MISSIONS) \
    X(HUDL_STARS) \
    X(HUDL_TIME) \
    X(MSG_ERASE_CONFIRM) \
    X(MENU_YES) \
    X(MENU_NO) \
    X(MSG_SAVED) \
    X(MSG_SAVE_FAILED) \
    X(NAV_SAVE_SELECT) \
    /* Campaign map (mockup 1b) */ \
    X(HUDL_MISSION) \
    X(CHAP_1) \
    X(CHAP_2) \
    X(CHAP_3) \
    X(CHAP_4) \
    X(CHAP_5) \
    X(VAL_LOCKED) \
    X(MSG_LOCKED) \
    X(HINT_MAP_MORE) \
    X(NAV_MAP) \
    /* Briefing (mockups 1c / 2d) */ \
    X(HUDL_OBJECTIVE) \
    X(HUDL_RANK) \
    X(WHO_SEN) \
    X(WHO_INDI) \
    X(WHO_REX) \
    X(HINT_ADVANCE) \
    X(NAV_BRIEFING) \
    /* HUD labels (mockups 1d, 1e) */ \
    X(HUDL_TO_PLAY) \
    X(HUDL_CAPT) \
    X(HUDL_MOVE) \
    X(HUDL_KOMI) \
    X(HUDL_LIBERTIES) \
    X(HUDL_INFO) \
    X(HUDL_MOVES) \
    X(HUDL_ESTIMATE) \
    X(HUDL_TERRITORY) \
    X(HUDL_PRISONERS) \
    X(HUDL_AREA) \
    X(HUDL_TOTAL) \
    X(HUDL_RESULT) \
    X(HUDL_DEAD) \
    X(HUDL_CAPTURES) \
    X(HUDL_UNDOS) \
    X(HUDL_HINTS) \
    X(HUDL_UNLOCKED) \
    X(HUDL_LEVEL) \
    X(HUDL_RECORD) \
    /* Stone colours (per skin: classic / pup / dino) */ \
    X(VAL_BLACK) \
    X(VAL_WHITE) \
    X(VAL_BLUE) \
    X(VAL_ORANGE) \
    X(VAL_OBSIDIAN) \
    X(VAL_AMBER) \
    X(VAL_LTR_BLACK) \
    X(VAL_LTR_WHITE) \
    X(VAL_LTR_BLUE) \
    X(VAL_LTR_ORANGE) \
    X(VAL_LTR_OBSIDIAN) \
    X(VAL_LTR_AMBER) \
    /* In-game button hints (mockup 1d bottom bar) */ \
    X(HINT_A_PLACE) \
    X(HINT_B_UNDO) \
    X(HINT_SEL_HINT) \
    X(HINT_R_PASS) \
    X(HINT_START_PAUSE) \
    X(HINT_A_CONFIRM) \
    X(HINT_B_CANCEL) \
    X(HINT_L_R_PANEL) \
    X(HINT_A_NEXT) \
    X(HINT_HINTS_SHOWN) \
    X(HINT_MARK_DEAD) \
    X(HINT_MARK_DONE) \
    /* Event banners (mockups 1e, 2b, 2c). CAPTURE gets a runtime suffix: tr(BAN_CAPTURE) + " +N" */ \
    X(BAN_ATARI) \
    X(BAN_ATARI_PUP) \
    X(BAN_ATARI_DINO) \
    X(BAN_CAPTURE) \
    X(BAN_PASS) \
    X(BAN_KO) \
    X(BAN_RESIGN) \
    /* Atari hint line (mockup 1e). Scene appends " " + coordinate + "." */ \
    X(MSG_ATARI_BLACK) \
    X(MSG_ATARI_WHITE) \
    /* Illegal moves */ \
    X(MSG_ILLEGAL_OCCUPIED) \
    X(MSG_ILLEGAL_SUICIDE) \
    X(MSG_ILLEGAL_KO) \
    X(MSG_ILLEGAL_SUPERKO) \
    /* Mission clear / fail (mockup 1f) */ \
    X(TITLE_MISSION_CLEAR) \
    X(TITLE_MISSION_FAIL) \
    X(MENU_RETRY) \
    X(MENU_NEXT) \
    X(MENU_MAP) \
    X(MSG_STARS_EARNED) \
    /* Unlock messages, in UNLOCK_* order (missions.h): index with MSG_UNLOCK_SANDBOX_9_EASY + id - 1 */ \
    X(MSG_UNLOCK_SANDBOX_9_EASY) \
    X(MSG_UNLOCK_NORMAL) \
    X(MSG_UNLOCK_13) \
    X(MSG_UNLOCK_HARD) \
    X(MSG_UNLOCK_19_MASTER) \
    /* Sandbox setup (mockup 1g) */ \
    X(TITLE_SANDBOX) \
    X(HUDL_BOARD) \
    X(HUDL_YOU_PLAY) \
    X(HUDL_AI) \
    X(HUDL_HANDICAP) \
    X(HUDL_SCORING) \
    X(HUDL_SKIN) \
    X(HUDL_PLAYERS) \
    X(MENU_START_GAME) \
    X(VAL_AREA) \
    X(VAL_TERRITORY) \
    X(VAL_VS_AI) \
    X(VAL_HOTSEAT) \
    X(VAL_PLAYER_1) \
    X(VAL_PLAYER_2) \
    X(MSG_PASS_DEVICE) \
    X(NAV_SANDBOX) \
    /* AI levels and their one-line descriptions (mockup 1g right column) */ \
    X(VAL_LVL_VERY_EASY) \
    X(VAL_LVL_EASY) \
    X(VAL_LVL_NORMAL) \
    X(VAL_LVL_HARD) \
    X(VAL_LVL_MASTER) \
    X(LVLB_VERY_EASY) \
    X(LVLB_EASY) \
    X(LVLB_NORMAL) \
    X(LVLB_HARD) \
    X(LVLB_MASTER) \
    /* Choose opponent (mockup 2a). Character names are proper nouns: identical in both languages. */ \
    X(TITLE_CHOOSE_OPPONENT) \
    X(OPP_PEBBLE) \
    X(OPP_SPROUT) \
    X(OPP_KOAN) \
    X(OPP_EMBER) \
    X(OPP_TENGEN) \
    X(OPPT_PEBBLE) \
    X(OPPT_SPROUT) \
    X(OPPT_KOAN) \
    X(OPPT_EMBER) \
    X(OPPT_TENGEN) \
    X(BLURB_PEBBLE) \
    X(BLURB_SPROUT) \
    X(BLURB_KOAN) \
    X(BLURB_EMBER) \
    X(BLURB_TENGEN) \
    X(MSG_BEST_WIN_BY) \
    X(MSG_NO_RECORD) \
    X(VAL_W) \
    X(VAL_L) \
    X(NAV_OPPONENT) \
    /* Opponent speech bubbles (mockup 2b): intro, on capturing, on being captured, win, lose, atari taunt */ \
    X(SAY_PEBBLE_INTRO) \
    X(SAY_PEBBLE_CAPTURE) \
    X(SAY_PEBBLE_CAPTURED) \
    X(SAY_PEBBLE_WIN) \
    X(SAY_PEBBLE_LOSE) \
    X(SAY_PEBBLE_ATARI) \
    X(SAY_SPROUT_INTRO) \
    X(SAY_SPROUT_CAPTURE) \
    X(SAY_SPROUT_CAPTURED) \
    X(SAY_SPROUT_WIN) \
    X(SAY_SPROUT_LOSE) \
    X(SAY_SPROUT_ATARI) \
    X(SAY_KOAN_INTRO) \
    X(SAY_KOAN_CAPTURE) \
    X(SAY_KOAN_CAPTURED) \
    X(SAY_KOAN_WIN) \
    X(SAY_KOAN_LOSE) \
    X(SAY_KOAN_ATARI) \
    X(SAY_EMBER_INTRO) \
    X(SAY_EMBER_CAPTURE) \
    X(SAY_EMBER_CAPTURED) \
    X(SAY_EMBER_WIN) \
    X(SAY_EMBER_LOSE) \
    X(SAY_EMBER_ATARI) \
    X(SAY_TENGEN_INTRO) \
    X(SAY_TENGEN_CAPTURE) \
    X(SAY_TENGEN_CAPTURED) \
    X(SAY_TENGEN_WIN) \
    X(SAY_TENGEN_LOSE) \
    X(SAY_TENGEN_ATARI) \
    /* Options (mockup 2e) */ \
    X(TITLE_OPTIONS) \
    X(HUDL_LANGUAGE) \
    X(HUDL_HELPER) \
    X(HUDL_EFFECTS) \
    X(HUDL_CONFIRM) \
    X(HUDL_MUSIC) \
    X(HUDL_SFX) \
    X(VAL_LANG_EN) \
    X(VAL_LANG_FR) \
    X(VAL_SKIN_CLASSIC) \
    X(VAL_SKIN_PUP) \
    X(VAL_SKIN_DINO) \
    X(VAL_EFFECTS_OFF) \
    X(VAL_EFFECTS_JUICY) \
    X(VAL_CONFIRM_2A) \
    X(VAL_CONFIRM_1A) \
    X(MSG_HELPER_ROLE) \
    X(MSG_HELPER_SEN) \
    X(MSG_HELPER_INDI) \
    X(MSG_HELPER_REX) \
    X(NAV_OPTIONS) \
    /* Helper interjections (bubble on events; also prefixes for tutor lines) */ \
    X(INTJ_SEN_ATARI) \
    X(INTJ_SEN_CAPTURE) \
    X(INTJ_SEN_CLEAR) \
    X(INTJ_SEN_FAIL) \
    X(INTJ_SEN_THINK) \
    X(INTJ_INDI_ATARI) \
    X(INTJ_INDI_CAPTURE) \
    X(INTJ_INDI_CLEAR) \
    X(INTJ_INDI_FAIL) \
    X(INTJ_INDI_THINK) \
    X(INTJ_REX_ATARI) \
    X(INTJ_REX_CAPTURE) \
    X(INTJ_REX_CLEAR) \
    X(INTJ_REX_FAIL) \
    X(INTJ_REX_THINK) \
    X(VOICE_INDI) \
    X(VOICE_REX) \
    /* Pause menu (mockup 1h) */ \
    X(TITLE_PAUSED) \
    X(MENU_RESUME) \
    X(MENU_UNDO) \
    X(MENU_HINT) \
    X(MENU_PASS) \
    X(MENU_ESTIMATE) \
    X(MENU_RESIGN) \
    X(MENU_QUIT) \
    X(MSG_RESIGN_CONFIRM) \
    X(MSG_QUIT_CONFIRM) \
    X(MSG_ESTIMATE_NOTE) \
    X(MSG_NO_HINT) \
    X(MSG_NO_UNDO) \
    X(NAV_PAUSE) \
    /* End of game: dead stone marking, then the score screen */ \
    X(TITLE_GAME_OVER) \
    X(TITLE_SCORE) \
    X(MSG_MARK_DEAD) \
    X(MSG_STUCK) \
    X(MSG_MARK_AI) \
    X(MSG_BLACK_WINS_BY) \
    X(MSG_WHITE_WINS_BY) \
    X(MSG_BLACK_WINS) \
    X(MSG_WHITE_WINS) \
    X(MSG_BY_RESIGNATION) \
    X(MSG_DRAW) \
    X(MSG_YOU_WIN) \
    X(MSG_YOU_LOSE) \
    X(MSG_POINTS) \
    /* Mission failure reasons (MissionRun::fail_reason_id) */ \
    X(MSG_FAIL_MOVES) \
    X(MSG_FAIL_ESCAPED) \
    X(MSG_FAIL_CAPTURED) \
    X(MSG_FAIL_LADDER) \
    X(MSG_FAIL_LIVED) \
    X(MSG_FAIL_DIED) \
    X(MSG_FAIL_INSIDE) \
    X(MSG_FAIL_MARGIN) \
    X(MSG_FAIL_LOST) \
    X(MSG_FAIL_RESIGNED)

// Everything the game can say, UI first, then the campaign text.
#define STRING_LIST(X) \
    UI_STRING_LIST(X) \
    MISSION_TEXT_LIST(X)
