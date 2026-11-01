/* Gods on the Amiga: what the SDL port (sdl/sdl_platform.h) does differently.  Here the program's addresses are the machine's and
 * the blitter is started by the store to BLTSIZE. */
#ifndef GODS_AMIGA_PLATFORM_H
#define GODS_AMIGA_PLATFORM_H

#define GUEST(address) (address)   /* an address of the program's memory as a pointer's value */

/* start the blitter: the registers are set, `size` (height and width) starts it */
static inline void blit_start(uint16_t size)
{
    custom->bltsize = size;
}

#endif
