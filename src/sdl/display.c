/* Gods SDL port, the display: the screens, the colours and the copper list that shows them: the screen buffers, the shadow table of the colours, the fades and the
 * frame tick.  This is amiga/display.c for the host: the same logic (the colours are still in the copper lists, which render.c reads), the chips' parts are the host's: no copper scroll to set, a vertical blank wait that lets the other threads run, no blitter to wait for. */
#include <sched.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "guest.h"

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
    host_cpu_frames(3);   /* the original clears the 2 x 33792 bytes with a loop of clr.l: about 3 frames */
}

/* orig $01CFCE: copy the other screen over the one that is drawn on */
void copy_other_screen(void)
{
    copy_screen_buffer((const uint32_t *)screen_other, (uint32_t *)screen_buffer);
}

static int flip_late;   /* the flips are shown a frame after the vertical blank that ends their wait (a flip that comes late in a frame of the original's work) */
static uint8_t *flip_pending;
static uint8_t *flip_pending_late;   /* a flip the original made late in its frame: taken by the blank after the next */
extern int host_flip_is_late(void);

void host_flip_late(int on)
{
    flip_late = on;
}

/* the blank that has latched what it shows takes the flip that was waiting for it (display.c: screens_flip) */
void host_flip_apply(void)
{
    if (flip_pending)
        custom->cop1lc = (uint32_t)(uintptr_t)flip_pending;
    flip_pending = flip_pending_late;
    flip_pending_late = 0;
}

/* orig $01D302: show the screen that was drawn and draw on the other: the per-screen state, the two screens and so which
 * copper list; its scroll is set for x_offset and the copper is told to use it, then the next vertical blank is waited for */
extern void shop_flip_strip(void);

void screens_flip(void)
{
    struct Rec6 state = buffer_state[0];
    uint8_t *list;
    uint8_t *swap;
    extern void host_flip_wait(void);

    host_flip_wait();

    buffer_state[0] = buffer_state[1];
    buffer_state[1] = state;
    swap = screen_buffer;
    screen_buffer = screen_other;
    screen_other = swap;
    shop_flip_strip();
    list = screen_other == screen_1 ? copper_list_a : copper_list_b;
    set_scroll(list, (uint8_t)x_offset);
    int late = flip_late && host_flip_is_late();

    if (late)
        flip_pending_late = list;
    else if (flip_late)
        flip_pending = list;
    else
        display_show(list);
    vbl_saturating = 0;
    wait_vblank();
    if (late)
        wait_vblank();   /* (the screen that is shown is not drawn on before the blank that takes the new one) */
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

static int display_fade_out;
static int display_fade_step;   /* the step of the fade-in the program is at (0 to 14) */   /* a fade-out is on (the old list's stripe of the band model is longer: the shop's entry) */
extern volatile int host_fade_line;
volatile int host_fade_first = -1;   /* the raster line the original began the first step of its latest fade at (the lockstep) */
extern volatile int display_fresh_lag;   /* render.c: (with display_fresh) shown a frame late */
void host_fade_lag(int on)
{
    display_fresh_lag = on;
}

/* the lag the fade is shown with: 4 is for a fade-out that the original shows its steps a frame after the registers have them when its first step began late in a frame (from the line 100: the
 * thread came to the call after the blank's interrupt had run) and as they are when it began early */
static int effective_lag(void)
{
    if (display_fresh_lag != 4)
        return display_fresh_lag;
    return host_fade_first >= 100 ? 1 : 0;
}

extern volatile int display_fresh;   /* render.c: the fades show their colours and copper list as they are */
extern volatile int display_fresh_tail;   /* render.c: one more frame of that after a fade that was shown a frame late (the last step is then shown a frame after it was made) */

/* orig $01D0C0: fade the colours in from black to the palette `target` (words, each colour a half of what it is on the
 * screen), a step per `delay + 1` frames; the 15 steps change the shadow table, the copper list's colours follow */
void fade_in(const uint16_t *target, uint16_t delay)
{
    host_fade_wait();   /* (the lockstep: a fade-in that was bound waits for the original's) */
    display_fresh = 1;
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
        display_fade_step = step;
        copper_fade_up();
        palette_dirty = 0xffff;
        for (int frame = 0; frame <= delay; frame++)
            wait_vblank();
    }
    palette_lit = 0xff;
    display_fresh_tail = effective_lag() != 0;
    display_fresh = 0;
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
    if (tick_hold_38a) {
        shop_message_frame();   /* $16C9E */
        return;
    }
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
    display_fresh = 1;
    display_fade_out = 1;

    (void)delay;
    message_start(arg);
    delay = 1;   /* the original's d0, which is the delay, is the result of the routine that starts the message (orig $016DF8: 1): every step waits two frames */
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
    if (!fade_keep_screens)
        clear_both_screens();
    wait_vblank();
    copper_move_a = 0x00840000;
    copper_move_b = 0x00865246;
    copper_colors_restore();
    palette_lit = 0;
    display_fresh_tail = effective_lag() != 0;
    display_fresh = 0;
    display_fade_out = 0;
}

