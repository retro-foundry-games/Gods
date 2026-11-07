#define _GNU_SOURCE
/* Gods SDL port: the game as it is played: the intro, the menu and the levels, in a window of three times the Amiga's screen; F10 quits.  The disks are the two images gods-disk1.adf and
 * gods-disk2.adf in the folder --disks names (default: the working folder), where the port keeps its own hiscores.bin as well.  The development program (devmain.c) has the other
 * options and the lockstep. */
#include <stdio.h>
#include <string.h>

#include "host.h"

static void usage(FILE *out)
{
    fprintf(out, "usage: gods-sdl [--disks DIR]\n"
                 "  --disks DIR   the folder with gods-disk1.adf and gods-disk2.adf (default: the working folder)\n"
                 "  --help        this text\n"
                 "hiscores.bin (the scores and the level codes) is kept in the working folder; F10 quits.\n");
}

int main(int argc, char **argv)
{
    struct HostOptions options = { .disks = ".", .scale = 3, .phase = PHASE_INTRO, .window_x = -1, .window_y = -1 };

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(argv[i], "--disks") && i + 1 < argc) {
            options.disks = argv[++i];
            continue;
        }
        fprintf(stderr, "gods-sdl: unknown option %s\n", argv[i]);
        usage(stderr);
        return 2;
    }
    return host_run(&options);
}
