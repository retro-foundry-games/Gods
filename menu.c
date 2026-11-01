/* Gods, the menu program: what is not placed in a module of its own yet.  The modules are disk.c, alloc.c, decrunch.c,
 * input.c and screen.c (menu.h includes their headers). */
#include "menu.h"

/* orig $00F518: clear the 80 longs the work area pointer points at */
void clear_work_area(void)
{
    uint32_t *p = work_area;
    int i;

    for (i = 0; i < 80; i++)
        *p++ = 0;
}

/* orig $01CF26: the request that was waiting, or 0; it is taken, so it is gone afterwards */
uint32_t take_pending_request(void)
{
    uint32_t request = pending_request;

    if (request != 0)
        pending_request = 0;
    return request;
}

/* orig $0053EC and $016D74: nothing; two places of the program where an empty routine is called */
void do_nothing(void)
{
}

/* orig $017CAC: mark the hundred entries of the table as empty */
void reset_marker_table(void)
{
    int i;

    for (i = 0; i < 100; i++)
        marker_table[i] = 0xffffffff;
}

/* orig $00F474: copy 200 records of 6 bytes into the record table */
void copy_record_table(const struct Rec6 *from)
{
    int i;

    for (i = 0; i < 200; i++)
        record_table[i] = from[i];
}

/* orig $017DF8: clear the 50 entries of the entry table */
void clear_entry_table(void)
{
    int i;

    for (i = 0; i < 50; i++) {
        entry_table[i].w[0] = 0;
        entry_table[i].w[1] = 0;
        entry_table[i].w[2] = 0;
    }
}

/* orig $00F382: make bytes of 506 words: the low byte of each */
void narrow_words_to_bytes(void)
{
    const uint16_t *from = words_source;
    uint8_t *to = bytes_dest;
    int i;

    for (i = 0; i < 506; i++)
        *to++ = (uint8_t)*from++;
}

/* orig $00F3C0: give the entries of the list their addresses, one after the other, from 400 bytes after the start of the
 * list on, each as far on as the size of the one before; the first with a negative first long ends the list */
void assign_addresses(void)
{
    struct Entry8 *entry = entry_list;
    uint32_t next = (uint32_t)(uintptr_t)entry + 400;

    for (;;) {
        uint32_t first = entry->address;
        uint32_t size = entry->size;

        if ((int32_t)first < 0)
            return;
        entry->address = next;
        next += size;
        entry++;
    }
}

/* orig $00F488: fill the 2400-byte buffer: first clear it, then copy the bytes in */
void load_buffer_2400(const uint8_t *from)
{
    uint8_t *to = buffer_cb6;
    int i;

    for (i = 0; i < 2400; i++)
        *to++ = 0;
    to = buffer_cb6;
    for (i = 0; i < 2400; i++)
        *to++ = *from++;
}

/* orig $0029B4: take the row of param_table that param_index names (the first is 1) into param_a to param_d */
void select_params(void)
{
    int16_t row = (int16_t)(param_index - 1);
    const uint16_t *at = param_table;

    at += (int16_t)(row * 4) / 2;
    at += (int16_t)(row * 8) / 2;
    param_a = ((uint32_t)at[0] << 16) | at[1];
    param_b = ((uint32_t)at[2] << 16) | at[3];
    param_c = at[4];
    param_d = at[5];
}

/* orig $004C90: build the 132 entries of the table: the first word counts up from 170, the third is the signed byte from
 * the table of offsets */
void build_table_14cf8(void)
{
    uint16_t count = 170;
    int i;

    for (i = 0; i < 132; i++) {
        table_14cf8[i].w[0] = count;
        table_14cf8[i].w[2] = (uint16_t)(int16_t)(int8_t)offsets_4cba[i];
        count++;
    }
}

/* orig $01494C: put away two words of each of the eleven slots */
void save_slot_words(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        saved_slot_words[i * 2] = object_slots[i].value_c;
        saved_slot_words[i * 2 + 1] = object_slots[i].value_1e;
    }
}

/* orig $01496A: and put them back */
void restore_slot_words(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        object_slots[i].value_c = saved_slot_words[i * 2];
        object_slots[i].value_1e = saved_slot_words[i * 2 + 1];
    }
}
