#define _GNU_SOURCE
/* Gods SDL port: the machine around the program.  One process runs the game as the real one does, intro, menu, level, one program after the other:
 * the program that is on runs on a thread of its own; this thread is the machine around it: it raises the vertical blank interrupt 50
 * times a second, makes the picture and the sound, and takes the keys.  When a program asks for the next one (the intro, on a fire button)
 * its thread ends, the memory is reset as the loader leaves it, and the next program is started on a new thread; the window and the audio
 * stream stay.  main.c runs it as the game is played; devmain.c adds the options and the tests (the lockstep with the original) through the hooks of host.h. */
#include <SDL3/SDL.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <execinfo.h>
#include <ucontext.h>
#endif

#include "guest.h"
#include "host.h"
#include "intro_host.h"

extern void menu_boot(void);    /* boot.c: the start-up (orig $039E50) */
extern void menu_start(void);   /* menu.c: the menu (orig $006388) */
extern void sr_set(uint16_t value);
extern void host_begin_quest(void);   /* shims.c: the menu's Begin Quest */
void intro_render(uint32_t *pixels);   /* intro_view.c */

int host_dev;

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
    guest_set_mask_buffer();   /* the area of the original's boot code: a buffer now */
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
    if (phase == PHASE_INTRO && ih_load_assets() != 0)
        exit(1);
    if (phase != PHASE_INTRO) {
        if (files_load_menu_image() != 0)
            exit(1);
        guest_set_heap();   /* the memory the program hands out (the boot block leaves it in the vector slot at $2C) */
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

#ifndef __EMSCRIPTEN__
static void crash(int sig, siginfo_t *info, void *context)
{
    void *frames[24];
    ucontext_t *uc = context;

    fprintf(stderr, "gods-sdl: signal %d at address %p, eip %#lx\n", sig, info->si_addr, (unsigned long)uc->uc_mcontext.gregs[REG_EIP]);
    backtrace_symbols_fd(frames, backtrace(frames, 24), 2);
    _exit(3);
}
#endif

/* a key: the intro takes the fire buttons, the menu and the game the keyboard */
void host_key(int phase, int code, int down)
{
    if (phase == PHASE_INTRO) {
        if (code == 0x40 || code == 0x44)
            ih_set_fire(down);
    } else {
        irq_key(code, down);
    }
}

#ifdef __EMSCRIPTEN__
/* The browser build: the machine's loop is a function the browser calls at the display's refresh (requestAnimationFrame); the program runs on a thread of its own (a web worker),
 * as it does on the desktop.  Nothing of the tests: no hooks, no window without the canvas. */
extern void host_blank_begin(void);
extern void display_latch_fresh(void);
extern void display_latch_late(void);

static struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioStream *audio;
    int phase;
    uint64_t next;
} web;

static void web_frame(void)
{
    static uint32_t pixels[SCREEN_W * SCREEN_H];
    SDL_Event event;
    int16_t samples[882 * 2];

    static int settle;

    if (next_phase >= 0) {   /* the program on asked for the next one; the browser's main thread must not block on the old thread's exit */
        if (settle == 0)
            pthread_detach(program_thread);
        if (++settle > 3) {
            settle = 0;
            web.phase = next_phase;
            next_phase = -1;
            start_phase(web.phase);
        }
    }
    while (SDL_PollEvent(&event)) {
        if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && !event.key.repeat) {
            int code = raw_code(event.key.scancode);

            if (event.key.scancode == SDL_SCANCODE_LCTRL && web.phase == PHASE_INTRO)
                code = 0x40;
            if (code >= 0)
                host_key(web.phase, code, event.type == SDL_EVENT_KEY_DOWN);
        }
    }

    uint64_t now = SDL_GetTicksNS();
    int due = 0;

    if (now > web.next + 100000000ull)
        web.next = now;   /* far behind (the tab was hidden): start again */
    while (web.next <= now && due < 3) {
        due++;
        web.next += 20000000ull;
    }
    for (int tick = 0; tick < due; tick++) {
        extern volatile int host_irq_hold;

        if (web.phase == PHASE_INTRO) {
            ih_vbl();
            ih_music_mix(samples, 882, 44100);
        } else {
            display_latch();
            host_blank_begin();
            if (!host_irq_hold)
                irq_vbl();
            display_latch_fresh();
            display_latch_late();
            sound_mix(samples);
        }
        if (web.audio && SDL_GetAudioStreamQueued(web.audio) < 44100 / 5 * 4)
            SDL_PutAudioStreamData(web.audio, samples, sizeof samples);
    }
    if (web.phase == PHASE_INTRO)
        intro_render(pixels);
    else
        display_render(pixels);
    SDL_UpdateTexture(web.texture, 0, pixels, SCREEN_W * 4);
    SDL_RenderClear(web.renderer);
    SDL_RenderTexture(web.renderer, web.texture, 0, 0);
    SDL_RenderPresent(web.renderer);
}
#endif

