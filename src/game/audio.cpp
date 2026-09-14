#include "game/audio.h"

#include "bn_music.h"
#include "bn_music_items.h"
#include "bn_sound.h"
#include "bn_sound_items.h"

#include "game/settings.h"
#include "game/test_iface.h"

namespace audio {

namespace {

Track g_track = Track::NONE;
Sfx g_last = Sfx::NONE;

// 0..8 in the options becomes 0..1 for Butano.
bn::fixed music_volume() { return bn::fixed(settings::music()) / 8; }
bn::fixed sfx_volume() { return bn::fixed(settings::sfx()) / 8; }

constexpr int MAX_SFX_PER_FRAME = 4;
Sfx g_frame_sfx[MAX_SFX_PER_FRAME] = {};
int g_frame_sfx_count = 0;

}  // namespace

void init()
{
    g_track = Track::NONE;
    g_last = Sfx::NONE;
    bn::sound::set_master_volume(sfx_volume());
}

void play_music(Track track)
{
    if(track == g_track && bn::music::playing())
    {
        return;
    }

    g_track = track;

    if(track == Track::NONE)
    {
        if(bn::music::playing())
        {
            bn::music::stop();
        }

        g_moku_test.music_id = 0;
        return;
    }

    const bn::fixed volume = music_volume();

    if(volume <= 0)
    {
        if(bn::music::playing())
        {
            bn::music::stop();
        }

        g_moku_test.music_id = uint16_t(track);
        return;
    }

    switch(track)
    {
    case Track::TITLE:
        bn::music_items::music_title.play(volume);
        break;

    case Track::MAP:
        bn::music_items::music_map.play(volume);
        break;

    case Track::PLAY:
        switch(settings::skin())
        {
        case 1:  bn::music_items::music_play_pup.play(volume); break;
        case 2:  bn::music_items::music_play_dino.play(volume); break;
        default: bn::music_items::music_play_classic.play(volume); break;
        }
        break;

    case Track::CLEAR:
        bn::music_items::music_clear.play(volume, false);
        break;

    case Track::LOSE:
        bn::music_items::music_lose.play(volume, false);
        break;

    default:
        break;
    }

    g_moku_test.music_id = uint16_t(track);
}

void publish_state()
{
    g_frame_sfx_count = 0;

    const bool playing = bn::music::playing();
    g_moku_test.reserved[11] = uint8_t(playing ? 1 : 0);

    const int position = playing ? bn::music::position() : 0;
    g_moku_test.reserved[14] = uint8_t(position);
    g_moku_test.reserved[15] = uint8_t(position >> 8);
}

void stop_music()
{
    play_music(Track::NONE);
}

void play(Sfx sfx)
{
    const bn::fixed volume = sfx_volume();

    if(volume <= 0 || sfx == Sfx::NONE)
    {
        return;
    }

    // Butano has a fixed pool of sound handles and asserts - which on a GBA means a hang - when it
    // runs out. A single frame can easily ask for more than a pool's worth: a move that captures a
    // group and puts two others in atari drains its event queue all at once, and so does the end of
    // a game. Nobody can hear sixteen effects fired on the same frame anyway, so the same effect is
    // played once per frame and no more than MAX_SFX_PER_FRAME start together.
    for(int i = 0; i < g_frame_sfx_count; ++i)
    {
        if(g_frame_sfx[i] == sfx)
        {
            return;
        }
    }

    if(g_frame_sfx_count >= MAX_SFX_PER_FRAME)
    {
        return;
    }

    g_frame_sfx[g_frame_sfx_count] = sfx;
    ++g_frame_sfx_count;

    // Published after the gate, so the test block reports what was heard rather than what was asked
    // for.
    g_last = sfx;
    g_moku_test.last_sfx = uint16_t(sfx);

    switch(sfx)
    {
    case Sfx::STONE:      bn::sound_items::sfx_stone.play(volume); break;
    case Sfx::CAPTURE:    bn::sound_items::sfx_capture.play(volume); break;
    case Sfx::ATARI:      bn::sound_items::sfx_atari.play(volume); break;
    case Sfx::MENU_MOVE:  bn::sound_items::sfx_menu_move.play(volume); break;
    case Sfx::MENU_OK:    bn::sound_items::sfx_menu_ok.play(volume); break;
    case Sfx::MENU_BACK:  bn::sound_items::sfx_menu_back.play(volume); break;
    case Sfx::TYPE:       bn::sound_items::sfx_type.play(volume); break;
    case Sfx::BARK:       bn::sound_items::sfx_bark.play(volume); break;
    case Sfx::ROAR:       bn::sound_items::sfx_roar.play(volume); break;
    case Sfx::BITE:       bn::sound_items::sfx_bite.play(volume); break;
    case Sfx::CHEER:      bn::sound_items::sfx_cheer.play(volume); break;
    case Sfx::STAMP:      bn::sound_items::sfx_stamp.play(volume); break;
    case Sfx::CONFETTI:   bn::sound_items::sfx_confetti.play(volume); break;
    case Sfx::ERROR:      bn::sound_items::sfx_error.play(volume); break;
    case Sfx::UNLOCK:     bn::sound_items::sfx_unlock.play(volume); break;
    case Sfx::PASS:       bn::sound_items::sfx_pass.play(volume); break;
    case Sfx::THINK:      bn::sound_items::sfx_think.play(volume); break;
    case Sfx::HINT:       bn::sound_items::sfx_hint.play(volume); break;
    default: break;
    }
}

void play_helper_voice(bool happy)
{
    switch(settings::helper())
    {
    case 1:  play(happy ? Sfx::BARK : Sfx::BARK); break;
    case 2:  play(happy ? Sfx::ROAR : Sfx::BITE); break;
    default: break;                      // Master Sen says nothing he has not already said
    }
}

void apply_volumes()
{
    bn::sound::set_master_volume(sfx_volume());

    if(bn::music::playing())
    {
        bn::music::set_volume(music_volume());
    }
    else if(g_track != Track::NONE && music_volume() > 0)
    {
        const Track track = g_track;
        g_track = Track::NONE;
        play_music(track);
    }
}

Track current_track() { return g_track; }
Sfx last_sfx() { return g_last; }

}  // namespace audio
