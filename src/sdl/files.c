/* Gods SDL port: the disks.  The two ADF images are read into memory; the game's file layer (orig $01D536..) finds a file in the
 * directory in sectors 22 to 25 of the disk: "godsd1\0", a word at +$10 that is the offset of the list of names, and from +$12 a
 * (first sector, sector count) pair of words (big endian) for each name; a name is a length byte (the name and its 0, $FF ends the
 * list) and the name.  The sectors of a file follow each other in the ADF.  The files are read whole sectors at a time, as the
 * original does.
 *
 * Words the program reads as native ones: the files ending in ".dat" are tables of words (swapped here), and the picture files
 * ("c" + name) are packed: unpack() below does the unpacking of the original's and then makes the words of the picture native.
 * The high score file (hiscores.bin) has longs - the ten scores (at +16 of each record of 20 bytes) and the four scrambled codes (+200) -
 * which are made native here as well.  Everything else in the files is bytes.
 *
 * The port keeps its own high score file, hiscores.bin in the working folder (where the program is run): when it is there it is read instead of the one of the disk, and the scores and
 * codes are written to it (scores_save below); the disk's own file is read when there is none. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest.h"

#define SECTOR 512
#define DIRECTORY_SECTOR 22

static uint8_t *disk[2];
static int unpack_swaps;   /* the words of what unpack() makes are made native: the file just read is a picture (.pi1: palette and planes) */
static uint32_t disk_size[2];
static char scores_path[1100];   /* the port's own high score file: hiscores.bin in the working folder */

#include "game.h"

static uint8_t *slurp(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *data;
    long n;

    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc((size_t)n);
    if (fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);
    *size = (uint32_t)n;
    return data;
}

int files_open(const char *folder)
{
    char path[1024];

    (void)folder;
    snprintf(scores_path, sizeof scores_path, "hiscores.bin");   /* in the working folder */

    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof path, "%s/gods-disk%d.adf", folder, i + 1);
        disk[i] = slurp(path, &disk_size[i]);
        if (!disk[i]) {
            fprintf(stderr, "gods-sdl: cannot read %s\n", path);
            return -1;
        }
    }
    return 0;
}

static unsigned be16(const uint8_t *p)
{
    return (unsigned)(p[0] << 8 | p[1]);
}

uint8_t *files_find(const char *name, int d, uint32_t *size)
{
    const uint8_t *dir = disk[d] + DIRECTORY_SECTOR * SECTOR;
    const uint8_t *names = dir + be16(dir + 0x10);
    size_t length = strlen(name);

    for (int index = 0; (names[0] & 0x80) == 0; index++) {
        if (names[0] == length + 1 && memcmp(names + 1, name, length) == 0) {
            unsigned first = be16(dir + 0x12 + 4 * index), count = be16(dir + 0x14 + 4 * index);

            if ((first + count) * SECTOR > disk_size[d])
                return 0;
            *size = count * SECTOR;
            return disk[d] + first * SECTOR;
        }
        names += 1 + names[0];
    }
    return 0;
}

static int ends_with(const char *s, const char *tail)
{
    size_t a = strlen(s), b = strlen(tail);

    return a >= b && strcmp(s + a - b, tail) == 0;
}

int files_read(const char *name, int prefer, uint8_t *dest)
{
    uint32_t size;
    uint8_t *file = 0;

    if (name[0] == '\\')
        name++;
    uint8_t *own = 0;   /* the port's own high score file, when there is one */

    if (strcmp(name, "hiscores.bin") == 0 && scores_path[0] != 0) {
        own = slurp(scores_path, &size);
        if (own && size < 216) {
            free(own);
            own = 0;
        }
        file = own;
    }
    if (!file)
        file = files_find(name, prefer, &size);
    if (!file)
        file = files_find(name, !prefer, &size);
    if (!file) {
        fprintf(stderr, "gods-sdl: file %s not on the disks\n", name);
        return -1;
    }
    if (!dest)
        return 0;
    /* (the name may be in the place the file goes to: the level 3 guardian leaves the text buffer inside file_buffer, which the level 4 loads its pictures to; the kind of the file is looked at first) */
    int is_picture = ends_with(name, ".pi1");
    int is_scores = ends_with(name, "hiscores.bin");
    int is_data = ends_with(name, ".dat");

    memcpy(dest, file, size);
    unpack_swaps = is_picture;
    if (is_scores && size >= 216)
        for (uint32_t i = 0; i < 14; i++) {
            uint8_t *p = dest + (i < 10 ? 16 + 20 * i : 200 + 4 * (i - 10));
            uint8_t t = p[0];

            p[0] = p[3];
            p[3] = t;
            t = p[1];
            p[1] = p[2];
            p[2] = t;
        }
    if (is_data)
        for (uint32_t i = 0; i + 1 < size; i += 2) {
            uint8_t t = dest[i];

            dest[i] = dest[i + 1];
            dest[i + 1] = t;
        }
    free(own);
    return 0;
}

