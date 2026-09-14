// Who says what, and when.
//
// The string tables hold six lines per opponent character and five per helper, in a fixed order.
// Rather than repeat a 5 x 6 switch, the ids are indexed - and the static_asserts below make the
// build fail the moment someone reorders strings_list.h.
#pragma once

#include <cstdint>

#include "game/strings_ids.h"

namespace game {

// Order of the SAY_<CHARACTER>_* block, per opponent.
enum class Say : uint8_t { INTRO = 0, CAPTURE = 1, CAPTURED = 2, WIN = 3, LOSE = 4, ATARI = 5 };

// Order of the INTJ_<HELPER>_* block, per helper.
enum class Intj : uint8_t { ATARI = 0, CAPTURE = 1, CLEAR = 2, FAIL = 3, THINK = 4 };

constexpr int SAY_PER_OPPONENT = 6;
constexpr int INTJ_PER_HELPER = 5;

static_assert(int(Str::SAY_PEBBLE_CAPTURE) == int(Str::SAY_PEBBLE_INTRO) + 1);
static_assert(int(Str::SAY_PEBBLE_ATARI) == int(Str::SAY_PEBBLE_INTRO) + 5);
static_assert(int(Str::SAY_SPROUT_INTRO) == int(Str::SAY_PEBBLE_INTRO) + SAY_PER_OPPONENT);
static_assert(int(Str::SAY_KOAN_INTRO) == int(Str::SAY_PEBBLE_INTRO) + 2 * SAY_PER_OPPONENT);
static_assert(int(Str::SAY_EMBER_INTRO) == int(Str::SAY_PEBBLE_INTRO) + 3 * SAY_PER_OPPONENT);
static_assert(int(Str::SAY_TENGEN_INTRO) == int(Str::SAY_PEBBLE_INTRO) + 4 * SAY_PER_OPPONENT);
static_assert(int(Str::SAY_TENGEN_ATARI) == int(Str::SAY_PEBBLE_INTRO) + 5 * SAY_PER_OPPONENT - 1);

static_assert(int(Str::INTJ_SEN_THINK) == int(Str::INTJ_SEN_ATARI) + 4);
static_assert(int(Str::INTJ_INDI_ATARI) == int(Str::INTJ_SEN_ATARI) + INTJ_PER_HELPER);
static_assert(int(Str::INTJ_REX_THINK) == int(Str::INTJ_SEN_ATARI) + 3 * INTJ_PER_HELPER - 1);

// `opponent` is the character index 0..4 (PEBBLE, SPROUT, KOAN, EMBER, TENGEN).
[[nodiscard]] constexpr uint16_t say_of(int opponent, Say what)
{
    const int index = (opponent < 0 ? 0 : (opponent > 4 ? 4 : opponent));
    return uint16_t(int(Str::SAY_PEBBLE_INTRO) + index * SAY_PER_OPPONENT + int(what));
}

// `helper` is the settings index 0..2 (SEN, INDI, REX).
[[nodiscard]] constexpr uint16_t intj_of(int helper, Intj what)
{
    const int index = (helper < 0 ? 0 : (helper > 2 ? 2 : helper));
    return uint16_t(int(Str::INTJ_SEN_ATARI) + index * INTJ_PER_HELPER + int(what));
}

}  // namespace game
