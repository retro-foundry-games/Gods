/* Gods, the menu program: drawing images and text on the screen.  The screen is 44 bytes a row (352 pixels), 192 rows. */
#include "draw.h"

#define SCREEN_ROW 44
#define SCREEN_LAST_ROW 191

/* the part both the one and the two word wide draw have: where on the screen, which rows of the image, and the shift.
 * Returns the number of rows (0: nothing to draw, the image is off the top or the bottom); `skip_bytes` is how many
 * bytes of image data a row takes */
static int rows_to_draw(int16_t y, const struct Image *img, int skip_bytes, const uint8_t **data, uint8_t **dst)
{
    int16_t height = img->height;

    if (y < 0) {   /* the top rows are off the screen */
        int16_t above = -y;

        height -= above;
        if (height < 0)
            return 0;
        *data += above * skip_bytes;
        *dst += above * SCREEN_ROW;
    } else {       /* the bottom rows may be */
        int16_t over = y - (SCREEN_LAST_ROW - height);

        if (over > 0) {
            height -= over;
            if (height < 0)
                return 0;
        }
    }
    return height + 1;
}

/* orig $017686: draw glyph or image `index` of the table at pixel (x, y), one word wide.  The part of the screen from
 * x = 0 to 336 is decompiled; the original has three more variants of the blit for x from -31 to -1 and from 337 to 351,
 * which the recording does not use (not decompiled yet: nothing is drawn there) */
void glyph_draw(int16_t x, int16_t y, uint16_t index)
{
    const struct Image *img = &image_table[index];
    const uint8_t *data = (const uint8_t *)img->data;
    uint8_t *dst;
    uint16_t plane;
    int rows;

    x += x_offset;
    if (x < 0 || x > 336)
        return;
    dst = screen_buffer + ((x & -16) >> 3) + y * SCREEN_ROW;
    rows = rows_to_draw(y, img, 2, &data, &dst);
    if (rows == 0)
        return;
    plane = glyph_plane_size != 0 ? glyph_plane_size : (img->height + 1) * 2;
    blit_image(dst, data, plane, x & 15, rows);
}

/* orig $0173C4: the same for an image two words wide (x from 0 to 320; the variants for the edges are not decompiled) */
void sprite_draw(int16_t x, int16_t y, uint16_t index)
{
    const struct Image *img = &image_table[index];
    const uint8_t *data = (const uint8_t *)img->data;
    uint8_t *dst;
    int rows;

    x += x_offset;
    if (x < 0 || x > 320)
        return;
    dst = screen_buffer + ((x & -16) >> 3) + y * SCREEN_ROW;
    rows = rows_to_draw(y, img, 4, &data, &dst);
    if (rows == 0)
        return;
    blit_image_wide(dst, data, (img->height + 1) * 4, x & 15, rows);
}

/* orig $003BF8: draw `text` in the large font, 16 pixels a character, from pixel column x; the glyph of a character is its
 * code plus 63 */
void text_draw_at(const char *text, int16_t x, int16_t y)
{
    for (;;) {
        int16_t ch = (int8_t)*text++;

        if (ch == 0)
            return;
        if (ch != ' ')
            glyph_draw(x, y, ch + 63);
        x += 16;
    }
}

/* orig $012D90: draw `text` in the large font, centred on the screen, at the row text_y */
void text_draw_centered(const char *text)
{
    uint16_t width = (uint16_t)-8;   /* the original starts at -8 and adds 8 for every byte, the terminator included */
    const char *p = text;

    do {
        width += 8;
    } while (*p++ != 0);
    text_draw_at(text, 160 - width, text_y);
}

/* orig $006A3A: one character of the small font: the glyph of the index, then on 10 pixels; returns the new x */
int16_t draw_digit(int16_t x, int16_t y, uint16_t index)
{
    glyph_draw(x, y, index + glyph_base);
    return x + 10;
}

/* orig $0069E8: draw `text` in the small font, 8 pixels a character, the glyph of a character its code plus glyph_base */
void draw_string_small(const char *text, int16_t x, int16_t y)
{
    for (;;) {
        uint8_t ch = (uint8_t)*text++;

        if (ch == 0)
            return;
        glyph_draw(x, y, ch + glyph_base);
        x += 8;
    }
}

/* orig $006A1C: the four bytes as eight hexadecimal digits in the small font (the glyphs of the digits are 0 to 15);
 * returns the x after them */
int16_t draw_hex_bytes(const uint8_t *bytes, int16_t x, int16_t y)
{
    int i;

    for (i = 0; i < 4; i++) {
        uint16_t byte = *bytes++;

        x = draw_digit(x, y, byte >> 4);
        x = draw_digit(x, y, byte & 15);
    }
    return x;
}

/* orig $006A52: draw 16 characters of `text` in the small font with the program's own numbering (it does not stop at a 0:
 * 0 and 28 are spaces): 1 to 26 are the letters (the glyph is 64 plus the code, as in ASCII), 27 a full stop, above 28 the
 * code less 29 (the digits, which come as 29 and up); returns the x after them */
int16_t draw_string_16(const uint8_t *text, int16_t x, int16_t y)
{
    int i;

    for (i = 0; i < 16; i++) {
        uint16_t ch = *text++;
        uint16_t glyph;

        if (ch == 0 || ch == 28)
            glyph = 32;
        else if (ch < 27)
            glyph = ch + 64;
        else if (ch == 27)
            glyph = 46;
        else
            glyph = ch - 29;
        x = draw_digit(x, y, glyph);
    }
    return x;
}
