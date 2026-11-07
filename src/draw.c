/* Gods, the menu program: drawing images and text on the screen.  The screen is 44 bytes a row (352 pixels), 192 rows. */
#include "draw.h"

#ifdef GODS_SDL
extern void (*host_draw_hook)(int kind, int16_t x, int16_t y, uint16_t index);   /* sdl/devmain.c: none in the regular program */
#endif

#define SCREEN_ROW 44
#define SCREEN_LAST_ROW 191

/* the part both the one and the two word wide draw have: where on the screen, which rows of the image, and the shift.
 * Returns the number of rows (0: nothing to draw, the image is off the top or the bottom); `skip_bytes` is how many
 * bytes of image data a row takes */
/* the last row a sprite may reach (the constant in the original's sprite routine at $17438: the level 4 guardian's head sets it to 89 for a call) */
uint16_t sprite_last_row = SCREEN_LAST_ROW;

static int rows_to_draw(int16_t y, const struct Image *img, int skip_bytes, const uint8_t **data, uint8_t **dst, int16_t last_row)
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
        int16_t over = y - (last_row - height);

        if (over > 0) {
            height -= over;
            if (height < 0)
                return 0;
        }
    }
    return height + 1;
}

/* orig $017208 (the common part of the masked blit of $017100-$0171FC, the entries that do not start at its beginning are the
 * functions below): draw an image `words` words wide (`rows` rows, its four planes `plane` bytes apart from `src`, shifted `shift`
 * pixels to the right) to the screen at `dst`.  The mask of the image (its planes ORed) is made in mask_buffer first.
 * `first_mask` and `last_mask` are the blitter's masks of the first and the last word of a row (the last one counts when the
 * shift is not 0).  `extra_modulo` (what the original keeps in blit_extra_modulo): the bytes of a row of the image that are not
 * drawn (2 when only the first word of an image two words wide is, or when the second is, on the screen); negative: the
 * shifted image gets no extra word on the right */
void blit_masked_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows, uint16_t words,
                              uint16_t extra_modulo, uint16_t first_mask, uint16_t last_mask)
{
    uint16_t size = (uint16_t)((rows << 6) | words);
    uint16_t dst_mod = (uint16_t)(44 - words * 2);
    uint16_t con1, a_mod, b_mod, mod;
    uint8_t *mask = (uint8_t *)mask_buffer;
    const uint8_t *s = src;
    uint8_t *d = dst, *dd = dst;
    int16_t stride;
    int i;

    blit_extra_modulo = extra_modulo;
    shift &= 0xff;

    custom->dmacon = 0x8400;
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    /* the mask: the planes 0, 1 and 2 ORed ... */
    custom->bltapt = ADDR(s);
    s += (int16_t)plane;
    custom->bltbpt = ADDR(s);
    s += (int16_t)plane;
    custom->bltcpt = ADDR(s);
    s += (int16_t)plane;
    custom->bltdpt = ADDR(mask);
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltdmod = 0;
    custom->bltcon1 = 0;
    custom->bltcon0 = 0x0ffe;
    mod = (int16_t)extra_modulo < 0 ? 0 : extra_modulo;
    custom->bltamod = mod;
    custom->bltbmod = mod;
    custom->bltcmod = mod;
    blit_start(size);
    /* ... and the plane 3 */
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltbmod = 0;
    custom->bltcon0 = 0x0dfc;
    custom->bltapt = ADDR(s);
    custom->bltbpt = ADDR(mask);
    custom->bltdpt = ADDR(mask);
    blit_start(size);
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltafwm = first_mask;
    custom->bltalwm = last_mask;
    if (shift == 0) {
        custom->bltcon0 = 0x0fca;
        custom->bltalwm = 0xffff;
        a_mod = 0;
        custom->bltamod = a_mod;
        custom->bltcon1 = 0;
        b_mod = (uint16_t)(a_mod + extra_modulo);
        custom->bltbmod = b_mod;
    } else {
        while (custom->dmaconr & DMAF_BLTBUSY)
            ;
        con1 = (uint16_t)(shift << 12);
        custom->bltcon1 = con1;
        custom->bltcon0 = con1 | 0x0fca;
        if ((int16_t)extra_modulo < 0) {
            a_mod = 0;
            b_mod = 0;
        } else {   /* one word wider, the destination modulo less 2 */
            dst_mod -= 2;
            size++;
            a_mod = (uint16_t)-2;
            b_mod = (uint16_t)(a_mod + extra_modulo);
        }
        custom->bltamod = a_mod;
        custom->bltbmod = b_mod;
    }
    custom->bltcmod = dst_mod;
    custom->bltdmod = dst_mod;

    /* the four planes of the image: the plane (B), the mask (A) and the screen (C and D) */
    stride = (int16_t)blit_plane_stride;
    s = src;
    for (i = 0; i < 4; i++) {
        if (i != 0) {
            d += stride;
            dd += stride;
            s += (int16_t)plane;
            while (custom->dmaconr & DMAF_BLTBUSY)
                ;
        }
        custom->bltcpt = ADDR(d);
        custom->bltbpt = ADDR(s);
        custom->bltapt = ADDR(mask);
        custom->bltdpt = ADDR(dd);
        if (i == 3)
            custom->dmacon = 0x0400;
        blit_start(size);
    }
}

