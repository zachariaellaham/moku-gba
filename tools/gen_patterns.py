#!/usr/bin/env python3
"""Generate src/ai/patterns_table.cpp: the 65536-entry 3x3 pattern weight table.

A pattern is a 3x3 template whose centre is the move point (always empty). The eight
neighbours are encoded, clockwise from the top-left, two bits each:

    0 = empty        1 = colour to move ("own")     2 = opponent      3 = off board

    index = NW<<0 | N<<2 | NE<<4 | E<<6 | SE<<8 | S<<10 | SW<<12 | W<<14

Template characters (rows top to bottom, centre must be '.'):

    .  empty                X  own            O  opponent         #  off board
    ?  anything             x  own or empty   o  opponent/empty    *  any stone
    -  empty or off board   +  own or off board

Every pattern is expanded over the eight symmetries of the square (four rotations x mirror).
The table keeps the highest weight that matched, so a strong shape wins over a weak one.

Weights are a *prior* in 0..255 used three ways:
  * the light playout policy picks, near the last move, among matching patterns;
  * the NORMAL evaluator adds the weight as a shape bonus;
  * MCTS seeds RAVE statistics of new children with it.

Run: python3 tools/gen_patterns.py   (writes src/ai/patterns_table.cpp)
"""

import os
import sys

import numpy as np

# --------------------------------------------------------------------------------------------
# pattern library
# --------------------------------------------------------------------------------------------
# (name, weight, template). Kept deliberately small and readable: 46 shapes, each one a move a
# decent club player would consider locally. Shape names follow standard Go terminology.
PATTERNS = [
    # The four orthogonal neighbours are always pinned down (they are what the shape is made of);
    # diagonals are pinned only when the shape depends on them, and are '?' otherwise.
    # ---- hane: reaching around an enemy stone. The bread and butter of contact fighting. ----
    ("hane_both_sides", 118, """
        XOX
        ...
        ?.?
    """),
    ("hane_simple", 112, """
        XO.
        ...
        ?.?
    """),
    ("hane_wall", 108, """
        XO?
        X..
        ?.?
    """),
    ("hane_edge", 116, """
        XO?
        ...
        ###
    """),
    ("hane_under", 100, """
        O.?
        X..
        ###
    """),
    ("hane_at_head", 114, """
        XOO
        ...
        ?.?
    """),
    # ---- cut: separating two enemy stones that are not solidly connected -------------------
    ("cut_diagonal", 124, """
        XO?
        O.x
        ?.?
    """),
    ("cut_keima", 120, """
        ?X?
        O.O
        ?x?
    """),
    ("cut_edge", 118, """
        XO?
        O..
        ###
    """),
    ("crosscut_extend", 122, """
        OX?
        X.O
        ?.?
    """),
    ("cut_and_capture", 120, """
        OXO
        X.X
        ?O?
    """),
    # ---- connect: answering a peep or repairing a cutting point -----------------------------
    ("connect_against_cut", 114, """
        ?O?
        X.X
        ?O?
    """),
    ("connect_solid", 96, """
        ?X?
        O.X
        ?O?
    """),
    ("connect_edge", 104, """
        X.X
        X.X
        ###
    """),
    ("bamboo", 88, """
        XX?
        ..O
        XX?
    """),
    # ---- tiger mouth / hanging connection: strong shape --------------------------------------
    ("tiger_mouth", 92, """
        X.X
        ...
        ?O?
    """),
    ("tiger_mouth_edge", 94, """
        X.X
        ...
        ###
    """),
    # ---- attachment, extension, stretching out of contact ------------------------------------
    ("attach", 78, """
        ?.?
        O.X
        ?.?
    """),
    ("attach_side", 82, """
        ?O?
        ..X
        ?.?
    """),
    ("extend_from_contact", 86, """
        .O.
        X..
        ?.?
    """),
    ("extend_pressed", 90, """
        OO?
        X..
        ?.?
    """),
    ("extend_edge", 92, """
        X.?
        ..O
        ###
    """),
    # ---- blocking the opponent's advance ------------------------------------------------------
    ("block", 100, """
        ?X?
        O..
        ?.?
    """),
    ("block_edge", 106, """
        O.X
        ...
        ###
    """),
    ("push_through", 98, """
        O.O
        X..
        ?.?
    """),
    # ---- diagonal answers ---------------------------------------------------------------------
    ("kosumi_defend", 76, """
        ?O?
        ..X
        ?X?
    """),
    ("diagonal_enemy", 68, """
        O..
        ...
        ?X?
    """),
    # ---- eye shape and defence ----------------------------------------------------------------
    ("make_eye", 84, """
        XXX
        X.X
        ?X?
    """),
    ("make_eye_edge", 88, """
        XXX
        X.X
        ###
    """),
    ("eye_corner", 90, """
        #XX
        #.X
        ###
    """),
    ("fill_false_eye", 70, """
        XOX
        X.X
        ?X?
    """),
    # ---- enemy shape destruction ---------------------------------------------------------------
    ("peep", 80, """
        ?O?
        X..
        ?O?
    """),
    ("wedge", 102, """
        O.O
        ...
        ?X?
    """),
    ("clamp", 96, """
        ?X?
        ..O
        ?O?
    """),
    ("nose_attach", 74, """
        OOO
        ...
        ?X?
    """),
    ("throw_in", 86, """
        OXO
        O.O
        ?O?
    """),
    # ---- shapes that punish or exploit a contact fight ------------------------------------------
    ("atari_shape", 110, """
        OX?
        O..
        ?.?
    """),
    ("double_contact", 104, """
        ?O?
        O.X
        ?.?
    """),
    ("counter_hane", 108, """
        XO?
        O.X
        ?.?
    """),
    ("descent_edge", 92, """
        ?X?
        O..
        ###
    """),
]

