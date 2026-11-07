#define _GNU_SOURCE
/* Gods SDL port, development: the options, the lockstep with the original and the other tests.  The regular program is main.c; both run the machine of host.c, this one with the hooks below.
 *
 *   gods-sdl-dev [--disks FOLDER] [--scale N] [--start intro|menu|level] [--skip-intro] [--level1-image FILE]
 *                [--headless --frames N [--dump PREFIX] [--dump-every N] [--keys FRAME:RAW:1|0,...]]
 *                [--lockstep [--lockstep-level N] [--lockstep-iteration N] [--lockstep-extra N]] [--quickload FILE] [--quicksave FILE] [--window-pos X,Y] [--diff-pos X,Y]
 *
 * F5 is the quicksave, F9 the quickload (the memory of the program, to FILE: shims.c, dev_game.c); the environment variables GODS_* of the tests are read here and in dev_game.c. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest.h"
#include "host.h"

void dev_game_init(void);   /* dev_game.c: the environment variables of the game's loop */
extern volatile int host_quick_request;   /* dev_game.c: F5 1, F9 2 */
extern const char *host_quicksave_file;   /* dev_game.c: --quicksave */
extern const char *host_quickload_file;   /* shims.c: --quickload */
extern int host_lockstep_level;   /* shims.c: --lockstep-level */
extern const char *host_level_image;  /* shims.c: --level1-image */
extern volatile int gods_waiting;    /* shims.c: the program waits for the original (GODS_FOLLOW) */

extern unsigned gods_iteration;

static void dump_ppm(const char *prefix, int frame, const uint32_t *pixels)
{
    char name[512];
    FILE *f;

    snprintf(name, sizeof name, "%s%05d.ppm", prefix, frame);
    f = fopen(name, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint8_t rgb[3] = { (uint8_t)(pixels[i] >> 16), (uint8_t)(pixels[i] >> 8), (uint8_t)pixels[i] };

        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* the frames of the port by iteration (the original's screenshot of its iteration n is put against the port's frame of its iteration n + 127), and the third window: the pixels that
 * differ in red over the port's frame dimmed.  The original (host/menu_takeover.lua live=) writes its screenshot and then the number of its iteration to /dev/shm. */
#define RING 32
static uint32_t frame_ring[RING][SCREEN_W * SCREEN_H];

static void diff_update(const uint32_t *pixels, SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture)
{
    static unsigned shown = 0;
    static uint32_t out[SCREEN_W * SCREEN_H];
    static uint8_t *file;
    unsigned n = 0;
    FILE *f;

    memcpy(frame_ring[gods_iteration % RING], pixels, sizeof out);
    if (!texture)
        return;
    f = fopen("/dev/shm/gods-live-orig.ppm.n", "r");
    if (!f)
        return;
    if (fscanf(f, "%u", &n) != 1)
        n = 0;
    fclose(f);
    if (n == 0 || n == shown || gods_iteration < n + 127 || gods_iteration > n + 127 + RING - 2)
        return;
    f = fopen("/dev/shm/gods-live-orig.ppm", "rb");
    if (!f)
        return;
    int w, h, max;
    if (fscanf(f, "P6 %d %d %d", &w, &h, &max) != 3 || w != 720 || h < 250) {
        fclose(f);
        return;
    }
    fgetc(f);
    if (!file)
        file = malloc(720 * 3 * 300);
    size_t got = fread(file, 3, (size_t)w * (size_t)h, f);
    fclose(f);
    if (got < (size_t)w * (size_t)h)
        return;
    shown = n;
    const uint32_t *mine = frame_ring[(n + 127) % RING];
    unsigned different = 0;

    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int orow = y + 18, ocol = 2 * (x + 20);
            uint32_t original = 0xff000000u;

            if (orow < h && ocol < w) {
                const uint8_t *p = file + 3 * ((size_t)orow * (size_t)w + (size_t)ocol);

                original = 0xff000000u | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
            }
            uint32_t value = mine[y * SCREEN_W + x];
            int inside = (y >= 16 && y < 208) || (y >= 209 && y < 216);

            if (inside && original != value) {
                out[y * SCREEN_W + x] = 0xffff0000u;
                different++;
            } else {
                out[y * SCREEN_W + x] = 0xff000000u | ((value >> 2) & 0x3f3f3fu);
            }
        }
    }
    char title[80];

    snprintf(title, sizeof title, "difference: %u pixels (iteration %u)", different, n);
    SDL_SetWindowTitle(window, title);
    SDL_UpdateTexture(texture, 0, out, SCREEN_W * 4);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, 0, 0);
    SDL_RenderPresent(renderer);
}