/* the first-word mask of a right edge (the word of mask_table at $017380 + 4 * (16 - shift)) and of a left edge (the next word) */
static uint16_t right_edge_mask(uint16_t shift)
{
    return mask_table[(16 - shift) * 2];
}

static uint16_t left_edge_mask(uint16_t shift)
{
    return mask_table[(16 - shift) * 2 + 1];
}

/* orig $017140: an image two words wide that starts 1 to 16 pixels left of the screen (x from -16 to -1, `shift` is x & 15; `dst` is the
 * start of the row): with a shift the image is drawn from two bytes left of the row, the first word masked to the pixels that
 * are on the screen; without (x = -16) only its second word is drawn */
static void blit_image_wide_left(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    if ((shift & 0xff) == 0)
        blit_masked_image(dst, src + 2, plane, shift, rows, 1, 2, 0xffff, 0);
    else
        blit_masked_image(dst - 2, src, plane, shift, rows, 2, 0, left_edge_mask(shift), 0);
}

/* orig $01718C: the same for x from -31 to -17: only the second word of the image is on the screen */
static void blit_image_wide_left_far(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst - 2, src + 2, plane, shift, rows, 1, 2, left_edge_mask(shift), 0);
}

/* orig $017116: an image two words wide at x from 321 to 335 (`shift` is x & 15, not 0): drawn two words wide, no extra word
 * on the right, the last word masked to the pixels that are on the screen */
static void blit_image_wide_right(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst, src, plane, shift, rows, 2, 0xffff, 0xffff, right_edge_mask(shift));
}

/* orig $017162: the same at x from 336 to 351: only the first word of the image is on the screen */
static void blit_image_wide_right_far(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst, src, plane, shift, rows, 1, 2, shift != 0 ? right_edge_mask(shift) : 0xffff, 0);   /* (with no shift the last mask is not used) */
}

/* orig $0171BA: an image one word wide at x from 337 to 351: the first word masked to the pixels that are on the screen */
static void blit_image_right(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst, src, plane, shift, rows, 1, 0, right_edge_mask(shift), 0);
}

/* orig $0171CE: an image one word wide at x from -15 to -1: drawn from two bytes left of the row, the first word masked to the
 * pixels that are on the screen */
static void blit_image_left(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst - 2, src, plane, shift, rows, 1, 0, left_edge_mask(shift), 0);
}

/* orig $017686: draw glyph or image `index` of the table at pixel (x, y), one word wide.  From x = 0 to 336 the blit is the
 * plain one; from 337 to 351 and from -15 to -1 the original has two variants that draw the part that is on the screen */
