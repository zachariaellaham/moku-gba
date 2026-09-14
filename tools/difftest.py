#!/usr/bin/env python3
"""Differential test: replay the C++ engine's random games through the Python reference rules.

tests/host/tool_difftest prints one line per move (`move x y colour` or `pass colour`) followed by
the board it produced and its prisoner counts. This script replays the same moves with
tools/goref.py and compares every position, so a disagreement about captures, suicide, ko or
legality shows up as a diff rather than as a strange bug months later.

    ./build-host/tool_difftest --games 200 | python3 tools/difftest.py
"""
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from goref import BLACK, WHITE, EMPTY, Board, CHARS


def main():
    board = None
    games = mismatches = moves = 0
    expect_rows = 0
    rows = []
    pending = None
    for line in sys.stdin:
        line = line.rstrip("\n")
        if expect_rows > 0:
            rows.append(line)
            expect_rows -= 1
            if expect_rows == 0:
                got = "\n".join(rows)
                mine = str(board)
                if got != mine:
                    mismatches += 1
                    print("MISMATCH after %s (game %d, move %d)" % (pending, games, moves))
                    print("engine:\n%s\nreference:\n%s" % (got, mine))
                    if mismatches > 3:
                        return 1
            continue
        if line.startswith("game "):
            games += 1
            board = Board(size=int(line.split()[1]))
            moves = 0
        elif line.startswith("move "):
            _, x, y, c = line.split()
            colour = BLACK if c == "B" else WHITE
            p = (int(x), int(y))
            if not board.is_legal(p, colour):
                print("reference says illegal: %s for %s (game %d, move %d)" % (p, c, games, moves))
                return 1
            board.play(p, colour)
            moves += 1
            pending = line
        elif line.startswith("pass "):
            board.ko = None
            moves += 1
            pending = line
        elif line.startswith("board"):
            rows = []
            expect_rows = board.size
        elif line.startswith("captures "):
            _, b, w = line.split()
            if (board.captures[BLACK], board.captures[WHITE]) != (int(b), int(w)):
                mismatches += 1
                print("capture mismatch: engine %s/%s, reference %s/%s (game %d)" %
                      (b, w, board.captures[BLACK], board.captures[WHITE], games))
        elif line.startswith("illegal "):
            _, x, y, c = line.split()
            colour = BLACK if c == "B" else WHITE
            if board.is_legal((int(x), int(y)), colour):
                print("engine rejected a move the reference allows: (%s,%s) %s (game %d)" % (x, y, c, games))
                mismatches += 1
    print("%d games, %d mismatches" % (games, mismatches))
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
