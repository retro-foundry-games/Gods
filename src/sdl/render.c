/* Gods SDL port: the display.  The program's copper lists are not run: what they do is emulated from the state the program keeps.
 *
 *   - which screen is shown: COP1LC names copper_list_a (the first screen) or copper_list_b (the second); the screens are 44 bytes
 *     a row, 192 rows, four bitplanes 8448 bytes apart;
 *   - the fine scroll: set_scroll() (display.c of the program) makes the copper list show the screen from pixel x_offset & 31 on;
 *   - the colours: the colour registers (the program writes them directly, the vertical blank copies the shadow table);
 *   - the message line (message.c): below the screen, seven lines of one bitplane (work_area, 40 bytes a row) in the colours of
 *     message_pairs on a background that goes from dark to lighter green and back.
 * Low resolution, 320 pixels wide, from line 60 on.  The words of the bitplanes are native words. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest.h"

#define FIRST_LINE 44   /* the first line of the picture on the screen (PAL) */
#define SCREEN_TOP 60   /* the program's screen starts here */
#define SCREEN_ROWS 192
#define MESSAGE_TOP 253
#define PLANE_SIZE 8448

extern uint8_t copper_list_a[];
extern uint8_t screen_1[];
extern uint8_t screen_2[];
extern uint16_t x_offset;
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

void display_latch_colors(void);

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
    latched_cop1lc = REG32(0x080);
    latched_x_offset = list_scroll[(uint8_t *)(uintptr_t)REG32(0x080) == copper_list_a ? 0 : 1];
    display_latch_colors();
    {
        uint32_t cop1lc = latched_cop1lc;
        const uint16_t *list = cop1lc < 0x10000 ? (const uint16_t *)guest_ptr(cop1lc) : (const uint16_t *)(uintptr_t)cop1lc;

        static uint16_t previous[1024];
        static int previous_valid;

        latched_list_valid = previous_valid;
        memcpy(latched_list, previous, sizeof previous);
        previous_valid = list != 0;
        if (list)
            memcpy(previous, list, sizeof previous);
    }
}

/* the colour registers: the vertical blank interrupt (irq_vbl) writes them from the shadow table, so what the frame shows is what they hold after it; the program's thread (woken at the end of
 * the interrupt) changes them later in the frame, which the hardware shows from that line on: the frame is drawn from what they were when it began */
void display_latch_colors(void)
{
    static uint16_t previous[16];

    memcpy(latched_colors, previous, sizeof previous);   /* (the colours the frame before) */
    for (int i = 0; i < 16; i++)
        previous[i] = REG16(0x180 + 2 * i);
}

static uint32_t argb(uint16_t color)
{
    uint32_t r = (color >> 8 & 15) * 17, g = (color >> 4 & 15) * 17, b = (color & 15) * 17;

    return 0xff000000u | r << 16 | g << 8 | b;
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
    }
    for (int i = 0; i + 1 < count; i += 2) {
        uint16_t first = list[i], second = list[i + 1];

        if (swapped) {
            first = (uint16_t)(first >> 8 | first << 8);
            second = (uint16_t)(second >> 8 | second << 8);
        }
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
        }
    }
    while (line < lines)
        memcpy(colors[line++], current, sizeof current);
}

void display_render(uint32_t *pixels)
{
    static uint16_t colors[320][16];
    uint32_t palette[16];
    const uint8_t *first_list = (const uint8_t *)(uintptr_t)latched_cop1lc;
    const uint8_t *screen = first_list == copper_list_a ? screen_1 : screen_2;
    int scroll = latched_x_offset & 31;

    line_palettes(colors, 320);
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
