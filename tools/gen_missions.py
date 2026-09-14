#!/usr/bin/env python3
"""Generate src/game/missions_data.cpp from readable ASCII diagrams, and prove every claim.

Each mission is a position plus a promise ("this ladder works", "this net cannot be escaped",
"only this move makes seki"). The promises are checked here with the reference rules engine in
tools/goref.py before a single byte is written, and again against the real C++ engine in
tests/host/test_missions.cpp. A mission whose promise fails is a bug the player would hit.

Diagram characters:
    .  empty        X  black stone      O  white stone
Overlay characters (the `marks` grid, same size, optional):
    *  marked point (MissionDef::marked)        +  second marked set (marked2)
    1-9  the scripted solution, in order        .  nothing

Run: python3 tools/gen_missions.py [--check-only]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from goref import BLACK, WHITE, EMPTY, Board, CAPTURED, ESCAPED, capture_search, other, read_ladder, two_eyes

# --------------------------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------------------------
def pad(rows, size):
    """Pad a partial diagram out to size x size with empty points."""
    out = [r + "." * (size - len(r)) for r in rows]
    out += ["." * size] * (size - len(out))
    return out[:size]


def parse_marks(marks, size):
    """Returns (marked, marked2, solution) as lists of (x, y); solution ordered by its digit."""
    marked, marked2, sol = [], [], []
    if not marks:
        return marked, marked2, sol
    marks = pad(marks, size)
    for y, row in enumerate(marks):
        for x, ch in enumerate(row):
            if ch == "*":
                marked.append((x, y))
            elif ch == "+":
                marked2.append((x, y))
            elif ch.isdigit() and ch != "0":
                sol.append((int(ch), (x, y)))
    sol.sort()
    return marked, marked2, [p for _, p in sol]


def board_of(m):
    return Board(rows=pad(m["rows"], m["board"]))


# --------------------------------------------------------------------------------------------
# the campaign
# --------------------------------------------------------------------------------------------
# opponent: NONE (passes), VERY_EASY, EASY, NORMAL, HARD, MASTER
# rank_s/a/b: the player-move counts that earn S, A and B (anything more is C)
MISSIONS = [
    dict(
        id=1, board=7, objective="PLACE_ON_MARKED", param=3, opponent="NONE",
        rows=["......."],
        marks=[".......",
               "..1....",
               ".......",
               "...2...",
               ".......",
               "....3..",
               "......."],
        marked_from_solution=True,
        move_limit=0, rank=(3, 4, 6), unlock="NONE",
        teaches="placing stones on the intersections",
    ),
    dict(
        id=2, board=7, objective="REDUCE_TO_1_LIB", param=1, opponent="NONE",
        rows=[".......",
              ".......",
              "...X...",
              "...O...",
              ".......",
              ".......",
              "......."],
        marks=[".......",
               ".......",
               ".......",
               "..1*2..",
               ".......",
               ".......",
               "......."],
        move_limit=5, rank=(2, 3, 4), unlock="NONE",
        teaches="liberties: a stone breathes through its empty neighbours",
    ),
    dict(
        id=3, board=7, objective="CAPTURE_MARKED", param=0, opponent="NONE",
        rows=[".......",
              ".......",
              "...X...",
              "..XOX..",
              ".......",
              ".......",
              "......."],
        marks=[".......",
               ".......",
               ".......",
               "...*...",
               "...1...",
               ".......",
               "......."],
        move_limit=4, rank=(1, 2, 3), unlock="NONE",
        teaches="capture: fill the last liberty",
    ),
    dict(
        id=4, board=7, objective="SAVE_MARKED", param=3, opponent="NONE",
        rows=[".......",
              ".......",
              "...O...",
              "..OXO..",
              ".......",
              ".......",
              "......."],
        marks=[".......",
               ".......",
               ".......",
               "...*...",
               "...1...",
               ".......",
               "......."],
        move_limit=4, rank=(1, 2, 3), unlock="NONE",
        teaches="escaping atari by extending",
    ),
    dict(
        id=5, board=9, objective="CONNECT_MARKED", param=0, opponent="NONE",
        rows=["..........",
              ".........",
              ".........",
              "....O....",
              "...X.X...",
              "....O....",
              ".........",
              ".........",
              "........."],
        marks=[".........",
               ".........",
               ".........",
               ".........",
               "...*1+...",
               ".........",
               ".........",
               ".........",
               "........."],
        move_limit=4, rank=(1, 2, 3), unlock="NONE",
        teaches="connecting against a cut",
    ),
    dict(
        id=6, board=9, objective="CAPTURE_LADDER", param=0, opponent="EASY",
        rows=[".........",
              "......X..",
              ".....XO..",
              ".....X...",
              ".........",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=[".........",
               ".........",
               "......*1.",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        move_limit=10, rank=(5, 7, 9), unlock="NONE",
        teaches="the ladder: atari on the side it runs to",
    ),
    dict(
        id=7, board=9, objective="CAPTURE_MARKED", param=0, opponent="NONE",
        rows=[".........",
              ".........",
              ".........",
              "....XX...",
              "...XOOX..",
              "....X....",
              ".........",
              ".........",
              "........."],
        marks=[".........",
               ".........",
               ".........",
               ".........",
               "....**...",
               ".....1...",
               ".........",
               ".........",
               "........."],
        move_limit=4, rank=(1, 2, 3), unlock="SANDBOX_9_EASY",
        teaches="a group shares its liberties",
    ),
    dict(
        id=8, board=9, objective="CAPTURE_NET", param=0, opponent="EASY",
        rows=[".........",
              ".........",
              "...XX....",
              "..XO.....",
              "..X......",
              ".....O...",
              ".........",
              ".........",
              "........."],
        marks=[".........",
               ".........",
               ".........",
               "...*.+...",
               "...+1....",
               ".........",
               ".........",
               ".........",
               "........."],
        move_limit=6, rank=(1, 2, 4), unlock="NONE",
        teaches="the net: catch a stone a ladder cannot hold",
    ),
    dict(
        id=9, board=9, objective="KO_WIN", param=1, opponent="EASY",
        rows=["XXOO.....",
              "XO.O.....",
              "XXOO.....",
              "XXXX.....",
              ".........",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=[".........",
               "..*......",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        marked2_points=[(1, 1)],
        solution_points=[(2, 1), (1, 1)],
        move_limit=6, rank=(2, 3, 5), unlock="NONE",
        teaches="the ko rule: White cannot take back at once",
    ),
    dict(
        id=10, board=9, objective="KILL_MARKED", param=0, opponent="EASY",
        rows=["OO.X.....",
              "O.OX.....",
              "OOOX.....",
              "XXXX.....",
              ".........",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=["**1*.....",
               "*2*......",
               "***......",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        marked_is_group=(0, 0),
        move_limit=6, rank=(2, 3, 5), unlock="NONE",
        teaches="one eye is not enough",
    ),
    dict(
        id=11, board=9, objective="MAKE_TWO_EYES", param=4, opponent="EASY",
        rows=["...XO....",
              "XXXXO....",
              "OOOOO....",
              ".........",
              ".........",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=[".1.*.....",
               "****.....",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        marked_is_group=(0, 1),
        move_limit=4, rank=(1, 2, 3), unlock="NORMAL",
        teaches="two eyes are life",
    ),
    dict(
        id=12, board=9, objective="KILL_MARKED", param=0, opponent="EASY",
        rows=["XOOO.....",
              "XO.OX....",
              "XXOXX....",
              "XXXXX....",
              ".........",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=[".***1....",
               ".*2*.....",
               "..*......",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        marked_is_group=(1, 0),
        move_limit=6, rank=(2, 3, 5), unlock="NONE",
        teaches="a false eye is no eye",
    ),
    dict(
        id=13, board=9, objective="SNAPBACK", param=2, opponent="EASY",
        rows=["..OX.....",
              "OOOX.....",
              "XXXX.....",
              ".........",
              ".........",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=["..*......",
               "***......",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        marked_is_group=(2, 0),
        solution_points=[(0, 0), (0, 0)],
        move_limit=5, rank=(2, 3, 4), unlock="NONE",
        teaches="the snapback: give one, take five",
    ),
    dict(
        id=14, board=9, objective="SEKI_SURVIVE", param=6, opponent="EASY",
        rows=["X.OX.....",
              "X.O......",
              "XXOX.....",
              "OOOX.....",
              "XXXX.....",
              ".........",
              ".........",
              ".........",
              "........."],
        marks=["*........",
               "*..1.....",
               "*........",
               ".........",
               ".........",
               ".........",
               ".........",
               ".........",
               "........."],
        marked_is_group=(0, 0),
        marked2_is_group=(2, 0),
        solution_points=[(3, 1)] + [(-1, -1)] * 6,
        move_limit=14, rank=(3, 5, 8), unlock="NONE",
        teaches="seki: a truce that counts as life",
    ),
    dict(
        id=15, board=9, objective="WIN_BY_MARGIN", param=10, opponent="EASY",
        rows=["........."],
        marks=None,
        move_limit=0, rank=(0, 0, 0), rank_by_margin=(20, 10, 1), unlock="NONE",
        teaches="counting a whole game",
    ),
    dict(
        id=16, board=13, objective="CAPTURE_MARKED", param=0, opponent="EASY",
        rows=[".......XXXXX.",
              ".......XOOOX.",
              ".......X.....",
              ".......XXO...",
              ".......XXXXX.",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              "............."],
        marks=None,
        marked_is_group=(8, 1),
        solution_points=[(9, 2), (10, 2), (8, 2)],
        move_limit=8, rank=(4, 6, 8), unlock="13",
        teaches="cut white apart, then stop the cut group reconnecting",
    ),
    dict(
        id=17, board=13, objective="LIVE_IN_CORNER", param=6, opponent="NORMAL",
        rows=["XX.XO........",
              "X..XO........",
              "XXXXO........",
              "OOOOO........",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              "............."],
        marks=["++++.........",
               "++++.........",
               "++++.........",
               "++++.........",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               "............."],
        solution_points=[(2, 1)],
        move_limit=8, rank=(6, 8, 8), unlock="HARD",
        teaches="the shape that lives in a corner after a 3-3 invasion",
    ),
    dict(
        id=18, board=13, objective="TSUMEGO", param=5, opponent="NONE",
        rows=["............."],
        marks=None,
        move_limit=0, rank=(6, 9, 12), unlock="NONE",
        teaches="five problems in a row",
    ),
    dict(
        id=19, board=13, objective="WIN_GAME", param=0, opponent="NORMAL",
        rows=["............."],
        marks=None,
        move_limit=0, rank=(0, 0, 0), rank_by_margin=(40, 20, 1), unlock="NONE",
        teaches="a whole game on a real board",
    ),
    dict(
        id=20, board=19, objective="WIN_GAME", param=0, opponent="HARD", handicap=3,
        rows=["..................."],
        marks=None,
        move_limit=0, rank=(0, 0, 0), rank_by_margin=(20, 10, 1), unlock="19_MASTER",
        teaches="the full board against a master",
    ),
]

# Mission 18: five chained problems, each in its own corner of the 13x13 board.
STAGES_18 = [
    dict(
        name=1, board=13, objective="CAPTURE_MARKED", param=0, opponent="NONE",
        rows=[".............",
              "..X..........",
              ".XOX.........",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              "............."],
        marks=[".............",
               ".............",
               "..*..........",
               "..1..........",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               "............."],
        move_limit=3, teaches="capture",
    ),
    dict(
        name=2, board=13, objective="MAKE_TWO_EYES", param=3, opponent="NONE",
        rows=["........OX...",
              "........OXXXX",
              "........OOOOO",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              "............."],
        marks=[".........*.*.",
               ".........****",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               "............."],
        marked_is_group=(9, 0),
        solution_points=[(11, 0)],
        move_limit=3, teaches="make two eyes",
    ),
    dict(
        name=3, board=13, objective="KILL_MARKED", param=0, opponent="EASY",
        rows=[".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              "XXXX.........",
              "OO.X.........",
              "O.OX.........",
              "OOOX.........",
              "XXXX........."],
        marks=[".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               "**1*.........",
               "*2*..........",
               "***..........",
               "............."],
        marked_is_group=(0, 9),
        move_limit=5, teaches="one eye dies",
    ),
    dict(
        name=4, board=13, objective="SNAPBACK", param=2, opponent="EASY",
        rows=[".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              ".........XXXX",
              ".........XOOO",
              ".........XO.."],
        marks=None,
        marked_is_group=(10, 11),
        solution_points=[(12, 12), (12, 12)],
        move_limit=5, teaches="snapback",
    ),
    dict(
        name=5, board=13, objective="CAPTURE_LADDER", param=0, opponent="EASY",
        rows=[".............",
              ".............",
              ".............",
              ".............",
              "..........X..",
              ".........XO..",
              ".........X...",
              ".............",
              ".............",
              ".............",
              ".............",
              ".............",
              "............."],
        marks=[".............",
               ".............",
               ".............",
               ".............",
               ".............",
               "..........*1.",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               ".............",
               "............."],
        move_limit=8, teaches="ladder",
    ),
]


# --------------------------------------------------------------------------------------------
# verification
# --------------------------------------------------------------------------------------------
def group_points(board, seed):
    return sorted(board.group(seed)[0])


def verify(m, label):
    """Checks a mission's promise with the reference engine. Returns (marked, marked2, solution)."""
    size = m["board"]
    b = board_of(m)
    marked, marked2, sol = parse_marks(m.get("marks"), size)
    if m.get("solution_points"):
        sol = [tuple(p) for p in m["solution_points"]]
    if m.get("marked_from_solution"):
        marked = list(sol)
    if "marked_is_group" in m:
        marked = group_points(b, m["marked_is_group"])
    if "marked2_is_group" in m:
        marked2 = group_points(b, m["marked2_is_group"])
    if "marked2_points" in m:
        marked2 = [tuple(p) for p in m["marked2_points"]]
    obj = m["objective"]
    problems = []

    def need(cond, msg):
        if not cond:
            problems.append(msg)

    # every setup position must be legal: no stone without a liberty
    for y in range(size):
        for x in range(size):
            if b.at((x, y)) != EMPTY and not b.group((x, y))[1]:
                problems.append("stone at %s has no liberties" % ((x, y),))

    play_moves = [p for p in sol if p[0] >= 0]
    # Only the first move has to be playable in the starting position: later ones may sit on points
    # that a capture clears (mission 9 settles the ko where White's stone stands today).
    if play_moves:
        need(b.is_legal(play_moves[0], BLACK), "the first solution move %s is not legal" % (play_moves[0],))

    if obj == "PLACE_ON_MARKED":
        need(len(sol) == m["param"], "expected %d solution points" % m["param"])
    elif obj == "REDUCE_TO_1_LIB":
        t = marked[0]
        nb = b.copy()
        for p in play_moves:
            nb.play(p, BLACK)
        need(len(nb.group(t)[1]) == 1, "marked stone has %d liberties after the solution" % len(nb.group(t)[1]))
        half = b.copy()
        for p in play_moves[:-1]:
            half.play(p, BLACK)
        need(len(half.group(t)[1]) > 1, "the last move is not needed")
    elif obj == "CAPTURE_MARKED":
        t = marked[0]
        nb = b.copy()
        for p in play_moves:
            nb.play(p, BLACK)
        need(nb.at(t) == EMPTY, "the marked group survives the solution")
    elif obj == "SAVE_MARKED":
        t = marked[0]
        need(len(b.group(t)[1]) == 1, "the marked stone should start in atari")
        nb = b.copy()
        for p in play_moves:
            nb.play(p, BLACK)
        libs = len(nb.group(play_moves[-1])[1])
        need(libs >= m["param"], "after the solution the group has %d liberties, wanted %d" % (libs, m["param"]))
    elif obj == "CONNECT_MARKED":
        need(len(marked) == 1 and len(marked2) == 1, "connect needs one stone in each marked set")
        need(b.group(marked[0])[0] != b.group(marked2[0])[0], "the marked groups are already connected")
        nb = b.copy()
        for p in play_moves:
            nb.play(p, BLACK)
        need(marked2[0] in nb.group(marked[0])[0], "the solution does not connect them")
    elif obj == "CAPTURE_LADDER":
        t = marked[0]
        need(len(b.group(t)[1]) == 2, "a ladder starts from two liberties, found %d" % len(b.group(t)[1]))
        good, bad = [], []
        for lib in sorted(b.group(t)[1]):
            nb = b.copy()
            nb.play(lib, BLACK)
            (good if read_ladder(nb, t, False, 60) == CAPTURED else bad).append(lib)
        need(len(good) == 1, "expected exactly one winning atari, found %s" % (good,))
        need(len(bad) >= 1, "the losing atari should be a real trap")
        need(play_moves and play_moves[0] in good, "the scripted first move %s is not the winning atari %s" % (sol[:1], good))
        # count how many player moves the ladder takes
        nb = b.copy()
        nb.play(play_moves[0], BLACK)
        moves = 1
        while nb.at(t) != EMPTY and moves < 40:
            libs = nb.group(t)[1]
            if len(libs) != 1:
                problems.append("ladder broke: %d liberties" % len(libs))
                break
            esc = list(libs)[0]
            if not nb.is_legal(esc, WHITE):
                break
            nb.play(esc, WHITE)
            if len(nb.group(t)[1]) >= 3:
                problems.append("white escaped")
                break
            nxt = None
            for lib in sorted(nb.group(t)[1]):
                if not nb.is_legal(lib, BLACK):
                    continue
                cb = nb.copy()
                cb.play(lib, BLACK)
                if cb.at(t) == EMPTY or read_ladder(cb, t, False, 60) == CAPTURED:
                    nxt = lib
                    break
            if nxt is None:
                problems.append("no winning atari at move %d" % moves)
                break
            nb.play(nxt, BLACK)
            moves += 1
        m["_ladder_moves"] = moves
        need(nb.at(t) == EMPTY, "the ladder does not capture")
    elif obj == "CAPTURE_NET":
        t = marked[0]
        need(len(b.group(t)[1]) == 2, "the net target should have two liberties")
        for lib in sorted(b.group(t)[1]):
            nb = b.copy()
            nb.play(lib, BLACK)
            need(read_ladder(nb, t, False, 60) == ESCAPED, "the ladder at %s works, so the net is not the only way" % (lib,))
        need(strict_net(b, t, play_moves[0]), "the net at %s does not hold" % (sol[0],))
    elif obj == "KO_WIN":
        t = marked[0]
        need(b.at(t) == EMPTY, "the ko point must be empty")
        need(len(marked2) == 1, "the point White would retake must be marked")
        victims = [q for q in b.neighbours(t) if b.at(q) == WHITE and len(b.group(q)[1]) == 1]
        need(len(victims) == 1, "expected exactly one white stone in atari next to the ko point")
        if victims:
            need(victims[0] == marked2[0], "the retake point should be where the captured stone sits")
            need(len(b.group(victims[0])[0]) == 1, "a ko captures a single stone")
            nb = b.copy()
            nb.play(t, BLACK)
            need(nb.ko is not None, "the engine should set a ko ban")
            need(len(nb.group(t)[1]) == 1, "after the capture the black stone should itself be in atari")
            need(not nb.is_legal(marked2[0], WHITE), "White must be banned from taking straight back")
            nb2 = nb.copy()
            nb2.ko = None                      # a move elsewhere lifts the ban
            need(nb2.is_legal(marked2[0], WHITE), "after a move elsewhere White should be able to retake")
            nb3 = nb.copy()
            nb3.ko = None                      # White has answered elsewhere; the ban is spent
            need(nb3.is_legal(marked2[0], BLACK), "Black must be able to close the ko")
            nb3.play(marked2[0], BLACK)
            need(len(nb3.group(marked2[0])[1]) >= 1, "closing the ko must leave Black with a liberty")
    elif obj == "KILL_MARKED":
        t = marked[0]
        nb = b.copy()
        for i, p in enumerate(play_moves):
            nb.play(p, BLACK)
            if i + 1 < len(play_moves):
                need(nb.at(t) != EMPTY, "the solution captures too early")
        need(nb.at(t) == EMPTY, "the marked group survives the solution")
    elif obj == "MAKE_TWO_EYES":
        t = marked[0]
        nb = b.copy()
        for p in play_moves:
            nb.play(p, BLACK)
        need(two_eyes(nb, t), "the solution does not make two eyes")
        for alt in sorted(b.group(t)[1]):
            if alt in sol:
                continue
            ab = b.copy()
            ab.play(alt, BLACK)
            need(not two_eyes(ab, t), "playing %s also lives, so the problem has no single answer" % (alt,))
    elif obj == "SNAPBACK":
        t = marked[0]
        nb = b.copy()
        nb.play(play_moves[0], BLACK)
        need(len(nb.group(play_moves[0])[1]) == 1, "the sacrifice stone should be in atari")
        wlibs = nb.group(t)[1]
        need(len(wlibs) == 1, "after the sacrifice the white group should be in atari")
        if len(wlibs) == 1:
            reply = list(wlibs)[0]
            need(nb.is_legal(reply, WHITE), "white cannot capture the sacrifice")
            nb.play(reply, WHITE)
            need(nb.at(play_moves[0]) == EMPTY, "white did not take the sacrifice")
            need(len(nb.group(t)[1]) == 1, "the snapback needs white back in atari")
            nb.play(play_moves[0], BLACK)
            need(nb.at(t) == EMPTY, "the recapture fails")
            need(nb.captures[BLACK] >= m["param"], "the snapback takes %d stones, wanted %d" % (nb.captures[BLACK], m["param"]))
    elif obj == "SEKI_SURVIVE":
        nb = b.copy()
        nb.play(play_moves[0], BLACK)
        bl = sorted(nb.group(marked[0])[1])
        wl = sorted(nb.group(marked2[0])[1])
        need(bl == wl and len(bl) == 2, "the solution does not produce two shared liberties (%s vs %s)" % (bl, wl))
        for alt in sorted(b.group(marked[0])[1]):
            ab = b.copy()
            if not ab.is_legal(alt, BLACK):
                continue
            ab.play(alt, BLACK)
            abl = sorted(ab.group(marked[0])[1])
            awl = sorted(ab.group(marked2[0])[1]) if ab.at(marked2[0]) != EMPTY else []
            need(not (abl == awl and len(abl) == 2), "filling %s also makes seki" % (alt,))
    elif obj == "LIVE_IN_CORNER":
        need(len(marked2) >= 4, "the corner area must be marked")
        need(len(play_moves) >= 1, "needs a first move")
        seed = None
        for (x, y) in marked2:
            if b.at((x, y)) == BLACK:
                seed = (x, y)
                break
        need(seed is not None, "the corner must already hold a black group to save")
        if seed:
            nb = b.copy()
            for p in play_moves:
                nb.play(p, BLACK)
            need(two_eyes(nb, seed), "the solution does not make the corner group alive")
            stones, _ = nb.group(seed)
            inside = all((x, y) in marked2 for (x, y) in stones)
            need(inside, "the living group leaves the marked corner")
            # and every other point of the eye space must fail
            for alt in sorted(b.group(seed)[1]):
                if alt in play_moves:
                    continue
                ab = b.copy()
                if not ab.is_legal(alt, BLACK):
                    continue
                ab.play(alt, BLACK)
                need(not two_eyes(ab, seed), "playing %s also lives, so the answer is not unique" % (alt,))
    elif obj in ("WIN_BY_MARGIN", "WIN_GAME", "TSUMEGO"):
        pass
    else:
        problems.append("unknown objective " + obj)

    for p in problems:
        print("  FAIL %-14s %s" % (label, p))
    return marked, marked2, sol, problems


