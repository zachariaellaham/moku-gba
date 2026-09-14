// MOKU rules engine - basic types. Platform-agnostic: compiles on host (g++ -std=c++20) and GBA.
#pragma once
#include <cstdint>

namespace go {

using Color = uint8_t;
enum : uint8_t { EMPTY = 0, BLACK = 1, WHITE = 2, BORDER = 3 };
constexpr Color opponent(Color c) { return Color(c ^ 3); }  // BLACK <-> WHITE

constexpr int MIN_SIZE = 7;
constexpr int MAX_SIZE = 19;
constexpr int MAX_STRIDE = MAX_SIZE + 2;                 // 21
constexpr int MAX_POINTS = MAX_STRIDE * MAX_STRIDE;      // 441 (padded board)
constexpr int MAX_MOVES = 1024;                          // game record capacity (moves + passes)

// A Point is an index into the padded board: p = (y + 1) * stride + (x + 1), stride = size + 2.
// Border points hold BORDER so neighbour checks never need bounds tests.
using Point = int16_t;
constexpr Point PASS = -1;
constexpr Point NO_POINT = -2;

struct Move {
    Point p;   // board point or PASS
    Color c;   // BLACK or WHITE
};

constexpr int point_index(int stride, int x, int y) { return (y + 1) * stride + (x + 1); }

// Deterministic PRNG shared by AI and tests (xorshift32). Never use rand().
struct Rng {
    uint32_t s = 2463534242u;
    explicit Rng(uint32_t seed = 2463534242u) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    // uniform in [0, n): uses the high bits (n <= 65535)
    uint32_t below(uint32_t n) { return (uint32_t)(((uint64_t)next() * n) >> 32); }
};

// IWRAM / ROM placement helpers (no-ops on the host).
#if defined(MOKU_GBA)
#define GO_IWRAM_CODE __attribute__((section(".iwram"), long_call))
#define GO_EWRAM_DATA __attribute__((section(".ewram")))
// Large zero-initialised buffers: devkitARM puts plain .bss in IWRAM (32 KB only), .sbss in EWRAM.
#define GO_EWRAM_BSS  __attribute__((section(".sbss")))
#define GO_ROM_CONST  __attribute__((section(".rodata")))
#else
#define GO_IWRAM_CODE
#define GO_EWRAM_DATA
#define GO_EWRAM_BSS
#define GO_ROM_CONST
#endif

}  // namespace go
