/* Gods, the menu program: the disk layer: MFM data as the disk drive reads and writes it, the track buffer, sector copies */
#include "disk.h"

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

/* orig $01DE42: clear the first long of the eleven slots after the first one, $440 bytes apart, of a track buffer */
void clear_track_slots(uint8_t *track)
{
    int slot;

    for (slot = 1; slot <= 11; slot++)
        *(uint32_t *)(track + slot * 0x440) = 0;
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
