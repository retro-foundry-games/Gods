/* Gods on the host (SDL port): what amiga/platform.h does on the Amiga.  The program's memory is at the Amiga's addresses (the
 * first 64 KiB are LOW_BASE higher: Linux does not map them), the custom chips and the CIAs are pages of plain memory at
 * their addresses, and the blitter runs when it is started. */
#ifndef GODS_SDL_PLATFORM_H
#define GODS_SDL_PLATFORM_H

#include <stdint.h>

#define GODS_LOW_BASE 0x01000000u
#define GODS_LOW_LIMIT 0x10000u

/* an address of the program's memory as a pointer's value */
#define GUEST(address) ((uintptr_t)(address) < GODS_LOW_LIMIT ? (uintptr_t)(address) + GODS_LOW_BASE : (uintptr_t)(address))

void sdl_blit_run(void);   /* sdl/blitter.c: do the blit the registers describe */

static inline void blit_start(uint16_t size)
{
    custom->bltsize = size;
    sdl_blit_run();
}

/* the original's routine at `address` is not replicated and was reached: said once on stderr (sdl/shims.c), so that what the play needs and the port lacks is known */
void host_missing_routine(uint32_t address);
int16_t host_start_level(void);
int host_quickload(void);   /* shims.c: --quickload FILE: the level is the original's RAM at some moment (loaded here); the game loop goes on from it */   /* the level Begin Quest starts: 1, or the lockstep's (platform.h) */
void host_iteration_mark(void);   /* the start of a frame of the game (platform.h) */
void host_iteration_cpu_next(unsigned frames);   /* the next host_iteration_cpu waits for that many frames instead */
void host_iteration_cpu(unsigned frames);   /* wait until that many video frames have passed since the mark */
void host_fade_lag(int on);   /* display.c: the next fade is shown a frame after its steps (the player's respawn) */
void host_cpu_frames(unsigned frames);   /* the original's CPU work that takes that many video frames (platform.h) */

/* a word and a long of the data of a loaded file (the files are big-endian) */
static inline uint16_t file_word(const void *p)
{
    const uint8_t *b = p;

    return (uint16_t)(b[0] << 8 | b[1]);
}

static inline uint32_t file_long(const void *p)
{
    const uint8_t *b = p;

    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}

/* `count` words and longs of a loaded file's data are made the machine's own */
static inline void file_words(void *p, uint32_t count)
{
    uint8_t *b = p;

    for (uint32_t i = 0; i < count; i++, b += 2) {
        uint8_t t = b[0];

        b[0] = b[1];
        b[1] = t;
    }
}

static inline void file_longs(void *p, uint32_t count)
{
    uint8_t *b = p;

    for (uint32_t i = 0; i < count; i++, b += 4) {
        uint8_t t = b[0];

        b[0] = b[3];
        b[3] = t;
        t = b[1];
        b[1] = b[2];
        b[2] = t;
    }
}

#endif
