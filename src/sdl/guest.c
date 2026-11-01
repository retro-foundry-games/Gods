/* Gods SDL port: the guest memory and the chips' register pages.
 *
 * The program keeps its variables at the Amiga's addresses (the symbols of guest.ld, made by gen_guest.py).  Here that memory is
 * mapped at the same addresses (the first 64 KiB are GUEST_LOW_BASE higher: Linux does not let a program map them), the pages
 * of the custom chips and the CIAs are mapped, and the memory the loader brings is made: the tables of menu_data.c are copied
 * in, and the menu program's image (the file bm.2 of disk 1, unpacked to address $84) is copied in except where a table is (those
 * are native C data; the image has the Amiga's big-endian bytes).  The few words and longs of the image that the program reads
 * as native ones (the copper lists, the message colours) are made native here. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "guest.h"
#include "guest_tables.h"
#include "guest_fixups.h"

#define IMAGE_BASE 0x84u

static void map_page(uintptr_t address, size_t size)
{
    void *p = mmap((void *)address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

    if (p != (void *)address) {
        fprintf(stderr, "gods-sdl: cannot map %#lx (%s)\n", (unsigned long)address, p == MAP_FAILED ? "failed" : "moved");
        exit(1);
    }
}

uint8_t *guest_ptr(uint32_t address)
{
    return (uint8_t *)(uintptr_t)(address < GUEST_LOW_LIMIT ? address + GUEST_LOW_BASE : address);
}

static int in_table(uint32_t address)
{
    for (unsigned i = 0; i < sizeof guest_tables / sizeof guest_tables[0]; i++)
        if (address >= guest_tables[i].address && address < guest_tables[i].address + guest_tables[i].size)
            return 1;
    return 0;
}

/* words of the image that the program reads as native words */
static void swap16_range(uint32_t address, uint32_t bytes)
{
    uint16_t *p = (uint16_t *)guest_ptr(address);

    for (uint32_t i = 0; i < bytes / 2; i++)
        p[i] = (uint16_t)((p[i] << 8) | (p[i] >> 8));
}

static void swap32_range(uint32_t address, uint32_t bytes)
{
    uint32_t *p = (uint32_t *)guest_ptr(address);

    for (uint32_t i = 0; i < bytes / 4; i++)
        p[i] = __builtin_bswap32(p[i]);
}

void guest_init(void)
{
    map_page(0x10000, GUEST_END - 0x10000);
    map_page(GUEST_LOW_BASE, GUEST_LOW_LIMIT);
    map_page(0xbfd000, 0x2000);   /* the CIAs */
    map_page(0xdff000, 0x1000);   /* the custom chips */
    guest_reset();
}

/* what the loader leaves when a program is loaded over the other: the memory and the chips' registers cleared, the tables in */
void guest_reset(void)
{
    memset((void *)0x10000, 0, GUEST_END - 0x10000);
    memset((void *)GUEST_LOW_BASE, 0, GUEST_LOW_LIMIT);
    memset((void *)0xdff000, 0, 0x1000);
    for (unsigned i = 0; i < sizeof guest_tables / sizeof guest_tables[0]; i++)
        memcpy(guest_ptr(guest_tables[i].address), guest_tables[i].data, guest_tables[i].size);
    *(volatile uint8_t *)0xbfe001 = 0xff;   /* CIA A port A: no fire button down */
}

