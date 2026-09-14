// Zobrist hashing tables (constexpr, generated with splitmix64 so host and GBA agree).
#pragma once
#include "go_types.h"

namespace go {

struct ZobristTable {
    uint64_t stone[3][MAX_POINTS];   // index [color][point]; stone[EMPTY] unused (zeros)
};

constexpr uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

constexpr ZobristTable make_zobrist() {
    ZobristTable t{};
    uint64_t seed = 0x4D4F4B5547424121ull;  // "MOKUGBA!"
    for (int c = 1; c <= 2; ++c)
        for (int p = 0; p < MAX_POINTS; ++p) t.stone[c][p] = splitmix64(seed);
    return t;
}

// Defined once in board.cpp: `constexpr ZobristTable ZOBRIST = make_zobrist();` lives in ROM.
extern const ZobristTable ZOBRIST;

}  // namespace go
