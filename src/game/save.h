// SRAM save layout (32 KB). One SaveFile at offset 0 with magic, version and CRC32.
// 3 campaign slots + global settings + one in-progress sandbox game.
#pragma once
#include <cstdint>
#include "go/go_types.h"

namespace save {

constexpr uint32_t MAGIC = 0x314B4F4D;   // 'MOK1'
constexpr uint16_t VERSION = 1;
constexpr int SLOT_COUNT = 3;
constexpr int OPPONENT_COUNT = 5;

struct Settings {
    uint8_t language = 0;     // 0 EN 1 FR
    uint8_t helper = 0;       // 0 SEN 1 INDI 2 REX
    uint8_t skin = 0;         // 0 CLASSIC 1 PUP 2 DINO
    uint8_t effects = 1;      // 0 OFF 1 JUICY
    uint8_t confirm_2a = 1;   // 1 = two presses of A
    uint8_t music = 6;        // 0..8
    uint8_t sfx = 8;          // 0..8
    uint8_t pad = 0;
};

struct OpponentRecord { uint16_t wins = 0, losses = 0; int16_t best_margin_x2 = -32768; };

struct SandboxGame {                     // in-progress sandbox game (resume from title)
    uint8_t valid = 0;
    uint8_t size = 9, player_black = 1, opponent = 0, handicap = 0, rules = 0, hotseat = 0;
    int16_t komi_x2 = 15;
    uint16_t move_count = 0;
    uint16_t moves[512];                 // point index (0xFFFF pass), colour alternates from the first mover
    uint8_t first_mover = 1;
};

struct Slot {
    uint8_t used = 0;
    uint8_t cleared_count = 0;           // missions cleared in order (1..20)
    uint8_t ranks[20] = {};              // 0 none 1 C 2 B 3 A 4 S
    uint32_t stars_mask = 0;             // bit (n-1) = mission n starred
    uint32_t unlocks = 0;                // bit n = UNLOCK_n granted
    OpponentRecord records[OPPONENT_COUNT];
    uint32_t play_seconds = 0;
};

struct SaveFile {
    uint32_t magic = MAGIC;
    uint16_t version = VERSION;
    uint16_t size = 0;                   // sizeof(SaveFile)
    uint32_t crc = 0;                    // CRC32 of everything after this field
    Settings settings;
    uint8_t last_slot = 0;
    uint8_t pad[3] = {};
    Slot slots[SLOT_COUNT];
    SandboxGame sandbox;
};

uint32_t crc32(const void* data, int size);
bool load(SaveFile& out);                // false if SRAM is empty/corrupt (out = defaults)
void store(SaveFile& file);              // recomputes crc and writes SRAM
void wipe();

}  // namespace save