/* ---- what the display needs of the chips (the host's) ---- */

/* orig $004D3E: the fine scroll of a copper list.  There is no list to change: the scroll is kept for the list (the first or the second) and the display (render.c) shows
 * the screen with the scroll of the list COP1LC names, as the copper does (the program has already set the next frame's x_offset when the screen is on show) */
uint8_t list_scroll[2];

void set_scroll(uint8_t *copper_list, uint8_t x)
{
    list_scroll[copper_list == copper_list_a ? 0 : 1] = x;
}

/* orig $01D05C: wait for the next vertical blank (the main thread raises it) */
volatile int host_in_wait;          /* the program's thread is inside wait_vblank (or a loop that waits for blanks: host_spin) */
volatile uint16_t host_processed;   /* the blank it has seen while it waits */
static int blank_was_waiting;

void wait_vblank(void)
{
    uint16_t seen = vbl_count;

    host_processed = seen;
    host_in_wait = 1;
    while (vbl_count == seen)
        sched_yield();
    host_in_wait = 0;
}

void host_spin(void)
{
    host_processed = vbl_count;
    host_in_wait = 1;
}

void host_spin_end(void)
{
    host_in_wait = 0;
}

/* (the lockstep) before a blank's interrupt: whether the program's thread is waiting for it, and how often it has waited */
void host_blank_begin(void)
{
    blank_was_waiting = host_in_wait;
}

/* (the lockstep) after the blank's interrupt: a thread that was waiting for it is let run until it waits again, and has seen the blank (what it does at the top of the frame - a fade's step, the
 * flip - is in the frame the original does it in, whatever the machine's load is); a thread that was busy has the moment the caller gives it.  Gives up after 300 ms */