def strict_net(b, t, m):
    """The net test the ROM itself runs (missions.cpp): after black plays m the target has at most
    two liberties, and every white escape is answered by an immediate capture or a won ladder."""
    if not b.is_legal(m, BLACK):
        return False
    nb = b.copy()
    nb.play(m, BLACK)
    if nb.at(t) == EMPTY:
        return False                      # that is a capture, not a net
    libs = sorted(nb.group(t)[1])
    if len(libs) > 2:
        return False
    for escape in libs:
        if not nb.is_legal(escape, WHITE):
            continue
        eb = nb.copy()
        eb.play(escape, WHITE)
        if eb.at(t) == EMPTY:
            continue
        answered = False
        for reply in sorted(eb.group(t)[1]):
            if not eb.is_legal(reply, BLACK):
                continue
            rb = eb.copy()
            rb.play(reply, BLACK)
            if rb.at(t) == EMPTY or read_ladder(rb, t, False, 60) == CAPTURED:
                answered = True
                break
        if not answered:
            return False
    return True


def check_net_unique(m):
    """For the net mission, make sure no other move traps the stone (so the lesson is exact)."""
    size = m["board"]
    b = board_of(m)
    marked, _, sol = parse_marks(m.get("marks"), size)
    t = marked[0]
    winners = []
    for mv in b.legal_moves(BLACK):
        nb = b.copy()
        nb.play(mv, BLACK)
        if nb.at(t) == EMPTY or strict_net(b, t, mv):
            winners.append(mv)
    return winners


