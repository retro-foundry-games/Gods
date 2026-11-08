/* Gods, the menu program: the screens, the colours and the copper list that shows them: the screen buffers, the shadow table of the colours, the fades and the
 * frame tick.  The host has its own (sdl/display.c). */
#include "../menu.h"

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
    } while (list < (const uint16_t *)guest.copper_list_end);
    return (const uint16_t *)(intptr_t)-1;
}

/* orig $01D2D8: set a word of the table */
void shadow_set(uint16_t index, uint16_t value)
{
    guest.shadow_table[(int16_t)(index * 2) / 2] = value;
}

/* orig $01D2EA: a word of the table */
uint16_t shadow_get(uint16_t index)
{
    return guest.shadow_table[(int16_t)(index * 2) / 2];
}

/* orig $01CFC0: clear both screens */
void clear_both_screens(void)
{
    clear_screen_buffer((uint32_t *)guest.screen_buffer);
    clear_screen_buffer((uint32_t *)guest.screen_other);
}

/* orig $01CFCE: copy the other screen over the one that is drawn on */
void copy_other_screen(void)
{
    copy_screen_buffer((const uint32_t *)guest.screen_other, (uint32_t *)guest.screen_buffer);
}

/* orig $01D302: show the screen that was drawn and draw on the other: the per-screen state, the two screens and so which
 * copper list; its scroll is set for x_offset and the copper is told to use it, then the next vertical blank is waited for */
void screens_flip(void)
{
    struct Rec6 state = guest.buffer_state[0];
    struct CopperHead *list;
    uint8_t *swap;

    guest.buffer_state[0] = guest.buffer_state[1];
    guest.buffer_state[1] = state;
    swap = guest.screen_buffer;
    guest.screen_buffer = guest.screen_other;
    guest.screen_other = swap;
    list = guest.screen_other == guest.screen_1 ? &guest.copper_lists.a : &guest.copper_lists.b;
    set_scroll(list, (uint8_t)guest.x_offset);
    display_show(list);
    guest.vbl_saturating = 0;
    wait_vblank();
}

/* the value word of the next move to COLOR01 in the copper list from `from` on, or 0 when there is none.  The search goes on
 * at the value word itself, as the original does (a colour of $0182 would be taken for a register) */
static uint16_t *next_color(const uint16_t *from)
{
    const uint16_t *found = find_color01_move(from);

    return (int32_t)(intptr_t)found < 0 ? 0 : (uint16_t *)found;
}

/* orig $01D168: every colour of the copper list a step towards the colour in the buffer copper_save points at: each of the
 * three components that differs goes up by one */
void copper_fade_up(void)
{
    const uint16_t *target = guest.copper_save;

    for (uint16_t *colour = next_color(guest.copper_colors); colour; colour = next_color(colour)) {
        uint16_t want = *target++;
        uint16_t now = *colour;

        if ((want & 0x00f) != (now & 0x00f))
            *colour += 0x001;
        if ((want & 0x0f0) != (now & 0x0f0))
            *colour += 0x010;
        if ((want & 0xf00) != (now & 0xf00))
            *colour += 0x100;
    }
}

/* orig $01D1C2: every colour of the copper list a step towards black: each component that is not 0 goes down by one */
void copper_fade_down(void)
{
    for (uint16_t *colour = next_color(guest.copper_colors); colour; colour = next_color(colour)) {
        uint16_t now = *colour;

        if (now & 0x00f)
            *colour -= 0x001;
        if (now & 0x0f0)
            *colour -= 0x010;
        if (now & 0xf00)
            *colour -= 0x100;
    }
}

/* orig $01D1FE: put the colours of the copper list in the buffer copper_save points at, and `and` them with `mask` (0 makes
 * them black, $FFFF leaves them) */
void copper_colors_save(uint16_t mask)
{
    uint16_t *to = guest.copper_save;

    for (uint16_t *colour = next_color(guest.copper_colors); colour; colour = next_color(colour)) {
        *to++ = *colour;
        *colour &= mask;
    }
}

