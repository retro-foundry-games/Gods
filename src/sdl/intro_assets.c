/* Gods SDL port, the intro: the music module and the pictures are read from bm.1 of disk 1, the file the original's loader unpacks to $84 (intro_data.c defines them, empty).  Compiled with the program's headers and flags, without SDL. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intro.h"
#include "intro_host.h"

uint8_t *files_find(const char *name, int disk, uint32_t *size);
uint8_t *files_unpack_file(const char *name, int disk, uint32_t *size);

#define IMAGE_BASE 0x84u       /* where the loader puts the unpacked file */
#define GRAPHICS_ADDRESS 0x3a62u
#define MUSIC_MODULE_ADDRESS 0x1b6f2u
/* The loader unpacks the module's last 19424 bytes ($6908C to the end) 2428 bytes too high and copies them down over the gap; the copy ends at $6908C and what lies above stays
 * where it was unpacked: the bytes at $6908C and up are the file's 2428 bytes earlier. */
#define DISPLACED_FROM 0x6908cu
#define DISPLACED_BY 2428u

_Static_assert(sizeof(struct Graphics) == 97424, "graphics size");
_Static_assert(sizeof(struct MusicModule) == 0x43c + 318960, "music module size");
_Static_assert(GRAPHICS_ADDRESS + sizeof(struct Graphics) == MUSIC_MODULE_ADDRESS, "the pictures and the module follow each other");

static void swap_words(void *p, size_t bytes)
{
    uint8_t *b = p;

    for (size_t i = 0; i + 1 < bytes; i += 2) {
        uint8_t t = b[i];

        b[i] = b[i + 1];
        b[i + 1] = t;
    }
}

int ih_load_assets(void)
{
    uint32_t size;
    uint8_t *file = files_unpack_file("bm.1", 0, &size);
    uint8_t *music_module_bytes = (uint8_t *)&music_module;

    if (!file || size < MUSIC_MODULE_ADDRESS + sizeof music_module - IMAGE_BASE) {
        fprintf(stderr, "gods-sdl: bm.1 of disk 1 is missing or too short\n");
        free(file);
        return -1;
    }
    memcpy(&graphics, file + (GRAPHICS_ADDRESS - IMAGE_BASE), sizeof graphics);
    swap_words(&graphics, sizeof graphics);   /* (the pictures and the table are words) */
    memcpy(music_module_bytes, file + (MUSIC_MODULE_ADDRESS - IMAGE_BASE), sizeof music_module);
    for (uint32_t at = DISPLACED_FROM; at < MUSIC_MODULE_ADDRESS + sizeof music_module; at++)
        music_module_bytes[at - MUSIC_MODULE_ADDRESS] = file[at - DISPLACED_BY - IMAGE_BASE];
    for (int i = 0; i < 31; i++)   /* (length, volume, loop start and length of each sample) */
        swap_words(&music_module.sample[i].length, 8);
    free(file);
    return 0;
}
