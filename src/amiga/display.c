/* Gods, the menu program: showing the screens and the colours */
#include "../menu.h"
#include "../screen.h"

/* orig $004D3E: the fine scroll of a copper list for the pixel offset `x` (only the low byte counts): the scroll nibbles
 * for the two fields of BPLCON1, where the display fetch starts, and the modulos of the bitplane pointers.  x of 16 or more
 * uses the next word. */
void set_scroll(uint8_t *copper_list, uint8_t x)
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
    copper_list[0x1b] = scroll;
    copper_list[0x13] = fetch;
    copper_list[0x23] = words;
    copper_list[0x27] = words;
    copper_list[0x2f] = extra;
    copper_list[0x37] = extra;
    copper_list[0x3f] = extra;
    copper_list[0x47] = extra;
}

/* orig $01D05C: wait for the next vertical blank */
__attribute__((noinline)) void wait_vblank(void)
{
    uint16_t seen = vbl_count;

    while (vbl_count == seen)
        ;
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
    custom->cop1lc = (uint32_t)(uintptr_t)list;
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
