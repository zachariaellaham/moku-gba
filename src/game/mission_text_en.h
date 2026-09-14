// English mission text. Order must match MISSION_TEXT_LIST in mission_text_list.h.
#pragma once
// Rows are T(ID, text); same order as the id list.
#define MISSION_TEXT_EN(T) \
    /* Mission 1 — First Stone (7x7): placing stones, turns */ \
    T(MNAME_01, "FIRST STONE") \
    T(OBJ_01, "Place a stone on each of\nthe three marks.") \
    T(RANKH_01, "Three moves, no hints.") \
    T(DLG_01_BEFORE, "Welcome. Stones sit on the\ncrossings, not in squares.\nPut one on each mark.") \
    T(DLG_01_HINT, "Move with the D-pad, press\nA to place a stone.") \
    T(DLG_01_WIN, "Good. The board is yours\nto shape now.") \
    T(DLG_01_FAIL, "No harm done. Try the\nmarked points again.") \
    /* Mission 2 — Breath (7x7): liberties */ \
    T(MNAME_02, "BREATH") \
    T(OBJ_02, "Leave the marked stone\nwith one breath.") \
    T(RANKH_02, "Two moves, no undo.") \
    T(DLG_02_BEFORE, "Every empty point next to\na stone is a breath. Take\nthem and the stone falls.") \
    T(DLG_02_HINT, "Play next to the stone, on\nan empty crossing.") \
    T(DLG_02_WIN, "One breath left. That is\ncalled atari.") \
    T(DLG_02_FAIL, "It found more room. Take\nthe breaths first.") \
    /* Mission 3 — Take It (7x7): capture */ \
    T(MNAME_03, "TAKE IT") \
    T(OBJ_03, "Capture the marked white\nstone.") \
    T(RANKH_03, "One move, no hints.") \
    T(DLG_03_BEFORE, "A stone with no breath\nis captured. This one has\nonly one left.") \
    T(DLG_03_HINT, "Fill the last breath.") \
    T(DLG_03_WIN, "Clean. The stone is yours.") \
    T(DLG_03_FAIL, "It slipped away. Aim for\nthe last breath.") \
    /* Mission 4 — Run! (7x7): escaping atari */ \
    T(MNAME_04, "RUN!") \
    T(OBJ_04, "Give your marked stone\nthree breaths.") \
    T(RANKH_04, "One move, no undo.") \
    T(DLG_04_BEFORE, "Your stone is in atari:\none breath left. Run out\ninto the open board.") \
    T(DLG_04_HINT, "Extend away from white,\nnot into the edge.") \
    T(DLG_04_WIN, "Three breaths. It lives.") \
    T(DLG_04_FAIL, "Caught. Next time run for\nthe wide side.") \
    /* Mission 5 — Connect (9x9): connection vs cut */ \
    T(MNAME_05, "CONNECT") \
    T(OBJ_05, "Join your two marked\ngroups into one.") \
    T(RANKH_05, "One move, no hints.") \
    T(DLG_05_BEFORE, "Two stones that touch\nshare their breaths.\nUnited, they hold.") \
    T(DLG_05_HINT, "One point joins them both.") \
    T(DLG_05_WIN, "One group now. Much\nstronger.") \
    T(DLG_05_FAIL, "White cut between them.\nFind the joining point.") \
    /* Mission 6 — Ladder (9x9): the ladder */ \
    T(MNAME_06, "LADDER") \
    T(OBJ_06, "Capture the marked stone\nwith a ladder.") \
    T(RANKH_06, "Five moves, no hints.") \
    T(DLG_06_BEFORE, "Atari, then atari again.\nEach step it keeps one\nbreath: that is a ladder.") \
    T(DLG_06_HINT, "Keep it at one breath and\npush it to the edge.") \
    T(DLG_06_WIN, "The ladder never breaks\nhere. Well read.") \
    T(DLG_06_FAIL, "It broke out. Atari from\nthe other side.") \
    /* Mission 7 — Last Breath (9x9): capturing a group (mockups 1c / 2d) */ \
    T(MNAME_07, "LAST BREATH") \
    T(OBJ_07, "Capture the marked white\ngroup.") \
    T(RANKH_07, "Do it in 1 move, no undo.") \
    T(DLG_07_BEFORE, "Two stones side by side\nshare one set of breaths.\nThis pair has one left.") \
    T(DLG_07_HINT, "Both stones fall together.\nOne point does it.") \
    T(DLG_07_WIN, "Two prisoners. A group\ndies as one.") \
    T(DLG_07_FAIL, "They got a breath back.\nCount again.") \
    /* Mission 8 — Net (9x9): the net (geta) */ \
    T(MNAME_08, "NET") \
    T(OBJ_08, "Catch the marked stone\nwithout a ladder.") \
    T(RANKH_08, "One move, no hints.") \
    T(DLG_08_BEFORE, "The ladder fails here.\nDon’t touch the stone:\nthrow a net around it.") \
    T(DLG_08_HINT, "Play one point away, in\nthe diagonal gap.") \
    T(DLG_08_WIN, "The net holds. No escape\nsquare left.") \
    T(DLG_08_FAIL, "Atari only pushes it out.\nSurround from afar.") \
    /* Mission 9 — Ko (9x9): the ko rule */ \
    T(MNAME_09, "KO") \
    T(OBJ_09, "Take the ko, then close\nit for good.") \
    T(RANKH_09, "Two moves, no hints.") \
    T(DLG_09_BEFORE, "Ko: white cannot take it\nback at once. That free\nmove is yours.") \
    T(DLG_09_HINT, "Capture, then fill the\npoint white would retake.") \
    T(DLG_09_WIN, "The ko is closed. That\nfree move was everything.") \
    T(DLG_09_FAIL, "White took it back. Close\nthe ko while you can.") \
    /* Mission 10 — One Eye (9x9): one eye dies */ \
    T(MNAME_10, "ONE EYE") \
    T(OBJ_10, "Kill the marked white\ngroup.") \
    T(RANKH_10, "Two moves, no hints.") \
    T(DLG_10_BEFORE, "An eye is an empty point\ninside a group. One eye\nis not enough to live.") \
    T(DLG_10_HINT, "Fill the outside breaths,\nthen the eye.") \
    T(DLG_10_WIN, "One eye is a house with\nno door. It died.") \
    T(DLG_10_FAIL, "It made a second eye.\nStart over.") \
    /* Mission 11 — Two Eyes (9x9): life */ \
    T(MNAME_11, "TWO EYES") \
    T(OBJ_11, "Make two eyes for the\nmarked group.") \
    T(RANKH_11, "One move, no hints.") \
    T(DLG_11_BEFORE, "Two separate eyes and the\ngroup lives for ever.\nWhite can never fill both.") \
    T(DLG_11_HINT, "Split the big empty space\nin two.") \
    T(DLG_11_WIN, "Alive. Nothing can touch\nit now.") \
    T(DLG_11_FAIL, "One eye only, or a false\none. Try again.") \
    /* Mission 12 — False Eye (9x9): false eyes */ \
    T(MNAME_12, "FALSE EYE") \
    T(OBJ_12, "Kill the group with the\nfalse eye.") \
    T(RANKH_12, "Two moves, no hints.") \
    T(DLG_12_BEFORE, "This eye is false: the\ncorner stones don’t hold\ntogether. Prove it.") \
    T(DLG_12_HINT, "Capture the stone that\nholds the corner.") \
    T(DLG_12_WIN, "A false eye is no eye\nat all.") \
    T(DLG_12_FAIL, "You filled the real eye.\nAttack the corner.") \
    /* Mission 13 — Snapback (9x9): snapback */ \
    T(MNAME_13, "SNAPBACK") \
    T(OBJ_13, "Take two stones or more\nwith a snapback.") \
    T(RANKH_13, "Two moves, no undo.") \
    T(DLG_13_BEFORE, "Give one stone away. When\nwhite takes it, you take\nthe whole group back.") \
    T(DLG_13_HINT, "Play inside, in the two\npoint space.") \
    T(DLG_13_WIN, "Snapback! One stone given,\nthe group taken.") \
    T(DLG_13_FAIL, "Nothing came back. The\nsacrifice goes inside.") \
    /* Mission 14 — Seki (9x9): seki */ \
    T(MNAME_14, "SEKI") \
    T(OBJ_14, "Reach seki and leave it\nalone for six moves.") \
    T(RANKH_14, "One stone, then let it be.") \
    T(DLG_14_BEFORE, "Seki: neither side can\nfill the shared breaths.\nBoth groups live.") \
    T(DLG_14_HINT, "Don’t play inside. Pass\nor play far away.") \
    T(DLG_14_WIN, "Seki. A truce that counts\nas life.") \
    T(DLG_14_FAIL, "Filling a shared breath\nkills your own group.") \
    /* Mission 15 — Count (9x9 game vs EASY): scoring. EMBER shows up from here on. */ \
    T(MNAME_15, "COUNT") \
    T(OBJ_15, "Win this 9×9 game by five\npoints or more.") \
    T(RANKH_15, "Win by five: that is an S.") \
    T(DLG_15_BEFORE, "Now a whole game. Count as\nyou play: your area, plus\nprisoners, minus komi.") \
    T(DLG_15_HINT, "Take the last big empty\ncorner first.") \
    T(DLG_15_WIN, "Counted and won. Numbers\nnever lie.") \
    T(DLG_15_FAIL, "Close. Secure your borders\nbefore the end.") \
    /* Mission 16 — Cut (13x13): cutting */ \
    T(MNAME_16, "CUT") \
    T(OBJ_16, "Cut white apart, then\ntake the cut group.") \
    T(RANKH_16, "Four moves, no hints.") \
    T(DLG_16_BEFORE, "Two stones that only touch\nat a corner can be cut.\nCut first, ask later.") \
    T(DLG_16_HINT, "Play the cutting point\nbetween them.") \
    T(DLG_16_WIN, "Cut, then captured. That\nis the order.") \
    T(DLG_16_FAIL, "White connected. The cut\nwas the only chance.") \
    /* Mission 17 — Invade 3-3 (13x13): the 3-3 invasion */ \
    T(MNAME_17, "INVADE 3-3") \
    T(OBJ_17, "Live inside the marked\ncorner.") \
    T(RANKH_17, "Live in six moves.") \
    T(DLG_17_BEFORE, "The 3-3 point is the key\nto a corner. Dive in and\nmake two eyes.") \
    T(DLG_17_HINT, "Slide under his stones,\nthen split the space.") \
    T(DLG_17_WIN, "Alive in the corner. His\nwall is not enough.") \
    T(DLG_17_FAIL, "No room left. Start lower\nand take the edge.") \
    /* Mission 18 — Life & Death (13x13): five chained tsumego */ \
    T(MNAME_18, "LIFE & DEATH") \
    T(OBJ_18, "Solve five problems in\na row.") \
    T(RANKH_18, "No hints, no undo.") \
    T(DLG_18_BEFORE, "Five short problems: take,\nlive, kill, trap and read\nto the very end.") \
    T(DLG_18_HINT, "Look for the vital point\nof the shape.") \
    T(DLG_18_WIN, "Five for five. Your eyes\nare sharp now.") \
    T(DLG_18_FAIL, "Reset and read one move\ndeeper.") \
    /* Mission 19 — Whole Board (13x13 game vs NORMAL) */ \
    T(MNAME_19, "WHOLE BOARD") \
    T(OBJ_19, "Beat KOAN on the full\nboard.") \
    T(RANKH_19, "Beat KOAN: that is an S.") \
    T(DLG_19_BEFORE, "Corners first, then sides,\nthen the centre. Play the\nbiggest empty place.") \
    T(DLG_19_HINT, "Answer his approach, keep\nyour groups connected.") \
    T(DLG_19_WIN, "A full game, well played.\nKOAN bows to you.") \
    T(DLG_19_FAIL, "He took the big points.\nTake them first next time.") \
    /* Mission 20 — Tengen (19x19 vs HARD, 3 handicap stones) */ \
    T(MNAME_20, "TENGEN") \
    T(OBJ_20, "Beat EMBER on 19×19\nwith three stones.") \
    T(RANKH_20, "Beat EMBER: that is an S.") \
    T(DLG_20_BEFORE, "Tengen is the centre of\nthe board. Hold it all.\nThis is the last one.") \
    T(DLG_20_HINT, "Use your three stones:\nplay simply and trade.") \
    T(DLG_20_WIN, "You hold the centre and\nthe game. Go well.") \
    T(DLG_20_FAIL, "EMBER burns bright. Rest,\nthen climb again.") \
    /* Mission 18 stages — five chained tsumego (MissionDef::stages) */ \
    T(MNAME_T18_1, "TAKE ONE") \
    T(OBJ_T18_1, "Capture the marked white\nstone.") \
    T(DLG_T18_1_BEFORE, "A gentle start: one white\nstone, one breath left.") \
    T(DLG_T18_1_HINT, "Take its last breath.") \
    T(DLG_T18_1_WIN, "Taken. Now the real work.") \
    T(MNAME_T18_2, "THE MIDDLE") \
    T(OBJ_T18_2, "Make the marked group\nlive.") \
    T(DLG_T18_2_BEFORE, "Three empty points in a\nrow. Only one of them\nmakes two eyes.") \
    T(DLG_T18_2_HINT, "Take the centre point.") \
    T(DLG_T18_2_WIN, "Two eyes. Alive.") \
    T(MNAME_T18_3, "TWO SPACES") \
    T(OBJ_T18_3, "Kill the marked white\ngroup.") \
    T(DLG_T18_3_BEFORE, "White has two spaces, but\nonly one is a real eye.\nFill the other first.") \
    T(DLG_T18_3_HINT, "The space your stones\nalready touch is no eye.") \
    T(DLG_T18_3_WIN, "One eye only. Dead.") \
    T(MNAME_T18_4, "GIVE ONE") \
    T(OBJ_T18_4, "Capture with a snapback.") \
    T(DLG_T18_4_BEFORE, "The corner again. Offer a\nstone, let white take it,\nthen take everything.") \
    T(DLG_T18_4_HINT, "Play in the corner point\nitself.") \
    T(DLG_T18_4_WIN, "Snapback in the corner.\nSharp.") \
    T(MNAME_T18_5, "TO THE EDGE") \
    T(OBJ_T18_5, "Capture the marked stone\nwith a ladder.") \
    T(DLG_T18_5_BEFORE, "Last one. Chase it to the\nedge, one breath at a go.") \
    T(DLG_T18_5_HINT, "Atari on the open side\nand keep going.") \
    T(DLG_T18_5_WIN, "The ladder holds. All five\nsolved.") \
    T(DLG_T18_FAIL, "Not this time. The stones\ngo back as they were.") \
    /* EMBER the rival, one line per mission from 15 on (shown after the tutor in the briefing) */ \
    T(DLG_RIVAL_M15, "So you can count. One day\nyou will count against me.") \
    T(DLG_RIVAL_M16, "Cutting is my favourite\nmove. Do not disappoint\nme, little stone.") \
    T(DLG_RIVAL_M17, "Invading my corner? Bold.\nI like bold.") \
    T(DLG_RIVAL_M18, "Five problems. I solved\nthem before my first fire.") \
    T(DLG_RIVAL_M19, "KOAN is patient. I am not.\nHurry up and win.") \
    T(DLG_RIVAL_M20, "Three stones ahead and the\nwhole board. No excuses\nleft, challenger.")
