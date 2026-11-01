/* Gods SDL port, the intro: the window, the 50 Hz vertical blank, the keys and the audio around the intro (intro.c) that runs on a thread of
 * its own.  When the intro asks for the next part of the game (fire or space) this program starts the menu program, gods-sdl-menu, in its
 * place with the same arguments.
 *
 *   gods-sdl [--disks FOLDER] [--scale N] [--skip-intro] [--headless --frames N [--dump PREFIX] [--dump-every N] [--keys FRAME:RAW:1|0,...]] */
#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "intro_host.h"

#define WIDTH 320
#define HEIGHT 256
#define FIRST_LINE 44
#define SCREEN_TOP 60

static char **saved_argv;
static int saved_argc;

static void *program(void *unused)
{
    (void)unused;
    game_main();
    return 0;
}

/* the intro is over: the menu program (next to this one) takes over, with this program's arguments */
void ih_start_menu(void)
{
    char path[4096], *args[64];
    ssize_t n = readlink("/proc/self/exe", path, sizeof path - 20);
    int count = 0;

    if (n > 0) {
        path[n] = 0;
        strcpy(strrchr(path, '/') + 1, "gods-sdl-menu");
        args[count++] = path;
        for (int i = 1; i < saved_argc && count < 60; i++)
            args[count++] = saved_argv[i];
        args[count] = 0;
        fflush(0);
        execv(path, args);
    }
    perror("gods-sdl: cannot start the menu program");
    exit(1);
}

static uint32_t argb(uint16_t color)
{
    return 0xff000000u | (uint32_t)((color >> 8 & 15) * 17) << 16 | (uint32_t)((color >> 4 & 15) * 17) << 8 | (uint32_t)((color & 15) * 17);
}

/* the picture: 320 x 200, five bitplanes (32 colours), from line 60 on */
static void render(uint32_t *pixels)
{
    const uint16_t *screen = ih_shown_screen();
    uint32_t palette[32];

    for (int i = 0; i < 32; i++)
        palette[i] = argb(ih_color(i));
    for (int i = 0; i < WIDTH * HEIGHT; i++)
        pixels[i] = palette[0];
    for (int row = 0; row < 200; row++)
        for (int x = 0; x < WIDTH; x++) {
            int index = 0;

            for (int plane = 0; plane < 5; plane++)
                index |= (screen[plane * 4000 + row * 20 + (x >> 4)] >> (15 - (x & 15)) & 1) << plane;
            pixels[(SCREEN_TOP - FIRST_LINE + row) * WIDTH + x] = palette[index];
        }
}

static void dump_ppm(const char *prefix, int frame, const uint32_t *pixels)
{
    char name[512];
    FILE *f;

    snprintf(name, sizeof name, "%s%05d.ppm", prefix, frame);
    f = fopen(name, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        uint8_t rgb[3] = { (uint8_t)(pixels[i] >> 16), (uint8_t)(pixels[i] >> 8), (uint8_t)pixels[i] };

        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *dump = 0, *keys = 0;
    int headless = 0, frames = 0, scale = 3, dump_every = 50, skip = 0;
    static uint32_t pixels[WIDTH * HEIGHT];
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;
    SDL_Texture *texture = 0;
    SDL_AudioStream *audio = 0;
    pthread_t thread;

    saved_argc = argc;
    saved_argv = argv;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--skip-intro"))
            skip = 1;
        else if (!strcmp(argv[i], "--headless"))
            headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc)
            dump = argv[++i];
        else if (!strcmp(argv[i], "--dump-every") && i + 1 < argc)
            dump_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc)
            keys = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc)
            scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--disks") && i + 1 < argc)
            i++;
        else
            fprintf(stderr, "gods-sdl: option %s is for the menu program\n", argv[i]);
    }
    if (skip)
        ih_start_menu();
    ih_map_chips();
    if (!headless) {
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
            fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return 1;
        }
        window = SDL_CreateWindow("Gods", WIDTH * scale, HEIGHT * scale, 0);
        renderer = window ? SDL_CreateRenderer(window, 0) : 0;
        texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT) : 0;
        if (!texture) {
            fprintf(stderr, "SDL window: %s\n", SDL_GetError());
            return 1;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT * 5 / 6, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
        if (audio)
            SDL_ResumeAudioStreamDevice(audio);
    }
    pthread_create(&thread, 0, program, 0);

    uint64_t next = SDL_GetTicksNS();

    for (int frame = 0; !frames || frame < frames; frame++) {
        SDL_Event event;

        while (!headless && SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                exit(0);
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && !event.key.repeat) {
                SDL_Scancode s = event.key.scancode;

                if (s == SDL_SCANCODE_F10)
                    exit(0);
                if (s == SDL_SCANCODE_SPACE || s == SDL_SCANCODE_LCTRL || s == SDL_SCANCODE_RETURN)
                    ih_set_fire(event.type == SDL_EVENT_KEY_DOWN);
            }
        }
        for (const char *k = keys; k && *k;) {   /* scripted keys: "frame:raw code:1|0", only the space bar (40) counts here */
            int at, code, down, used = 0;

            if (sscanf(k, "%d:%x:%d%n", &at, &code, &down, &used) >= 3 && at == frame && code == 0x40)
                ih_set_fire(down);
            k += used ? used : strlen(k);
            if (*k == ',')
                k++;
        }
        ih_vbl();
        render(pixels);
        if (dump && dump_every && frame % dump_every == 0)
            dump_ppm(dump, frame, pixels);
        if (!headless) {
            int16_t samples[882 * 2];

            ih_music_mix(samples, 882, 44100);
            if (audio && SDL_GetAudioStreamQueued(audio) < 44100 / 5 * 4)
                SDL_PutAudioStreamData(audio, samples, sizeof samples);
            SDL_UpdateTexture(texture, 0, pixels, WIDTH * 4);
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
    return 0;
}