/* orig $01D216: put the colours back */
void copper_colors_restore(void)
{
    const uint16_t *from = guest.copper_save;

    for (uint16_t *colour = next_color(guest.copper_colors); colour; colour = next_color(colour))
        *colour = *from++;
}

/* orig $01D0C0: fade the colours in from black to the palette `target` (words, each colour a half of what it is on the
 * screen), a step per `delay + 1` frames; the 15 steps change the shadow table, the copper list's colours follow */
void fade_in(const uint16_t *target, uint16_t delay)
{
    copper_colors_save(0);
    if (guest.copper_setup_done == 0) {
        wait_vblank();
        guest.copper_lists.tail.move_a = (struct CopperMove){ 0x0084, 0x0000 };
        guest.copper_lists.tail.move_b = (struct CopperMove){ 0x0086, 0x4e3a };
    }
    for (int step = 0; step < 15; step++) {
        for (int i = 15; i >= 0; i--) {
            uint16_t colour = shadow_get(i);
            uint16_t want = target[i] << 1;

            if ((want & 0x00f) != (colour & 0x00f))
                colour += 0x001;
            if ((want & 0x0f0) != (colour & 0x0f0))
                colour += 0x010;
            if ((want & 0xf00) != (colour & 0xf00))
                colour += 0x100;
            shadow_set(i, colour);
        }
        copper_fade_up();
        guest.palette_dirty = 0xffff;
        for (int frame = 0; frame <= delay; frame++)
            wait_vblank();
    }
    guest.palette_lit = 0xff;
}

/* orig $016C30: the work of every frame (the vertical blank interrupt calls it): the scroll word of the copper lists, the
 * sound, the frame count, and the message ticker (the line of text that scrolls in) */
void frame_tick(void)
{
    uint16_t scroll = 0;

    if (guest.copper_setup_done)
        scroll = guest.shadow_table[1];
    guest.copper_lists.a.color01 = scroll;
    guest.copper_lists.b.color01 = scroll;
    if (guest.sound_enabled)
        sound_tick();
    guest.tick_count++;
    if (guest.jump_boost_timer)
        guest.jump_boost_timer--;
    if (!guest.message_shown)
        return;
    if (guest.tick_hold_38a) {
        shop_message_frame();   /* $16C9E */
        return;
    }
    if (guest.tick_hold_296 || (int16_t)guest.message_state < 0 || guest.tick_hold_26c)
        return;
    if (guest.message_hold <= 1) {   /* the message scrolls in the work area */
        work_area_shift_left();
        message_ticker();
    }
    if (--guest.message_hold < 0)
        guest.message_hold = 0;
}

/* orig $01D23E: fade the colours out to black, 15 steps of `delay + 1` frames; the message (message.c) goes on while it does,
 * and starts over (`arg` is what message_start gets); then both screens are cleared and the copper list's colours are as
 * they were before */
void fade_out(uint16_t delay, uint16_t arg)
{
    (void)delay;
    message_start(arg);
    delay = 1;   /* the original's d0, which is the delay, is the result of the routine that starts the message (orig $016DF8: 1): every step waits two frames */
    guest.message_current = 0;
    copper_colors_save(0xffff);
    for (int step = 0; step < 15; step++) {
        for (int i = 15; i >= 0; i--) {
            uint16_t colour = shadow_get(i);

            if (colour & 0x00f)
                colour -= 0x001;
            if (colour & 0x0f0)
                colour -= 0x010;
            if (colour & 0xf00)
                colour -= 0x100;
            shadow_set(i, colour);
        }
        copper_fade_down();
        guest.palette_dirty = 0xffff;
        for (int frame = 0; frame <= delay; frame++) {
            message_tick();
            wait_vblank();
        }
    }
    if (!fade_keep_screens)
        clear_both_screens();
    wait_vblank();
    guest.copper_lists.tail.move_a = (struct CopperMove){ 0x0084, 0x0000 };
    guest.copper_lists.tail.move_b = (struct CopperMove){ 0x0086, 0x5246 };
    copper_colors_restore();
    guest.palette_lit = 0;
}