# --------------------------------------------------------------------------------------------
# code generation
# --------------------------------------------------------------------------------------------
def xy_array(name, pts):
    if not pts:
        return None, "nullptr", 0
    body = ", ".join("{%d, %d}" % (x, y) for x, y in pts)
    return "const XY %s[] = { %s };" % (name, body), name, len(pts)


def stones_array(name, rows, size):
    stones = []
    for y, row in enumerate(pad(rows, size)):
        for x, ch in enumerate(row):
            if ch == "X":
                stones.append((x, y, "go::BLACK"))
            elif ch == "O":
                stones.append((x, y, "go::WHITE"))
    if not stones:
        return None, "nullptr", 0
    body = ", ".join("{%d, %d, %s}" % s for s in stones)
    return "const SetupStone %s[] = { %s };" % (name, body), name, len(stones)


def emit_mission(m, prefix, text_prefix, stage_list_name=None, stage_count=0):
    size = m["board"]
    marked, marked2, sol, _ = verify(m, prefix)
    lines = []
    decl, stones_ref, stones_n = stones_array(prefix + "_STONES", m["rows"], size)
    if decl:
        lines.append(decl)
    decl, marked_ref, marked_n = xy_array(prefix + "_MARKED", marked)
    if decl:
        lines.append(decl)
    decl, marked2_ref, marked2_n = xy_array(prefix + "_MARKED2", marked2)
    if decl:
        lines.append(decl)
    decl, sol_ref, sol_n = xy_array(prefix + "_SOLUTION", sol)
    if decl:
        lines.append(decl)
    rank = m.get("rank", (0, 0, 0))
    fields = [
        ("id", str(m["id"])),
        ("board", str(size)),
        ("player", "go::BLACK"),
        ("opponent", "Opponent::" + m["opponent"]),
        ("handicap", str(m.get("handicap", 0))),
        ("rules", "go::Ruleset::AREA"),
        ("komi_x2", str(m.get("komi_x2", 15 if m["objective"] in ("WIN_BY_MARGIN", "WIN_GAME") else 0))),
        ("stones", stones_ref), ("stone_count", str(stones_n)),
        ("marked", marked_ref), ("marked_count", str(marked_n)),
        ("marked2", marked2_ref), ("marked2_count", str(marked2_n)),
        ("objective", "Objective::" + m["objective"]),
        ("param", str(m["param"])),
        ("move_limit", str(m.get("move_limit", 0))),
        ("rank_s", str(rank[0])), ("rank_a", str(rank[1])), ("rank_b", str(rank[2])),
        ("solution", sol_ref), ("solution_len", str(sol_n)),
        ("unlock", "UNLOCK_" + m.get("unlock", "NONE")),
        ("stages", stage_list_name or "nullptr"), ("stage_count", str(stage_count)),
        ("name_id", "uint16_t(Str::MNAME_%s)" % text_prefix),
        ("objective_id", "uint16_t(Str::OBJ_%s)" % text_prefix),
        ("rank_hint_id", "uint16_t(Str::RANKH_%s)" % text_prefix if not text_prefix.startswith("T18") else "0"),
        ("text_before", "uint16_t(Str::DLG_%s_BEFORE)" % text_prefix),
        ("text_hint", "uint16_t(Str::DLG_%s_HINT)" % text_prefix),
        ("text_win", "uint16_t(Str::DLG_%s_WIN)" % text_prefix),
        ("text_fail", "uint16_t(Str::DLG_%s_FAIL)" % (text_prefix if not text_prefix.startswith("T18") else "T18")),
    ]
    init = ",\n    ".join(".%s = %s" % (k, v) for k, v in fields)
    return lines, "{\n    %s\n}" % init