void host_blank_wait(void)
{
    if (!blank_was_waiting)
        return;
    for (int n = 0; n < 6000 && !(host_in_wait && host_processed == vbl_count); n++) {
        struct timespec ts = { 0, 50000 };   /* 50 microseconds */

        nanosleep(&ts, 0);
    }
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
extern volatile int gods_trace;
extern uint8_t *screen_buffer;

void blit_tile(int16_t src_step, int16_t dst_step, int16_t rows, const uint8_t *src, uint8_t *dst)
{
    if (gods_trace && (long)(dst - screen_buffer) < 44 * 4)
        fprintf(stderr, "trace blit_tile row %ld col %ld rows %d src %p\n", (long)(dst - screen_buffer) / 44, (long)(dst - screen_buffer) % 44, rows, (const void *)src);
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

/* orig $01F54E, $01F4B6: the image at `source` (four planes, plane_size bytes apart, `rows` rows of `words` words, modulo_ab bytes between its rows) is
 * drawn to `dest` (four planes 8448 bytes apart, modulo_cd between rows) through the mask of its four planes or'ed: D = mask ? plane : screen */
void blit_masked(const uint8_t *source, uint8_t *dest, int16_t modulo_ab, uint16_t words, int16_t modulo_cd, uint32_t plane_size, uint16_t rows)
{
    unsigned source_row = words * 2u + (unsigned)(int)modulo_ab, dest_row = words * 2u + (unsigned)(int)modulo_cd;

    if (gods_trace && (long)(dest - screen_buffer) < 44 * 6)
        fprintf(stderr, "trace blit_masked row %ld col %ld words %u rows %u\n", (long)(dest - screen_buffer) / 44, (long)(dest - screen_buffer) % 44, words, rows);

    for (unsigned plane = 0; plane < 4; plane++) {
        for (unsigned row = 0; row < rows; row++) {
            for (unsigned word = 0; word < words; word++) {
                unsigned at = row * source_row + 2 * word;
                uint16_t mask = 0;
                uint16_t *to = (uint16_t *)(dest + plane * 8448u + row * dest_row + 2 * word);

                for (unsigned i = 0; i < 4; i++)
                    mask |= *(const uint16_t *)(source + i * plane_size + at);
                *to = (uint16_t)((mask & *(const uint16_t *)(source + plane * plane_size + at)) | (~mask & *to));
            }
        }
    }
}

/* orig $01F43C (the entry 0 of the table at $0D9A: word_dde 0): the same without the mask, the image's four planes are copied (D = B) */
static void blit_plain(const uint8_t *source, uint8_t *dest, int16_t modulo_ab, uint16_t words, int16_t modulo_cd, uint32_t plane_size, uint16_t rows)
{
    unsigned source_row = words * 2u + (unsigned)(int)modulo_ab, dest_row = words * 2u + (unsigned)(int)modulo_cd;

    for (unsigned plane = 0; plane < 4; plane++)
        for (unsigned row = 0; row < rows; row++)
            for (unsigned word = 0; word < words; word++)
                *(uint16_t *)(dest + plane * 8448u + row * dest_row + 2 * word) = *(const uint16_t *)(source + plane * plane_size + row * source_row + 2 * word);
}

/* the clipped draw of an image whose x is not on a word's line (word_d86 not 0: flags_0ddd bit 1): the original shifts the image into a work area ($1F1E6) and blits it with the shift; the host draws it pixel by pixel:
 * the pixels x to x + width (the clip's x0 to x1 only), rows `height` + 1 from the row word_dd4 of the source, a pixel where the mask (the four planes or'ed) is set takes the image's bits on the screen's four planes */
static void blit_shifted(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *source, const int16_t *clip)
{
    unsigned bytes = ((uint16_t)width + 1u) >> 3;
    uint32_t plane_size = long_dd8;
    const int16_t plane_stride = 8448;

    for (int r = 0; r <= height; r++) {
        unsigned source_row = (unsigned)word_dd4 + (unsigned)r;
        uint16_t *screen_row = (uint16_t *)(screen_buffer + (long)(int16_t)(y + r) * 44);

        for (int px = 0; px <= width; px++) {
            int16_t dx = (int16_t)(x + px);

            if (dx < clip[0] || dx > clip[1])
                continue;

            unsigned at = source_row * bytes + (unsigned)(px >> 4) * 2u;
            unsigned bit = 15u - (unsigned)(px & 15);
            uint16_t mask = 0;

            for (unsigned i = 0; i < 4; i++)
                mask |= *(const uint16_t *)(source + i * plane_size + at);
            if (!((mask >> bit) & 1))
                continue;
            for (unsigned i = 0; i < 4; i++) {
                uint16_t *to = (uint16_t *)((uint8_t *)screen_row + i * plane_stride) + (dx >> 4);
                uint16_t pixel = (uint16_t)(1u << (15 - (dx & 15)));

                if ((*(const uint16_t *)(source + i * plane_size + at) >> bit) & 1)
                    *to |= pixel;
                else
                    *to &= (uint16_t)~pixel;
            }
        }
    }
}

/* orig $01F0AA: the image (x, y, `width` pixels, `height` + 1 rows, at `source`: the first of its four planes) is clipped by the
 * rectangle `clip` (x0, x1, y0, y1) and drawn on the screen buffer.  Only the way that ran is written: an image that is mirrored
 * (word_d82 negative), the shift in the bits of x (word_d86) and the other draw routines of the table at $0D9A are not */
void blit_clipped(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *source, const int16_t *clip)
{
    int16_t low = clip[2];
    int16_t high = clip[3];
    int16_t rest;

    if (gods_trace && y < 6)
        fprintf(stderr, "trace blit_clipped x %d y %d width %d height %d clip %d,%d,%d,%d\n", x, y, width, height, clip[0], clip[1], clip[2], clip[3]);
    int16_t modulo_ab;
    int16_t modulo_cd;
    uint16_t words;
    int16_t offset;
    uint32_t product;

    if (y < low) {   /* $01F0B6: the top of the image is above the clip: its rows above are left out (the source goes on by them) */
        height = (int16_t)(height + y);
        if (height < low)
            return;
        height = (int16_t)(height - low);

        int16_t skipped = (int16_t)(low - y);

        if (flags_0ddd & 0x20)
            skipped = (int16_t)-skipped;
        word_dd4 = (uint16_t)(word_dd4 + skipped);
        y = low;
    }
    if (y > high)
        return;
    if ((int16_t)(height + y) > high)
        height = (int16_t)(high - y);
    if (word_d86 != 0) {
        blit_shifted(x, y, width, height, source, clip);
        return;
    }
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
        word_d88 = 1;
    }
    high = clip[1];
    if (x > high)
        return;
    if ((int16_t)(rest + x) > high) {
        rest = (int16_t)(high - x);
        word_d88 = (uint16_t)(word_d88 + 2);
    }
    words = (uint16_t)rest >> 4;
    rest = (int16_t)(words << 3);   /* the original's d5 */
    if (!(word_d82 & 0x8000))
        return;   /* goes on at $01F13A */
    width = (int16_t)(((uint16_t)width >> 1) & 0xFFF8);
    product = (uint32_t)(uint16_t)(width + 8) * word_dd4;
    source += (int16_t)((uint16_t)product >> 2);
    if (flags_0ddd & 0x20)
        return;   /* goes on at $01F186 */
    modulo_ab = (int16_t)(width - rest);
    modulo_cd = (int16_t)(((uint16_t)(word_dd2 - 8 - rest)) >> 2);
    x = (int16_t)((uint16_t)x & 0xFFF0);
    x = (int16_t)((uint16_t)x >> 3);
    x = (int16_t)(x + (uint16_t)((uint16_t)y * 44));
    if (word_d86 != 0)
        return;   /* goes on at $01F1BE */
    if (flags_0ddd & 4)
        return;   /* goes on at the routine of the table at $0D9A that word_dde and bit 2 of flags_0ddd name (the entries of bit 2 are not written) */
    if (word_dde == 0) {
        blit_plain(source, screen_buffer + x, (int16_t)(modulo_ab >> 2), (uint16_t)(words + 1), modulo_cd, long_dd8, (uint16_t)(height + 1));
        return;
    }
    if (word_dde != 1)
        return;
    blit_masked(source, screen_buffer + x, (int16_t)(modulo_ab >> 2), (uint16_t)(words + 1), modulo_cd, long_dd8, (uint16_t)(height + 1));
}

/* ================ the blits (was blitter.c) ================ */
/* Gods SDL port: the blits.  The chip is not emulated: the program's routine that draws a masked image with the blitter
 * (blit_masked_image of draw.c, orig $017208) is done here directly, and so are the two entries of the asm that call it
 * (blit_image, blit_image_wide, orig $0171E4 and $0171F6).  The blit that follows BLTSIZE (sdl_blit_run, blitter_regs.c) is the chip's, for the routines of the
 * game proper (game_b.c, game_d.c) that program the chip are not used by the menu.
 *
 * The picture: an image `words` words wide of four planes (`plane` bytes apart, a row `words * 2 + extra_modulo` bytes), its mask the planes
 * ORed; both shifted `shift` pixels to the right (one word wider when extra_modulo is not negative), the first and the last word of a
 * row cut by first_mask and last_mask; the screen's rows are 44 bytes and its planes blit_plane_stride apart.  The words are native. */


extern uint16_t blit_extra_modulo;
extern uint32_t blit_plane_stride;


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
volatile int gods_trace;   /* the lockstep's peer: GODS_TRACE=FIRST-LAST (ticks of the port): the draws near the top of the screen are written to stderr */
extern uint8_t *screen_buffer;

void blit_masked_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows, uint16_t words,
                       uint16_t extra_modulo, uint16_t first_mask, uint16_t last_mask)
{
    int extra = (int16_t)extra_modulo < 0 ? 0 : (int16_t)extra_modulo;
    unsigned stride = words * 2u + (unsigned)extra;
    unsigned out_words = words + ((shift & 0xff) != 0 && (int16_t)extra_modulo >= 0 ? 1 : 0);
    unsigned count = rows;
    unsigned shift_bits = shift & 0xff;
    int16_t plane_stride = (int16_t)blit_plane_stride;

    if (gods_trace && (long)(dst - screen_buffer) < 44 * 6)
        fprintf(stderr, "trace blit_masked_image row %ld col %ld words %u rows %u shift %u src %p first %04x last %04x\n", (long)(dst - screen_buffer) / 44, (long)(dst - screen_buffer) % 44, words, rows, shift, (const void *)src, first_mask, last_mask);
    if (src == 0)
        return;   /* an image the table has no data for: on the Amiga the vectors at address 0 are drawn */
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


/* ================ the picture (was render.c) ================ */
/* Gods SDL port: the display.  The program's copper lists are not run: what they do is emulated from the state the program keeps.
 *
 *   - which screen is shown: COP1LC names copper_list_a (the first screen) or copper_list_b (the second); the screens are 44 bytes
 *     a row, 192 rows, four bitplanes 8448 bytes apart;
 *   - the fine scroll: set_scroll() (display.c of the program) makes the copper list show the screen from pixel x_offset & 31 on;
 *   - the colours: the colour registers (the program writes them directly, the vertical blank copies the shadow table);
 *   - the message line (message.c): below the screen, seven lines of one bitplane (work_area, 40 bytes a row) in the colours of
 *     message_pairs on a background that goes from dark to lighter green and back.
 * Low resolution, 320 pixels wide, from line 60 on.  The words of the bitplanes are native words. */


#define FIRST_LINE 44   /* the first line of the picture on the screen (PAL) */
#define SCREEN_TOP 60   /* the program's screen starts here */
#define SCREEN_ROWS 192
#define MESSAGE_TOP 253
#define PLANE_SIZE 8448

extern uint8_t copper_list_a[];
extern uint8_t screen_1[];
extern uint8_t screen_2[];
extern uint32_t *work_area;
extern const uint32_t message_pairs[8];
extern uint16_t message_shown;
extern uint16_t menu_active;

/* what the display hardware has at the start of a frame: the copper restarts from COP1LC at the vertical blank, the scroll and the colours come from the
 * copper list and the registers as they were then.  The program's thread sets them whenever it likes (it flips the screens just after the vertical blank
 * that ended its wait), so they are latched before the blank of the frame is signalled, as the hardware does, and the frame is drawn from them */
static uint32_t latched_cop1lc;
static uint16_t latched_x_offset;
extern uint8_t list_scroll[2];   /* display.c: the scroll set for each copper list */
static uint16_t latched_colors[16];
static uint16_t latched_list[1024];   /* the copper list that COP1LC names, as it was: the program's thread changes its colour moves whenever it likes (the fades) */
static int latched_list_valid;
static uint32_t latched_list_head;   /* COP1LC of the list that latched_list is a copy of */

void display_latch_colors(void);

volatile int display_fresh_lag;   /* (with display_fresh) the fade's colours and list are shown a frame after they were made: the fade of the player's respawn */
volatile int display_fresh_tail;   /* (after a fade shown a frame late) the next frame still shows as the fade did */
volatile int display_fresh;   /* set by the fades (display.c): their colours and copper list are shown as they are, not as they were two frames before */

void display_latch(void)
{
    static uint32_t last_list;
    static unsigned frames_since;
    static int trace = -1;

    if (trace < 0)
        trace = getenv("GODS_FLIPTRACE") != 0;   /* GODS_FLIPTRACE=1: the frames between two changes of the shown screen, on stderr */
    frames_since++;
    if (trace && REG32(0x080) != last_list) {
        fprintf(stderr, "flip after %u frames\n", frames_since);
        frames_since = 0;
        last_list = REG32(0x080);
    }
    /* (the screen and its scroll are not latched: the original shows a flip in the frame the program's logic makes it, the program's thread having flipped just after the blank) */
    latched_cop1lc = REG32(0x080);
    latched_x_offset = list_scroll[(uint8_t *)(uintptr_t)REG32(0x080) == copper_list_a ? 0 : 1];
    display_latch_colors();
    {
        uint32_t cop1lc = REG32(0x080);
        const uint16_t *list = cop1lc < 0x10000 ? (const uint16_t *)guest_ptr(cop1lc) : (const uint16_t *)(uintptr_t)cop1lc;

        static uint16_t previous[1024], before_previous[1024];
        static int previous_valid, before_previous_valid;
        static uint32_t previous_head, before_previous_head;

        latched_list_valid = before_previous_valid;
        latched_list_head = before_previous_head;
        memcpy(latched_list, before_previous, sizeof previous);
        memcpy(before_previous, previous, sizeof previous);
        before_previous_head = previous_head;
        before_previous_valid = previous_valid;
        previous_valid = list != 0;
        previous_head = cop1lc;
        if (list)
            memcpy(previous, list, sizeof previous);
        if ((display_fresh || display_fresh_tail) && list) {
            latched_list_valid = 1;
            latched_list_head = cop1lc;
            memcpy(latched_list, list, sizeof previous);
        }
    }
    host_flip_apply();
}

/* the colour registers: the vertical blank interrupt (irq_vbl) writes them from the shadow table, so what the frame shows is what they hold after it; the program's thread (woken at the end of
 * the interrupt) changes them later in the frame, which the hardware shows from that line on: the frame is drawn from what they were when it began */
void display_latch_colors(void)
{
    static uint16_t previous[16];

    static uint16_t before_previous[16];

    memcpy(latched_colors, before_previous, sizeof previous);   /* (the colours two frames before) */
    memcpy(before_previous, previous, sizeof previous);
    for (int i = 0; i < 16; i++)
        previous[i] = REG16(0x180 + 2 * i);
    if (display_fresh || display_fresh_tail)
        memcpy(latched_colors, previous, sizeof previous);
    display_fresh_tail = 0;
}

/* in a fade: after the blank's interrupt has written the colour registers the frame shows them and the copper list as they are (called right after irq_vbl, before the program's thread goes on) */
void display_latch_fresh(void)
{
    if (!display_fresh)
        return;
    if (effective_lag() != 1)   /* (1: the colours are shown a frame late) */
        for (int i = 0; i < 16; i++)
            latched_colors[i] = REG16(0x180 + 2 * i);
    uint32_t cop1lc = REG32(0x080);
    const uint16_t *list = cop1lc < 0x10000 ? (const uint16_t *)guest_ptr(cop1lc) : (const uint16_t *)(uintptr_t)cop1lc;

    if (list) {
        latched_list_valid = 1;
        latched_list_head = cop1lc;
        memcpy(latched_list, list, sizeof latched_list);
    }
}

/* in a fade shown with display_fresh_lag 2 (the fade-in of the player's respawn): the colours are as the blank's interrupt left them, but the copper list is taken after the program's
 * thread has had its moment (the original's fade writes the list at the top of the frame, before the raster reaches the bands: the frame shows it) */
static uint16_t late_list[1024];   /* (the list after the thread's moment, kept in the shop's fade-in: see display_render) */
static int late_list_valid;

void display_latch_late(void)
{
    if (display_fresh && effective_lag() == 0) {
        uint32_t head = REG32(0x080);
        const uint16_t *now = head < 0x10000 ? (const uint16_t *)guest_ptr(head) : (const uint16_t *)(uintptr_t)head;

        if (now) {
            memcpy(late_list, now, sizeof late_list);
            late_list_valid = 1;
        }
        return;
    }
    if (!display_fresh || effective_lag() != 2)
        return;
    uint32_t cop1lc = REG32(0x080);
    const uint16_t *list = cop1lc < 0x10000 ? (const uint16_t *)guest_ptr(cop1lc) : (const uint16_t *)(uintptr_t)cop1lc;

    if (list) {
        latched_list_valid = 1;
        latched_list_head = cop1lc;
        memcpy(latched_list, list, sizeof latched_list);
    }
}

static uint32_t argb(uint16_t color)
{
    uint32_t r = (color >> 8 & 15) * 17, g = (color >> 4 & 15) * 17, b = (color & 15) * 17;

    return 0xff000000u | r << 16 | g << 8 | b;
}

/* whether the first instructions of a list read as copper instructions in the machine's byte order and not in the other (a WAIT has bit 0 set, a MOVE names a register below $200) */
static int list_looks_native(const uint16_t *list, int count)
{
    int native = 0, swapped = 0;

    for (int i = 0; i + 1 < count && i < 24; i += 2) {
        uint16_t first = list[i], other = (uint16_t)(first >> 8 | first << 8);

        native += (first & 1) || (first < 0x200 && !(first & 1) && (first & 0x1fe) >= 0x80 && (first & 0x1fe) < 0x1c0);
        swapped += (other & 1) || (other < 0x200 && !(other & 1) && (other & 0x1fe) >= 0x80 && (other & 0x1fe) < 0x1c0);
    }
    return native > swapped;
}

/* the colours of every line of the picture: what the colour registers hold, changed by the MOVEs to COLOR00..COLOR15 of the copper
 * list that COP1LC names (a WAIT puts the following moves at its line; the horizontal position is not looked at).  The list is
 * as the program made it: words of the machine, or (the lists that are in the program's image) big-endian ones; the one in which
 * the list ends with $FFFF,$FFFE is read. */
static void line_palettes(uint16_t colors[][16], int lines)
{
    /* (an address in the low 64 KiB is the program's, not the host's: the boot code leaves one there until the screens are set up) */
    const uint16_t *list = latched_list_valid ? latched_list : 0;
    uint16_t current[16];
    int line = 0, swapped = 0, count = 0;

    for (int i = 0; i < 16; i++)
        current[i] = latched_colors[i];
    if (list) {
        for (int i = 0; i < 1024 && !count; i += 2) {
            if (list[i] == 0xffff && list[i + 1] == 0xfffe)
                count = i + 2;
            else if (list[i] == 0xffff && list[i + 1] == 0xfeff)
                count = i + 2, swapped = 1;
        }
        if (swapped && list_looks_native(list, count))
            swapped = 0;   /* (the end of the list is the image's, big-endian, bytes, the moves before it were made by the program) */
    }
    /* the list goes on where a MOVE to COPJMP2 sends it: the address that the moves to COP2LCH and COP2LCL made (the two lists of the program end
     * so: the gradient of the level's colours is jumped over when the moves name the list at $5246, as the menu's do).  The addresses are the
     * program's: the first list is at $4D8A (copper_list_a) or $4DDE.  The list the jump comes to may be the image's own (big-endian words) */
    uint32_t base = (const uint8_t *)(uintptr_t)latched_list_head == copper_list_a ? 0x4d8a : 0x4dde, jump_to = 0;
    int steps = 0;

    for (int i = 0; count && i + 1 < 1024 && steps++ < 4096; i += 2) {
        uint16_t first = list[i], second = list[i + 1];

        if (!swapped && i >= (int)(0x5246 - base) / 2 && !list_looks_native(list + i, 1024 - i))
            first = (uint16_t)(first >> 8 | first << 8), second = (uint16_t)(second >> 8 | second << 8);
        if (swapped) {
            first = (uint16_t)(first >> 8 | first << 8);
            second = (uint16_t)(second >> 8 | second << 8);
        }
        /* the three moves that end each of the program's two lists are the longs copper_move_a and copper_move_b (a long is stored the machine's way: its
         * halves are the other way round) and the strobe the image has (its words are not made native) */
        if (base + 2 * (uint32_t)i == 0x4e2e || base + 2 * (uint32_t)i == 0x4e32)
            first = (uint16_t)(list[i + 1]), second = list[i];
        else if (base + 2 * (uint32_t)i == 0x4e36)
            first = 0x008a, second = 0;
        if (first == 0xffff && second == 0xfffe)
            break;   /* the end of the list */
        if (first & 1) {   /* WAIT */
            int wait = first >> 8;

            if (wait < (line & 255))
                wait += 256 * ((line >> 8) + 1);
            else
                wait += 256 * (line >> 8);
            while (line < wait && line < lines)
                memcpy(colors[line++], current, sizeof current);
        } else if ((first & 0x1fe) >= 0x180 && (first & 0x1fe) < 0x1a0) {
            current[((first & 0x1fe) - 0x180) / 2] = second;
        } else if ((first & 0x1fe) == 0x84) {
            jump_to = (jump_to & 0xffff) | (uint32_t)second << 16;
        } else if ((first & 0x1fe) == 0x86) {
            jump_to = (jump_to & 0xffff0000u) | second;
        } else if ((first & 0x1fe) == 0x8a && jump_to >= base && jump_to < base + 2 * 1024) {
            i = (int)(jump_to - base) / 2 - 2;
        }
    }
    while (line < lines)
        memcpy(colors[line++], current, sizeof current);
}

/* (the lockstep) the raster line the original's thread began the copper fade of this step at, or -1 (devmain.c: fadeline).  The routine edits the colour moves of the list one after the other and
 * the raster is passing them: for about 58 lines after the line it began at the bands above have been drawn from the list as it was the frame before */
volatile int host_fade_line = -1;
static uint16_t previous_list[1024];
static int fade_in_edits;   /* (the edits seen in the shop's fade-in so far: the first is not shown in part) */
static int previous_list_valid;

void display_render(uint32_t *pixels)
{
    static uint16_t colors[320][16];
    static uint16_t colors_before[320][16];
    uint32_t palette[16];
    latched_cop1lc = REG32(0x080);   /* the screen and the scroll as they are now (see display_latch) */
    latched_x_offset = list_scroll[(uint8_t *)(uintptr_t)latched_cop1lc == copper_list_a ? 0 : 1];
    const uint8_t *first_list = (const uint8_t *)(uintptr_t)latched_cop1lc;
    const uint8_t *screen = first_list == copper_list_a ? screen_1 : screen_2;
    int scroll = latched_x_offset & 31;

    line_palettes(colors, 320);
    if (host_fade_line >= 0 && display_fresh && previous_list_valid && latched_list_valid) {
        static uint16_t now_list[1024];
        int cut = (host_fade_line * 26 - 90) / 10;

        memcpy(now_list, latched_list, sizeof now_list);
        if (effective_lag() == 0 && late_list_valid) {
            /* the shop's fade-in: the original's edit of the list is slow.  A frame after it began (at the raster line host_fade_line) it has done the lines from 2.6 times that line for 63 lines
             * and the raster passes them after the edit: they show the new list, the others the one before */
            int first = 72 + (host_fade_line * 8 + 5) / 10;   /* (104 for the line 40 of the shop's fade-in, 77 for the line 6 of the level end's fade-out) */
            int stripe_end = first + (display_fade_out ? 100 : 63);
            if (!display_fade_out)
                first -= 2 * ((display_fade_step + 1) / 5);   /* (the shop's fade-in: the edit comes to the bands sooner as the fade goes on, a band's two lines in five steps) */
            if (first < cut)
                first = cut - 1;   /* (the lines above the cut were drawn before the edit began) */

            int dark = 1;

            for (int i = 0; i < 16; i++)
                dark &= latched_colors[i] == 0;
            if (fade_in_edits++ > 0 || !dark) {   /* (the first edit of a fade-in from black is not shown in part; the first of a fade-out is) */
                memcpy(latched_list, late_list, sizeof now_list);
                line_palettes(colors_before, 320);
                memcpy(latched_list, now_list, sizeof now_list);
                for (int line = first; line < stripe_end && line < 320; line++)
                    memcpy(colors[line], colors_before[line], sizeof colors[line]);
            }
        } else {
            memcpy(latched_list, previous_list, sizeof now_list);
            line_palettes(colors_before, 320);
            memcpy(latched_list, now_list, sizeof now_list);
            for (int line = 0; line < cut && line < 320; line++)
                memcpy(colors[line], colors_before[line], sizeof colors[line]);
        }
    }
    if (!display_fresh)
        fade_in_edits = 0;
    if (latched_list_valid) {
        memcpy(previous_list, latched_list, sizeof previous_list);
        previous_list_valid = 1;
    }
    for (int i = 0; i < 16; i++)
        palette[i] = argb(latched_colors[i]);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        pixels[i] = palette[0];

    for (int row = 0; row < SCREEN_ROWS - 1; row++) {   /* (the display window ends a line early: the last row of the screen is not shown) */
        uint32_t *out = pixels + (SCREEN_TOP - FIRST_LINE + row) * SCREEN_W;

        for (int i = 0; i < 16; i++)
            palette[i] = argb(colors[SCREEN_TOP + row][i]);
        for (int x = 0; x < SCREEN_W; x++) {
            int bx = x + scroll;
            const uint8_t *word = screen + row * 44 + (bx >> 4) * 2;
            int bit = 15 - (bx & 15), index = 0;

            for (int plane = 0; plane < 4; plane++)
                index |= (*(const uint16_t *)(word + plane * PLANE_SIZE) >> bit & 1) << plane;
            out[x] = palette[index];
        }
    }

    /* the message: seven lines, the colour of the text and of the background by line */
    if (message_shown || !menu_active) {   /* (the band is there in the level also when no message is on show) */
        static const uint16_t background[7] = { 0x030, 0x042, 0x060, 0x082, 0x060, 0x042, 0x030 };
        static const int band[7] = { 0, 2, 4, 6, 5, 3, 1 };   /* which of message_pairs colours the text of the line */

        for (int line = 0; line < 7; line++) {
            uint32_t *out = pixels + (MESSAGE_TOP - FIRST_LINE + line) * SCREEN_W;
            const uint16_t *text_color = (const uint16_t *)(uintptr_t)message_pairs[band[line]] + 1;
            const uint16_t *bits = (const uint16_t *)((const uint8_t *)work_area + 40 * (line + 1));   /* words of the machine; the band shows the work area from its second row on (the text is drawn from there) */

            for (int x = 0; x < SCREEN_W; x++)
                out[x] = (message_shown && (bits[x >> 4] >> (15 - (x & 15)) & 1)) ? argb(*text_color) : argb(background[line]);
        }
    }
}

/* the chip's blitter is not emulated (the program's routines that programmed its registers are replaced above): said once if one is reached */
void host_blitter_not_emulated(void)
{
    static int said;

    if (!said++)
        fprintf(stderr, "gods-sdl: a routine that programs the blitter's registers was reached (not emulated)\n");
}
