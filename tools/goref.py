#!/usr/bin/env python3
"""A small, deliberately simple reference implementation of the Go rules.

It exists to *verify* the campaign positions (tools/gen_missions.py) before they are compiled into
the ROM: ladders, nets, snapbacks and life-and-death shapes are easy to get subtly wrong by hand.
Correctness and clarity matter here; speed does not. The C++ engine in src/go is the real one, and
tests/host/test_missions.cpp checks the same claims against it.
"""

from copy import deepcopy

EMPTY, BLACK, WHITE = 0, 1, 2
CHARS = {EMPTY: ".", BLACK: "X", WHITE: "O"}


def other(c):
    return BLACK if c == WHITE else WHITE


class Board:
    def __init__(self, size=9, rows=None):
        self.size = size
        self.grid = [[EMPTY] * size for _ in range(size)]
        self.ko = None
        self.captures = {BLACK: 0, WHITE: 0}
        if rows:
            self.set_rows(rows)

    # -- setup ---------------------------------------------------------------------------------
    def set_rows(self, rows):
        """rows: list of strings, '.' empty, 'X' black, 'O' white, anything else empty."""
        self.size = len(rows)
        self.grid = [[EMPTY] * self.size for _ in range(self.size)]
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch == "X":
                    self.grid[y][x] = BLACK
                elif ch == "O":
                    self.grid[y][x] = WHITE

    def copy(self):
        b = Board(self.size)
        b.grid = [row[:] for row in self.grid]
        b.ko = self.ko
        b.captures = dict(self.captures)
        return b

    def __str__(self):
        return "\n".join("".join(CHARS[c] for c in row) for row in self.grid)

    # -- queries -------------------------------------------------------------------------------
    def on_board(self, p):
        x, y = p
        return 0 <= x < self.size and 0 <= y < self.size

    def at(self, p):
        return self.grid[p[1]][p[0]]

    def neighbours(self, p):
        x, y = p
        for q in ((x, y - 1), (x - 1, y), (x + 1, y), (x, y + 1)):
            if self.on_board(q):
                yield q

    def group(self, p):
        """Stones and liberties of the group at p: (set_of_stones, set_of_liberties)."""
        colour = self.at(p)
        if colour == EMPTY:
            return set(), set()
        stones, libs, stack = {p}, set(), [p]
        while stack:
            cur = stack.pop()
            for q in self.neighbours(cur):
                v = self.at(q)
                if v == EMPTY:
                    libs.add(q)
                elif v == colour and q not in stones:
                    stones.add(q)
                    stack.append(q)
        return stones, libs

    def liberties(self, p):
        return len(self.group(p)[1])

    def is_legal(self, p, colour):
        if not self.on_board(p) or self.at(p) != EMPTY or p == self.ko:
            return False
        test = self.copy()
        test.grid[p[1]][p[0]] = colour
        # captures first
        for q in test.neighbours(p):
            if test.at(q) == other(colour) and not test.group(q)[1]:
                return True
        return bool(test.group(p)[1])  # no suicide

    def play(self, p, colour):
        """Plays a stone. Returns the number of captured stones. Raises on an illegal move."""
        if not self.is_legal(p, colour):
            raise ValueError("illegal move %s for %s\n%s" % (p, CHARS[colour], self))
        self.grid[p[1]][p[0]] = colour
        captured = []
        for q in self.neighbours(p):
            if self.at(q) == other(colour):
                stones, libs = self.group(q)
                if not libs:
                    captured.extend(stones)
        for q in set(captured):
            self.grid[q[1]][q[0]] = EMPTY
        self.captures[colour] += len(set(captured))
        stones, _ = self.group(p)
        self.ko = list(set(captured))[0] if len(set(captured)) == 1 and len(stones) == 1 else None
        return len(set(captured))

    def legal_moves(self, colour):
        return [(x, y) for y in range(self.size) for x in range(self.size) if self.is_legal((x, y), colour)]


