#define _GNU_SOURCE
/* Gods SDL port: the program.  The menu program (menu.c ...) runs on a thread of its own; this thread is the machine around it: it
 * raises the vertical blank interrupt 50 times a second, makes the picture from the chips' registers, and takes the keys.
 *
 *   gods-sdl [--disks FOLDER] [--scale N] [--headless --frames N [--dump PREFIX] [--dump-every N] [--keys FRAME:RAW:1|0,...]] */
#include <SDL3/SDL.h>
#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest.h"

extern void menu_boot(void);    /* boot.c: the start-up (orig $039E50) */
extern void menu_start(void);   /* ui.c: the menu (orig $006388) */
extern uint32_t heap_base;
extern uint8_t *mask_buffer;   /* shares its address with restart_address, which menu_boot sets to its own address: the original blits into the (finished) boot code */

static void *program(void *unused)
{
    (void)unused;
    menu_boot();
    mask_buffer = (uint8_t *)0x39e50;   /* the area of the original's boot code: a buffer now */
    for (;;)
        menu_start();   /* a menu line that starts the game (original code) comes back here */
    return 0;
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

int main(int argc, char **argv)
{
    struct sigaction sa = { .sa_sigaction = crash, .sa_flags = SA_SIGINFO };

    sigaction(SIGSEGV, &sa, 0);
    const char *disks = "projects/gods/local";
    const char *dump = 0;
    int headless = 0, frames = 0, scale = 3, dump_every = 50;
    const char *keys = 0;   /* a scripted run: "frame:raw code (hex):1 pressed or 0 released", comma separated */
    static uint32_t pixels[SCREEN_W * SCREEN_H];
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;
    SDL_Texture *texture = 0;
    pthread_t thread;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disks") && i + 1 < argc)
            disks = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc)
            scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--skip-intro"))
            ;
        else if (!strcmp(argv[i], "--headless"))
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
            fprintf(stderr, "usage: gods-sdl [--disks FOLDER] [--scale N] [--headless --frames N [--dump PREFIX] [--dump-every N]]\n");
            return 2;
        }
    }
    guest_init();
    if (files_open(disks) != 0 || files_load_menu_image() != 0)
        return 1;
    heap_base = 0x80400;   /* the memory the program hands out (the boot block leaves it in the vector slot at $2C) */

    if (!headless) {
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
        sound_open();
    }
    pthread_create(&thread, 0, program, 0);

    uint64_t next = SDL_GetTicksNS();
    for (int frame = 0; !frames || frame < frames; frame++) {
        SDL_Event event;

        while (!headless && SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                exit(0);
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && !event.key.repeat) {
                int code = raw_code(event.key.scancode);

                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F10)
                    exit(0);
                if (code >= 0)
                    irq_key(code, event.type == SDL_EVENT_KEY_DOWN);
            }
        }
        for (const char *k = keys; k && *k;) {   /* the scripted keys of this frame */
            int at, code, down, used = 0;

            if (sscanf(k, "%d:%x:%d%n", &at, &code, &down, &used) >= 3 && at == frame)
                irq_key(code, down);
            k += used ? used : strlen(k);
            if (*k == ',')
                k++;
        }
        irq_vbl();
        display_render(pixels);
        if (dump && dump_every && frame % dump_every == 0)
            dump_ppm(dump, frame, pixels);
        if (!headless) {
            SDL_UpdateTexture(texture, 0, pixels, SCREEN_W * 4);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, 0, 0);
            SDL_RenderPresent(renderer);
            next += 20000000ull;
            uint64_t now = SDL_GetTicksNS();

            if (next > now)
                SDL_DelayNS(next - now);
            else
                next = now;
        }
    }
    if (dump)
        dump_ppm(dump, 99999, pixels);
    sound_close();
    return 0;
}
