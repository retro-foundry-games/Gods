/* Gods SDL port: the guest memory and the chips' register pages.
 *
 * The program keeps its variables at the Amiga's addresses (the symbols of guest.ld, made by gen_guest.py).  Here that memory is
 * mapped at the same addresses (the first 64 KiB are GUEST_LOW_BASE higher: Linux does not let a program map them), the pages
 * of the custom chips and the CIAs are mapped, and the memory the loader brings is made: the tables of data.c are copied
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

/* the words and longs of the program's variables (guest_fixups.h, from menu.symbols' types) are made native: they are the image's, big endian, what the loader leaves in memory */
static void apply_fixups(long size)
{
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
}

void guest_load_image(const uint8_t *image, uint32_t size)
{
    uint32_t *pairs = (uint32_t *)(uintptr_t)message_pairs;   /* (the table list declares it as bytes) */

    for (uint32_t i = 0; i < size; i++)
        if (!in_table(IMAGE_BASE + i))
            *guest_ptr(IMAGE_BASE + i) = image[i];

    /* the variables the image has: their words and longs are made native (what the types of menu.symbols say) */
    apply_fixups(IMAGE_BASE + size);
    swap16_range(0x4d8a, 0xa8);  /* the two copper lists (the program changes their words, the display reads them as words) */

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
 * The image is the original's chip RAM (big endian); the tables of data.c stay as they are, the rest is copied in and then the words
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

/* what load_pattern_data (the loader of the creatures' paths) makes native when the files are read: the hundred longs of the list entry_list points at (an address and a size
 * for each unit, ending with an address that is negative) and, for each unit of six bytes or more, the first two words of its path (three when the first is $2345); the
 * rest of a path is bytes */
static void swap_entry_list(void)
{
    uint32_t list = *(uint32_t *)guest_ptr(0xcae);   /* (entry_list, made native by the fixups) */

    if (list < 0x10000 || list >= 0x200000)
        return;
    swap32_range(list, 100 * 4);
    for (const uint32_t *entry = (const uint32_t *)guest_ptr(list); (int32_t)entry[0] >= 0 && entry < (const uint32_t *)guest_ptr(list) + 100; entry += 2) {
        uint32_t path = entry[0];

        if (entry[1] < 6 || path < 0x10000 || path >= 0x200000)
            continue;
        swap16_range(path, 4);   /* (two words: swap16_range counts bytes) */
        if (*(uint16_t *)guest_ptr(path) == 0x2345)
            swap16_range(path + 4, 2);   /* ... and the third */
    }
}

extern void level_tables_quickload(void);

volatile int host_irq_hold;   /* set while the memory of the program is loaded from an image (main.c does not run the vertical blank interrupt then) */

static int load_ram_image_held(const char *path, int slots);

static int load_ram_image(const char *path, int slots)
{
    int result;

    host_irq_hold = 1;
    result = load_ram_image_held(path, slots);
    host_irq_hold = 0;
    return result;
}

static int load_ram_image_held(const char *path, int slots)
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
        if (!in_table(i) || (slots && i >= 0x14988 && i < 0x14988 + 880))   /* (object_slot_image is the table of what the slots start as; the slots themselves, the weapons, are there) */
            *guest_ptr(i) = image[i];
    if (slots && size >= 0x16bb4 + 24) {   /* block_16bb4: the words of how the shots move, which the power-ups change (a table of the host's own, so the loop above skipped it): the original's, as words of the host */
        uint16_t *block = (uint16_t *)guest_ptr(0x16bb4);

        for (int i = 0; i < 12; i++)
            block[i] = (uint16_t)((image[0x16bb4 + 2 * i] << 8) | image[0x16bb4 + 2 * i + 1]);
    }
    free(image);
    apply_fixups(size);
    if (size >= 0x80000)
        swap16_range(0x6f800, 0x80000 - 0x6f800);        /* the two screens */
    swap_image_table();
    for (int i = 0; i < 16; i++)   /* the colours the chip has: the shadow table, as the original's interrupt keeps them */
        REG16(0x180 + 2 * i) = shadow_table[i];
    {   /* the sprites' planes: as far as the level's assets go (the level 4's reach past the 0x21D68 bytes of the first levels) */
        uint32_t first = (uint32_t)(uintptr_t)asset_memory;
        uint32_t end = *(uint32_t *)guest_ptr(0xd3e);   /* asset_mem_end */
        uint32_t bytes = end > first && end - first >= 0x21d68 && end - first < 0x40000 ? end - first : 0x21d68;

        swap16_range(first, bytes);
    }
    swap16_range((uint32_t)(uintptr_t)memory_base, 0x8500);      /* the background */
    swap16_range(0x4d8a, 0xa8);   /* the two copper lists (as guest_load_image does: the program changes their words, the display reads them as words) */
    swap16_range(0x4e3c, 0x530a - 0x4e3c);   /* (the first word, copper_colors, has its fixup) the colour bands of the level's copper list (copper_list_make) and the list at $5246 that the two lists go on at, up to its end ($FFFFFFFE) */
    if (slots) {
        swap_entry_list();
        level_tables_quickload();   /* game.c: the level's tables whose words the program makes native when it loads the level */
        {
            extern void sound_restore_quick(void);

            sound_restore_quick();   /* sound.c: the voices the original has going */
        }
    }
    return 0;
}

int guest_load_level_image(const char *path)
{
    return load_ram_image(path, 0);
}

/* the quickload: the chip RAM of the original at some moment of a level (gods_ramdump.lua writes it from a saved state), the weapons and everything else the game keeps in it */
int host_quick_state_load(const char *path) __attribute__((weak));   /* dev_game.c: a quicksave of the port's own memory */

int guest_load_quick(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t magic = 0;

    if (f) {   /* "GQS1": the port's own quicksave (F5, the lockstep's F5): its memory as it was, not the original's RAM */
        int own = fread(&magic, 4, 1, f) == 1 && magic == 0x31535147u;

        fclose(f);
        if (own && host_quick_state_load)
            return host_quick_state_load(path);
    }
    return load_ram_image(path, 1);
}
