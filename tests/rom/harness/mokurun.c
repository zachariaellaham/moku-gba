// mokurun - headless GBA test runner built on libmgba.
//
// Usage: mokurun [-v] ROM.gba [SCRIPT]   (SCRIPT defaults to stdin)
//
// Script: one command per line, '#' comments. ADDR may be hex (0x...), decimal,
// or $symbol[+offset] after `symbols FILE` (FILE = `arm-none-eabi-nm ELF` output).
//
//   symbols FILE
//   frames N                       run N frames with the held keys
//   hold K[,K..]  / release K[,K..]|all   keys: A B SELECT START RIGHT LEFT UP DOWN R L
//   press K[,K..] [HOLD=2] [GAP=2] hold HOLD frames, release, run GAP frames
//   screenshot PATH.png
//   read8|read16|read32 ADDR       prints "READ <addr> = <value>"
//   write8|write16|write32 ADDR VAL
//   assert8|assert16|assert32 ADDR VAL       exit 1 on mismatch
//   wait8|wait16|wait32 ADDR VAL MAXFRAMES   run until mem == VAL
//   waitne8|waitne16|waitne32 ADDR VAL MAXFRAMES   run until mem != VAL
//   waitchange8|16|32 ADDR MAXFRAMES         run until value changes
//   sram_save PATH / sram_load PATH
//   reset
//   random N SEED [MASK]           N frames of random input (MASK = allowed keys, default all)
//   record_start DIR [RATE]        capture every frame from now on: DIR/000000.png + DIR/audio.raw
//                                  (signed 16-bit stereo at RATE, default 32768) - for the trailer
//   record_stop                    stop capturing and print the frame count and sample rate
//   echo TEXT
//   stats                          prints frames run and wall-clock time
//   exit CODE
// mgba/flags.h must come first: struct mCore has #ifdef USE_DEBUGGERS members, so a program that
// does not see the same flags the library was built with gets a shorter struct and calls the wrong
// function pointers (savedataClone and everything after it in the vtable).
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/interface.h>
#include <mgba/core/blip_buf.h>
#include <mgba-util/vfs.h>
#include <png.h>
#include <mgba-util/png-io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

static int verbose = 0;
static struct mCore* core;
static uint32_t held = 0;
static uint64_t total_frames = 0;
static int crashed = 0;
static color_t* vbuf;
static unsigned vw, vh;

struct sym { char name[96]; uint32_t addr; };
static struct sym* syms; static int nsyms;

static void quiet_log(struct mLogger* l, int cat, enum mLogLevel lvl, const char* fmt, va_list args) {
    (void)l;
    const char* name = mLogCategoryName(cat);
    if (name && !strcmp(name, "GBA Debug")) { printf("ROMLOG: "); vprintf(fmt, args); printf("\n"); fflush(stdout); return; }
    if (verbose || lvl == mLOG_FATAL || lvl == mLOG_ERROR) { vfprintf(stderr, fmt, args); fputc('\n', stderr); }
}
static struct mLogger logger = { .log = quiet_log };

static void on_crash(void* ctx) { (void)ctx; crashed = 1; fprintf(stderr, "CORE CRASHED at frame %llu\n", (unsigned long long)total_frames); }

static void die(const char* msg) { fprintf(stderr, "FAIL: %s (frame %llu)\n", msg, (unsigned long long)total_frames); exit(1); }

// --- recording ------------------------------------------------------------------------------
// A trailer wants what a test does not: every frame and the sound that went with it. Recording is
// a mode rather than a command, so the whole script vocabulary (press, wait, random...) keeps
// working while the camera runs.
static int recording = 0;
static char rec_dir[480];
static long rec_frames = 0;
static FILE* rec_audio = NULL;
static int rec_rate = 32768;

static void screenshot(const char* path);

