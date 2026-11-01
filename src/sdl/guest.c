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
