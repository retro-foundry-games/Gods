#define _GNU_SOURCE
/* Gods SDL port: the program.  One process runs the game as the real one does, intro, menu, level, one program after the other:
 * the program that is on runs on a thread of its own; this thread is the machine around it: it raises the vertical blank interrupt 50
 * times a second, makes the picture and the sound, and takes the keys.  When a program asks for the next one (the intro, on a fire button)
 * its thread ends, the memory is reset as the loader leaves it, and the next program is started on a new thread; the window and the audio
 * stream stay.
 *
 *   gods-sdl [--disks FOLDER] [--scale N] [--start intro|menu|level] [--skip-intro] [--level1-image FILE]
 *            [--headless --frames N [--dump PREFIX] [--dump-every N] [--keys FRAME:RAW:1|0,...]] */
#include <SDL3/SDL.h>
#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include "guest.h"
#include "intro_host.h"

extern void menu_boot(void);    /* boot.c: the start-up (orig $039E50) */
extern void menu_start(void);   /* ui.c: the menu (orig $006388) */
extern uint32_t heap_base;
extern void sr_set(uint16_t value);
extern uint8_t *mask_buffer;    /* shares its address with restart_address, which menu_boot sets to its own address: the original blits into the (finished) boot code */
extern void host_begin_quest(void);   /* shims.c: the menu's Begin Quest */
extern const char *host_level_image;  /* shims.c: --level1-image */
void intro_render(uint32_t *pixels);   /* intro_view.c */

enum { PHASE_INTRO, PHASE_MENU, PHASE_LEVEL };

static volatile int next_phase = -1;   /* set by a program that asks for the next one */
static pthread_t program_thread;

static void *intro_program(void *unused)
{
    (void)unused;
    game_main();
    return 0;
}

static void *menu_program(void *unused)
{
    (void)unused;
    menu_boot();
    mask_buffer = (uint8_t *)0x39e50;   /* the area of the original's boot code: a buffer now */
    for (;;)
        menu_start();   /* a menu line that starts the game comes back here when the game is not there */
    return 0;
}

static void *level_program(void *unused)
{
    (void)unused;
    host_begin_quest();
    return 0;
}

/* orig $000DC4 (the intro): the loader brings the next part of the game: the menu program.  This thread ends here. */
void ih_start_menu(void)
{
    next_phase = PHASE_MENU;
    pthread_exit(0);
}

/* start a program: the memory as the loader leaves it, then the program on a thread of its own */
static void start_phase(int phase)
{
    void *(*entry)(void *) = intro_program;

    guest_reset();
    sr_set(0x2700);
    sound_reset();
    ih_set_fire(0);
    if (phase != PHASE_INTRO) {
        if (files_load_menu_image() != 0)
            exit(1);
        heap_base = 0x80400;   /* the memory the program hands out (the boot block leaves it in the vector slot at $2C) */
        entry = phase == PHASE_LEVEL ? level_program : menu_program;
    }
    pthread_create(&program_thread, 0, entry, 0);
}

static int raw_code(SDL_Scancode s)
{
    switch (s) {
    case SDL_SCANCODE_SPACE: return 0x40;
    case SDL_SCANCODE_RETURN: return 0x44;
    case SDL_SCANCODE_ESCAPE: return 0x45;
    case SDL_SCANCODE_BACKSPACE: return 0x41;
    case SDL_SCANCODE_TAB: return 0x42;
    case SDL_SCANCODE_DELETE: return 0x46;
    case SDL_SCANCODE_UP: return 0x4c;
    case SDL_SCANCODE_DOWN: return 0x4d;
    case SDL_SCANCODE_RIGHT: return 0x4e;
    case SDL_SCANCODE_LEFT: return 0x4f;
    case SDL_SCANCODE_LSHIFT: return 0x60;
    case SDL_SCANCODE_RSHIFT: return 0x61;
    case SDL_SCANCODE_PERIOD: return 0x39;
    case SDL_SCANCODE_COMMA: return 0x38;
    default: break;
    }
    static const char rows[3][11] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const int first[3] = { 0x10, 0x20, 0x31 };

    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) {
        char letter = (char)('A' + (s - SDL_SCANCODE_A));

        for (int r = 0; r < 3; r++)
            for (int i = 0; rows[r][i]; i++)
                if (rows[r][i] == letter)
                    return first[r] + i;
    }
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9)
        return 0x01 + (s - SDL_SCANCODE_1);
    if (s == SDL_SCANCODE_0)
        return 0x0a;
    return -1;
}

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

static void crash(int sig, siginfo_t *info, void *context)
{
    void *frames[24];
    ucontext_t *uc = context;

    fprintf(stderr, "gods-sdl: signal %d at address %p, eip %#lx\n", sig, info->si_addr, (unsigned long)uc->uc_mcontext.gregs[REG_EIP]);
    backtrace_symbols_fd(frames, backtrace(frames, 24), 2);
    _exit(3);
}

/* a key: the intro takes the fire buttons, the menu and the game the keyboard */
static void key(int phase, int code, int down)
{
    if (phase == PHASE_INTRO) {
        if (code == 0x40 || code == 0x44)
            ih_set_fire(down);
    } else {
        irq_key(code, down);
    }
}