static void record_capture(void) {
    char path[600];
    snprintf(path, sizeof path, "%s/%06ld.png", rec_dir, rec_frames++);
    screenshot(path);

    if (!rec_audio) return;

    blip_t* left = core->getAudioChannel(core, 0);
    blip_t* right = core->getAudioChannel(core, 1);
    if (!left || !right) return;

    int avail = blip_samples_avail(left);
    int other = blip_samples_avail(right);
    if (other < avail) avail = other;

    while (avail > 0) {
        static short buf[2048 * 2];
        int n = avail > 2048 ? 2048 : avail;
        blip_read_samples(left, buf, n, 1);
        blip_read_samples(right, buf + 1, n, 1);
        fwrite(buf, sizeof(short), (size_t)n * 2, rec_audio);
        avail -= n;
    }
}

static void run_frames(long n) {
    for (long i = 0; i < n; ++i) {
        core->setKeys(core, held);
        core->runFrame(core);
        ++total_frames;
        if (crashed) die("core crashed");
        if (recording) record_capture();
    }
}

static uint32_t key_bit(const char* k) {
    static const char* names[] = {"A","B","SELECT","START","RIGHT","LEFT","UP","DOWN","R","L"};
    for (int i = 0; i < 10; ++i) if (!strcasecmp(k, names[i])) return 1u << i;
    if (!strcasecmp(k, "all")) return 0x3FF;
    fprintf(stderr, "unknown key %s\n", k); exit(2);
}
static uint32_t key_mask(char* list) {
    uint32_t m = 0; char* save; for (char* t = strtok_r(list, ",", &save); t; t = strtok_r(NULL, ",", &save)) m |= key_bit(t); return m;
}

static uint32_t parse_addr(const char* s) {
    if (s[0] == '$') {
        const char* plus = strchr(s, '+'); char name[96]; size_t n = plus ? (size_t)(plus - s - 1) : strlen(s) - 1;
        if (n >= sizeof name) { n = sizeof name - 1; }
        memcpy(name, s + 1, n); name[n] = 0;
        for (int i = 0; i < nsyms; ++i) if (!strcmp(syms[i].name, name)) return syms[i].addr + (plus ? (uint32_t)strtoul(plus + 1, NULL, 0) : 0);
        fprintf(stderr, "unknown symbol %s\n", name); exit(2);
    }
    return (uint32_t)strtoul(s, NULL, 0);
}
static uint32_t rd(int w, uint32_t a) { return w == 8 ? core->busRead8(core, a) : w == 16 ? core->busRead16(core, a) : core->busRead32(core, a); }
static void wr(int w, uint32_t a, uint32_t v) { if (w == 8) core->busWrite8(core, a, v); else if (w == 16) core->busWrite16(core, a, v); else core->busWrite32(core, a, v); }

static void load_symbols(const char* path) {
    FILE* f = fopen(path, "r"); if (!f) die("cannot open symbols file");
    char line[512]; int cap = 1024; syms = malloc(cap * sizeof *syms); nsyms = 0;
    while (fgets(line, sizeof line, f)) {
        char a[32], t[8], n[256];
        if (sscanf(line, "%31s %7s %255s", a, t, n) != 3) continue;
        if (nsyms == cap) { cap *= 2; syms = realloc(syms, cap * sizeof *syms); }
        strncpy(syms[nsyms].name, n, sizeof syms[nsyms].name - 1); syms[nsyms].name[sizeof syms[nsyms].name - 1] = 0;
        syms[nsyms].addr = (uint32_t)strtoul(a, NULL, 16); ++nsyms;
    }
    fclose(f);
}

static void screenshot(const char* path) {
    struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC); if (!vf) die("cannot open screenshot path");
    png_structp png = PNGWriteOpen(vf); png_infop info = PNGWriteHeader(png, vw, vh);
    PNGWritePixels(png, vw, vh, vw, vbuf); PNGWriteClose(png, info); vf->close(vf);
}

static uint32_t rng_state = 1;
static uint32_t rng(void) { rng_state = rng_state * 1664525u + 1013904223u; return rng_state >> 8; }