/* ---- what the display needs of the chips ---- */

/* orig $004D3E: the fine scroll of a copper list for the pixel offset `x` (only the low byte counts): the scroll nibbles
 * for the two fields of BPLCON1, where the display fetch starts, and the modulos of the bitplane pointers.  x of 16 or more
 * uses the next word. */
void set_scroll(struct CopperHead *list, uint8_t x)
{
    uint8_t fetch = 0x38;
    uint8_t words = 4;
    uint8_t extra = (x & 0x10) ? 2 : 0;
    uint8_t scroll = x & 15;

    if (scroll != 0) {
        uint8_t delay = 16 - scroll;

        scroll = delay | (uint8_t)(delay << 4);
        fetch = 0x30;
        words = 2;
    }
    /* (the low bytes of the values: the 68000 stores the words big endian) */
    ((uint8_t *)&list->bplcon1.value)[1] = scroll;
    ((uint8_t *)&list->ddfstrt.value)[1] = fetch;
    ((uint8_t *)&list->modulo_odd.value)[1] = words;
    ((uint8_t *)&list->modulo_even.value)[1] = words;
    for (int i = 0; i < 4; i++)
        ((uint8_t *)&list->planes[i].low.value)[1] = extra;
}

/* orig $01D05C: wait for the next vertical blank */
__attribute__((noinline)) void wait_vblank(void)
{
    uint16_t seen = guest.vbl_count;

    while (guest.vbl_count == seen)
        ;
}

/* orig $01DFD8: wait until the blitter is done */
void blitter_wait(void)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
}

/* tell the copper to run `list` (the copper list of the screen that is shown) */
void display_show(const void *list_head)
{
    const uint8_t *list = list_head;
    custom->cop1lc = (uint32_t)(uintptr_t)list;
}

/* ---- the blitter's copies (the host does them directly: sdl/display.c) ---- */

/* orig $01E372: the blitter gets priority over the processor until it is done */
void blitter_grab_wait(void)
{
    custom->dmacon = 0x8400;
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->dmacon = 0x0400;
}

/* orig $017082: one column of a tile is copied by four blits of (rows + 1) rows and two words (A to D, no mask): the source
 * moves by src_step and the destination by dst_step bytes between them */
void blit_tile(int16_t src_step, int16_t dst_step, int16_t rows, const uint8_t *src, uint8_t *dst)
{
    uint16_t size = 1026;

    blitter_grab_wait();
    if (rows != 15)
        size = (uint16_t)(((rows + 1) << 6) | 2);
    custom->bltapt = ADDR(src);
    custom->bltdpt = ADDR(dst);
    custom->dmacon = 0x8400;
    blit_start(size);
    dst += dst_step;
    src += src_step;
    for (int i = 0; i < 2; i++) {
        while (custom->dmaconr & DMAF_BLTBUSY)
            ;
        custom->bltapt = ADDR(src);
        custom->bltdpt = ADDR(dst);
        blit_start(size);
        dst += dst_step;
        src += src_step;
    }
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltapt = ADDR(src);
    custom->bltdpt = ADDR(dst);
    custom->dmacon = 0x0400;
    blit_start(size);
}

/* orig $017044: the blitter is set up for plain copies (A to D, full masks, source modulo 0, destination modulo 40); the
 * original also leaves 64 in d4 and $2100 in d5, the steps between the four blits of a tile */
void blitter_setup_copy(void)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltcon0 = 2544;
    custom->bltcon1 = 0;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltamod = 0;
    custom->bltdmod = 40;
}

/* orig $017814 and $01786E..: the whole screen (4 planes of 192 rows, 44 bytes a row) is copied by the blitter from `from` to `to` (the
 * original waits for the blitter first, clears the first/last word masks and the modulos, and the copy is A to D with no shift) */
void copy_screen(const uint8_t *from, uint8_t *to)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt = ADDR(from);
    custom->bltdpt = ADDR(to);
    custom->bltdmod = 0;
    custom->bltamod = 0;
    custom->bltcon1 = 0;
    custom->bltcon0 = 2544;
    custom->dmacon = 0x0400;
    blit_start(0xC016);
}

