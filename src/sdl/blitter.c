/* Gods SDL port: the blits.  The chip is not emulated: the program's routine that draws a masked image with the blitter
 * (blit_masked_image of draw.c, orig $017208) is done here directly, and so are the two entries of the asm that call it
 * (blit_image, blit_image_wide, orig $0171E4 and $0171F6).  The blit that follows BLTSIZE (sdl_blit_run) does nothing: the routines of the
 * game proper (game_b.c, game_d.c) that program the chip are not used by the menu.
 *
 * The picture: an image `words` words wide of four planes (`plane` bytes apart, a row `words * 2 + extra_modulo` bytes), its mask the planes
 * ORed; both shifted `shift` pixels to the right (one word wider when extra_modulo is not negative), the first and the last word of a
 * row cut by first_mask and last_mask; the screen's rows are 44 bytes and its planes blit_plane_stride apart.  The words are native. */
#include <stdint.h>
#include <string.h>

#include "guest.h"

extern uint8_t *mask_buffer;
extern uint16_t blit_extra_modulo;
extern uint32_t blit_plane_stride;

void sdl_blit_run(void)
{
}

/* the mask word j of row r (words wide): the four planes ORed; the word after the last one is the first of the next row */
static uint16_t mask_word(const uint8_t *src, unsigned plane, unsigned stride, unsigned words, unsigned rows, unsigned r, unsigned j)
{
    uint16_t mask = 0;

    if (j >= words) {
        j -= words;
        r++;
    }
    if (r >= rows)
        return 0;
    for (unsigned i = 0; i < 4; i++)
        mask |= *(const uint16_t *)(src + i * plane + r * stride + 2 * j);
    return mask;
}

/* orig $017208 (draw.c): the arguments are the program's */
void blit_masked_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows, uint16_t words,
                       uint16_t extra_modulo, uint16_t first_mask, uint16_t last_mask)
{
    int extra = (int16_t)extra_modulo < 0 ? 0 : (int16_t)extra_modulo;
    unsigned stride = words * 2u + (unsigned)extra;
    unsigned out_words = words + ((shift & 0xff) != 0 && (int16_t)extra_modulo >= 0 ? 1 : 0);
    unsigned count = rows;
    unsigned shift_bits = shift & 0xff;
    int16_t plane_stride = (int16_t)blit_plane_stride;

    blit_extra_modulo = extra_modulo;
    if (shift_bits == 0)
        last_mask = 0xffff;
    for (unsigned i = 0; i < 4; i++) {
        for (unsigned r = 0; r < count; r++) {
            uint16_t *out = (uint16_t *)(dst + i * plane_stride + r * 44u);
            uint32_t previous_a = 0, previous_b = 0;

            for (unsigned j = 0; j < out_words; j++) {
                uint16_t a = mask_word(src, plane, stride, words, count, r, j);
                uint16_t b = *(const uint16_t *)(src + i * plane + r * stride + 2 * j);
                uint16_t keep = 0xffff, a_shifted, b_shifted;

                if (j == 0)
                    keep &= first_mask;
                if (j == out_words - 1)
                    keep &= last_mask;
                a &= keep;
                a_shifted = (uint16_t)(((previous_a << 16 | a) >> shift_bits) & 0xffff);
                b_shifted = (uint16_t)(((previous_b << 16 | b) >> shift_bits) & 0xffff);
                previous_a = a;
                previous_b = b;
                out[j] = (uint16_t)((a_shifted & b_shifted) | (~a_shifted & out[j]));
            }
        }
    }
}

/* orig $0171E4: draw an image one word wide */
void blit_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst, src, plane, shift, rows, 1, 0, 0xffff, 0);
}

/* orig $0171F6: ... and two words wide */
void blit_image_wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst, src, plane, shift, rows, 2, 0, 0xffff, 0);
}