int main(int argc, char **argv)
{
    struct sigaction sa = { .sa_sigaction = crash, .sa_flags = SA_SIGINFO };
    const char *disks = "projects/gods/local";
    const char *dump = 0, *wav = getenv("GODS_WAV");
    int headless = 0, frames = 0, scale = 3, dump_every = 50, phase = PHASE_INTRO;
    const char *keys = 0;   /* a scripted run: "frame:raw code (hex):1 pressed or 0 released", comma separated */
    static uint32_t pixels[SCREEN_W * SCREEN_H];
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;
    SDL_Texture *texture = 0;
    SDL_AudioStream *audio = 0;

    sigaction(SIGSEGV, &sa, 0);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disks") && i + 1 < argc)
            disks = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc)
            scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--level1-image") && i + 1 < argc)
            host_level_image = argv[++i];
        else if (!strcmp(argv[i], "--skip-intro"))
            phase = PHASE_MENU;
        else if (!strcmp(argv[i], "--start") && i + 1 < argc) {
            const char *what = argv[++i];

            phase = !strcmp(what, "menu") ? PHASE_MENU : !strcmp(what, "level") ? PHASE_LEVEL : PHASE_INTRO;
        } else if (!strcmp(argv[i], "--headless"))
            headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc)
            dump = argv[++i];
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc)
            keys = argv[++i];
        else if (!strcmp(argv[i], "--dump-every") && i + 1 < argc)
            dump_every = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: gods-sdl [--disks FOLDER] [--scale N] [--start intro|menu|level] [--skip-intro] [--level1-image FILE]\n"
                            "                [--headless --frames N [--dump PREFIX] [--dump-every N] [--keys FRAME:RAW:1|0,...]]\n");
            return 2;
        }
    }
    if (phase == PHASE_LEVEL && !host_level_image) {
        fprintf(stderr, "gods-sdl: --start level needs --level1-image FILE\n");
        return 2;
    }
    guest_init();
    if (files_open(disks) != 0)
        return 1;

    if (!headless) {
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
            fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return 1;
        }
        window = SDL_CreateWindow("Gods", SCREEN_W * scale, SCREEN_H * scale, 0);
        renderer = window ? SDL_CreateRenderer(window, 0) : 0;
        texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H) : 0;
        if (!texture) {
            fprintf(stderr, "SDL window: %s\n", SDL_GetError());
            return 1;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H * 5 / 6, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
        if (audio)
            SDL_ResumeAudioStreamDevice(audio);
    }
    start_phase(phase);

    int vsync = !headless && renderer && SDL_SetRenderVSync(renderer, 1);   /* presented at the display's refresh */
    uint64_t next = SDL_GetTicksNS();
    for (int frame = 0; !frames || frame < frames; frame++) {
        SDL_Event event;
        int16_t samples[882 * 2];

        if (next_phase >= 0) {   /* the program on asked for the next one */
            pthread_join(program_thread, 0);
            phase = next_phase;
            next_phase = -1;
            start_phase(phase);
        }
        while (!headless && SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                exit(0);
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && !event.key.repeat) {
                int code = raw_code(event.key.scancode);

                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F10)
                    exit(0);
                if (event.key.scancode == SDL_SCANCODE_LCTRL && phase == PHASE_INTRO)
                    code = 0x40;
                if (code >= 0)
                    key(phase, code, event.type == SDL_EVENT_KEY_DOWN);
            }
        }
        for (const char *k = keys; k && *k;) {   /* the scripted keys of this frame */
            int at, code, down, used = 0;

            if (sscanf(k, "%d:%x:%d%n", &at, &code, &down, &used) >= 3 && at == frame)
                key(phase, code, down);
            k += used ? used : strlen(k);
            if (*k == ',')
                k++;
        }
        /* In a window the screen is presented at the display's refresh (vsync) and the 50 Hz frames of the machine are counted from the clock: a refresh
         * has none, one or two of them to run.  Without a window (and without vsync) one frame is run for every pass, paced by the sleep below. */
        int due = 1;
        extern volatile int gods_waiting;

        if (gods_waiting && phase == PHASE_LEVEL) {   /* (GODS_FOLLOW) the machine's time stands still while the port waits for the original */
            SDL_DelayNS(500000);
            continue;
        }

        if (vsync) {
            uint64_t now = SDL_GetTicksNS();

            due = 0;
            if (now > next + 100000000ull)
                next = now;   /* far behind (the window was moved, the machine was busy): start again */
            while (next <= now && due < 3) {
                due++;
                next += 20000000ull;
            }
        }
        for (int tick = 0; tick < due; tick++) {
            if (phase == PHASE_INTRO) {
                ih_vbl();
                ih_music_mix(samples, 882, 44100);
            } else {
                display_latch();
                irq_vbl();
                sound_mix(samples);
            }
            if (vsync && audio && SDL_GetAudioStreamQueued(audio) < 44100 / 5 * 4)
                SDL_PutAudioStreamData(audio, samples, sizeof samples);
        }
        if (phase == PHASE_INTRO)
            intro_render(pixels);
        else
            display_render(pixels);
        if (wav) {   /* a test: the sound as raw signed 16 bit stereo 44100 Hz */
            FILE *f = fopen(wav, "ab");

            if (f) {
                fwrite(samples, 1, sizeof samples, f);
                fclose(f);
            }
        }
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
        if (dump && dump_every && frame % dump_every == 0)
            dump_ppm(dump, frame, pixels);
        if (!headless) {
            if (!vsync && audio && SDL_GetAudioStreamQueued(audio) < 44100 / 5 * 4)
                SDL_PutAudioStreamData(audio, samples, sizeof samples);
            SDL_UpdateTexture(texture, 0, pixels, SCREEN_W * 4);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, 0, 0);
            SDL_RenderPresent(renderer);
        }
        if (!vsync && !getenv("GODS_FAST")) {   /* (GODS_FAST: as fast as it goes: the lockstep with the original) */
            next += 20000000ull;   /* 50 frames a second (also without a window: the program's thread needs its time) */
            {
            uint64_t now = SDL_GetTicksNS();

            if (next > now)
                SDL_DelayNS(next - now);
            else
                next = now;
            }
        }
    }
    if (dump)
        dump_ppm(dump, 99999, pixels);
    return 0;
}
