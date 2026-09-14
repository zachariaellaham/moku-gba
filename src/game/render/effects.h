// The juice: banners, screen shake, particle bursts, a full-screen tint flash, the helper popping
// in with a one-liner, and the rank stamp.
//
// Everything here is bounded: no effect lasts longer than 45 frames and A skips whatever is
// running. With EFFECTS: OFF (and always in the CLASSIC skin) only the atari banner survives, as
// CLASSIC is meant to be sober.
#pragma once

#include <cstdint>

#include "bn_fixed_point.h"
#include "bn_optional.h"
#include "bn_sprite_item.h"
#include "bn_sprite_ptr.h"
#include "bn_string_view.h"
#include "bn_vector.h"

#include "game/render/dialogue.h"
#include "game/render/text.h"

namespace render {

enum class BannerStyle : uint8_t { STRAIGHT = 0, SLANTED = 1 };

constexpr int MAX_PARTICLES = 14;
constexpr int BANNER_FRAMES = 45;
constexpr int POPIN_FRAMES = 90;

class effects {

public:
    effects();

    ~effects();

    effects(const effects&) = delete;
    effects& operator=(const effects&) = delete;

    // CLASSIC (skin 0) never shakes, flashes or throws particles.
    void set_skin(int skin);
    void set_juicy(bool juicy);
    [[nodiscard]] bool juicy() const { return _juicy && _skin != 0; }

    void banner(const bn::string_view& text, BannerStyle style = BannerStyle::STRAIGHT, Ink ink = Ink::PAPER);
    void shake(int frames, int amplitude = 2);
    void burst(int screen_x, int screen_y, int count, bool capture);
    void tint(int frames);
    void helper_popin(const bn::string_view& line, Pose pose = Pose::CHEER, int frames = POPIN_FRAMES);

    // The opponent character speaking (mockup 2b). Same slot as the helper's bubble - only one of
    // them talks at a time - and, unlike the helper, it is not part of the juice: the characters
    // speak in CLASSIC too.
    void opponent_popin(const bn::string_view& line, int opponent, int frames = POPIN_FRAMES);
    void stamp(int rank);                 // 0 S, 1 A, 2 B, 3 C
    void clear_stamp();

    void update();
    void skip();
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bn::fixed_point offset() const { return _offset; }
    [[nodiscard]] bool banner_visible() const { return _banner_frames > 0; }
    [[nodiscard]] bool popin_visible() const { return _popin_frames > 0; }
    [[nodiscard]] bool opponent_visible() const { return _popin_frames > 0 && _popin_is_opponent; }
    [[nodiscard]] uint8_t flags() const;

private:
    struct Particle {
        bn::sprite_ptr sprite;
        bn::fixed vx, vy;
        int16_t life;
    };

    void _clear_banner();
    void _clear_popin();
    void _show_popin(const bn::sprite_item& item, int frame, const bn::string_view& line, int frames);

    bn::vector<bn::sprite_ptr, 2> _plates;
    text_block _banner_text;
    bn::vector<Particle, MAX_PARTICLES> _particles;
    bn::optional<bn::sprite_ptr> _popin;
    bn::vector<bn::sprite_ptr, 4> _bubble;
    text_block _popin_text;
    text_block _popin_text2;
    bn::optional<bn::sprite_ptr> _stamp;

    bn::fixed_point _offset;
    int _skin = 0;
    int _banner_frames = 0;
    int _shake_frames = 0;
    int _shake_amp = 2;
    int _tint_frames = 0;
    int _tint_total = 0;
    int _popin_frames = 0;
    int _stamp_frames = 0;
    bool _juicy = true;
    bool _popin_is_opponent = false;
};

}  // namespace render
