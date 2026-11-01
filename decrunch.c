/* Gods, the menu program: the unpacker for the program's packed files */
#include "decrunch.h"

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
