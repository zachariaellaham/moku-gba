// Mission text ids (names, objectives, rank hints, dialogue, tsumego stages, rival
// lines). Part of the same Str enum: STRING_LIST = UI_STRING_LIST + MISSION_TEXT_LIST.
#pragma once
#define MISSION_TEXT_LIST(X) \
    /* Mission 1 — First Stone (7x7): placing stones, turns */ \
    X(MNAME_01) \
    X(OBJ_01) \
    X(RANKH_01) \
    X(DLG_01_BEFORE) \
    X(DLG_01_HINT) \
    X(DLG_01_WIN) \
    X(DLG_01_FAIL) \
    /* Mission 2 — Breath (7x7): liberties */ \
    X(MNAME_02) \
    X(OBJ_02) \
    X(RANKH_02) \
    X(DLG_02_BEFORE) \
    X(DLG_02_HINT) \
    X(DLG_02_WIN) \
    X(DLG_02_FAIL) \
    /* Mission 3 — Take It (7x7): capture */ \
    X(MNAME_03) \
    X(OBJ_03) \
    X(RANKH_03) \
    X(DLG_03_BEFORE) \
    X(DLG_03_HINT) \
    X(DLG_03_WIN) \
    X(DLG_03_FAIL) \
    /* Mission 4 — Run! (7x7): escaping atari */ \
    X(MNAME_04) \
    X(OBJ_04) \
    X(RANKH_04) \
    X(DLG_04_BEFORE) \
    X(DLG_04_HINT) \
    X(DLG_04_WIN) \
    X(DLG_04_FAIL) \
    /* Mission 5 — Connect (9x9): connection vs cut */ \
    X(MNAME_05) \
    X(OBJ_05) \
    X(RANKH_05) \
    X(DLG_05_BEFORE) \
    X(DLG_05_HINT) \
    X(DLG_05_WIN) \
    X(DLG_05_FAIL) \
    /* Mission 6 — Ladder (9x9): the ladder */ \
    X(MNAME_06) \
    X(OBJ_06) \
    X(RANKH_06) \
    X(DLG_06_BEFORE) \
    X(DLG_06_HINT) \
    X(DLG_06_WIN) \
    X(DLG_06_FAIL) \
    /* Mission 7 — Last Breath (9x9): capturing a group (mockups 1c / 2d) */ \
    X(MNAME_07) \
    X(OBJ_07) \
    X(RANKH_07) \
    X(DLG_07_BEFORE) \
    X(DLG_07_HINT) \
    X(DLG_07_WIN) \
    X(DLG_07_FAIL) \
    /* Mission 8 — Net (9x9): the net (geta) */ \
    X(MNAME_08) \
    X(OBJ_08) \
    X(RANKH_08) \
    X(DLG_08_BEFORE) \
    X(DLG_08_HINT) \
    X(DLG_08_WIN) \
    X(DLG_08_FAIL) \
    /* Mission 9 — Ko (9x9): the ko rule */ \
    X(MNAME_09) \
    X(OBJ_09) \
    X(RANKH_09) \
    X(DLG_09_BEFORE) \
    X(DLG_09_HINT) \
    X(DLG_09_WIN) \
    X(DLG_09_FAIL) \
    /* Mission 10 — One Eye (9x9): one eye dies */ \
    X(MNAME_10) \
    X(OBJ_10) \
    X(RANKH_10) \
    X(DLG_10_BEFORE) \
    X(DLG_10_HINT) \
    X(DLG_10_WIN) \
    X(DLG_10_FAIL) \
    /* Mission 11 — Two Eyes (9x9): life */ \
    X(MNAME_11) \
    X(OBJ_11) \
    X(RANKH_11) \
    X(DLG_11_BEFORE) \
    X(DLG_11_HINT) \
    X(DLG_11_WIN) \
    X(DLG_11_FAIL) \
    /* Mission 12 — False Eye (9x9): false eyes */ \
    X(MNAME_12) \
    X(OBJ_12) \
    X(RANKH_12) \
    X(DLG_12_BEFORE) \
    X(DLG_12_HINT) \
    X(DLG_12_WIN) \
    X(DLG_12_FAIL) \
    /* Mission 13 — Snapback (9x9): snapback */ \
    X(MNAME_13) \
    X(OBJ_13) \
    X(RANKH_13) \
    X(DLG_13_BEFORE) \
    X(DLG_13_HINT) \
    X(DLG_13_WIN) \
    X(DLG_13_FAIL) \
    /* Mission 14 — Seki (9x9): seki */ \
    X(MNAME_14) \
    X(OBJ_14) \
    X(RANKH_14) \
    X(DLG_14_BEFORE) \
    X(DLG_14_HINT) \
    X(DLG_14_WIN) \
    X(DLG_14_FAIL) \
    /* Mission 15 — Count (9x9 game vs EASY): scoring. EMBER shows up from here on. */ \
    X(MNAME_15) \
    X(OBJ_15) \
    X(RANKH_15) \
    X(DLG_15_BEFORE) \
    X(DLG_15_HINT) \
    X(DLG_15_WIN) \
    X(DLG_15_FAIL) \
    /* Mission 16 — Cut (13x13): cutting */ \
    X(MNAME_16) \
    X(OBJ_16) \
    X(RANKH_16) \
    X(DLG_16_BEFORE) \
    X(DLG_16_HINT) \
    X(DLG_16_WIN) \
    X(DLG_16_FAIL) \
    /* Mission 17 — Invade 3-3 (13x13): the 3-3 invasion */ \
    X(MNAME_17) \
    X(OBJ_17) \
    X(RANKH_17) \
    X(DLG_17_BEFORE) \
    X(DLG_17_HINT) \
    X(DLG_17_WIN) \
    X(DLG_17_FAIL) \
    /* Mission 18 — Life & Death (13x13): five chained tsumego */ \
    X(MNAME_18) \
    X(OBJ_18) \
    X(RANKH_18) \
    X(DLG_18_BEFORE) \
    X(DLG_18_HINT) \
    X(DLG_18_WIN) \
    X(DLG_18_FAIL) \
    /* Mission 19 — Whole Board (13x13 game vs NORMAL) */ \
    X(MNAME_19) \
    X(OBJ_19) \
    X(RANKH_19) \
    X(DLG_19_BEFORE) \
    X(DLG_19_HINT) \
    X(DLG_19_WIN) \
    X(DLG_19_FAIL) \
    /* Mission 20 — Tengen (19x19 vs HARD, 3 handicap stones) */ \
    X(MNAME_20) \
    X(OBJ_20) \
    X(RANKH_20) \
    X(DLG_20_BEFORE) \
    X(DLG_20_HINT) \
    X(DLG_20_WIN) \
    X(DLG_20_FAIL) \
    /* Mission 18 stages — five chained tsumego (MissionDef::stages) */ \
    X(MNAME_T18_1) \
    X(OBJ_T18_1) \
    X(DLG_T18_1_BEFORE) \
    X(DLG_T18_1_HINT) \
    X(DLG_T18_1_WIN) \
    X(MNAME_T18_2) \
    X(OBJ_T18_2) \
    X(DLG_T18_2_BEFORE) \
    X(DLG_T18_2_HINT) \
    X(DLG_T18_2_WIN) \
    X(MNAME_T18_3) \
    X(OBJ_T18_3) \
    X(DLG_T18_3_BEFORE) \
    X(DLG_T18_3_HINT) \
    X(DLG_T18_3_WIN) \
    X(MNAME_T18_4) \
    X(OBJ_T18_4) \
    X(DLG_T18_4_BEFORE) \
    X(DLG_T18_4_HINT) \
    X(DLG_T18_4_WIN) \
    X(MNAME_T18_5) \
    X(OBJ_T18_5) \
    X(DLG_T18_5_BEFORE) \
    X(DLG_T18_5_HINT) \
    X(DLG_T18_5_WIN) \
    X(DLG_T18_FAIL) \
    /* EMBER the rival, one line per mission from 15 on (shown after the tutor in the briefing) */ \
    X(DLG_RIVAL_M15) \
    X(DLG_RIVAL_M16) \
    X(DLG_RIVAL_M17) \
    X(DLG_RIVAL_M18) \
    X(DLG_RIVAL_M19) \
    X(DLG_RIVAL_M20)
