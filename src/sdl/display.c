/* Gods SDL port, the display: the screens, the colours and the copper list that shows them: the screen buffers, the shadow table of the colours, the fades and the
 * frame tick.  This is amiga/display.c for the host: the same logic (the colours are still in the copper lists, which render.c reads), the chips' parts are the host's: no copper scroll to set, a vertical blank wait that lets the other threads run, no blitter to wait for. */
#include "menu.h"
#include <sched.h>
#include <string.h>

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
    } while ((int32_t)(uintptr_t)list < (int32_t)GUEST(0x5246));
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

/* orig $01CFC0: clear both screens */
void clear_both_screens(void)
{
    clear_screen_buffer((uint32_t *)screen_buffer);
    clear_screen_buffer((uint32_t *)screen_other);
}

/* orig $01CFCE: copy the other screen over the one that is drawn on */
void copy_other_screen(void)
{
    copy_screen_buffer((const uint32_t *)screen_other, (uint32_t *)screen_buffer);
}

/* orig $01D302: show the screen that was drawn and draw on the other: the per-screen state, the two screens and so which
 * copper list; its scroll is set for x_offset and the copper is told to use it, then the next vertical blank is waited for */
void screens_flip(void)
{
    struct Rec6 state = buffer_state[0];
    uint8_t *list;
    uint8_t *swap;

    buffer_state[0] = buffer_state[1];
    buffer_state[1] = state;
    swap = screen_buffer;
    screen_buffer = screen_other;
    screen_other = swap;
    list = screen_other == screen_1 ? copper_list_a : copper_list_b;
    set_scroll(list, (uint8_t)x_offset);
    display_show(list);
    vbl_saturating = 0;
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
    const uint16_t *target = copper_save;

    for (uint16_t *colour = next_color(copper_colors); colour; colour = next_color(colour)) {
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
    for (uint16_t *colour = next_color(copper_colors); colour; colour = next_color(colour)) {
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
    uint16_t *to = copper_save;

    for (uint16_t *colour = next_color(copper_colors); colour; colour = next_color(colour)) {
        *to++ = *colour;
        *colour &= mask;
    }
}

/* orig $01D216: put the colours back */
void copper_colors_restore(void)
{
    const uint16_t *from = copper_save;

    for (uint16_t *colour = next_color(copper_colors); colour; colour = next_color(colour))
        *colour = *from++;
}

/* orig $01D0C0: fade the colours in from black to the palette `target` (words, each colour a half of what it is on the
 * screen), a step per `delay + 1` frames; the 15 steps change the shadow table, the copper list's colours follow */
void fade_in(const uint16_t *target, uint16_t delay)
{
    copper_colors_save(0);
    if (copper_setup_done == 0) {
        wait_vblank();
        copper_move_a = 0x00840000;
        copper_move_b = 0x00864e3a;
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
        palette_dirty = 0xffff;
        for (int frame = 0; frame <= delay; frame++)
            wait_vblank();
    }
    palette_lit = 0xff;
}

/* orig $016C30: the work of every frame (the vertical blank interrupt calls it): the scroll word of the copper lists, the
 * sound, the frame count, and the message ticker (the line of text that scrolls in) */
void frame_tick(void)
{
    uint16_t scroll = 0;

    if (copper_setup_done)
        scroll = scroll_value;
    copper_a_scroll = scroll;
    copper_b_scroll = scroll;
    if (sound_enabled)
        sound_tick();
    tick_count++;
    if (countdown_220)
        countdown_220--;
    if (!message_shown)
        return;
    if (tick_hold_38a)
        return;   /* goes on at $16C9E, which did not run */
    if (tick_hold_296 || (int16_t)message_state < 0 || tick_hold_26c)
        return;
    if (message_hold <= 1) {   /* the message scrolls in the work area */
        work_area_shift_left();
        message_ticker();
    }
    if (--message_hold < 0)
        message_hold = 0;
}

/* orig $01D23E: fade the colours out to black, 15 steps of `delay + 1` frames; the message (message.c) goes on while it does,
 * and starts over (`arg` is what message_start gets); then both screens are cleared and the copper list's colours are as
 * they were before */
void fade_out(uint16_t delay, uint16_t arg)
{
    message_start(arg);
    message_current = 0;
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
        palette_dirty = 0xffff;
        for (int frame = 0; frame <= delay; frame++) {
            message_tick();
            wait_vblank();
        }
    }
    clear_both_screens();
    wait_vblank();
    copper_move_a = 0x00840000;
    copper_move_b = 0x00865246;
    copper_colors_restore();
    palette_lit = 0;
}

/* ---- what the display needs of the chips (the host's) ---- */

/* orig $004D3E: the fine scroll of a copper list: the display (render.c) takes it from x_offset, there is no list to change */
void set_scroll(uint8_t *copper_list, uint8_t x)
{
    (void)copper_list;
    (void)x;
}

/* orig $01D05C: wait for the next vertical blank (the main thread raises it) */
void wait_vblank(void)
{
    uint16_t seen = vbl_count;

    while (vbl_count == seen)
        sched_yield();
}

/* orig $01DFD8: wait until the blitter is done (the host's blitter runs when it is started) */
void blitter_wait(void)
{
}

/* the copper list of the screen shown: COP1LC (render.c reads it) */
void display_show(const uint8_t *list)
{
    custom->cop1lc = (uint32_t)(uintptr_t)list;
}

/* ---- the blitter's copies, done directly ---- */

/* orig $01E372: (the blitter grabs the bus: nothing to wait for) */
void blitter_grab_wait(void)
{
}

/* orig $017044: the blitter is set up for plain copies: blit_tile and copy_screen below copy directly */
void blitter_setup_copy(void)
{
}

/* orig $017082: one column of a tile: four copies of (rows + 1) rows of 4 bytes (two words) from consecutive source bytes to the screen's rows (44 bytes
 * apart); the source moves by src_step and the destination by dst_step bytes between them */
void blit_tile(int16_t src_step, int16_t dst_step, int16_t rows, const uint8_t *src, uint8_t *dst)
{
    for (int pass = 0; pass < 4; pass++) {
        for (int row = 0; row <= rows; row++)
            memcpy(dst + row * 44, src + row * 4, 4);
        dst += dst_step;
        src += src_step;
    }
}

/* orig $017814: the whole screen (4 planes of 192 rows of 44 bytes) is copied */
void copy_screen(const uint8_t *from, uint8_t *to)
{
    memcpy(to, from, 4 * 192 * 44);
}

/* orig $017100: an image five words wide, drawn with the mask of its planes (the host does it as blit_image_wide does: sdl/blitter.c) */
extern void blit_masked_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows, uint16_t words,
                              uint16_t extra_modulo, uint16_t first_mask, uint16_t last_mask);

void blit_image_5wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_masked_image(dst, src, plane, shift, rows, 5, 0, 0xffff, 0);
}
