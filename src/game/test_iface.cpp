#include "game/test_iface_ext.h"

#include "bn_core.h"

#include "game/scene.h"
#include "game/settings.h"

// The harness addresses this block as `$g_moku_test+OFFSET`, so it must keep external C linkage and
// stay volatile (nothing here may be optimised away or reordered across the harness' reads).
// Plain .bss would put it in IWRAM; the block is documented as living in EWRAM, and .sbss is the
// zero-initialised EWRAM section of the devkitARM GBA linker script.
extern "C" {
__attribute__((section(".sbss"))) volatile TestState g_moku_test;
}

namespace test_iface {

namespace {

constexpr int MAX_HANDLERS = 4;

struct Handler {
    CommandFn fn = nullptr;
    void* ctx = nullptr;
};

Handler g_handlers[MAX_HANDLERS];
Handler g_scene_handler;

[[nodiscard]] bool in_range(uint32_t arg, uint32_t max_value) { return arg <= max_value; }

// Commands every build understands. Runs last, so a scene or a subsystem can override any of them.
[[nodiscard]] bool builtin(uint32_t cmd, uint32_t arg, uint32_t& result) {
    switch (cmd) {

    case TCMD_RESET_COUNTERS:
        reset_counters();
        return true;

    case TCMD_SET_LANGUAGE:
        if (!in_range(arg, 1)) { result = TRES_BAD_ARG; return true; }
        settings::set_language(int(arg));
        return true;

    case TCMD_SET_SKIN:
        if (!in_range(arg, 2)) { result = TRES_BAD_ARG; return true; }
        settings::set_skin(int(arg));
        return true;

    case TCMD_SET_HELPER:
        if (!in_range(arg, 2)) { result = TRES_BAD_ARG; return true; }
        settings::set_helper(int(arg));
        return true;

    case TCMD_SET_EFFECTS:
        if (!in_range(arg, 1)) { result = TRES_BAD_ARG; return true; }
        settings::set_effects(int(arg));
        return true;

    case TCMD_SET_CONFIRM:
        if (!in_range(arg, 1)) { result = TRES_BAD_ARG; return true; }
        settings::set_confirm_2a(arg == 0 ? 1 : 0);   // arg 0 = two A presses, 1 = one
        return true;

    case TCMD_SET_MUSIC_SFX:
        settings::set_music(int(arg & 0xFF));
        settings::set_sfx(int((arg >> 8) & 0xFF));
        return true;

    case TCMD_SELECT_SLOT:
        if (!in_range(arg, uint32_t(save::SLOT_COUNT - 1))) { result = TRES_BAD_ARG; return true; }
        settings::set_slot(int(arg));
        return true;

    case TCMD_UNLOCK_ALL: {
        // Default meaning: every UNLOCK_* id is granted. The campaign code can register a
        // handler that also fills in ranks / cleared_count if a test needs those.
        save::Slot& slot = settings::slot();
        slot.unlocks = 0xFFFFFFFFu;
        slot.used = 1;
        settings::touch();
        return true;
    }

    case TCMD_SET_SEED:
        g_moku_test.rng_seed = arg;
        return true;

    case TCMD_WIPE_SRAM:
        settings::wipe();
        return true;

    case TCMD_SAVE_NOW:
        settings::store();
        return true;

    case TCMD_GOTO_TITLE:
        if (!scene::switch_now(SceneId::TITLE, 0)) { result = TRES_REFUSED; }
        return true;

    case TCMD_GOTO_SCENE:
        if (arg > uint32_t(SceneId::PAUSE)) { result = TRES_BAD_ARG; return true; }
        if (!scene::switch_now(SceneId(arg), 0)) { result = TRES_REFUSED; }
        return true;

    default:
        return false;
    }
}

}  // namespace

void init() {
    g_moku_test.magic = 0x554B4F4Du;   // 'MOKU'
    g_moku_test.heartbeat = 0;
    g_moku_test.frame = 0;
    g_moku_test.cmd = TCMD_NONE;
    g_moku_test.cmd_arg = 0;
    g_moku_test.cmd_result = TRES_NONE;
    g_moku_test.scene = uint8_t(SceneId::BOOT);
    reset_counters();
}

void reset_counters() {
    g_moku_test.max_missed_frames = 0;
    g_moku_test.cpu_max_permille = 0;
}

void begin_frame() {
    // Read-modify-write: ++ on a volatile is deprecated in C++20.
    g_moku_test.heartbeat = g_moku_test.heartbeat + 1;
    g_moku_test.frame = g_moku_test.frame + 1;

    int missed = bn::core::last_missed_frames();

    if (missed > int(g_moku_test.max_missed_frames)) {
        g_moku_test.max_missed_frames = uint16_t(missed);
    }

    // last_cpu_usage() is a bn::fixed fraction of one frame: 1.0 == the whole frame.
    int permille = (bn::core::last_cpu_usage() * 1000).right_shift_integer();

    if (permille > int(g_moku_test.cpu_max_permille)) {
        g_moku_test.cpu_max_permille = uint32_t(permille);
    }
}

void end_frame() {
    settings::publish();
}

void poll() {
    uint32_t cmd = g_moku_test.cmd;

    if (cmd == TCMD_NONE) {
        return;
    }

    uint32_t arg = g_moku_test.cmd_arg;
    uint32_t result = TRES_OK;
    bool handled = false;

    if (g_scene_handler.fn) {
        handled = g_scene_handler.fn(g_scene_handler.ctx, cmd, arg, result);
    }

    for (int index = MAX_HANDLERS - 1; index >= 0 && !handled; --index) {
        if (g_handlers[index].fn) {
            handled = g_handlers[index].fn(g_handlers[index].ctx, cmd, arg, result);
        }
    }

    if (!handled) {
        handled = builtin(cmd, arg, result);
    }

    if (!handled) {
        result = TRES_UNKNOWN;
    }

    g_moku_test.cmd_result = result;
    g_moku_test.cmd = TCMD_NONE;
}

void add_handler(CommandFn fn, void* ctx) {
    if (!fn) {
        return;
    }

    for (int index = 0; index < MAX_HANDLERS; ++index) {
        if (g_handlers[index].fn == fn && g_handlers[index].ctx == ctx) {
            return;
        }
    }

    for (int index = 0; index < MAX_HANDLERS; ++index) {
        if (!g_handlers[index].fn) {
            g_handlers[index].fn = fn;
            g_handlers[index].ctx = ctx;
            return;
        }
    }
}

void remove_handler(CommandFn fn, void* ctx) {
    for (int index = 0; index < MAX_HANDLERS; ++index) {
        if (g_handlers[index].fn == fn && g_handlers[index].ctx == ctx) {
            g_handlers[index].fn = nullptr;
            g_handlers[index].ctx = nullptr;
            return;
        }
    }
}

void set_scene_handler(CommandFn fn, void* ctx) {
    g_scene_handler.fn = fn;
    g_scene_handler.ctx = ctx;
}

void set_text_probe(const char* text) {
    constexpr int SIZE = int(sizeof(g_moku_test.text_probe));
    int index = 0;

    if (text) {
        while (index < SIZE - 1 && text[index]) {
            g_moku_test.text_probe[index] = text[index];
            ++index;
        }
    }

    while (index < SIZE) {
        g_moku_test.text_probe[index] = 0;
        ++index;
    }
}

void set_event_flag(int bit, bool on) {
    uint8_t mask = uint8_t(1u << bit);

    if (on) {
        g_moku_test.event_flags = uint8_t(g_moku_test.event_flags | mask);
    } else {
        g_moku_test.event_flags = uint8_t(g_moku_test.event_flags & uint8_t(~mask));
    }
}

}  // namespace test_iface