void glyph_draw(int16_t x, int16_t y, uint16_t index)
{
#ifdef GODS_SDL
    if (host_draw_hook)
        host_draw_hook(0, x, y, index);   /* (the dev build's log of the draws: the lockstep compares them with the original's) */
#endif
    const struct Image *img = &image_table[index];
    const uint8_t *data = (const uint8_t *)img->data;
    uint8_t *dst;
    uint16_t plane;
    int rows;

    x += x_offset;
    if (x <= -16 || x >= 352)
        return;
    dst = screen_buffer + y * SCREEN_ROW;
    if (x >= 0)
        dst += (x & -16) >> 3;
    rows = rows_to_draw(y, img, 2, &data, &dst, SCREEN_LAST_ROW);
    if (rows == 0)
        return;
    plane = glyph_plane_size != 0 ? glyph_plane_size : (img->height + 1) * 2;
    if (x < 0)
        blit_image_left(dst, data, plane, x & 15, rows);
    else if (x > 336)
        blit_image_right(dst, data, plane, x & 15, rows);
    else
        blit_image(dst, data, plane, x & 15, rows);
}

/* orig $0173C4: the same for an image two words wide: plain blit from x = 0 to 320; the variants for x from 321 to 351 and
 * from -31 to -1 draw the part that is on the screen */
/* the last rows of the door's sprite (the image 553 of the level 4) lie in the first bytes of the block at memory_base before the tiles (what the program keeps there is bytes in the original's order) while the port's
 * sprites read native words: the bytes that are there are swapped in a copy of the image's planes */
static const uint8_t *sprite_planes_as_words(const uint8_t *data, unsigned bytes)
{
    static uint8_t copy[4096];
    uintptr_t first = (uintptr_t)memory_base;
    uintptr_t last = (uintptr_t)ptr_0c70 > first && (uintptr_t)ptr_0c70 < first + 0x8500 ? (uintptr_t)ptr_0c70 : first + 0x8500;   /* (the tiles, from ptr_0c70 on, are words of the port's too) */
    uintptr_t from = (uintptr_t)data;
    uintptr_t to = from + bytes;

    if (bytes > sizeof copy || to <= first || from >= last)
        return data;
    for (unsigned i = 0; i < bytes; i++)
        copy[i] = data[i];
    for (uintptr_t a = from & ~(uintptr_t)1; a < to; a += 2) {
        if (a < first || a + 1 >= last || a < from)
            continue;
        copy[a - from] = ((const uint8_t *)a)[1];
        copy[a + 1 - from] = ((const uint8_t *)a)[0];
    }
    return copy;
}

void sprite_draw(int16_t x, int16_t y, uint16_t index)
{
#ifdef GODS_SDL
    if (host_draw_hook)
        host_draw_hook(1, x, y, index);   /* (the dev build's log of the draws: the lockstep compares them with the original's) */
#endif
    const struct Image *img = &image_table[index];
    const uint8_t *data = (const uint8_t *)img->data;
    uint8_t *dst;
    uint16_t plane;
    int rows;

    x += x_offset;
    if (x <= -32 || x >= 352)
        return;
    data = sprite_planes_as_words(data, (unsigned)(img->height + 1) * 16);
    dst = screen_buffer + y * SCREEN_ROW;
    if (x >= 0)
        dst += (x & -16) >> 3;
    rows = rows_to_draw(y, img, 4, &data, &dst, (int16_t)sprite_last_row);
    if (rows == 0)
        return;
    plane = (img->height + 1) * 4;
    if (x < -16)
        blit_image_wide_left_far(dst, data, plane, x & 15, rows);
    else if (x < 0)
        blit_image_wide_left(dst, data, plane, x & 15, rows);
    else if (x > 335)
        blit_image_wide_right_far(dst, data, plane, x & 15, rows);
    else if (x > 320)
        blit_image_wide_right(dst, data, plane, x & 15, rows);
    else
        blit_image_wide(dst, data, plane, x & 15, rows);
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
