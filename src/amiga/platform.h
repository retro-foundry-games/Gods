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

/* the original's CPU work takes `frames` video frames here (a loop of clr.l, the drawing of a frame); the Amiga's own time is that, the host waits for them */
/* the level Begin Quest starts: always the first (the lockstep with the original may start another) */
static inline int16_t host_start_level(void)
{
    return 1;
}

/* the SDL port's quickload (--quickload FILE): not on the Amiga */
static inline int host_quickload(void)
{
    return 0;
}

static inline void host_fade_lag(int on)
{
    (void)on;
}

static inline void host_disk_extra(unsigned frames)
{
    (void)frames;
}

static inline void host_disk_wait(void)
{
}

static inline void host_flip_late(int on)
{
    (void)on;
}

static inline void host_cpu_frames(unsigned frames)
{
    (void)frames;
}

static inline void host_spin(void)
{
}

static inline void host_spin_end(void)
{
}

/* the start of a frame of the game, and the least time its work takes: the host has done the work at once and waits for the video frames it took on the Amiga */
static inline void host_iteration_mark(void)
{
}

static inline void host_iteration_cpu(unsigned frames)
{
    (void)frames;
}

static inline void host_iteration_cpu_next(unsigned frames)
{
    (void)frames;
}

static inline void host_world_load_wait(void)
{
}

static inline void host_fade_bind(void)
{
}

static inline void host_flip_bind(void)
{
}

#endif