def main():
    check_only = "--check-only" in sys.argv
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = os.path.join(root, "src", "game", "missions_data.cpp")

    failures = 0
    arrays, inits = [], []

    stage_arrays, stage_inits = [], []
    for s in STAGES_18:
        pre = "T18_%d" % s["name"]
        lines, init = emit_mission(dict(s, id=18, param=s.get("param", 0)), pre, pre)
        stage_arrays.extend(lines)
        stage_inits.append(init)
    stage_table = "const MissionDef STAGES_18[] = {\n%s\n};" % ",\n".join(stage_inits)

    for m in MISSIONS:
        pre = "M%02d" % m["id"]
        text = "%02d" % m["id"]
        is18 = m["id"] == 18
        lines, init = emit_mission(m, pre, text, "STAGES_18" if is18 else None, len(STAGES_18) if is18 else 0)
        arrays.extend(lines)
        inits.append(init)

    # extra proofs that do not fit the table-driven checks
    net = next(m for m in MISSIONS if m["objective"] == "CAPTURE_NET")
    winners = check_net_unique(net)
    _, _, net_sol = parse_marks(net["marks"], net["board"])[0], None, parse_marks(net["marks"], net["board"])[2]
    if winners != net_sol[:1]:
        print("  FAIL M08        moves that trap the stone: %s, wanted exactly %s" % (winners, net_sol[:1]))
        failures += 1
    else:
        print("  ok   M08        the net at %s is the only move that traps the stone" % (net_sol[0],))

    lad = next(m for m in MISSIONS if m["id"] == 6)
    print("  ok   M06        the ladder takes %d player moves" % lad.get("_ladder_moves", 0))

    for m in MISSIONS:
        _, _, _, probs = verify(m, "M%02d" % m["id"])
        failures += len(probs)
    for s in STAGES_18:
        _, _, _, probs = verify(dict(s, id=18), "T18_%d" % s["name"])
        failures += len(probs)

    if failures:
        print("\n%d verification failures - not writing missions_data.cpp" % failures)
        return 1
    if check_only:
        print("\nall missions verified")
        return 0

    body = [
        "// Generated by tools/gen_missions.py - do not edit by hand, edit the generator.",
        "// Every position in this file is verified by the reference engine in tools/goref.py:",
        "// the ladders work, the net is the only move that traps the stone, the snapback recaptures,",
        "// only one move makes the seki, and each solution really solves its problem.",
        "",
        '#include "game/missions.h"',
        '#include "game/strings_ids.h"',
        "",
        "namespace campaign {",
        "namespace {",
        "",
    ]
    body.extend(stage_arrays)
    body.append("")
    body.append(stage_table)
    body.append("")
    body.extend(arrays)
    body.append("")
    body.append("const MissionDef MISSIONS[MISSION_COUNT] = {\n%s\n};" % ",\n".join(inits))
    body.extend([
        "",
        "}  // namespace",
        "",
        "const MissionDef& mission(int index) { return MISSIONS[index]; }",
        "const MissionDef* all_missions() { return MISSIONS; }",
        "",
        "}  // namespace campaign",
        "",
    ])
    with open(out_path, "w") as f:
        f.write("\n".join(body))
    print("\nwrote %s" % out_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