# --------------------------------------------------------------------------------------------
# tactical search used to prove the campaign claims
# --------------------------------------------------------------------------------------------
def capture_search(board, target, attacker_to_move, depth=14, node_budget=[0]):
    """Can `attacker` (the opponent of the target's colour) capture the group at `target`?

    Returns True if the attacker captures with correct play from both sides. The defender wins by
    reaching four liberties (practically unkillable in these small problems) or by capturing enough
    attacking stones to break out.
    """
    node_budget[0] += 1
    if node_budget[0] > 120000:
        return False                     # unproven within the budget: treat as "no capture"
    if board.at(target) == EMPTY:
        return True                      # already captured
    defender = board.at(target)
    attacker = other(defender)
    stones, libs = board.group(target)
    if len(libs) >= 4:
        return False
    if depth <= 0:
        return False

    if attacker_to_move:
        # try every liberty of the target, plus capturing an adjacent attacker group in atari
        moves = list(libs)
        for s in stones:
            for q in board.neighbours(s):
                if board.at(q) == attacker and board.liberties(q) == 1:
                    moves.extend(board.group(q)[1])
        for m in dict.fromkeys(moves):
            if not board.is_legal(m, attacker):
                continue
            nb = board.copy()
            nb.play(m, attacker)
            if nb.at(target) == EMPTY:
                return True
            if capture_search(nb, target, False, depth - 1, node_budget):
                return True
        return False
    else:
        moves = list(libs)
        for s in stones:
            for q in board.neighbours(s):
                if board.at(q) == attacker and board.liberties(q) == 1:
                    moves.extend(board.group(q)[1])
        moves.append(None)               # the defender may also tenuki (pass) - usually losing
        for m in dict.fromkeys(moves):
            if m is None:
                nb = board.copy()
                nb.ko = None
            else:
                if not board.is_legal(m, defender):
                    continue
                nb = board.copy()
                nb.play(m, defender)
                if nb.at(target) == EMPTY:
                    continue             # the defender killed its own group
            if not capture_search(nb, target, True, depth - 1, node_budget):
                return False             # the defender found a way out
        return True


def group_is_dead(board, target, defender_to_move, depth=12):
    """True when the attacker can kill the group even if the defender moves first."""
    return capture_search(board, target, not defender_to_move, depth, [0])


def two_eyes(board, target):
    """True when the group at target has at least two separate one-point eyes (a simple check that
    is sufficient for the small campaign shapes)."""
    stones, libs = board.group(target)
    colour = board.at(target)
    eyes = 0
    for lib in libs:
        if all(board.at(q) == colour for q in board.neighbours(lib)):
            diagonals = [(lib[0] + dx, lib[1] + dy) for dx in (-1, 1) for dy in (-1, 1)]
            on = [d for d in diagonals if board.on_board(d)]
            bad = sum(1 for d in on if board.at(d) == other(colour))
            limit = 0 if len(on) < 4 else 1
            if bad <= limit:
                eyes += 1
    return eyes >= 2


def rows_of(board):
    return [line for line in str(board).splitlines()]


def show(board, title=""):
    letters = "ABCDEFGHJKLMNOPQRST"[: board.size]
    print(("--- " + title) if title else "---")
    print("   " + " ".join(letters))
    for y, row in enumerate(str(board).splitlines()):
        print("%2d " % (y + 1) + " ".join(row))


# --------------------------------------------------------------------------------------------
# ladder reader: mirrors go::read_ladder in src/go/ladder.h so the campaign and the ROM agree
# --------------------------------------------------------------------------------------------
CAPTURED, ESCAPED, UNKNOWN = "CAPTURED", "ESCAPED", "UNKNOWN"


def read_ladder(board, target, attacker_to_move, max_plies=60, line=None):
    """Can the attacker capture the group at `target` in a ladder?

    The defender may extend to its single liberty or capture an adjacent attacking group that is in
    atari; the attacker may play any liberty of the target. A group with three or more liberties has
    escaped. `line` collects the principal variation when the attacker wins.
    """
    if board.at(target) == EMPTY:
        return CAPTURED
    defender = board.at(target)
    attacker = other(defender)
    stones, libs = board.group(target)
    if len(libs) >= 3:
        return ESCAPED
    if max_plies <= 0:
        return UNKNOWN

    if attacker_to_move:
        result = ESCAPED
        for m in sorted(libs):
            if not board.is_legal(m, attacker):
                continue
            nb = board.copy()
            nb.play(m, attacker)
            if nb.at(target) == EMPTY:
                if line is not None:
                    line.append((attacker, m))
                return CAPTURED
            sub = [] if line is not None else None
            r = read_ladder(nb, target, False, max_plies - 1, sub)
            if r == CAPTURED:
                if line is not None:
                    line.append((attacker, m))
                    line.extend(sub)
                return CAPTURED
            if r == UNKNOWN:
                result = UNKNOWN
        return result
    else:
        if len(libs) != 1:
            return ESCAPED               # not forced: the defender simply lives
        moves = [list(libs)[0]]
        for s in stones:
            for q in board.neighbours(s):
                if board.at(q) == attacker and board.liberties(q) == 1:
                    moves.extend(board.group(q)[1])
        result = CAPTURED
        for m in dict.fromkeys(moves):
            if not board.is_legal(m, defender):
                continue
            nb = board.copy()
            nb.play(m, defender)
            if nb.at(target) == EMPTY:
                continue
            sub = [] if line is not None else None
            r = read_ladder(nb, target, True, max_plies - 1, sub)
            if r == ESCAPED:
                return ESCAPED
            if r == UNKNOWN:
                result = UNKNOWN
            elif result == CAPTURED and line is not None and not getattr(read_ladder, "_kept", False):
                line.append((defender, m))
                line.extend(sub)
                read_ladder._kept = True
        read_ladder._kept = False
        if not [m for m in dict.fromkeys(moves) if board.is_legal(m, defender)]:
            return CAPTURED
        return result
