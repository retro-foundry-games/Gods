/* Gods on the host (SDL port): what amiga/platform.h does on the Amiga.  The program's memory is at the Amiga's addresses (the
 * block is struct Guest), the custom chips and the CIAs are objects of plain memory, and the blitter runs when it is started. */
#ifndef GODS_SDL_PLATFORM_H
#define GODS_SDL_PLATFORM_H

#include <stdint.h>

struct Guest;
extern struct Guest guest;   /* guest_struct.h */

/* an address of the program's memory (struct Guest, guest_struct.h: its variables at the offsets of their original addresses, and its heap) as a pointer's value */
#define GUEST(address) ((uintptr_t)&guest + (uintptr_t)(address))

void host_blitter_not_emulated(void);   /* display.c: the chip's blitter is not emulated: the program's routines that program its registers are replaced by C (said once on stderr if one is reached) */

static inline void blit_start(uint16_t size)
{
    custom->bltsize = size;
    host_blitter_not_emulated();
}

/* the original's routine at `address` is not replicated and was reached: said once on stderr (sdl/shims.c), so that what the play needs and the port lacks is known */
void host_missing_routine(uint32_t address);
int16_t host_start_level(void);
int host_quickload(void);   /* shims.c: --quickload FILE: the level is the original's RAM at some moment (loaded here); the game loop goes on from it */   /* the level Begin Quest starts: 1, or the lockstep's (platform.h) */
void host_iteration_mark(void);   /* the start of a frame of the game (platform.h) */
void host_iteration_cpu_next(unsigned frames);   /* the next host_iteration_cpu waits for that many frames instead */
void host_iteration_cpu(unsigned frames);   /* wait until that many video frames have passed since the mark */
void host_flip_bind(void);   /* the next flip (not an iteration's) is made in the frame of the original's, in the lockstep */
void host_disk_extra(unsigned frames);   /* shims.c: the next host_disk_wait goes on for that many frames more */
void host_disk_wait(void);   /* shims.c: the original's read of a file takes its time (the lockstep: until the original is done)*/
void host_flip_late(int on);   /* display.c: the flips are shown a frame after the blank that ends their wait */
void host_read_skip(void);   /* shims.c: a read of the keys that the original's start frame makes is counted, not waited for */
void host_read_wait(void);   /* shims.c: the lockstep: the read of the keys of an iteration waits for the original's */
void host_spin(void);   /* display.c: the program's thread is in a loop that waits for blanks (the lockstep lets it run until then) */
void host_spin_end(void);   /* display.c: and is out of it */
void host_fade_lag(int on);   /* display.c: the next fade is shown a frame after its steps (the player's respawn) */
void host_fade_bind(void);   /* shims.c: the next fade-in begins when the original's call of its own has been made (the teleport) */
void host_fade_wait(void);   /* shims.c: at the top of fade_in */
void host_fade_sync_start(void);   /* shims.c: the lockstep's sync begins: the fade-ins are counted from here */
void host_world_load_wait(void);   /* shims.c: the load of a world is done: the lockstep waits for the original's end of its own */
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

/* `count` longs of a loaded RAM image of the original that are addresses of its data (it is memory of the original's: the block's own now) */
#define GODS_GUEST_END 0x200000u

static inline void file_pointers(void *p, uint32_t count)
{
    uint32_t *w = p;

    file_longs(p, count);
    for (uint32_t i = 0; i < count; i++)
        if (w[i] != 0 && w[i] < GODS_GUEST_END)
            w[i] = (uint32_t)GUEST(w[i]);
}

#endif
