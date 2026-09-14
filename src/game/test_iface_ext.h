// Runtime side of the harness debug block (src/game/test_iface.h holds the frozen layout).
//
// The scene manager drives this: begin_frame() once per frame, then poll() to execute at most one
// harness command. A command is handled by the first handler that claims it:
//
//   1. the current scene (scene::Scene::handle_test_command)
//   2. subsystem handlers registered with test_iface::add_handler (audio, effects, ...)
//   3. the built-in defaults below (settings, save, scene switching, counters)
//
// A handler returns true when it has handled the command, and sets `result` to one of the
// TRES_* codes. cmd_result keeps that code; cmd is cleared to 0 so the harness knows the command
// ran (`wait32 $g_moku_test+72 0 <frames>` then `read32 $g_moku_test+80`).
#pragma once

#include <cstdint>

#include "game/test_iface.h"

namespace test_iface {

enum TestResult : uint32_t {
    TRES_NONE = 0,        // no command has run yet
    TRES_OK = 1,
    TRES_UNKNOWN = 2,     // no handler claimed the command
    TRES_BAD_ARG = 3,     // the argument is out of range
    TRES_REFUSED = 4,     // valid command, but not possible in this state (e.g. no such scene yet)
};

// ctx is the pointer passed to add_handler; return true to claim the command.
using CommandFn = bool (*)(void* ctx, uint32_t cmd, uint32_t arg, uint32_t& result);

// Fills in the magic and clears the counters. Called once from main().
void init();

// Heartbeat, frame counter, missed-frame / CPU peaks. Called once per frame by the scene manager,
// right after bn::core::update() returned (i.e. at the top of the next frame).
void begin_frame();

// Executes the pending command, if any.
void poll();

// Mirrors the settings and the current save slot into the block. Called once per frame by the
// scene manager, after the scene update and before bn::core::update(), so a harness command and
// its effect are visible in the same frame the command is cleared.
void end_frame();

// Subsystem handlers (max 4). Registering the same (fn, ctx) twice is a no-op.
void add_handler(CommandFn fn, void* ctx);
void remove_handler(CommandFn fn, void* ctx);

// Used by scene.cpp; scenes should override Scene::handle_test_command instead.
void set_scene_handler(CommandFn fn, void* ctx);

// Copies up to 31 bytes of the most recently shown banner / dialogue text into text_probe.
void set_text_probe(const char* text);

// event_flags bit helpers (bit0 atari banner ... bit7 dialogue box; see test_iface.h).
void set_event_flag(int bit, bool on);

void reset_counters();

}  // namespace test_iface
