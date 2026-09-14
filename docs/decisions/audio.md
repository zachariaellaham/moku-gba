# Audio (synthesized music + SFX, `tools/gen_audio.py`)

Area: audio. Files: `tools/gen_audio.py` (generator), `audio/*.mod` + `audio/*.wav`
(generated — edit the generator, never the assets), `tools/audio_capture.c` (optional hardware
validation tool).

```
python3 tools/gen_audio.py                      # regenerate audio/, validate, exit 1 on problems
python3 tools/gen_audio.py --render /tmp/aud    # + 16-bit WAV renders of every module and effect
python3 tools/gen_audio.py --list               # print the Butano item names
```

Nothing is sampled or downloaded: every waveform, drum and chiptune instrument is synthesized with
numpy inside the generator, and the `.mod` files are written by a ProTracker writer in the same
file. The generator also contains a small ProTracker **mixer** (`ModRenderer`) that models the
maxmod GBA mixer, so each module can be rendered to WAV and measured without a ROM build.

## Item names (exactly what Butano generates from the file names)

| `bn::music_items::` | key / mood | loop | tempo | bytes |
|---|---|---|---|---|
| `music_title` | D hirajoshi koto piece, calm | 80.0 s | 96 bpm, 32 bars | 9 564 |
| `music_map` | G major pentatonic, gentle travelling | 85.7 s | 112 bpm, 40 bars | 11 568 |
| `music_play_classic` | A kumoi, sparse and meditative | 75.8 s | 76 bpm, 24 bars | 9 788 |
| `music_play_pup` | C major, bouncy, full drum kit | 87.3 s | 132 bpm, 48 bars | 15 598 |
| `music_play_dino` | E minor pentatonic, tribal toms | 73.8 s | 104 bpm, 32 bars | 11 758 |
| `music_clear` | C major fanfare, **non-looping** | 6.4 s | 150 bpm, 4 bars | 7 528 |
| `music_lose` | A minor sting, **non-looping** | 6.0 s | 160 bpm, 4 bars | 3 582 |

`bn::sound_items::` — `sfx_stone` (wood click, 80 ms), `sfx_capture` (pop, 110), `sfx_atari`
(two-tone alert, 158), `sfx_menu_move` (38), `sfx_menu_ok` (155), `sfx_menu_back` (145),
`sfx_type` (typewriter tick, 26), `sfx_bark` (200), `sfx_roar` (850), `sfx_bite` (170),
`sfx_cheer` (1100), `sfx_stamp` (slam, 290), `sfx_confetti` (780), `sfx_error` (205),
`sfx_unlock` (640), `sfx_pass` (260), `sfx_think` (soft tick, 42), `sfx_hint` (bell, 460).

Total on disk 157.7 KB; **maxmod soundbank in the ROM: 129.3 KB** (budget 400 KB).

## How the game should play it

```cpp
bn::music::play(bn::music_items::music_title, music_volume);           // looping tracks
bn::music_items::music_clear.play(music_volume, false);                // fanfare / sting: no loop
bn::sound_items::sfx_stone.play();                                     // effects are pre-balanced
```

