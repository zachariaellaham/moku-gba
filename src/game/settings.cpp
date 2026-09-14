#include "game/settings.h"

#include "game/test_iface.h"

namespace settings {

namespace {

// Plain .bss lands in the 32 KB of IWRAM with this linker script, so the save file is pinned to
// EWRAM. It must be .ewram (loaded from ROM) and not .sbss (NOLOAD): SaveFile has non-zero default
// member initialisers, and a NOLOAD section would silently drop them.
#define MOKU_EWRAM_DATA __attribute__((section(".ewram")))

MOKU_EWRAM_DATA save::SaveFile g_file;
MOKU_EWRAM_DATA uint32_t g_revision = 0;

[[nodiscard]] int clamp(int value, int max_value) {
    if (value < 0) {
        return 0;
    }

    return value > max_value ? max_value : value;
}

void apply_all() {
    ::set_language(g_file.settings.language == 1 ? Language::FR : Language::EN);
}

}  // namespace

save::SaveFile& file() { return g_file; }

save::Settings& options() { return g_file.settings; }

uint32_t revision() { return g_revision; }

void touch() { ++g_revision; }

void init() {
    load();
}

bool load() {
    bool ok = save::load(g_file);
    apply_all();
    touch();
    return ok;
}

void store() {
    save::store(g_file);
}

void wipe() {
    save::wipe();
    save::load(g_file);   // SRAM is empty now, so this resets g_file to the defaults
    apply_all();
    touch();
}

void set_language(int value) {
    g_file.settings.language = uint8_t(clamp(value, 1));
    apply_all();
    touch();
}

void set_skin(int value) {
    g_file.settings.skin = uint8_t(clamp(value, 2));
    touch();
}

void set_helper(int value) {
    g_file.settings.helper = uint8_t(clamp(value, 2));
    touch();
}

void set_effects(int value) {
    g_file.settings.effects = uint8_t(clamp(value, 1));
    touch();
}

void set_confirm_2a(int value) {
    g_file.settings.confirm_2a = uint8_t(clamp(value, 1));
    touch();
}

void set_music(int value) {
    g_file.settings.music = uint8_t(clamp(value, 8));
    touch();
}

void set_sfx(int value) {
    g_file.settings.sfx = uint8_t(clamp(value, 8));
    touch();
}

void set_slot(int value) {
    g_file.last_slot = uint8_t(clamp(value, save::SLOT_COUNT - 1));
    touch();
}

int language() { return g_file.settings.language; }

int skin() { return g_file.settings.skin; }

int helper() { return g_file.settings.helper; }

int effects() { return g_file.settings.effects; }

bool confirm_2a() { return g_file.settings.confirm_2a != 0; }

int music() { return g_file.settings.music; }

int sfx() { return g_file.settings.sfx; }

int slot_index() { return g_file.last_slot; }

save::Slot& slot() { return g_file.slots[g_file.last_slot]; }

Language language_id() { return g_file.settings.language == 1 ? Language::FR : Language::EN; }

void publish() {
    const save::Settings& opt = g_file.settings;
    g_moku_test.language = opt.language;
    g_moku_test.skin = opt.skin;
    g_moku_test.helper = opt.helper;
    g_moku_test.effects = opt.effects;
    g_moku_test.confirm_mode = uint8_t(opt.confirm_2a ? 0 : 1);   // test block: 0 = 2xA, 1 = 1xA
    g_moku_test.save_slot = g_file.last_slot;

    const save::Slot& current = g_file.slots[g_file.last_slot];
    g_moku_test.unlocks = current.unlocks;
    g_moku_test.cleared_count = current.cleared_count;

    for (int index = 0; index < 20; ++index) {
        g_moku_test.ranks[index] = current.ranks[index];
    }

    g_moku_test.stars_mask_lo = uint8_t(current.stars_mask & 0xFF);
    g_moku_test.stars_mask_mid = uint8_t((current.stars_mask >> 8) & 0xFF);
    g_moku_test.stars_mask_hi = uint8_t((current.stars_mask >> 16) & 0xFF);
}

}  // namespace settings