int host_run(const struct HostOptions *options)
{
#ifndef __EMSCRIPTEN__
    struct sigaction sa = { .sa_sigaction = crash, .sa_flags = SA_SIGINFO };
#endif
    const struct HostHooks *hooks = options->hooks;
    const int headless = options->headless, scale = options->scale;
    int phase = options->phase;
    static uint32_t pixels[SCREEN_W * SCREEN_H];
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;
    SDL_Texture *texture = 0;
    SDL_AudioStream *audio = 0;

#ifndef __EMSCRIPTEN__
    sigaction(SIGSEGV, &sa, 0);
#endif
    guest_init();
    if (files_open(options->disks) != 0)
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
        if (options->window_x >= 0)
            SDL_SetWindowPosition(window, options->window_x, options->window_y);
        if (hooks && hooks->windows)
            hooks->windows(scale);
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
        if (audio)
            SDL_ResumeAudioStreamDevice(audio);
    }
    start_phase(phase);
#ifdef __EMSCRIPTEN__
    web.window = window;
    web.renderer = renderer;
    web.texture = texture;
    web.audio = audio;
    web.phase = phase;
    web.next = SDL_GetTicksNS();
    emscripten_set_main_loop(web_frame, 0, 1);   /* (does not return) */
    return 0;
#endif

    int vsync = !headless && renderer && !(hooks && hooks->no_vsync) && SDL_SetRenderVSync(renderer, 1);   /* presented at the display's refresh */
    uint64_t next = SDL_GetTicksNS();
    for (int frame = 0; !options->frames || frame < options->frames; frame++) {
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
                if (hooks && hooks->key && hooks->key(event.key.scancode, event.type == SDL_EVENT_KEY_DOWN))
                    continue;
                if (event.key.scancode == SDL_SCANCODE_LCTRL && phase == PHASE_INTRO)
                    code = 0x40;
                if (code >= 0 && !(hooks && hooks->no_keyboard))
                    host_key(phase, code, event.type == SDL_EVENT_KEY_DOWN);
            }
        }
        if (hooks && hooks->frame_start && !hooks->frame_start(frame, pixels, phase))
            return 0;
        /* In a window the screen is presented at the display's refresh (vsync) and the 50 Hz frames of the machine are counted from the clock: a refresh
         * has none, one or two of them to run.  Without a window (and without vsync) one frame is run for every pass, paced by the sleep below. */
        int due = 1;

        if (hooks && hooks->hold && hooks->hold(phase)) {   /* the machine's time stands still */
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
        {
            extern volatile int host_irq_hold;

            while (host_irq_hold)   /* the program's thread is loading memory (quickload): the frame waits, it would read half of it */
                SDL_DelayNS(200000);
        }
        for (int tick = 0; tick < due; tick++) {
            if (phase == PHASE_INTRO) {
                ih_vbl();
                ih_music_mix(samples, 882, 44100);
            } else {
                extern volatile int host_irq_hold;

                display_latch();
                {
                    extern void host_blank_begin(void);

                    host_blank_begin();
                }
                if (!host_irq_hold)   /* (the quickload or the quick load of the program's thread is writing the memory: the interrupt would run on half of it) */
                    irq_vbl();
                {
                    extern void display_latch_fresh(void);

                    display_latch_fresh();
                }
                if (hooks && hooks->after_blank)
                    hooks->after_blank();   /* (the lockstep: the program's thread flips the screens right after the blank it waited for: a moment for that before the frame is drawn) */
                {
                    extern void display_latch_late(void);

                    display_latch_late();
                }
                sound_mix(samples);
            }
            if (vsync && audio && SDL_GetAudioStreamQueued(audio) < 44100 / 5 * 4)
                SDL_PutAudioStreamData(audio, samples, sizeof samples);
        }
        {
            extern volatile int host_irq_hold;

            while (host_irq_hold)
                SDL_DelayNS(200000);
        }
        if (phase == PHASE_INTRO)
            intro_render(pixels);
        else
            display_render(pixels);
        if (hooks && hooks->after_frame)
            hooks->after_frame(frame, phase, pixels, samples);
        if (options->wav) {   /* a test: the sound as raw signed 16 bit stereo 44100 Hz */
            FILE *f = fopen(options->wav, "ab");

            if (f) {
                fwrite(samples, 1, sizeof samples, f);
                fclose(f);
            }
        }
        if (!headless) {
            if (!vsync && audio && SDL_GetAudioStreamQueued(audio) < 44100 / 5 * 4)
                SDL_PutAudioStreamData(audio, samples, sizeof samples);
            static uint64_t last_present;
            uint64_t now = SDL_GetTicksNS();

            if (!(hooks && hooks->no_vsync) || now - last_present > 20000000ull) {   /* (following the original: as fast as it goes, shown 50 times a second) */
                last_present = now;
                SDL_UpdateTexture(texture, 0, pixels, SCREEN_W * 4);
                SDL_RenderClear(renderer);
                SDL_RenderTexture(renderer, texture, 0, 0);
                SDL_RenderPresent(renderer);
            }
        }
        if (!vsync && (!hooks || !hooks->paced || hooks->paced())) {
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
    if (hooks && hooks->finish)
        hooks->finish(pixels);
    return 0;
}