* **Volumes.** The effects are loudness-balanced against each other, so play them at `1` (or at the
  player's SFX setting) — do not hand-tune per call site. Music was mastered for **0.65–0.75**;
  `0.7` is the value used for every measurement below, and is the recommended default for the
  options screen.
* **Non-looping.** `music_clear` and `music_lose` must be played with `loop = false`, otherwise they
  restart. Both end in a real fade, so `bn::music::playing()` going false is a safe cue to leave the
  screen. `play_jingle()` also works if the map/board music should resume afterwards.
* **Effect channels.** `BN_CFG_AUDIO_MAX_SOUND_CHANNELS` is 4: at most four effects sound at once,
  the oldest/lowest priority is dropped. Use `play_with_priority` for `sfx_capture`/`sfx_atari` if a
  juicy event ever fires five effects in one frame.
* **`sfx_think`** is one 42 ms tick, not a loop: retrigger it every ~20 frames while the AI thinks.
* Do not start `sfx_stamp` and `sfx_confetti` on the same frame — a couple of frames apart both
  reads better and keeps the mix out of the limiter (see head-room below).

* **Soundbank ids** (`src/game/test_iface.h` exposes `music_id` / `last_sfx`): mmutil numbers items
  by *alphabetical file name* over the whole `audio/` folder, so today music is 0..6
  (`music_clear` 0, `music_lose` 1, `music_map` 2, `music_play_classic` 3, `music_play_dino` 4,
  `music_play_pup` 5, `music_title` 6) and the effects are 21..38. Those numbers move if any audio
  file is ever added or removed, so fill the debug block with `item.id() + 1` (0 = nothing playing)
  and let ROM tests compare against `bn::music_items::x.id() + 1` — never against a literal.

## Head-room (why the modules are normalized the way they are)

From `butano/hw/3rd_party/maxmod/src/gba/mixer_asm.s`: each mixer channel adds
`sample * fvol / 256` to a signed 8-bit output that is hard-clamped to ±127, and a MOD channel at
volume 64 with the module volume at 1.0 ends up at `fvol = 128`. So the quantity that matters is

```
M(t) = sum over channels of |sample(t)| * channel_volume / 64      (sample in [-1,1])
```

and maxmod clips when `M >= 2.0`. The generator renders every module, measures `peak(M)` and
`rms(M)`, and scales **all** volume bytes (instrument defaults, `Cxx`, `Axy` slide rates) by
`min(1.15 / peak, 0.28 / rms)`. Peak-only normalization would leave the sparse pieces far quieter
than the busy ones, so whichever limit binds first wins; the result is 1.11–1.16 peak for every
track and 0.18–0.28 RMS.

Measured on the real thing (mGBA capture of the ROM, music at 0.7, fraction of full scale):

| | title | map | classic | pup | dino | clear | lose |
|---|---|---|---|---|---|---|---|
| peak | 0.51 | 0.31 | 0.46 | 0.40 | 0.37 | 0.51 | 0.50 |
| rms | 0.099 | 0.087 | 0.138 | 0.096 | 0.074 | 0.100 | 0.125 |
| clipped samples | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

Music + `sfx_stamp` + `sfx_confetti` together peak at 0.63 of full scale. Only a deliberately
absurd stack (music + cheer + roar + stamp + capture in the same 30 ms) reaches the clamp, and then
for 0.007 % of samples.

## Format decisions

* **SFX: 8-bit unsigned mono WAV at 16 000 Hz.** maxmod stores samples as 8-bit signed, so a WAV
  byte is a ROM byte — no conversion loss and no surprise. 16 kHz sits right on Butano's default
  mixing rate (`BN_AUDIO_MIXING_RATE_16_KHZ` = 15 768 Hz), so effects are neither resampled up nor
  aliased down. Everything is band-limited below 7 kHz, DC-blocked, and faded to exactly zero at
  both ends so no effect can click.
* **Music: 4-channel ProTracker `M.K.` modules.** The samples inside are tiny looped single cycles
  (16–128 bytes) plus one-shot drums designed at the exact playback rate of the tracker note they
  are triggered at, which is why 7 tracks of 60–90 s fit in 69 KB. Pattern data is deduplicated by
  the writer, so repeated sections cost one order byte each.
* **Tuning.** A looped cycle of length `L` played at tracker note C-1 sounds at `4181.7 / L` Hz
  (mmutil tunes MOD samples to mid-C = 8363 Hz, the NTSC Amiga constant). Powers of two land 1.8
  cents from equal temperament, so every instrument uses `L` = 16/32/64/128 and finetune 0 — the
  whole soundtrack is in tune with itself and with A = 440.
* **Volume envelopes.** MOD has no envelopes: plucked instruments bake a bright→mellow attack into
  the sample before the loop point, and the generator writes `Axy` slides (5 volume units per row at
  speed 6) or explicit `Cxx` ramps (`fade()`) where a slower or smoother decay is needed.
* **Stereo.** mmutil applies the usual Amiga-style panning separation (channels 1/4 left, 2/3
  right, roughly 3:1, not hard-panned). It sums to mono on the console speaker and gives a pleasant
  spread on headphones; nothing in the game code needs to care. If it is ever unwanted it is a
  Makefile one-liner (`AUDIOTOOL := $(BN_MMUTIL) -p0`), not an asset change.
* **Tempo on hardware.** maxmod ticks from the 59.7275 Hz vblank, so the real playback is ~0.45 %
  slower than the nominal tempo: an 80.0 s loop is 80.4 s on the console. Irrelevant musically,
  but it is why the capture drifts ~30 ms against the Python render over 7 s.

## Validation performed

1. `python3 tools/gen_audio.py` fails (exit 1) on: a module outside its length window (60–120 s for
   loops, 5–8 s for the fanfare, 3–7 s for the sting), any clipped sample, a mix peak above 1.45, a
   silent gap longer than 1.5 s, an effect that does not start and end at zero, an effect peak above
   0.95, a loop-point click, or a total over 400 KB.
2. Loop-point click test: the module is rendered twice back to back; restarting always produces the
   same downbeat transient as starting from silence, so the test compares the step across the loop
   point with the step at the very beginning and only complains about the excess.
3. `mmutil` is run over the whole folder and the generated soundbank header is checked for
   `MOD_<name>` / `SFX_<name>` for all 25 items.
4. Instrument tuning is checked end to end by rendering one note per instrument and measuring its
   fundamental (all land on 440.00 / 110.00 Hz).
5. The written notes of every song were dumped as pitch classes and checked against the intended
   mode (D hirajoshi, G major pentatonic, A kumoi, C major, E minor pentatonic).
6. A scratch Butano project (`AUDIO := <repo>/audio`) builds the soundbank without warnings
   and generates exactly the 7 + 18 expected item names; the ROM plays every track and effect
   headless under `tests/rom/harness/mokurun` with no crash, at 4–10 % CPU with music + effects.
7. `tools/audio_capture.c` (`gcc -O2 -o audio_capture tools/audio_capture.c -lmgba`) records the
   emulated GBA audio output to a WAV. The capture confirms: no clipping, correct effect durations,
   and a 0.91–0.97 envelope correlation / 0.69–0.90 spectral correlation against the Python
   renderer — i.e. the renderer is a trustworthy stand-in for listening.

## Known limitations

* Notes above ~E5 play their sample back faster than the 15 768 Hz mixing rate, so their top one or
  two harmonics fold down a little. The waveforms are rolled off hard enough that it reads as
  chiptune character; melodies are deliberately written in the C3–C5 register where it cannot
  happen.
* `music_title` has one 0.4 s gap in its quietest bar. That is the piece breathing, not a dropout.
* The sting and the fanfare are one pattern each; if the mission-clear screen ever needs more than
  6.4 s of music, extend `song_clear()` rather than looping it.
