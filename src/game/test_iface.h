// Debug/test interface block read and written by the ROM harness (tests/rom/harness/mokurun).
// The block is a C symbol `g_moku_test` in EWRAM; scripts address fields as `$g_moku_test+OFFSET`.
// Offsets are frozen by the static_asserts below: NEVER reorder fields, only append.
#pragma once
#include <cstdint>
#include <cstddef>

enum class SceneId : uint8_t {
    BOOT = 0, TITLE = 1, SAVE_SELECT = 2, CAMPAIGN_MAP = 3, BRIEFING = 4, PLAY = 5, MISSION_CLEAR = 6,
    SANDBOX_SETUP = 7, CHOOSE_OPPONENT = 8, OPTIONS = 9, GAME_OVER = 10, PAUSE = 11
};

// Harness -> ROM commands. The ROM executes the command at the start of the next frame, writes
// cmd_result and clears cmd to 0.
enum TestCmd : uint32_t {
    TCMD_NONE = 0,
    TCMD_GOTO_TITLE = 1,
    TCMD_LOAD_MISSION = 2,       // arg = mission id 1..20 (skips briefing, goes to PLAY)
    TCMD_START_SANDBOX = 3,      // arg bits: size(0-7: 7/9/13/19 index 0..3) | player_black<<4 | level<<8 | handicap<<12 | rules<<16 | komi_x2<<20 | hotseat<<28
    TCMD_SET_LANGUAGE = 4,       // arg 0 EN / 1 FR
    TCMD_SET_SKIN = 5,           // arg 0 CLASSIC / 1 PUP / 2 DINO
    TCMD_SET_HELPER = 6,         // arg 0 SEN / 1 INDI / 2 REX
    TCMD_SET_EFFECTS = 7,        // arg 0 OFF / 1 JUICY
    TCMD_UNLOCK_ALL = 8,
    TCMD_RESET_COUNTERS = 9,     // max_missed_frames, cpu max
    TCMD_SET_SEED = 10,          // arg = rng seed for AI
    TCMD_AI_FINISH_NOW = 11,     // force the current AI think to finish this frame
    TCMD_SKIP_EFFECTS = 12,      // end any running banner/effect/dialogue immediately
    TCMD_SET_CONFIRM = 13,       // arg 0 = 2xA, 1 = 1xA
    TCMD_WIPE_SRAM = 14,
    TCMD_SELECT_SLOT = 15,       // arg 0..2
    TCMD_PLAY_POINT = 16,        // arg = x | y<<8 : play this point for the human side immediately (test shortcut)
    TCMD_PASS = 17,
    TCMD_GOTO_SCENE = 18,        // arg = SceneId (where meaningful: OPTIONS, SANDBOX_SETUP, CHOOSE_OPPONENT, CAMPAIGN_MAP)
    TCMD_SET_MUSIC_SFX = 19,     // arg = music | sfx<<8 (0..8)
    TCMD_SAVE_NOW = 20,          // force a SRAM write of the current state
    TCMD_BENCH_PLAYOUTS = 21,    // arg = playout count (1..500) from the board on screen; result = playouts/s
    TCMD_AUTOPLAY = 22,          // arg = max moves (1..400): plays both sides with the playout policy until the game ends; result = moves played
};

struct TestState {
    uint32_t magic;              // 0   0x554B4F4D 'MOKU'
    uint32_t heartbeat;          // 4   +1 every frame (stops if the ROM hangs/asserts)
    uint8_t scene;               // 8   SceneId
    uint8_t scene_sub;           // 9   scene-specific sub-state (documented per scene)
    uint8_t language;            // 10
    uint8_t skin;                // 11
    uint8_t helper;              // 12
    uint8_t effects;             // 13
    uint8_t confirm_mode;        // 14
    uint8_t board_size;          // 15
    uint8_t to_move;             // 16  go colour
    uint8_t phase;               // 17  go::Phase
    uint8_t cursor_x;            // 18
    uint8_t cursor_y;            // 19
    uint16_t move_count;         // 20
    uint16_t caps_black;         // 22  prisoners taken by black
    uint16_t caps_white;         // 24
    uint8_t mission_id;          // 26  1..20, 0 = sandbox / none
    uint8_t mission_status;      // 27  0 playing 1 clear 2 fail
    uint8_t mission_stage;       // 28
    uint8_t ai_level;            // 29
    uint8_t ai_thinking;         // 30
    uint8_t event_flags;         // 31  bit0 atari banner, bit1 capture banner, bit2 helper pop-in, bit3 shake,
                                 //     bit4 tint flash, bit5 particles, bit6 rank stamp, bit7 dialogue box
    uint32_t playouts;           // 32  playouts of the last AI think
    uint16_t last_ai_move;       // 36  point index, 0xFFFF = pass
    uint16_t max_missed_frames;  // 38  max bn::core::last_missed_frames() since reset
    int16_t score_margin_x2;     // 40  black - white (final or estimate)
    uint8_t winner;              // 42
    uint8_t save_slot;           // 43
    uint32_t unlocks;            // 44  bitmask of UNLOCK_* (bit n = unlock id n)
    uint8_t cleared_count;       // 48
    uint8_t ranks[20];           // 49..68  0 none 1 C 2 B 3 A 4 S
    uint8_t sandbox_saved;       // 69  1 when SRAM holds an unfinished sandbox game
    uint8_t hint_offered;        // 70  1 once the helper has offered a hint on this mission
    uint8_t pad0;                // 71
    uint32_t cmd;                // 72
    uint32_t cmd_arg;            // 76
    uint32_t cmd_result;         // 80
    uint32_t cpu_max_permille;   // 84  max last_cpu_usage() since reset (1000 = full frame)
    uint32_t rng_seed;           // 88
    char text_probe[32];         // 92  first 31 bytes of the most recently shown dialogue/banner text
    uint8_t dialogue_visible;    // 124
    uint8_t hud_panel;           // 125 0 info / 1 move list / 2 estimate
    uint8_t menu_index;          // 126 highlighted item in the current menu
    uint8_t pending_confirm;     // 127 ghost stone shown, waiting for 2nd A
    uint32_t playouts_per_sec;   // 128 measured during the last think
    uint32_t frame;              // 132 frames since boot
    uint16_t music_id;           // 136 currently playing music (0 none)
    uint16_t last_sfx;           // 138 last sfx id played
    uint8_t opponent_id;         // 140 sandbox opponent 0..4, 0xFF hot-seat
    uint8_t stars_mask_lo;       // 141 bits: missions 1..8 starred
    uint8_t stars_mask_mid;      // 142 9..16
    uint8_t stars_mask_hi;       // 143 17..20
    uint8_t reserved[16];        // 144..159
};
static_assert(offsetof(TestState, heartbeat) == 4);
static_assert(offsetof(TestState, scene) == 8);
static_assert(offsetof(TestState, move_count) == 20);
static_assert(offsetof(TestState, mission_id) == 26);
static_assert(offsetof(TestState, playouts) == 32);
static_assert(offsetof(TestState, unlocks) == 44);
static_assert(offsetof(TestState, ranks) == 49);
static_assert(offsetof(TestState, cmd) == 72);
static_assert(offsetof(TestState, text_probe) == 92);
static_assert(offsetof(TestState, playouts_per_sec) == 128);
static_assert(offsetof(TestState, opponent_id) == 140);
static_assert(sizeof(TestState) == 160);

extern "C" volatile TestState g_moku_test;
