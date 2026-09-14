// The one in-RAM copy of the save file, plus the global options every scene reads.
//
// There is exactly one SaveFile in the game (settings::file()). Read it freely; change it through
// the setters below (or write it directly and call touch()), then call store() to persist it.
// Every frame the scene manager mirrors it into the test block (g_moku_test.language, .skin,
// .helper, .effects, .confirm_mode, .save_slot, .unlocks, .cleared_count, .ranks, .stars_mask_*),
// so a test can read progress without the scenes publishing anything themselves.
#pragma once

#include <cstdint>

#include "game/save.h"
#include "game/strings_ids.h"

namespace settings {

// Loads SRAM (defaults if empty/corrupt) and applies the loaded options. Call once, after
// bn::core::init() and before the first scene is created.
void init();

[[nodiscard]] save::SaveFile& file();
[[nodiscard]] save::Settings& options();

// Bumped by every setter (and by touch()). A scene that caches something derived from the
// options - a skin palette, a generated string - re-reads it when this value changes.
[[nodiscard]] uint32_t revision();
void touch();

void set_language(int value);     // 0 EN, 1 FR. Also switches the string table.
void set_skin(int value);         // 0 CLASSIC, 1 PUP, 2 DINO
void set_helper(int value);       // 0 SEN, 1 INDI, 2 REX
void set_effects(int value);      // 0 OFF, 1 JUICY
void set_confirm_2a(int value);   // 0 = one A press, 1 = two A presses
void set_music(int value);        // 0..8
void set_sfx(int value);          // 0..8
void set_slot(int value);         // 0..2

[[nodiscard]] int language();
[[nodiscard]] int skin();
[[nodiscard]] int helper();
[[nodiscard]] int effects();
[[nodiscard]] bool confirm_2a();
[[nodiscard]] int music();
[[nodiscard]] int sfx();
[[nodiscard]] int slot_index();
[[nodiscard]] save::Slot& slot();

[[nodiscard]] Language language_id();

bool load();        // re-reads SRAM into file(); false (and defaults) if empty or corrupt
void store();       // writes file() to SRAM (recomputes the CRC)
void wipe();        // erases SRAM and resets file() to defaults

// Copies the options and the current slot into g_moku_test. Called once per frame by the scene
// manager; scenes never need to call it.
void publish();

}  // namespace settings
