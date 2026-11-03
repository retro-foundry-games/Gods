/* Gods SDL port: the machine around the program (host.c), shared by the regular program (main.c) and the development one (devmain.c) */
#ifndef GODS_SDL_HOST_H
#define GODS_SDL_HOST_H

#include <SDL3/SDL.h>
#include <stdint.h>

enum { PHASE_INTRO, PHASE_MENU, PHASE_LEVEL };

/* what devmain.c adds to the machine's loop: the options, the lockstep with the original and the other tests.  The regular program passes none of it (a null `hooks`). */
struct HostHooks {
    /* a key of the keyboard before the machine's own handling: nonzero when it was taken (the quicksave keys) */
    int (*key)(SDL_Scancode scancode, int down);
    /* the window is made: more windows (the difference window) */
    void (*windows)(int scale);
    /* at the start of every pass of the loop, after the events: 0 ends the run; keys of a script and the lockstep's commands are given here (host_key) */
    int (*frame_start)(int frame, uint32_t *pixels, int phase);
    /* nonzero: leave this pass out (the machine's time stands still) */
    int (*hold)(int phase);
    /* a vertical blank has been raised and the program's thread given its time (the lockstep waits a moment for it) */
    void (*after_blank)(void);
    /* the frame is made (pixels, samples): the lockstep answers, the tests write what they want */
    void (*after_frame)(int frame, int phase, uint32_t *pixels, const int16_t *samples);
    /* nonzero: the machine is paced to 50 frames a second by sleeping (without a window, and without vsync) */
    int (*paced)(void);
    /* the run is over */
    void (*finish)(uint32_t *pixels);
    int no_vsync;        /* do not wait for the display's refresh: the frames are shown 50 times a second (following a live original) */
    int no_keyboard;     /* the keyboard is not taken: the original is played live and this port follows it */
};

struct HostOptions {
    const char *disks;   /* the folder with gods-disk1.adf and gods-disk2.adf */
    int scale;           /* the window is 320 * scale wide */
    int phase;           /* the program the machine starts: PHASE_INTRO, PHASE_MENU */
    int headless;        /* no window and no sound */
    int frames;          /* 0: for ever */
    int window_x, window_y;   /* -1: where the system puts it */
    const char *wav;     /* the sound is also written to this file as raw 16 bit stereo (a test) */
    const struct HostHooks *hooks;
};

int host_run(const struct HostOptions *options);   /* the machine: returns when it is over (the window closed: exit) */
void host_key(int phase, int code, int down);      /* a key of the Amiga's keyboard (raw code): the intro takes the fire buttons, the menu and the game the keyboard */
extern int host_dev;   /* set by devmain.c only: the environment variables of the tests are read only then */

#endif
