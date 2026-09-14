// MOKU - Tactics of Go. Entry point: bring up Butano, load the save, run the scene loop.
#include "bn_core.h"
#include "bn_color.h"
#include "bn_bg_palettes.h"

#include "bn_log.h"
#include "bn_memory.h"

#include "game/scene.h"
#include "game/settings.h"
#include "game/test_iface_ext.h"

int main() {
    bn::core::init();

    // Ink #1b1f2a: the backdrop only shows where no background covers the screen.
    bn::bg_palettes::set_transparent_color(bn::color(3, 3, 5));

    test_iface::init();
    settings::init();
    BN_LOG("EWRAM free at boot: ", bn::memory::available_alloc_ewram(), " bytes");
    scene::init(SceneId::TITLE);

    while (true) {
        scene::run_frame();
    }
}