/* ---- the lockstep with the original (tools/ami9000_host/lua/amiga/lockstep.lua, projects/gods/host/gods_lockstep.lua) ----------------
 * --lockstep: no window; the port boots to level 1 by itself (the fire key at frame 300 of the menu), runs the first 128 iterations of the level (the original's saved state is
 * that far in) and --lockstep-extra N more frames (to be in the same phase of the frame as the emulator), and then answers the protocol on stdin/stdout, one line a command: `key CODE DOWN`
 * (CODE is the Amiga raw code), `mouse DX DY` and `button B DOWN` (not used), `step` (one video frame: the vertical blank, and the program's thread given a moment) and `image`; the answer to
 * `step` and `image` is `frame TICKS 320 256 NAME -` and 320*256*3 bytes of RGB. */
static int lockstep_ready;
static unsigned lockstep_ticks;

static void lockstep_reply(const uint32_t *pixels, int phase)
{
    static uint8_t rgb[SCREEN_W * SCREEN_H * 3];

    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        rgb[3 * i] = (uint8_t)(pixels[i] >> 16);
        rgb[3 * i + 1] = (uint8_t)(pixels[i] >> 8);
        rgb[3 * i + 2] = (uint8_t)pixels[i];
    }
    printf("frame %u %d %d %s -\n", lockstep_ticks, SCREEN_W, SCREEN_H, phase == PHASE_INTRO ? "intro" : "level");
    fwrite(rgb, 1, sizeof rgb, stdout);
    fflush(stdout);
}

/* the draws of the program (glyphs and sprites) of the last iterations, for the lockstep to compare with the original's (`draws` answers them as text, newest last) */
extern void (*host_draw_hook)(int kind, int16_t x, int16_t y, uint16_t index);   /* shims.c */
#define DRAW_RING 4096
static struct { unsigned iteration; int16_t kind, x, y; uint16_t index; } draw_ring[DRAW_RING];
static unsigned draw_count;

static void draw_log(int kind, int16_t x, int16_t y, uint16_t index)
{
    unsigned at = draw_count++ % DRAW_RING;

    draw_ring[at].iteration = gods_iteration;
    draw_ring[at].kind = (int16_t)kind;
    draw_ring[at].x = x;
    draw_ring[at].y = y;
    draw_ring[at].index = index;
}

