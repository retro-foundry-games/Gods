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

/* a word and a long of the data of a loaded file (the files are big-endian: on the Amiga they are read as they are) */
static inline uint16_t file_word(const void *p)
{
    return *(const uint16_t *)p;
}

static inline uint32_t file_long(const void *p)
{
    return *(const uint32_t *)p;
}

/* `count` words and longs of a loaded file's data are made the machine's own */
static inline void file_words(void *p, uint32_t count)
{
    (void)p;
    (void)count;
}

static inline void file_longs(void *p, uint32_t count)
{
    (void)p;
    (void)count;
}

/* the original's routine at `address` is not replicated and was reached (the host says so; here nothing is done, as the original's code is not there to call either) */
static inline void host_missing_routine(uint32_t address)
{
    (void)address;
}

#endif
