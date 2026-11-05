/* Gods SDL port: routines of the program whose behaviour depends on the Amiga's memory, replaced here (this object is linked before
 * the program's, whose own copies are not used).  Each is the program's routine with the one difference said.  Compiled with the
 * program's headers and flags. */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "guest.h"

/* orig $016ECA (message.c): draw message_text in the work area, 40 characters at most, centred, and start the colours fading in.
 * The program calls it with message_text 0 when it fades a message out with nothing to follow: on the Amiga the text is then read
 * from address 0 (the first bytes of the memory, which are 0: an empty text); here it is an empty text. */
void message_draw(void)
{
    static const uint8_t empty[1] = { 0 };
    const uint8_t *text = message_text ? message_text : empty;
    int16_t length = 0;

    while (*text++ != 0)
        length++;
    message_wide = -1;

    int16_t start = (40 - length) >> 1;

    message_x = start - 8;
    if (start < 0) {
        start = 0;
        message_x = 0;
        message_wide = 320;
        length = 40;
    }
    uint8_t *to = (uint8_t *)work_area + 40 + start;

    text = message_text ? message_text : empty;
    for (int i = 0; i < length; i++) {
        const uint8_t *glyph = font_8x8 + (*text++ << 3);

        /* (the rows are words of the machine: the shifting and the ticker of the message work on words, a byte of the row is the high or the low half of its word) */
        for (int row = 0; row < 7; row++) {
            uint16_t *word = (uint16_t *)(to - ((uintptr_t)to & 1) + 40 * row);
            int low = (int)((uintptr_t)to & 1);

            *word = (uint16_t)(low ? (*word & 0xff00) | glyph[row] : (*word & 0x00ff) | (glyph[row] << 8));
        }
        to++;
    }
    message_shown = 1;
    message_words[8] = 0x0320;
    message_words[9] = 0x0532;
    message_words[10] = 0x0754;
    message_words[11] = 0x0765;
    message_words[0] = message_words[4];
    message_words[1] = message_words[5];
    message_words[2] = message_words[6];
    message_words[3] = message_words[7];
    message_state = -1;
    message_timer = 10;
}

/* the Amiga's game_loop is the original's loop at $2C20 sent to game_frame (begin_quest ends by entering it): on the host it is the
 * loop of game_frame, for ever, with the interrupts on */
extern void sr_set(uint16_t value);

void (*host_draw_hook)(int kind, int16_t x, int16_t y, uint16_t index);   /* the dev build's log of the draws (devmain.c); none in the regular program */
volatile int gods_waiting;   /* the program waits for the original (GODS_FOLLOW, devmain): the machine's time does not go on */
unsigned gods_iteration;   /* the iteration of the loop now running (for the debugger) */
void (*host_iteration_hook)(unsigned iteration);   /* devmain's: called at the top of every iteration (dev_game.c); none in the regular program */

void game_loop(void)
{
    unsigned iteration = 0;

    sr_set(0x2000);
    for (;;) {
        iteration++;
        gods_iteration = iteration;
        if (host_iteration_hook)
            host_iteration_hook(iteration);
        game_frame();
    }
}

/* ---- Begin Quest on the host -------------------------------------------------------------------------------------------------------
 * The menu's lines call the original's code (original_begin, original_continue, original_after_password), which has no C yet.  On the
 * host they call host_begin_quest(): with --level1-image the level is started from that RAM image of the original (a test) and the game
 * frame runs for ever; without it the call is logged and the menu goes on.  The functions are weak: the C ones, when they exist and
 * keep these names, take their place; when the menu calls another name this hook is not used. */
const char *host_level_image;   /* set by main.c: --level1-image */
extern int guest_load_level_image(const char *path);
extern void sr_set(uint16_t value);

void host_begin_quest(void)
{
    if (!host_level_image) {
        fprintf(stderr, "gods-sdl: Begin Quest: the C is not there yet (try --level1-image)\n");
        return;
    }
    if (guest_load_level_image(host_level_image) != 0) {
        fprintf(stderr, "gods-sdl: cannot read %s\n", host_level_image);
        return;
    }
    sr_set(0x2000);   /* the interrupts on */
    for (;;)
        game_frame();
}

/* the quickload (--quickload FILE): begin_quest takes the level from a RAM image of the original instead of loading it */
const char *host_quickload_file;
extern int guest_load_quick(const char *path);

int host_quickload(void)
{
    if (!host_quickload_file)
        return 0;
    if (guest_load_quick(host_quickload_file) != 0) {
        fprintf(stderr, "gods-sdl: cannot read %s\n", host_quickload_file);
        exit(1);
    }
    return 1;
}

__attribute__((weak)) void original_begin(void)
{
    host_begin_quest();
}