/* orig $017100 (with the common blit of $017208 written out): an image five words wide (the part of the screen is `dst`, its four planes
 * `plane` bytes apart from `src`, `rows` + 1 rows high, shifted `shift` pixels to the right) is drawn with the mask the blitter
 * makes of its planes in mask_buffer.  blit_extra_modulo is cleared */
void blit_image_5wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    uint16_t size = (uint16_t)((rows << 6) | 5);
    uint16_t dst_mod = 44 - 10;
    uint16_t first_mask = 0xffff, last_mask = 0;
    uint16_t con0 = 0x0fca, con1 = 0;
    uint16_t a_mod = 0, b_mod;
    const uint8_t *s = src;
    uint8_t *mask = (uint8_t *)guest.mask_buffer;
    uint8_t *d = dst, *dd = dst;

    guest.blit_extra_modulo = 0;
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
    custom->bltcon0 = 4094;
    custom->bltamod = 0;
    custom->bltbmod = 0;
    custom->bltcmod = 0;
    blit_start(size);
    /* ... and the plane 3 */
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltbmod = 0;
    custom->bltcon0 = 3580;
    custom->bltapt = ADDR(s);
    custom->bltbpt = ADDR(mask);
    custom->bltdpt = ADDR(mask);
    blit_start(size);
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltafwm = first_mask;
    custom->bltalwm = last_mask;
    if (shift == 0) {
        custom->bltcon0 = con0;
        custom->bltalwm = 0xffff;
        a_mod = 0;
        custom->bltamod = a_mod;
        custom->bltcon1 = 0;
        b_mod = a_mod + guest.blit_extra_modulo;
        custom->bltbmod = b_mod;
    } else {
        while (custom->dmaconr & DMAF_BLTBUSY)
            ;
        con1 = (uint16_t)(shift << 12);
        custom->bltcon1 = con1;
        con0 = con1 | 0x0fca;
        custom->bltcon0 = con0;
        /* blit_extra_modulo is 0 here (not negative): one word wider, the destination modulo less 2 */
        dst_mod -= 2;
        size++;
        a_mod = (uint16_t)-2;
        custom->bltamod = a_mod;
        b_mod = a_mod + 0;
        custom->bltbmod = b_mod;
    }
    custom->bltcmod = dst_mod;
    custom->bltdmod = dst_mod;

    /* the four planes of the image: the mask (A), the plane (B) and the screen (C and D) */
    int16_t stride = (int16_t)guest.blit_plane_stride;
    s = src;
    for (int i = 0; i < 4; i++) {
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

/* orig $01F54E (the mask) and $01F4B6 with $01F52C (the cookie cut; $01E356, the blitter wait, is blitter_grab_wait): the image at `source` (four planes, plane_size bytes each, `rows`
 * rows of `words` words; the blitter's modulo for it is modulo_ab) is drawn to `dest` (four planes 8448 bytes apart, modulo_cd) through a
 * mask that is its four planes or'ed (made in mask_buffer by two blits) */
void blit_masked(const uint8_t *source, uint8_t *dest, int16_t modulo_ab, uint16_t words, int16_t modulo_cd, uint32_t plane_size,
                        uint16_t rows)
{
    uint16_t size = (uint16_t)((rows << 6) | words);

    blitter_grab_wait();
    custom->bltdpt = ADDR(guest.mask_buffer);
    custom->bltapt = ADDR(source);
    custom->bltbpt = ADDR(source + plane_size);
    custom->bltcpt = ADDR(source + 2 * plane_size);
    custom->bltamod = (uint16_t)modulo_ab;
    custom->bltbmod = (uint16_t)modulo_ab;
    custom->bltcmod = (uint16_t)modulo_ab;
    custom->bltdmod = (uint16_t)modulo_ab;
    custom->bltcon1 = 0;
    custom->bltcon0 = 4094;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    blit_start(size);
    blitter_grab_wait();
    custom->bltcpt = ADDR(source + 3 * plane_size);
    custom->bltbpt = ADDR(guest.mask_buffer);
    custom->bltdpt = ADDR(guest.mask_buffer);
    custom->bltcon0 = 2030;
    blit_start(size);

    blitter_grab_wait();
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt = ADDR(guest.mask_buffer);
    custom->bltbpt = ADDR(source);
    custom->bltcpt = ADDR(dest);
    custom->bltdpt = ADDR(dest);
    custom->bltdmod = (uint16_t)modulo_cd;
    custom->bltcmod = (uint16_t)modulo_cd;
    custom->bltamod = (uint16_t)modulo_ab;
    custom->bltbmod = (uint16_t)modulo_ab;
    custom->bltcon1 = 0;
    custom->bltcon0 = 4042;
    blit_start(size);
    for (int plane = 0; plane < 3; plane++) {
        dest += 8448;
        source += plane_size;
        blitter_grab_wait();
        custom->bltbpt = ADDR(source);
        custom->bltapt = ADDR(guest.mask_buffer);
        custom->bltcpt = ADDR(dest);
        custom->bltdpt = ADDR(dest);
        blit_start(size);
    }
}

/* orig $01F0AA: the image (x, y, `width` pixels, `height` + 1 rows, at `source`: the first of its four planes) is clipped by the
 * rectangle `clip` (x0, x1, y0, y1) and drawn on the screen buffer.  Only the way that ran is written: an image that is mirrored
 * (image_mirrored negative), the shift in the bits of x (image_x_shift) and the other draw routines of the table at $0D9A are not */
void blit_clipped(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *source, const int16_t *clip)
{
    int16_t low = clip[2];
    int16_t high = clip[3];
    int16_t rest;
    int16_t modulo_ab;
    int16_t modulo_cd;
    uint16_t words;
    int16_t offset;
    uint32_t product;

    if (y < low)
        return;   /* goes on at $01F0B6 */
    if (y > high)
        return;
    if ((int16_t)(height + y) > high)
        height = (int16_t)(high - y);
    low = clip[0];
    rest = width;
    if (x <= low) {
        rest = (int16_t)(rest + x);
        if (rest < low)
            return;
        rest = (int16_t)(rest - low);
        offset = (int16_t)(((uint16_t)(low - x) & 0xFFF0) >> 3);
        source += offset;
        x = low;
        guest.image_clip_flags = 1;
    }
    high = clip[1];
    if (x > high)
        return;
    if ((int16_t)(rest + x) > high) {
        rest = (int16_t)(high - x);
        guest.image_clip_flags = (uint16_t)(guest.image_clip_flags + 2);
    }
    words = (uint16_t)rest >> 4;
    rest = (int16_t)(words << 3);   /* the original's d5 */
    if (!(guest.image_mirrored & 0x8000))
        return;   /* goes on at $01F13A */
    width = (int16_t)(((uint16_t)width >> 1) & 0xFFF8);
    product = (uint32_t)(uint16_t)(width + 8) * guest.image_clip_skip_rows;
    source += (int16_t)((uint16_t)product >> 2);
    if (guest.image_draw_flags & 0x20)
        return;   /* goes on at $01F186 */
    modulo_ab = (int16_t)(width - rest);
    modulo_cd = (int16_t)(((uint16_t)(guest.screen_row_bytes - 8 - rest)) >> 2);
    x = (int16_t)((uint16_t)x & 0xFFF0);
    x = (int16_t)((uint16_t)x >> 3);
    x = (int16_t)(x + (uint16_t)((uint16_t)y * 44));
    if (guest.image_x_shift != 0)
        return;   /* goes on at $01F1BE */
    if (guest.draw_routine_index != 1 || (guest.image_draw_flags & 4))
        return;   /* goes on at the routine of the table at $0D9A that draw_routine_index and bit 2 of image_draw_flags name */
    blit_masked(source, guest.screen_buffer + x, (int16_t)(modulo_ab >> 2), (uint16_t)(words + 1), modulo_cd, guest.image_plane_bytes, (uint16_t)(height + 1));
}
