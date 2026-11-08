/* Gods SDL port: the guest memory and the chips' registers (the loading of images into it is guest_load.c).
 *
 * The program's memory is one block, struct Gods (game.h): its variables are members at the offsets of the Amiga's addresses, and its end is the heap the program hands out.  The registers of the custom chips
 * and of the CIAs are objects of their own.  Here: the block and the chips, where an address of the original's memory is (tables of data.c, or the block), the reset and the port's own save state. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "guest.h"
#include "guest_tables.h"

struct Gods guest __attribute__((aligned(16)));
/* the Atari ST's video address registers that the program reads as the seed of its random numbers: what the emulated Amiga gives */
volatile uint8_t video_address_high = 0x64;
volatile uint8_t video_address_low = 0xFF;
volatile uint8_t host_custom_raw[0x1000] __attribute__((aligned(4)));
volatile uint8_t host_cia_raw[0x2000];   /* from $BFD000 */

#include "guest_members.h"

/* the variable of the program (the member of struct Gods) that has an address of the original's memory: where it is here, 0 when none has it */
uint8_t *guest_member_ptr(uint32_t address)
{
    unsigned low = 0, high = sizeof guest_members / sizeof guest_members[0];

    while (low < high) {   /* the members are sorted by address and do not overlap */
        unsigned mid = (low + high) / 2;

        if (address < guest_members[mid].address)
            high = mid;
        else if (address >= guest_members[mid].address + guest_members[mid].size)
            low = mid + 1;
        else
            return (uint8_t *)guest_members[mid].dst + (address - guest_members[mid].address);
    }
#ifdef MATCH_MEMORY_LAYOUT
    return (uint8_t *)&guest + address;
#else
    return 0;
#endif
}

/* where an address of the original's memory is here: in a table (the constant data of data.c: it is not in the block) or in the member of struct Gods that has it, at its offset in it; 0 when no object
 * of the port has it (the original's memory that nothing names).  With the layout of the original (MATCH_MEMORY_LAYOUT) the rest of the block is the original's memory too, at its address. */
void *guest_resolve(uint32_t address)
{
    for (unsigned i = 0; i < sizeof guest_tables / sizeof guest_tables[0]; i++)
        if (address >= guest_tables[i].address && address < guest_tables[i].address + guest_tables[i].size)
            return (void *)(guest_tables[i].data + (address - guest_tables[i].address));
    return guest_member_ptr(address);
}

uint8_t *guest_ptr(uint32_t address)
{
    return guest_resolve(address);
}

/* the address of the original's memory that a pointer into struct Gods (or a table) is: the reverse of guest_resolve */
uint32_t guest_offset(const void *p)
{
    const uint8_t *q = p;

    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++)
        if (q >= (const uint8_t *)guest_members[i].dst && q < (const uint8_t *)guest_members[i].dst + guest_members[i].size)
            return guest_members[i].address + (uint32_t)(q - (const uint8_t *)guest_members[i].dst);
    for (unsigned i = 0; i < sizeof guest_tables / sizeof guest_tables[0]; i++)
        if (q >= guest_tables[i].data && q < guest_tables[i].data + guest_tables[i].size)
            return guest_tables[i].address + (uint32_t)(q - guest_tables[i].data);
#ifdef MATCH_MEMORY_LAYOUT
    return (uint32_t)(q - (const uint8_t *)&guest);
#else
    return 0;
#endif
}

/* the buffer the blits make their masks in: the original keeps it in the area of its boot code (a buffer of its own is taken off the start of the heap here) */
#define MASK_BUFFER_SIZE 0x1000

void guest_set_heap(void)
{
    guest.heap_base = (uint32_t)(uintptr_t)(guest.heap + MASK_BUFFER_SIZE);   /* (the program's heap goes on after the mask buffer) */
}

void guest_set_mask_buffer(void)
{
    guest.mask_buffer = (uint16_t *)guest.heap;
}

/* the same as a pointer's value (32 bits) */
uint32_t guest_address(uint32_t address)
{
    return (uint32_t)(uintptr_t)guest_resolve(address);
}

static void dump_members_at_exit(void)
{
    FILE *f = fopen(getenv("GODS_DUMP_MEMBERS"), "wb");

    if (!f)
        return;
    for (unsigned i = 0; i < sizeof guest_members / sizeof guest_members[0]; i++) {
        fwrite(&guest_members[i].address, 4, 1, f);
        fwrite(&guest_members[i].size, 4, 1, f);
        fwrite(guest_members[i].dst, 1, guest_members[i].size, f);
    }
    fclose(f);
}

void guest_init(void)
{
    guest_reset();
    if (getenv("GODS_DUMP_MEMBERS"))
        atexit(dump_members_at_exit);   /* (a test: the members at the end, to compare two layouts) */
}

/* what the loader leaves when a program is loaded over the other: the memory and the chips' registers cleared, the tables in */
void guest_reset(void)
{
    memset(&guest, 0, sizeof guest);
    memset((void *)host_custom_raw, 0, sizeof host_custom_raw);
    memset((void *)host_cia_raw, 0, sizeof host_cia_raw);
    host_cia_raw[0x1001] = 0xff;   /* CIA A port A: no fire button down */
}

/* ---- the port's own save state: struct Gods as it is (and the registers of the custom chips) ----------------------------------------------------------------------------------------------------
 * The file is the memory of the program, which is one block: a header (the magic, the block's size and the address it was at), the block and the chips' registers.  The pointers in it are the block's own
 * addresses: when the block is at another address now (another build) every long that was a pointer into it is moved by the difference (the longs of the block are at even offsets). */
#define STATE_MAGIC 0x32535147u   /* "GQS2" */

int guest_state_write(FILE *f)
{
    uint32_t header[3] = { STATE_MAGIC, (uint32_t)sizeof guest, (uint32_t)(uintptr_t)&guest };

    return fwrite(header, 4, 3, f) == 3 && fwrite(&guest, 1, sizeof guest, f) == sizeof guest && fwrite((const void *)host_custom_raw, 1, sizeof host_custom_raw, f) == sizeof host_custom_raw ? 0 : -1;
}

int guest_state_read(FILE *f)
{
    uint32_t header[3];

    if (fread(header, 4, 3, f) != 3 || header[0] != STATE_MAGIC || header[1] != sizeof guest)
        return -1;

    uint8_t *block = malloc(sizeof guest);
    uint8_t *chips = malloc(sizeof host_custom_raw);
    int ok = block && chips && fread(block, 1, sizeof guest, f) == sizeof guest && fread(chips, 1, sizeof host_custom_raw, f) == sizeof host_custom_raw;

    if (ok) {
        uint32_t was = header[2], now = (uint32_t)(uintptr_t)&guest, delta = now - was;

        if (delta != 0)
            for (uint32_t i = 0; i + 4 <= sizeof guest; i += 2) {
                uint32_t v;

                memcpy(&v, block + i, 4);
                if (v >= was && v < was + sizeof guest) {
                    v += delta;
                    memcpy(block + i, &v, 4);
                    i += 2;
                }
            }
        memcpy(&guest, block, sizeof guest);
        memcpy((void *)host_custom_raw, chips, sizeof host_custom_raw);
    }
    free(block);
    free(chips);
    return ok ? 0 : -1;
}