void guest_load_image(const uint8_t *image, uint32_t size)
{
    uint32_t *pairs = (uint32_t *)(uintptr_t)message_pairs;   /* (the table list declares it as bytes) */

    for (uint32_t i = 0; i < size; i++)
        if (!in_table(IMAGE_BASE + i))
            *guest_ptr(IMAGE_BASE + i) = image[i];

    /* the variables the program reads as words that the image has: the message colours (message_words) */
    swap16_range(0x53ee, 32);
    swap32_range(0x3b6, 4);      /* blit_plane_stride */
    swap16_range(0x6384, 2);     /* menu_item_count */

    /* What the original's start-up writes that is not in the unpacked image: the name of the first asset set and the lists of
     * file names of the asset sets (the table asset_names points at them; the names are on the disks' directories) */
    memcpy(guest_ptr(0x196d6), "always", 7);
    static const char names[] =
           "always1\0always2\0always3\0godfont\0\0obj1\0obj2\0\0smlfnt\0\0shopspr\0shopfnt\0\0level1a\0level1b\0\0guard11\0guard12\0\0"
           "level2a\0level2b\0\0amdragn\0\0level3a\0level3b\0\0guard31\0guard32\0\0level4a\0level4b\0\0guard41\0guard42\0\0";
    memcpy(guest_ptr(0x1ed72), names, sizeof names);

    /* message_pairs (a table of longs, in the executable): the addresses of the copper colour moves the message fades */
    mprotect((void *)((uintptr_t)pairs & ~0xfffu), 0x2000, PROT_READ | PROT_WRITE);
    for (int i = 0; i < 8; i++)
        if (pairs[i] < GUEST_LOW_LIMIT) {
            pairs[i] += GUEST_LOW_BASE;
            *((uint16_t *)(uintptr_t)pairs[i] + 1) = 0;   /* the message starts dark (the copper list's colour of its line) */
        }
}

/* ---- a RAM image of the original in level 1 (a test of the level on the host: --level1-image) ---------------------------------------
 * The image is the original's chip RAM (big endian); the tables of menu_data.c stay as they are, the rest is copied in and then the words
 * and longs of the program's variables (guest_fixups.h, from menu.symbols' types) are made native, and so are the pictures: the two
 * screens, the sprites of the image table, the background.  What the image has that the C reads in other ways (the level's map, ...) is
 * not known here and is left as it is. */
extern uint8_t *image_table, *asset_memory, *memory_base;
extern uint16_t shadow_table[];

static void swap_image_table(void)
{
    uint8_t *p = image_table;

    for (unsigned i = 0; i < 0x1450 / 8; i++, p += 8) {
        swap16_range((uint32_t)(uintptr_t)p, 4);
        swap32_range((uint32_t)(uintptr_t)p + 4, 4);
    }
}

int guest_load_level_image(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t *image;
    long size;

    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    image = malloc((size_t)size);
    if (fread(image, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(image);
        return -1;
    }
    fclose(f);
    for (uint32_t i = IMAGE_BASE; i < (uint32_t)size; i++)
        if (!in_table(i))
            *guest_ptr(i) = image[i];
    free(image);
    uint8_t *swapped = calloc((size_t)size + 8, 1);
    for (unsigned i = 0; i < sizeof guest_fixups / sizeof guest_fixups[0]; i++) {
        if ((long)guest_fixups[i].address + 4 * guest_fixups[i].count > size)
            continue;
        for (unsigned j = 0; j < guest_fixups[i].count; j++) {   /* (variables overlap: a byte is swapped once) */
            uint32_t at = guest_fixups[i].address + guest_fixups[i].width * j;
            int fresh = 1, width = guest_fixups[i].width == 2 ? 2 : 4;

            at = guest_fixups[i].address + (unsigned)width * j;
            for (int k = 0; k < width; k++)
                fresh &= !swapped[at + (unsigned)k];
            if (!fresh)
                continue;
            for (int k = 0; k < width; k++)
                swapped[at + (unsigned)k] = 1;
            if (width == 2) {
                swap16_range(at, 2);
            } else {
                uint32_t *p = (uint32_t *)guest_ptr(at);

                swap32_range(at, 4);
                if (guest_fixups[i].width == 5 && *p != 0 && *p < GUEST_LOW_LIMIT)
                    *p += GUEST_LOW_BASE;
            }
        }
    }
    free(swapped);
    if (size >= 0x80000)
        swap16_range(0x6f800, 0x80000 - 0x6f800);        /* the two screens */
    swap_image_table();
    for (int i = 0; i < 16; i++)   /* the colours the chip has: the shadow table, as the original's interrupt keeps them */
        REG16(0x180 + 2 * i) = shadow_table[i];
    swap16_range((uint32_t)(uintptr_t)asset_memory, 0x21d68);   /* the sprites' planes */
    swap16_range((uint32_t)(uintptr_t)memory_base, 0x8500);      /* the background */
    return 0;
}