extern void host_file_read_message(void);

/* orig $01D536 (and $01D58A, the part that shows "LOADING"): load the file `name` to `dest`; -1 when it is not on the disks.  When tick_hold_38a is set (the shop and the loading of a world set it) the first
 * read clears it and asks for the message "LOADING" (the message is held for the load: tick_hold_26c) */
int32_t file_read(const char *name, void *dest)
{
    int wanted = guest.disk_digit == '2' ? 1 : 0;

    host_file_read_message();   /* (shims.c: the game's side of the $01D58A part) */

    return files_read(name, wanted, dest);
}

/* orig $01CEEA: the high scores are written: record_text (216 bytes, its longs made native) goes to the port's own hiscores.bin in the working folder (the disks are not written) */
void scores_save(void)
{
    uint8_t out[216];

    if (scores_path[0] == 0)
        return;
    memcpy(out, guest.record_text, sizeof out);
    for (uint32_t i = 0; i < 14; i++) {   /* the longs are native in the program's memory: back to the file's byte order (the swap of files_read) */
        uint8_t *p = out + (i < 10 ? 16 + 20 * i : 200 + 4 * (i - 10));
        uint8_t t = p[0];

        p[0] = p[3];
        p[3] = t;
        t = p[1];
        p[1] = p[2];
        p[2] = t;
    }

    FILE *f = fopen(scores_path, "wb");

    if (!f) {
        fprintf(stderr, "gods-sdl: cannot write %s\n", scores_path);
        return;
    }
    fwrite(out, 1, sizeof out, f);
    fclose(f);
}

/* orig $01D966: the drives are put to rest */
void drives_idle(void)
{
}

/* orig $01E260: the packer of the program's picture files (decrunch.c), and then the words of the picture made native */
uint8_t *unpack(uint8_t *to, const uint8_t *from)
{
    uint8_t *first = to, *start = to;

    for (;;) {
        uint32_t length = from[0] | (from[1] << 8);
        const uint8_t *end;

        start = to;   /* the offsets of the copies of a block are from where the block's output begins ($01E262: a1 = a0 at each block) */
        from += 2;
        if ((uint16_t)length == 0)
            break;
        end = from + length;
        while (end > from) {
            uint16_t count = *from++;

            if ((int8_t)count >= 0) {
                uint16_t n = count - 1;

                do {
                    *to++ = *from++;
                } while (n-- != 0);
            } else {
                const uint8_t *copy;
                uint16_t n;

                if (count == 0xff) {
                    count = from[0] | (from[1] << 8);
                    from += 2;
                } else {
                    count &= 0x7f;
                }
                copy = start + (int16_t)(from[0] | (from[1] << 8));
                from += 2;
                n = count - 1;
                do {
                    *to++ = *copy++;
                } while (n-- != 0);
            }
        }
    }
    if (unpack_swaps)
        for (uint8_t *p = first; p + 1 < to; p += 2) {
            uint8_t t = p[0];

            p[0] = p[1];
            p[1] = t;
        }
    return to;
}

/* the menu program's image: the file bm.2 of disk 1, unpacked to the guest memory at $84 (what the original loader does) */
int files_load_menu_image(void)
{
    uint32_t size;
    const uint8_t *packed = files_find("bm.2", 0, &size);
    uint8_t *out;
    uint8_t *end;

    if (!packed) {
        fprintf(stderr, "gods-sdl: bm.2 not on disk 1\n");
        return -1;
    }
    out = malloc(0x80000);
    unpack_swaps = 0;   /* the loader's unpacker leaves bytes */
    end = unpack(out, packed);
    guest_load_image(out, (uint32_t)(end - out));
    free(out);
    return 0;
}