/* the commands up to the next `step` (returns 1), answering `image` on the way; 0 when the other side is gone or says quit */
static int lockstep_commands(const uint32_t *pixels, int phase)
{
    char line[200];

    while (fgets(line, sizeof line, stdin)) {
        int code, down, a, b;

        if (sscanf(line, "key %d %d", &code, &down) == 2)
            irq_key(code, down);
        else if (sscanf(line, "mouse %d %d", &a, &b) == 2 || sscanf(line, "button %d %d", &a, &b) == 2)
            continue;
        else if (sscanf(line, "mem %x %x", (unsigned *)&a, (unsigned *)&b) == 2) {   /* `mem ADDR LEN`: `mem LEN` and LEN bytes of the program's memory (a debugging aid: tools/ami9000_host/ctl.py) */
            printf("mem %d\n", b);
            fwrite(guest_ptr((uint32_t)a), 1, (size_t)b, stdout);
            fflush(stdout);
        } else if (!strncmp(line, "draws", 5)) {   /* `draws N`: the draws of the last N iterations, a line each: `iteration kind x y index`, then a line `end` */
            unsigned last = gods_iteration, first = (unsigned)atoi(line + 5);
            unsigned from = draw_count > DRAW_RING ? draw_count - DRAW_RING : 0;

            first = first == 0 ? 1 : first;
            for (unsigned i = from; i < draw_count; i++) {
                if (draw_ring[i % DRAW_RING].iteration + first > last)
                    printf("%u %d %d %d %u\n", draw_ring[i % DRAW_RING].iteration, draw_ring[i % DRAW_RING].kind, draw_ring[i % DRAW_RING].x,
                           draw_ring[i % DRAW_RING].y, draw_ring[i % DRAW_RING].index);
            }
            printf("end\n");
            fflush(stdout);
        } else if (!strncmp(line, "quicksave ", 10)) {   /* `quicksave PATH`: the program's memory is written to PATH at the top of its next iteration (F5 of the window, dev_game.c; the lockstep driver's F5) */
            static char path[512];
            size_t length = strcspn(line + 10, "\r\n");

            if (length > 0 && length < sizeof path) {
                memcpy(path, line + 10, length);
                path[length] = 0;
                host_quicksave_file = path;
                host_quick_request = 1;
            }
        } else if (sscanf(line, "weapon %d", &a) == 1) {   /* `weapon ITEM`: the weapon ITEM is picked up at the top of the next iteration (dev_game.c) */
            extern volatile unsigned host_weapon_count;
            extern volatile int16_t host_weapon_items[32];

            host_weapon_items[host_weapon_count & 31] = (int16_t)a;
            host_weapon_count++;
        } else if (sscanf(line, "poke %x %x %d", (unsigned *)&a, (unsigned *)&b, &code) == 3) {   /* `poke ADDR VALUE SIZE`: SIZE (1, 2 or 4) bytes of the program's memory are set, VALUE in the host's own order, as the words and longs of the program are there (the lockstep driver's port_poke) */
            if (code == 1)
                *(volatile uint8_t *)guest_ptr((uint32_t)a) = (uint8_t)b;
            else if (code == 2)
                *(volatile uint16_t *)guest_ptr((uint32_t)a) = (uint16_t)b;
            else if (code == 4)
                *(volatile uint32_t *)guest_ptr((uint32_t)a) = (uint32_t)b;
        } else if (sscanf(line, "poke16 %x %x", (unsigned *)&a, (unsigned *)&b) == 2) {   /* `poke16 ADDR VALUE`: a word of the program's memory (the lockstep gives the per-frame counters the phase the original's loading gave them) */
            *(volatile uint16_t *)guest_ptr((uint32_t)a) = (uint16_t)b;
        } else if (sscanf(line, "tick %d", &a) == 1) {   /* `tick DELTA`: tick_count goes up by DELTA (the original's count includes the frames of its disk loading, which the port does not wait for; the lockstep driver gives the port the original's count) */
            *(volatile uint32_t *)guest_ptr(0xce2) += (uint32_t)a;   /* tick_count */
        } else if (sscanf(line, "flipsync %d", &a) == 1) {   /* `flipsync OFFSET`: the port's flips wait for the original's (host_flip_wait) */
            extern volatile int host_flip_sync, host_flips_offset;

            extern void host_fade_sync_start(void);

            host_flips_offset = a;
            host_flip_sync = 1;
            host_fade_sync_start();
            {
                extern void host_flip_late(int on);

                host_flip_late(1);   /* the flips are let go by the original's count of the same step and shown at the next blank, as the original's are */
            }
        } else if (sscanf(line, "fadeline %d", &a) == 1) {   /* `fadeline N`: the original began the copper fade of its step at the raster line N of this frame (-1: it did not) */
            extern volatile int host_fade_line;

            host_fade_line = a;
        } else if (sscanf(line, "flipline %d %d", &a, &b) == 2) {   /* `flipline N L`: the original's Nth flip was made at the raster line L of its frame */
            extern void host_flip_line_set(int n, int line);

            host_flip_line_set(a, b);
        } else if (sscanf(line, "fadefirst %d", &a) == 1) {   /* `fadefirst N`: the raster line the first step of the original's latest fade began at */
            extern volatile int host_fade_first;

            host_fade_first = a;
        } else if (sscanf(line, "reads %d", &a) == 1) {   /* `reads N`: the original has read the keys N times since the sync began (host_read_wait) */
            extern volatile int host_reads_orig;

            host_reads_orig = a;
        } else if (sscanf(line, "diskdone %d", &a) == 1) {   /* `diskdone N`: the original has finished N reads of the background (host_disk_wait) */
            extern volatile int host_disk_sync, host_disk_done;

            host_disk_done = a;
            host_disk_sync = 1;
        } else if (sscanf(line, "worldloads %d", &a) == 1) {   /* `worldloads N`: the original has come to the end of the loading of a world N times since the sync began (host_world_load_wait) */
            extern volatile unsigned host_world_loads_orig;

            host_world_loads_orig = (unsigned)a;
        } else if (sscanf(line, "fades %d", &a) == 1) {   /* `fades N`: the original has called its fade-in N times since the sync began (host_fade_wait) */
            extern volatile unsigned host_fades_orig;

            host_fades_orig = (unsigned)a;
        } else if (sscanf(line, "flips %d", &a) == 1) {   /* `flips N`: the original has flipped N times since the sync began */
            extern volatile unsigned host_flips_orig;

            host_flips_orig = (unsigned)a;
        } else if (!strncmp(line, "image", 5))
            lockstep_reply(pixels, phase);
        else if (!strncmp(line, "step", 4))
            return 1;
        else if (!strncmp(line, "quit", 4))
            return 0;
    }
    return 0;
}


