#include "game/scene.h"

#include "bn_core.h"
#include "bn_timer.h"
#include "bn_unique_ptr.h"

#include "game/audio.h"
#include "game/settings.h"

namespace scene {

namespace {

constexpr int MAX_FACTORIES = 8;

// Registered before main() by FactoryRegistrar objects: these are plain zero-initialised globals
// and add_factory() only writes to them, so there is no static initialisation order problem.
Factory g_factories[MAX_FACTORIES];
int g_factory_count;

bn::unique_ptr<Scene> g_current;
SceneId g_current_id = SceneId::BOOT;
uint32_t g_current_arg;

bool g_pending;
SceneId g_pending_id = SceneId::BOOT;
uint32_t g_pending_arg;

bool scene_command(void* ctx, uint32_t cmd, uint32_t arg, uint32_t& result) {
    return static_cast<Scene*>(ctx)->handle_test_command(cmd, arg, result);
}

[[nodiscard]] Scene* build(SceneId id, uint32_t arg) {
    for (int index = g_factory_count - 1; index >= 0; --index) {
        if (Scene* built = g_factories[index](id, arg)) {
            return built;
        }
    }

    return nullptr;
}

void adopt(Scene* built, SceneId id, uint32_t arg) {
    g_current.reset(built);
    g_current_id = id;
    g_current_arg = arg;
    g_moku_test.scene = uint8_t(id);
    g_moku_test.scene_sub = 0;
    test_iface::set_scene_handler(scene_command, built);
    built->enter();
}

void destroy_current() {
    if (g_current) {
        g_current->exit();
        test_iface::set_scene_handler(nullptr, nullptr);
        g_current.reset();
    }
}

// The old scene is destroyed before the new one is built, so the two never hold VRAM at the same
// time. If no factory owns the requested id (a scene that does not exist yet), the
// previous scene is rebuilt from scratch so the game always has something on screen.
bool install(SceneId id, uint32_t arg) {
    bn::timer install_timer;
    SceneId previous_id = g_current_id;
    uint32_t previous_arg = g_current_arg;
    bool had_scene = g_current != nullptr;

    destroy_current();

    // Butano only releases the VRAM of the sprites and backgrounds we just dropped during its own
    // update, so the new scene would try to allocate on top of blocks that are still marked
    // "to remove" and assert. One update between the two scenes commits the frees.
    bn::core::update();

    if (Scene* built = build(id, arg)) {
        adopt(built, id, arg);
        uint32_t ticks = uint32_t(install_timer.elapsed_ticks());
        g_moku_test.reserved[12] = uint8_t(ticks);
        g_moku_test.reserved[13] = uint8_t(ticks >> 8);
        return true;
    }

    if (had_scene) {
        if (Scene* restored = build(previous_id, previous_arg)) {
            adopt(restored, previous_id, previous_arg);
            return false;
        }
    }

    // Nothing on screen: say so instead of reporting the scene that was just destroyed.
    g_current_id = SceneId::BOOT;
    g_moku_test.scene = uint8_t(SceneId::BOOT);
    return false;
}

void apply_pending() {
    // enter() may request another scene; bound the chain so a mistake cannot hang the frame.
    for (int guard = 0; guard < 4 && g_pending; ++guard) {
        SceneId id = g_pending_id;
        uint32_t arg = g_pending_arg;
        g_pending = false;
        install(id, arg);
    }

    g_pending = false;
}

}  // namespace

void add_factory(Factory factory) {
    if (factory && g_factory_count < MAX_FACTORIES) {
        g_factories[g_factory_count] = factory;
        ++g_factory_count;
    }
}

bool init(SceneId first, uint32_t arg) {
    return install(first, arg);
}

void request(SceneId id, uint32_t arg) {
    g_pending = true;
    g_pending_id = id;
    g_pending_arg = arg;
}

bool switch_now(SceneId id, uint32_t arg) {
    g_pending = false;

    if (!install(id, arg)) {
        return false;
    }

    apply_pending();
    return true;
}

SceneId current_id() { return g_current_id; }

Scene* current() { return g_current.get(); }

void run_frame() {
    test_iface::begin_frame();
    test_iface::poll();
    apply_pending();

    if (g_current) {
        g_current->update();
    }

    apply_pending();
    audio::publish_state();
    g_moku_test.sandbox_saved = uint8_t(settings::file().sandbox.valid ? 1 : 0);
    test_iface::end_frame();
    bn::core::update();
}

}  // namespace scene
