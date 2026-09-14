#include "game/render/effects.h"

#include "bn_bg_palettes.h"
#include "bn_colors.h"
#include "bn_display.h"
#include "bn_sprite_items_banner_plate.h"
#include "bn_sprite_items_confetti.h"
#include "bn_sprite_items_egg_shards.h"
#include "bn_sprite_items_helper_indi.h"
#include "bn_sprite_items_helper_rex.h"
#include "bn_sprite_items_helper_sen.h"
#include "bn_sprite_items_particles.h"
#include "bn_sprite_items_opponents.h"
#include "bn_sprite_items_rank_stamp.h"
#include "bn_sprite_items_speech_bubble.h"
#include "bn_sprite_palettes.h"

#include "game/audio.h"
#include "game/settings.h"
#include "game/test_iface.h"

namespace render {

namespace {

constexpr int BANNER_Y = 64;
constexpr int BANNER_LEFT = 40;          // two 64 px plates cover 8..136: the board column
constexpr int POPIN_X = 196;
constexpr int POPIN_Y = 112;

// A tiny deterministic generator: effects must not depend on the AI's stream.
uint32_t g_rng = 0x9E3779B9u;

uint32_t rnd()
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

int sprite_x(int screen_x) { return screen_x - (bn::display::width() / 2); }
int sprite_y(int screen_y) { return screen_y - (bn::display::height() / 2); }

const bn::sprite_item& helper_item()
{
    switch(settings::helper())
    {
    case 1:  return bn::sprite_items::helper_indi;
    case 2:  return bn::sprite_items::helper_rex;
    default: return bn::sprite_items::helper_sen;
    }
}

bn::color tint_colour(int skin)
{
    switch(skin)
    {
    case 2:  return bn::color(31, 13, 3);     // DINO: lava
    case 1:  return bn::color(31, 20, 8);     // PUP: warm orange
    default: return bn::color(24, 7, 5);
    }
}

}  // namespace

effects::effects() = default;

effects::~effects() = default;

void effects::set_skin(int skin)
{
    _skin = skin;
}

void effects::set_juicy(bool juicy)
{
    _juicy = juicy;
}

uint8_t effects::flags() const
{
    uint8_t f = 0;

    if(_banner_frames > 0) f = uint8_t(f | 0x02);
    if(_popin_frames > 0)  f = uint8_t(f | 0x04);
    if(_shake_frames > 0)  f = uint8_t(f | 0x08);
    if(_tint_frames > 0)   f = uint8_t(f | 0x10);
    if(! _particles.empty()) f = uint8_t(f | 0x20);
    if(_stamp)             f = uint8_t(f | 0x40);

    return f;
}

void effects::banner(const bn::string_view& text, BannerStyle style, Ink ink)
{
    _clear_banner();

    const int frame = (_skin * 2) + int(style);

    for(int i = 0; i < 2; ++i)
    {
        _plates.push_back(bn::sprite_items::banner_plate.create_sprite(
            sprite_x(BANNER_LEFT + 32 + i * 64), sprite_y(BANNER_Y), frame));
        _plates.back().set_bg_priority(0);
        _plates.back().set_z_order(-8);
    }

    _banner_text.setup(BANNER_LEFT + 64, BANNER_Y - 8, Font::DISPLAY, ink, Align::CENTER);
    _banner_text.set_z_order(-9);
    _banner_text.force(text);
    _banner_frames = BANNER_FRAMES;
}

void effects::shake(int frames, int amplitude)
{
    if(! juicy())
    {
        return;
    }

    _shake_frames = frames;
    _shake_amp = amplitude;
}

void effects::burst(int screen_x, int screen_y, int count, bool capture)
{
    if(! juicy())
    {
        return;
    }

    for(int i = 0; i < count && ! _particles.full(); ++i)
    {
        const uint32_t r = rnd();
        bn::sprite_ptr sprite = [&]() -> bn::sprite_ptr
        {
            if(capture && _skin == 2)
            {
                return bn::sprite_items::egg_shards.create_sprite(
                    sprite_x(screen_x), sprite_y(screen_y), int((r >> 3) % 6));
            }

            if(capture && _skin == 1)
            {
                return bn::sprite_items::confetti.create_sprite(
                    sprite_x(screen_x), sprite_y(screen_y), int((r >> 3) % 12));
            }

            const int frame = (_skin * 16) + int((r >> 3) % 4) * 4 + int((r >> 7) % 4);
            return bn::sprite_items::particles.create_sprite(sprite_x(screen_x), sprite_y(screen_y), frame);
        }();

        sprite.set_bg_priority(0);
        sprite.set_z_order(-6);

        Particle p{ bn::move(sprite), 0, 0, 0 };
        p.vx = bn::fixed(int((r >> 11) % 48) - 24) / 16;
        p.vy = bn::fixed(-16 - int((r >> 17) % 24)) / 16;
        p.life = int16_t(18 + (r >> 23) % 12);
        _particles.push_back(bn::move(p));
    }
}

void effects::tint(int frames)
{
    if(! juicy())
    {
        return;
    }

    _tint_frames = frames;
    _tint_total = frames;
    bn::bg_palettes::set_fade(tint_colour(_skin), 0);
    bn::sprite_palettes::set_fade(tint_colour(_skin), 0);
}

void effects::_show_popin(const bn::sprite_item& item, int frame, const bn::string_view& line, int frames)
{
    _clear_popin();
    _popin = item.create_sprite(sprite_x(POPIN_X), sprite_y(POPIN_Y + 24), frame);
    _popin->set_bg_priority(0);
    _popin->set_z_order(-5);

    const int base = settings::skin() * 3;

    // Left cap, two middles and the tailed right cap: 128 px of bubble, ending just short of the
    // right edge of the screen and just above the portrait the tail points at.
    const int part[4] = { 0, 1, 1, 2 };

    for(int i = 0; i < 4; ++i)
    {
        _bubble.push_back(bn::sprite_items::speech_bubble.create_sprite(
            sprite_x(POPIN_X - 80 + i * 32), sprite_y(POPIN_Y - 26), base + part[i]));
        _bubble.back().set_bg_priority(0);
        _bubble.back().set_z_order(-6);
    }

    // The bubble body is 128 x 24: one line centred, or two at the dialogue box's spacing. Lines
    // longer than that are broken at a space rather than run off the art.
    bn::string<32> wrapped[2];
    const int count = wrap(line, 118, wrapped, 2, Font::SMALL);

    // The four bubble parts span POPIN_X-96 .. POPIN_X+32, so their middle is POPIN_X-32.
    _popin_text.setup(POPIN_X - 32, count > 1 ? POPIN_Y - 38 : POPIN_Y - 30, Font::SMALL, Ink::INK,
                      Align::CENTER);
    _popin_text.set_z_order(-7);
    _popin_text.force(count > 0 ? bn::string_view(wrapped[0]) : line);

    if(count > 1)
    {
        _popin_text2.setup(POPIN_X - 32, POPIN_Y - 26, Font::SMALL, Ink::INK, Align::CENTER);
        _popin_text2.set_z_order(-7);
        _popin_text2.force(wrapped[1]);
    }

    _popin_frames = frames;
}

void effects::helper_popin(const bn::string_view& line, Pose pose, int frames)
{
    if(! juicy())
    {
        return;
    }

    _show_popin(helper_item(), int(pose), line, frames);
    _popin_is_opponent = false;
}

void effects::opponent_popin(const bn::string_view& line, int opponent, int frames)
{
    _show_popin(bn::sprite_items::opponents, opponent, line, frames);
    _popin_is_opponent = true;
}

void effects::stamp(int rank)
{
    _stamp = bn::sprite_items::rank_stamp.create_sprite(sprite_x(72), sprite_y(76), rank);
    _stamp->set_bg_priority(0);
    _stamp->set_z_order(-10);
    _stamp_frames = 20;
    shake(6, 3);
    audio::play(audio::Sfx::STAMP);
}

void effects::clear_stamp()
{
    _stamp.reset();
    _stamp_frames = 0;
}

void effects::_clear_banner()
{
    _plates.clear();
    _banner_text.clear();
    _banner_frames = 0;
}

void effects::_clear_popin()
{
    _popin.reset();
    _bubble.clear();
    _popin_text.clear();
    _popin_text2.clear();
    _popin_frames = 0;
    _popin_is_opponent = false;
}

void effects::update()
{
    if(_banner_frames > 0 && --_banner_frames == 0)
    {
        _clear_banner();
    }

    if(_popin_frames > 0 && --_popin_frames == 0)
    {
        _clear_popin();
    }

    if(_shake_frames > 0)
    {
        --_shake_frames;
        const int amp = _shake_amp;
        _offset = bn::fixed_point(int(rnd() % uint32_t(amp * 2 + 1)) - amp,
                                  int(rnd() % uint32_t(amp * 2 + 1)) - amp);

        if(_shake_frames == 0)
        {
            _offset = bn::fixed_point(0, 0);
        }
    }

    if(_tint_frames > 0)
    {
        --_tint_frames;
        // ramp up for the first third, then back down
        const int half = _tint_total / 2;
        const int up = _tint_total - _tint_frames;
        const bn::fixed intensity = (up <= half)
            ? bn::fixed(up) / bn::fixed(half > 0 ? half * 2 : 1)
            : bn::fixed(_tint_frames) / bn::fixed(half > 0 ? half * 2 : 1);
        bn::bg_palettes::set_fade_intensity(intensity);
        bn::sprite_palettes::set_fade_intensity(intensity);

        if(_tint_frames == 0)
        {
            bn::bg_palettes::set_fade_intensity(0);
            bn::sprite_palettes::set_fade_intensity(0);
        }
    }

    if(_stamp_frames > 0)
    {
        --_stamp_frames;
        // the stamp slams down: it starts big and settles
        const bn::fixed scale = bn::fixed(1) + bn::fixed(_stamp_frames) / 16;
        _stamp->set_scale(scale);

        if(_stamp_frames == 0)
        {
            _stamp->set_scale(1);
        }
    }

    for(int i = int(_particles.size()) - 1; i >= 0; --i)
    {
        Particle& p = _particles[i];
        p.sprite.set_position(p.sprite.x() + p.vx, p.sprite.y() + p.vy);
        p.vy += bn::fixed(1) / 8;              // gravity

        if(--p.life <= 0)
        {
            _particles.erase(_particles.begin() + i);
        }
    }

    g_moku_test.event_flags = uint8_t((g_moku_test.event_flags & 0x81) | flags());
}

void effects::skip()
{
    _clear_banner();
    _clear_popin();
    _particles.clear();
    _shake_frames = 0;
    _offset = bn::fixed_point(0, 0);

    if(_tint_frames > 0)
    {
        _tint_frames = 0;
        bn::bg_palettes::set_fade_intensity(0);
        bn::sprite_palettes::set_fade_intensity(0);
    }

    if(_stamp_frames > 0)
    {
        _stamp_frames = 0;
        _stamp->set_scale(1);
    }
}

bool effects::busy() const
{
    return _banner_frames > 0 || _popin_frames > 0 || ! _particles.empty() || _shake_frames > 0 ||
           _tint_frames > 0 || _stamp_frames > 0;
}

}  // namespace render