static const char *dump_prefix, *keys;
static int dump_every = 50, follow, lockstep, lockstep_extra;
static unsigned lockstep_menu;   /* --lockstep-menu N: the lockstep begins in the menu, at the Nth frame (no Begin Quest: the keys are the player's) */
static unsigned lockstep_iteration = 129;   /* --lockstep-level N: Begin Quest starts level N (host_start_level); --lockstep-iteration N: the lockstep begins at that iteration of the game loop (the original's saved state is N - 1 iterations into the level) */
static int diff_x = -1, diff_y = -1;
static SDL_Window *diff_window;
static SDL_Renderer *diff_renderer;
static SDL_Texture *diff_texture;

static int dev_key(SDL_Scancode scancode, int down)
{
    if (down && (scancode == SDL_SCANCODE_F5 || scancode == SDL_SCANCODE_F9)) {   /* the quicksave and the quickload (dev_game.c) */
        host_quick_request = scancode == SDL_SCANCODE_F5 ? 1 : 2;
        return 1;
    }
    return 0;
}

static void dev_windows(int scale)
{
    if (!follow)
        return;
    /* the third window: where the port's frame differs from the original's */
    diff_window = SDL_CreateWindow("difference", SCREEN_W * scale, SCREEN_H * scale, 0);
    diff_renderer = diff_window ? SDL_CreateRenderer(diff_window, 0) : 0;
    diff_texture = diff_renderer ? SDL_CreateTexture(diff_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H) : 0;
    if (diff_texture) {
        SDL_SetTextureScaleMode(diff_texture, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(diff_renderer, SCREEN_W, SCREEN_H * 5 / 6, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        if (diff_x >= 0)
            SDL_SetWindowPosition(diff_window, diff_x, diff_y);
    }
}

static int dev_frame_start(int frame, uint32_t *pixels, int phase)
{
    if (lockstep && lockstep_ready && !lockstep_commands(pixels, phase))
        return 0;
    for (const char *k = keys; k && *k && !lockstep_ready; ) {   /* the scripted keys of this frame */
        int at, code, down, used = 0;

        if (sscanf(k, "%d:%x:%d%n", &at, &code, &down, &used) >= 3 && at == frame)
            host_key(phase, code, down);
        k += used ? used : strlen(k);
        if (*k == ',')
            k++;
    }
    return 1;
}

static int dev_hold(int phase)
{
    return gods_waiting && phase == PHASE_LEVEL;   /* (GODS_FOLLOW) the machine's time stands still while the port waits for the original */
}

static void dev_after_blank(void)
{
    if (lockstep) {
        extern void host_blank_wait(void);

        SDL_DelayNS(3000000);   /* the program's thread flips the screens right after the blank it waited for: give it that moment before the frame is drawn */
        host_blank_wait();   /* and, when it was waiting for the blank, until it waits again (a loaded machine does not make it late) */
    }
}

static void dev_after_frame(int frame, int phase, uint32_t *pixels, const int16_t *samples)
{
    (void)samples;
    if (lockstep) {
        lockstep_ticks++;
        {   /* GODS_TRACE=FIRST-LAST: the draws near the top of the screen of those ticks go to stderr (blitter.c) */
            extern volatile int gods_trace;
            unsigned first = 0, last = 0;

            if (getenv("GODS_TRACE") && sscanf(getenv("GODS_TRACE"), "%u-%u", &first, &last) == 2)
                gods_trace = lockstep_ticks + 1 >= first && lockstep_ticks <= last;
            if (gods_trace) {
                static unsigned last_iteration;

                if (gods_iteration != last_iteration)
                    fprintf(stderr, "trace iteration %u starts by tick %u\n", gods_iteration, lockstep_ticks);
                last_iteration = gods_iteration;
            }
        }
        if (!lockstep_ready && lockstep_ticks % 200 == 0)
            fprintf(stderr, "gods-sdl: lockstep boot: step %u, phase %d, iteration %u\n", lockstep_ticks, phase, gods_iteration);
        if (lockstep_ready)
            lockstep_reply(pixels, phase);
        else if (lockstep_menu ? (lockstep_ticks >= lockstep_menu && lockstep_extra-- <= 0) : (gods_iteration >= lockstep_iteration && lockstep_extra-- <= 0))   /* (the level runs inside the menu's phase: Begin Quest calls the game loop) */
            lockstep_ready = 1;
    }
    if (follow && phase == PHASE_LEVEL)
        diff_update(pixels, diff_window, diff_renderer, diff_texture);
    {   /* GODS_ITERSHOT=N,N,...: the frame shown when the Nth iteration of the game loop starts is written to GODS_ITERSHOT_PREFIX<N>.ppm (a test) */
        static const char *list, *prefix;
        static int init;

        if (!init) {
            init = 1;
            list = getenv("GODS_ITERSHOT");
            prefix = getenv("GODS_ITERSHOT_PREFIX");
        }
        for (const char *l = list; l && prefix && *l; ) {
            char *end;
            unsigned n = (unsigned)strtoul(l, &end, 10);

            if (end == l)
                break;
            if (gods_iteration == n + 1)
                dump_ppm(prefix, (int)n, pixels);
            l = *end ? end + 1 : end;
        }
    }
    if (dump_prefix && dump_every && frame % dump_every == 0)
        dump_ppm(dump_prefix, frame, pixels);
}

static int dev_paced(void)
{
    return !lockstep && !getenv("GODS_FAST");   /* (GODS_FAST: as fast as it goes: the lockstep with the original) */
}

static void dev_finish(uint32_t *pixels)
{
    if (dump_prefix)
        dump_ppm(dump_prefix, 99999, pixels);
}

int main(int argc, char **argv)
{
    struct HostOptions options = { .disks = "projects/gods/local", .scale = 3, .phase = PHASE_INTRO, .window_x = -1, .window_y = -1, .wav = getenv("GODS_WAV") };
    struct HostHooks hooks = { .key = dev_key, .windows = dev_windows, .frame_start = dev_frame_start, .hold = dev_hold, .after_blank = dev_after_blank, .after_frame = dev_after_frame,
                               .paced = dev_paced, .finish = dev_finish };

    host_dev = 1;
    follow = getenv("GODS_FOLLOW") != 0;   /* the original is played live and this port follows it: it takes no keys of its own */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disks") && i + 1 < argc)
            options.disks = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc)
            options.scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lockstep")) {
            extern int keys_space_is_fire;

            keys_space_is_fire = 1;
            lockstep = options.headless = 1;
            host_draw_hook = draw_log;
        }
        else if (!strcmp(argv[i], "--quicksave") && i + 1 < argc)
            host_quicksave_file = argv[++i];
        else if (!strcmp(argv[i], "--quickload") && i + 1 < argc)
            host_quickload_file = argv[++i];
        else if (!strcmp(argv[i], "--lockstep-level") && i + 1 < argc)
            host_lockstep_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lockstep-menu") && i + 1 < argc)
            lockstep_menu = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lockstep-iteration") && i + 1 < argc)
            lockstep_iteration = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lockstep-extra") && i + 1 < argc)
            lockstep_extra = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--window-pos") && i + 1 < argc)
            sscanf(argv[++i], "%d,%d", &options.window_x, &options.window_y);
        else if (!strcmp(argv[i], "--diff-pos") && i + 1 < argc)
            sscanf(argv[++i], "%d,%d", &diff_x, &diff_y);
        else if (!strcmp(argv[i], "--level1-image") && i + 1 < argc)
            host_level_image = argv[++i];
        else if (!strcmp(argv[i], "--skip-intro"))
            options.phase = PHASE_MENU;
        else if (!strcmp(argv[i], "--start") && i + 1 < argc) {
            const char *what = argv[++i];

            options.phase = !strcmp(what, "menu") ? PHASE_MENU : !strcmp(what, "level") ? PHASE_LEVEL : PHASE_INTRO;
        } else if (!strcmp(argv[i], "--headless"))
            options.headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            options.frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc)
            dump_prefix = argv[++i];
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc)
            keys = argv[++i];
        else if (!strcmp(argv[i], "--dump-every") && i + 1 < argc)
            dump_every = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: gods-sdl-dev [--disks FOLDER] [--scale N] [--start intro|menu|level] [--skip-intro] [--level1-image FILE]\n"
                            "                    [--headless --frames N [--dump PREFIX] [--dump-every N] [--keys FRAME:RAW:1|0,...]]\n"
                            "                    [--lockstep [--lockstep-level N] [--lockstep-iteration N] [--lockstep-extra N]] [--quickload FILE] [--quicksave FILE]\n");
            return 2;
        }
    }
    if (options.phase == PHASE_LEVEL && !host_level_image) {
        fprintf(stderr, "gods-sdl-dev: --start level needs --level1-image FILE\n");
        return 2;
    }
    hooks.no_vsync = hooks.no_keyboard = follow;
    options.hooks = &hooks;
    dev_game_init();
    return host_run(&options);
}
