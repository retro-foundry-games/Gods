/* Gods: the menu program as C.  See menu.h.  Every function says where the original has it. */
#include "menu.h"

/* ---- memory and screens ------------------------------------------------------------------------------------------ */

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

/* orig $01D4E4: copy `sectors` blocks of 512 bytes.  The count is multiplied by 128 as a 16-bit number, so 0 copies 65536
 * longs, as the original does */
void copy_sectors(const uint32_t *from, uint32_t *to, uint16_t sectors)
{
    uint16_t n = (uint16_t)(sectors << 7) - 1;

    do {
        *to++ = *from++;
    } while (n-- != 0);
}

/* ---- disk: MFM and the track buffer -------------------------------------------------------------------------------- */

/* orig $01DE42: clear the first long of the eleven slots after the first one, $440 bytes apart, of a track buffer */
void clear_track_slots(uint8_t *track)
{
    int slot;

    for (slot = 1; slot <= 11; slot++)
        *(uint32_t *)(track + slot * 0x440) = 0;
}

/* orig $01DE76: one long of data from two MFM longs, odd bits first, then the even ones */
uint32_t mfm_decode_long(const uint32_t *odd_even)
{
    uint32_t odd = odd_even[0] & 0x55555555;
    uint32_t even = odd_even[1] & 0x55555555;

    return (odd << 1) | even;
}

/* orig $01DE9C: the checksum of the ten longs from offset 8 of a sector: their exclusive or, odd bits only */
uint32_t mfm_checksum(const uint8_t *track)
{
    const uint32_t *p = (const uint32_t *)(track + 8);
    uint32_t sum = 0;
    int i;

    for (i = 0; i < 10; i++)
        sum ^= *p++;
    return sum & 0x55555555;
}

/* orig $01E03A: the MFM form of the odd or even bits (already shifted into place) of a long: the data bits and the clock
 * bits between them, where both neighbours are zero; the top clock bit depends on the last data bit before it */
uint32_t *mfm_encode_long(uint32_t value, uint32_t *to)
{
    uint32_t data = value & 0x55555555;
    uint32_t zeros = data ^ 0x55555555;
    uint32_t after = zeros << 1;
    uint32_t before = (zeros >> 1) | 0x80000000;
    uint32_t mfm = data | (before & after);

    if (((const uint8_t *)to)[-1] & 1)
        mfm &= ~0x80000000;
    *to++ = mfm;
    return to;
}

/* orig $01E260: unpack: blocks of a 16-bit length (little endian) and what the block says, up to a block of length 0;
 * a byte below $80 is that many bytes to copy, a byte of $80 to $FE a copy from the output (that length, then a 16-bit
 * offset from the start of the output), and $FF a copy with a 16-bit length.  Returns where the output ends.  (The counts
 * are 16-bit as in the original: 0 means 65536.) */
uint8_t *unpack(uint8_t *to, const uint8_t *from)
{
    uint8_t *start = to;

    for (;;) {
        uint32_t length = from[0] | (from[1] << 8);
        const uint8_t *end;

        from += 2;
        if ((uint16_t)length == 0)
            return to;
        end = from + length;
        while (end > from) {
            uint16_t count = *from++;

            if ((int8_t)count >= 0) {   /* that many bytes follow */
                uint16_t n = count - 1;

                do {
                    *to++ = *from++;
                } while (n-- != 0);
            } else {
                const uint8_t *copy;
                uint16_t n;

                if (count == 0xff) {   /* the length is a word */
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
}

/* ---- the chips ---------------------------------------------------------------------------------------------------- */

/* orig $01DFD8: wait until the blitter is done */
void blitter_wait(void)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
}

/* how far the mouse moved: the old and the new counter (8 bits, they wrap) give the step, taken from the smaller of the two
 * ways round; added to the position, which stays between 0 and `limit` */
static int16_t move_pointer(int16_t position, uint8_t old, uint8_t now, int16_t limit)
{
    uint8_t step = old - now;
    int borrow = old < now;
    int negative = (step & 0x80) != 0;

    if (borrow ? !negative : negative)
        step += 0xff;
    position = position - (int8_t)step;
    if (position < 0)
        position = 0;
    if (position > limit)
        position = limit;
    return position;
}

/* orig $01E39C: read the fire buttons, the joystick, the mouse counters and the pot lines: the button bits, the joystick
 * byte and the pointer position */
void input_poll(void)
{
    uint8_t pra;
    uint8_t joy = 0;
    uint16_t stick;
    int16_t position;

    button_bits[0] = 0;
    button_bits[1] = 0;
    pra = CIAA_PRA;
    if (!(pra & 0x80)) {
        joy |= 0x80;
        button_bits[0] |= 1;
        button_presses |= 1;
    }
    if (!(pra & 0x40)) {
        button_bits[0] |= 2;
        button_presses |= 4;
    }
    stick = custom->joy1dat;
    if (stick & 0x0200) {   /* left */
        joy |= 4;
        stick ^= 0x0100;
    }
    if (stick & 0x0002) {   /* right */
        joy |= 8;
        stick ^= 0x0001;
    }
    if (stick & 0x0100)   /* up */
        joy |= 1;
    if (stick & 0x0001)   /* down */
        joy |= 2;
    joystick = joy;

    mouse_counter = custom->joy0dat;
    position = move_pointer(mouse_y, mouse_counter_old >> 8, mouse_counter >> 8, 0xc7);
    mouse_y = position;
    mouse_y_copy = position;
    position = move_pointer(mouse_x, mouse_counter_old & 0xff, mouse_counter & 0xff, 0x200);
    mouse_x = position;
    mouse_x_copy = position;
    mouse_counter_old = mouse_counter;

    if (custom->pot0dat & 0xfc00) {
        button_bits[0] |= 1;
        button_presses |= 4;
        joystick |= 0x80;
    }
    custom->potgo = 0xffff;
}
