// Music and sound, with the volumes from the options applied and skin-aware choices.
#pragma once

#include <cstdint>

namespace audio {

enum class Track : uint8_t { NONE = 0, TITLE, MAP, PLAY, CLEAR, LOSE };

enum class Sfx : uint8_t {
    NONE = 0, STONE, CAPTURE, ATARI, MENU_MOVE, MENU_OK, MENU_BACK, TYPE, BARK, ROAR, BITE,
    CHEER, STAMP, CONFETTI, ERROR, UNLOCK, PASS, THINK, HINT, COUNT
};

void init();

// PLAY picks the track that goes with the current skin. Asking for the track already playing does
// nothing, so scenes can call this every frame.
void play_music(Track track);
void stop_music();
void play(Sfx sfx);

// The helper's own voice: a bark for INDI, a roar for REX, nothing for SEN.
void play_helper_voice(bool happy);

// Publishes what the sound hardware is actually doing to the test block, once a frame:
// reserved[11] = 1 while a module is really being mixed, reserved[14..15] = its sequence
// position. A position that keeps moving is the proof that music plays, which `music_id` on its
// own cannot give: that is only what was last asked for.
void publish_state();

// Re-reads the volumes after an options change.
void apply_volumes();

[[nodiscard]] Track current_track();
[[nodiscard]] Sfx last_sfx();

}  // namespace audio
