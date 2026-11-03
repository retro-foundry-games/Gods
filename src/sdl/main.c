#define _GNU_SOURCE
/* Gods SDL port: the game as it is played.  No options, no tests: the intro, the menu and the levels, in a window of three times the Amiga's screen; F10 quits.  The disks are the
 * two images in the folder GODS_DISKS_DEFAULT names (the folder projects/gods/local, set by the Makefile).  The development program (devmain.c) has the options and the lockstep. */
#include "host.h"

#ifndef GODS_DISKS_DEFAULT
#define GODS_DISKS_DEFAULT "projects/gods/local"
#endif

int main(void)
{
    struct HostOptions options = { .disks = GODS_DISKS_DEFAULT, .scale = 3, .phase = PHASE_INTRO, .window_x = -1, .window_y = -1 };

    return host_run(&options);
}
