// audio_capture - run a GBA ROM headless and write its audio output to a WAV file.
//
// Optional audio validation tool (not part of the ROM build): it is how audio/ was checked
// against the real maxmod mixer - levels, clipping, effect durations, tempo.
//
//   gcc -O2 -Wall -o /tmp/audio_capture tools/audio_capture.c -lmgba
//   /tmp/audio_capture moku.gba 5750 /tmp/capture.wav 32768
//
// Usage: audio_capture ROM.gba FRAMES OUT.wav [RATE]   (stereo 16-bit WAV, default 32768 Hz)
#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void quiet_log(struct mLogger* l, int cat, enum mLogLevel lvl, const char* fmt, va_list a) {
    (void) l; (void) cat; (void) lvl; (void) fmt; (void) a;
}
static struct mLogger logger = { .log = quiet_log };

static void put32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s ROM FRAMES OUT.wav [RATE]\n", argv[0]); return 2; }
    int frames = atoi(argv[2]);
    int rate = argc > 4 ? atoi(argv[4]) : 32768;
    mLogSetDefaultLogger(&logger);

    struct mCore* core = mCoreFind(argv[1]);
    if (!core) { fprintf(stderr, "no core for %s\n", argv[1]); return 1; }
    core->init(core);
    mCoreInitConfig(core, NULL);
    if (!mCoreLoadFile(core, argv[1])) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    core->reset(core);

    size_t bufsize = 4096;
    core->setAudioBufferSize(core, bufsize);
    blip_t* left = core->getAudioChannel(core, 0);
    blip_t* right = core->getAudioChannel(core, 1);
    blip_set_rates(left, core->frequency(core), rate);
    blip_set_rates(right, core->frequency(core), rate);

    FILE* out = fopen(argv[3], "wb");
    if (!out) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
    fwrite("RIFF", 1, 4, out); put32(out, 0); fwrite("WAVEfmt ", 1, 8, out);
    put32(out, 16); put16(out, 1); put16(out, 2); put32(out, rate);
    put32(out, rate * 4); put16(out, 4); put16(out, 16);
    fwrite("data", 1, 4, out); put32(out, 0);

    int16_t* lbuf = malloc(bufsize * sizeof(int16_t));
    int16_t* rbuf = malloc(bufsize * sizeof(int16_t));
    int16_t* inter = malloc(bufsize * 2 * sizeof(int16_t));
    uint32_t total = 0;
    for (int f = 0; f < frames; ++f) {
        core->runFrame(core);
        int avail = blip_samples_avail(left);
        while (avail > 0) {
            int n = avail > (int) bufsize ? (int) bufsize : avail;
            blip_read_samples(left, lbuf, n, 0);
            blip_read_samples(right, rbuf, n, 0);
            for (int i = 0; i < n; ++i) { inter[i * 2] = lbuf[i]; inter[i * 2 + 1] = rbuf[i]; }
            fwrite(inter, sizeof(int16_t) * 2, n, out);
            total += n;
            avail = blip_samples_avail(left);
        }
    }
    long bytes = (long) total * 4;
    fseek(out, 4, SEEK_SET); put32(out, (uint32_t)(36 + bytes));
    fseek(out, 40, SEEK_SET); put32(out, (uint32_t) bytes);
    fclose(out);
    printf("captured %u frames -> %u samples @ %d Hz (%.2f s)\n", frames, total, rate,
           (double) total / rate);
    core->deinit(core);
    return 0;
}