__attribute__((weak)) void original_continue(void)
{
    host_begin_quest();
}

__attribute__((weak)) void original_after_password(void)
{
    host_begin_quest();
}

/* the original's CPU work (a loop that clears a screen, the drawing of a frame) takes frames of the video; the host does it at once and waits for them */
/* the lockstep with the original (`flipsync OFFSET`): the port's flips wait for the original's (the driver gives the count of the times the original reached its flip, $1D302), so
 * the CPU time of an iteration is not modelled (host_iteration_cpu does nothing); the other waits stay, as the least time the original takes */
volatile int host_flip_sync;
volatile unsigned host_flips_orig;   /* the original's flips since the sync began */
volatile int host_flips_offset;
static unsigned flips_done;
static int flip_bound;   /* the next flip ends an iteration (host_iteration_cpu): it waits for the original's */

void host_flip_wait(void)
{
    extern void wait_vblank(void);

    if (!host_flip_sync)
        return;
    if (flip_bound) {
        /* (the count of the original's flips is the one of the frame before: the flip of the frame the port is in is seen in the next; a flip bound as `early` is not waited for that frame) */
        while ((int)(flips_done + 1) - host_flips_offset > (int)host_flips_orig + (flip_bound == 2))
            wait_vblank();
        flip_bound = 0;
    }
    flips_done++;
}

/* the fade-in of the teleport (host_fade_bind before it): in the lockstep it begins when the original's call of its fade-in has been made (its time depends on what the original draws
 * before the flip), whatever the port's own time was; every fade-in is counted, so the count goes with the original's */
volatile unsigned host_fades_orig;   /* the original's calls of its fade-in since the sync began */
static unsigned fades_done;
static int fade_bound;

void host_fade_sync_start(void)
{
    fades_done = 0;
}

void host_fade_bind(void)
{
    if (host_flip_sync)
        fade_bound = 1;
}

void host_fade_wait(void)
{
    extern void wait_vblank(void);

    fades_done++;
    if (fade_bound) {
        while ((int)fades_done > (int)host_fades_orig)
            wait_vblank();
        fade_bound = 0;
    }
}

volatile int host_disk_sync, host_disk_done;
static unsigned disk_extra;   /* frames that the next wait for the disk goes on for */

void host_disk_extra(unsigned frames)
{
    disk_extra = frames;
}   /* the lockstep tells how many of the original's reads of the background are done */

/* the original's read of a file takes as long as its drive does: in the lockstep the port waits until the original has finished the read that began with its own, otherwise
 * for about as many frames as it took in the one that was looked at */
void host_disk_wait(void)
{
    extern void wait_vblank(void);

    if (!host_disk_sync) {
        host_cpu_frames(14);
        return;
    }
    int done = host_disk_done;

    while (host_disk_done == done)
        wait_vblank();
    host_cpu_frames(disk_extra);
    disk_extra = 0;
}

void host_cpu_frames(unsigned frames)
{
    extern void wait_vblank(void);

    while (frames--)
        wait_vblank();
}

static uint16_t iteration_mark;

int host_lockstep_level;   /* set by main.c: --lockstep-level */

int16_t host_start_level(void)
{
    return host_lockstep_level ? (int16_t)host_lockstep_level : 1;
}

void host_iteration_mark(void)
{
    iteration_mark = vbl_count;
}

/* the drawing and moving of a frame of the game takes the original's CPU about two video frames, so a frame that is not held back by the wait for the last flip (the first
 * after a fade) still ends two frames after it began */
static unsigned iteration_cpu_next;

/* the next flip is the one of a drawing that is not an iteration's (the respawn): in the lockstep it is made in the frame of the original's flip */
void host_flip_bind(void)
{
    if (host_flip_sync)
        flip_bound = 2;
}

void host_iteration_cpu_next(unsigned frames)
{
    iteration_cpu_next = frames;
}

void host_iteration_cpu(unsigned frames)
{
    extern void wait_vblank(void);

    if (host_flip_sync) {
        iteration_cpu_next = 0;
        flip_bound = 1;
        return;
    }
    if (iteration_cpu_next) {
        frames = iteration_cpu_next;
        iteration_cpu_next = 0;
    }
    while ((uint16_t)(vbl_count - iteration_mark) < frames)
        wait_vblank();
}

/* an original routine that is not replicated was reached: once each, on stderr */
void host_missing_routine(uint32_t address)
{
    static uint32_t said[256];
    static unsigned count;

    for (unsigned i = 0; i < count; i++)
        if (said[i] == address)
            return;
    if (count < 256)
        said[count++] = address;
    fprintf(stderr, "gods-sdl: the original's routine at $%X is not replicated and was reached\n", address);
}
