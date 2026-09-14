// Scene stack-free scene manager: one live scene at a time, switched between frames.
//
// HOW TO ADD A SCENE (no existing file has to change):
//
//   1. Derive from scene::Scene, override update() and id(); enter()/exit() are optional
//      (the constructor/destructor already run at the right time, so most scenes do not need them).
//   2. Write a factory that returns a new scene for the ids it owns and nullptr for the rest:
//
//        namespace {
//        scene::Scene* factory(SceneId id, uint32_t arg) {
//            if (id == SceneId::OPTIONS) { return new options_scene(arg); }
//            return nullptr;
//        }
//        scene::FactoryRegistrar registrar(factory);   // self-registers before main()
//        }
//
//      Factories are tried newest-first, so a real scene always wins over the placeholder
//      fallbacks in this skeleton (board_demo_scene answers SceneId::PLAY until the play scene
//      registers its own factory).
//   3. Ask for a switch with scene::request(id, arg). The switch happens at the end of the current
//      frame: the old scene is destroyed (freeing its VRAM) before the new one is built.
//
// RULES
//   * A scene's update() must NOT call bn::core::update(); run_frame() does that once per frame.
//   * A scene must release every bn::*_ptr it owns in its destructor (i.e. own them as members).
//   * Scenes report their state in g_moku_test (scene_sub, menu_index, cursor_x/y, ...); the scene
//     id itself is published by the manager.
#pragma once

#include <cstdint>

#include "game/test_iface.h"
#include "game/test_iface_ext.h"

namespace scene {

class Scene {

public:
    Scene() = default;

    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Called right after construction (VRAM is already free from the previous scene).
    virtual void enter() {}

    // Called right before destruction.
    virtual void exit() {}

    // One frame of scene logic. Never call bn::core::update() here.
    virtual void update() = 0;

    [[nodiscard]] virtual SceneId id() const = 0;

    // First chance at a harness command. Return true (and set result) when handled.
    [[nodiscard]] virtual bool handle_test_command(uint32_t cmd, uint32_t arg, uint32_t& result) {
        (void)cmd;
        (void)arg;
        (void)result;
        return false;
    }
};

// Returns a new scene for the given id, or nullptr if this factory does not own the id.
using Factory = Scene* (*)(SceneId id, uint32_t arg);

void add_factory(Factory factory);

// Static object that registers a factory before main() runs.
struct FactoryRegistrar {
    explicit FactoryRegistrar(Factory factory) { add_factory(factory); }
};

// Builds the first scene. Returns false if no factory owns the id (the game then has no scene).
bool init(SceneId first, uint32_t arg = 0);

// Asks for a switch at the end of the current frame.
void request(SceneId id, uint32_t arg = 0);

// Switches immediately (used by the harness command dispatcher, which runs before the scene
// update). Returns false and keeps the current scene if no factory owns the id.
bool switch_now(SceneId id, uint32_t arg = 0);

[[nodiscard]] SceneId current_id();
[[nodiscard]] Scene* current();

// One full frame: counters, harness command, scene update, pending switch, bn::core::update().
void run_frame();

}  // namespace scene
