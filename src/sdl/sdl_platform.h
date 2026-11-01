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

#endif
