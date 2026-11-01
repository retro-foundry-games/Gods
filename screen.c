/* Gods, the menu program: screen buffers, the colour shadow table, the copper list and the blitter */
#include "screen.h"

/* orig $01CFAC: clear a screen buffer, 8448 longs */
void clear_screen_buffer(uint32_t *buffer)
{
    int i;

    for (i = 0; i < 0x2100; i++)
        *buffer++ = 0;
}

/* orig $01CFE4: copy a screen buffer, 8448 longs */
void copy_screen_buffer(const uint32_t *from, uint32_t *to)
{
    int i;

    for (i = 0; i < 0x2100; i++)
        *to++ = *from++;
}

/* orig $01D22A: in a copper list, the word after the first one that is the address of COLOR01 ($0182), or -1 if there
 * is none below $5246 (the list is searched from `list` on, a word at a time) */
const uint16_t *find_color01_move(const uint16_t *list)
{
    do {
        if (*list++ == 0x0182)
            return list;
    } while ((int32_t)(uintptr_t)list < 0x5246);
    return (const uint16_t *)(intptr_t)-1;
}

/* orig $01D2D8: set a word of the table */
void shadow_set(uint16_t index, uint16_t value)
{
    shadow_table[(int16_t)(index * 2) / 2] = value;
}

/* orig $01D2EA: a word of the table */
uint16_t shadow_get(uint16_t index)
{
    return shadow_table[(int16_t)(index * 2) / 2];
}

/* orig $01DFD8: wait until the blitter is done */
void blitter_wait(void)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
}