CLASSES = {
    ".": (0,),
    "X": (1,),
    "O": (2,),
    "#": (3,),
    "?": (0, 1, 2, 3),
    "x": (0, 1),
    "o": (0, 2),
    "*": (1, 2),
    "-": (0, 3),
    "+": (1, 3),
}

# 3x3 cells in key order: NW N NE E SE S SW W, as (col, row) with the centre at (1, 1).
ORDER = [(0, 0), (1, 0), (2, 0), (2, 1), (2, 2), (1, 2), (0, 2), (0, 1)]


def parse(template):
    rows = [r.strip() for r in template.strip().splitlines()]
    if len(rows) != 3 or any(len(r) != 3 for r in rows):
        raise ValueError("pattern must be 3x3:\n" + template)
    if rows[1][1] != ".":
        raise ValueError("pattern centre must be '.':\n" + template)
    return {(x, y): rows[y][x] for y in range(3) for x in range(3) if (x, y) != (1, 1)}


def transform(cells, sym):
    """Apply one of the 8 symmetries to a {(x, y): char} map centred on (1, 1)."""
    out = {}
    for (x, y), ch in cells.items():
        dx, dy = x - 1, y - 1
        if sym & 4:  # mirror
            dx = -dx
        for _ in range(sym & 3):  # rotate 90 degrees clockwise
            dx, dy = -dy, dx
        out[(dx + 1, dy + 1)] = ch
    return out


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = os.path.join(root, "src", "ai", "patterns_table.cpp")

    # digits[i] holds, for every key, the 2-bit code at neighbour slot i.
    keys = np.arange(65536, dtype=np.uint32)
    digits = [((keys >> (2 * i)) & 3).astype(np.uint8) for i in range(8)]

    table = np.zeros(65536, dtype=np.uint8)
    stats = []
    for name, weight, template in PATTERNS:
        cells = parse(template)
        if not 0 < weight < 256:
            raise ValueError("weight out of range for " + name)
        matched_before = int((table > 0).sum())
        for sym in range(8):
            t = transform(cells, sym)
            mask = np.ones(65536, dtype=bool)
            for slot, pos in enumerate(ORDER):
                allowed = CLASSES[t[pos]]
                if len(allowed) == 4:
                    continue
                sub = np.zeros(65536, dtype=bool)
                for code in allowed:
                    sub |= digits[slot] == code
                mask &= sub
            np.maximum(table, np.where(mask, weight, 0).astype(np.uint8), out=table)
        stats.append((name, weight, int((table > 0).sum()) - matched_before))

    # Keys that cannot occur on a real board are harmless but let us report how many do.
    plausible = np.ones(65536, dtype=bool)
    for a, b, c in ((0, 1, 2), (2, 3, 4), (4, 5, 6), (6, 7, 0)):  # corner, side, corner triples
        # a border side forces its two adjacent corners to be border too
        plausible &= ~((digits[b] == 3) & ((digits[a] != 3) | (digits[c] != 3)))
    covered = int((table[plausible] > 0).sum())
    total = int(plausible.sum())

    lines = [
        "// Generated by tools/gen_patterns.py - do not edit by hand.",
        "//",
        "// 3x3 shape priors. index = NW<<0 | N<<2 | NE<<4 | E<<6 | SE<<8 | S<<10 | SW<<12 | W<<14,",
        "// two bits per neighbour: 0 empty, 1 own, 2 opponent, 3 off board.",
        "// %d shapes x 8 symmetries; %d of the %d reachable neighbourhoods carry a weight."
        % (len(PATTERNS), covered, total),
        "",
        '#include "ai/patterns.h"',
        "",
        "namespace ai {",
        "",
        "const uint8_t PATTERN_TABLE[PATTERN_TABLE_SIZE] GO_ROM_CONST = {",
    ]
    for i in range(0, 65536, 32):
        chunk = ",".join("%d" % v for v in table[i : i + 32])
        lines.append("    " + chunk + ",")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace ai")
    lines.append("")

    with open(out_path, "w") as f:
        f.write("\n".join(lines))

    print("wrote %s (%d entries, %d non-zero)" % (out_path, 65536, int((table > 0).sum())))
    print("reachable neighbourhoods: %d, with a shape prior: %d (%.1f%%)" % (total, covered, 100.0 * covered / total))
    bands = [(1, 69), (70, 99), (100, 255)]
    for lo, hi in bands:
        n = int(((table >= lo) & (table <= hi) & plausible).sum())
        print("  weight %3d-%3d: %5d reachable keys (%.1f%%)" % (lo, hi, n, 100.0 * n / total))
    widest = max(len(n) for n, _, _ in stats)
    for name, weight, added in stats:
        print("  %-*s w=%3d  new keys %d" % (widest, name, weight, added))
    return 0


if __name__ == "__main__":
    sys.exit(main())