int main(int argc, char** argv) {
    int ai = 1; if (ai < argc && !strcmp(argv[ai], "-v")) { verbose = 1; ++ai; }
    if (ai >= argc) { fprintf(stderr, "usage: mokurun [-v] ROM.gba [SCRIPT]\n"); return 2; }
    const char* rom = argv[ai++]; FILE* in = stdin;
    if (ai < argc && strcmp(argv[ai], "-")) { in = fopen(argv[ai], "r"); if (!in) { perror("script"); return 2; } }
    mLogSetDefaultLogger(&logger);
    core = mCoreFind(rom); if (!core) { fprintf(stderr, "not a GBA rom: %s\n", rom); return 2; }
    core->init(core);
    core->desiredVideoDimensions(core, &vw, &vh);
    vbuf = malloc(vw * vh * sizeof(color_t)); core->setVideoBuffer(core, vbuf, vw);
    mCoreConfigInit(&core->config, "mokurun");
    mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");
    // Without these the core mixes at volume zero and a recording comes out silent.
    mCoreConfigSetDefaultValue(&core->config, "volume", "256");
    mCoreConfigSetDefaultValue(&core->config, "mute", "0");
    mCoreLoadConfig(core);
    if (!mCoreLoadFile(core, rom)) { fprintf(stderr, "cannot load %s\n", rom); return 2; }
    static struct mCoreCallbacks cb; cb.coreCrashed = on_crash; core->addCoreCallbacks(core, &cb);
    core->reset(core);
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    char line[1024]; int lineno = 0;
    while (fgets(line, sizeof line, in)) {
        ++lineno; char* p = line; while (isspace((unsigned char)*p)) ++p;
        if (!*p || *p == '#') { continue; }
        char* nl = strchr(p, '\n'); if (nl) { *nl = 0; }
        char cmd[32], a1[256], a2[256], a3[256];
        int n = sscanf(p, "%31s %255s %255s %255s", cmd, a1, a2, a3);
        if (n < 1) continue;
        if (!strcmp(cmd, "symbols")) load_symbols(a1);
        else if (!strcmp(cmd, "frames")) run_frames(atol(a1));
        else if (!strcmp(cmd, "hold")) held |= key_mask(a1);
        else if (!strcmp(cmd, "release")) held &= ~key_mask(a1);
        else if (!strcmp(cmd, "press")) { uint32_t m = key_mask(a1); long h = n > 2 ? atol(a2) : 2, g = n > 3 ? atol(a3) : 2; held |= m; run_frames(h); held &= ~m; run_frames(g); }
        else if (!strcmp(cmd, "screenshot")) screenshot(a1);
        else if (!strncmp(cmd, "read", 4)) { int w = atoi(cmd + 4); uint32_t a = parse_addr(a1); printf("READ %s = %u (0x%x)\n", a1, rd(w, a), rd(w, a)); fflush(stdout); }
        else if (!strncmp(cmd, "write", 5)) { int w = atoi(cmd + 5); wr(w, parse_addr(a1), (uint32_t)strtoul(a2, NULL, 0)); }
        else if (!strncmp(cmd, "assert", 6)) { int w = atoi(cmd + 6); uint32_t a = parse_addr(a1), v = rd(w, a), e = (uint32_t)strtoul(a2, NULL, 0); if (v != e) { fprintf(stderr, "ASSERT %s: got %u expected %u\n", a1, v, e); die("assert failed"); } }
        else if (!strncmp(cmd, "waitchange", 10)) { int w = atoi(cmd + 10); uint32_t a = parse_addr(a1), v0 = rd(w, a); long max = atol(a2), i; for (i = 0; i < max && rd(w, a) == v0; ++i) run_frames(1); if (i >= max) { fprintf(stderr, "WAITCHANGE %s stuck at %u\n", a1, v0); die("timeout"); } }
        else if (!strncmp(cmd, "waitne", 6)) { int w = atoi(cmd + 6); uint32_t a = parse_addr(a1), e = (uint32_t)strtoul(a2, NULL, 0); long max = atol(a3), i; for (i = 0; i < max && rd(w, a) == e; ++i) run_frames(1); if (i >= max) { fprintf(stderr, "WAITNE %s still %u\n", a1, e); die("timeout"); } }
        else if (!strncmp(cmd, "wait", 4)) { int w = atoi(cmd + 4); uint32_t a = parse_addr(a1), e = (uint32_t)strtoul(a2, NULL, 0); long max = atol(a3), i; for (i = 0; i < max && rd(w, a) != e; ++i) run_frames(1); if (i >= max) { fprintf(stderr, "WAIT %s: got %u expected %u\n", a1, rd(w, a), e); die("timeout"); } }
        else if (!strcmp(cmd, "sram_save")) { void* sram = NULL; size_t sz = core->savedataClone(core, &sram); FILE* f = fopen(a1, "wb"); if (!f || !sram) die("sram_save"); fwrite(sram, 1, sz, f); fclose(f); free(sram); printf("SRAM saved %zu bytes\n", sz); }
        else if (!strcmp(cmd, "sram_load")) { FILE* f = fopen(a1, "rb"); if (!f) die("sram_load open"); fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); void* buf = malloc(sz); if (fread(buf, 1, sz, f) != (size_t)sz) die("sram_load read"); fclose(f); if (!core->savedataRestore(core, buf, sz, true)) die("savedataRestore"); free(buf); }
        else if (!strcmp(cmd, "reset")) { core->reset(core); }
        else if (!strcmp(cmd, "random")) { long nf = atol(a1); rng_state = (uint32_t)strtoul(a2, NULL, 0); uint32_t mask = n > 3 ? key_mask(a3) : 0x3FF; long left = 0; for (long i = 0; i < nf; ++i) { if (left-- <= 0) { held = rng() & mask & 0x3FF; if ((rng() & 3) == 0) held = 0; left = 1 + (rng() % 20); } run_frames(1); } held = 0; }
        else if (!strcmp(cmd, "record_start")) {
            snprintf(rec_dir, sizeof rec_dir, "%s", a1);
            mkdir(rec_dir, 0755);
            if (n > 2) rec_rate = atoi(a2);
            char apath[600]; snprintf(apath, sizeof apath, "%s/audio.raw", rec_dir);
            rec_audio = fopen(apath, "wb"); if (!rec_audio) die("cannot open audio.raw");
            core->setAudioBufferSize(core, 4096);
            blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), rec_rate);
            blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), rec_rate);
            blip_clear(core->getAudioChannel(core, 0));
            blip_clear(core->getAudioChannel(core, 1));
            rec_frames = 0; recording = 1;
            printf("RECORDING to %s at %d Hz\n", rec_dir, rec_rate); fflush(stdout);
        }
        else if (!strcmp(cmd, "record_stop")) {
            recording = 0;
            if (rec_audio) { fclose(rec_audio); rec_audio = NULL; }
            printf("RECORDED %ld frames, audio %d Hz stereo s16le\n", rec_frames, rec_rate); fflush(stdout);
        }
        else if (!strcmp(cmd, "echo")) {
            // While the camera is running an echo is also a timeline marker: the encoder reads
            // these to know when each caption belongs.
            if (recording) printf("MARK %ld %s\n", rec_frames, p + 5);
            else printf("%s\n", p + 5);
            fflush(stdout);
        }
        else if (!strcmp(cmd, "stats")) { struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1); double s = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9; printf("STATS frames=%llu wall=%.2fs (%.0f fps)\n", (unsigned long long)total_frames, s, s > 0 ? total_frames / s : 0); fflush(stdout); }
        else if (!strcmp(cmd, "exit")) return atoi(a1);
        else { fprintf(stderr, "line %d: unknown command %s\n", lineno, cmd); return 2; }
    }
    return 0;
}
