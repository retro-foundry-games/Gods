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

static uint32_t argb(uint16_t color)
{
    uint32_t r = (color >> 8 & 15) * 17, g = (color >> 4 & 15) * 17, b = (color & 15) * 17;

    return 0xff000000u | r << 16 | g << 8 | b;
}

void display_render(uint32_t *pixels)
{
    uint32_t palette[16];
    const uint8_t *first_list = (const uint8_t *)(uintptr_t)REG32(0x080);
    const uint8_t *screen = first_list == copper_list_a ? screen_1 : screen_2;
    int scroll = x_offset & 31;

    for (int i = 0; i < 16; i++)
        palette[i] = argb(REG16(0x180 + 2 * i));
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        pixels[i] = palette[0];

    for (int row = 0; row < SCREEN_ROWS; row++) {
        uint32_t *out = pixels + (SCREEN_TOP - FIRST_LINE + row) * SCREEN_W;

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
    if (message_shown) {
        static const uint16_t background[7] = { 0x030, 0x042, 0x060, 0x082, 0x060, 0x042, 0x030 };
        static const int band[7] = { 0, 2, 4, 6, 5, 3, 1 };   /* which of message_pairs colours the text of the line */

        for (int line = 0; line < 7; line++) {
            uint32_t *out = pixels + (MESSAGE_TOP - FIRST_LINE + line) * SCREEN_W;
            const uint16_t *text_color = (const uint16_t *)(uintptr_t)message_pairs[band[line]] + 1;
            const uint8_t *bits = (const uint8_t *)work_area + 40 * line;

            for (int x = 0; x < SCREEN_W; x++)
                out[x] = (bits[x >> 3] >> (7 - (x & 7)) & 1) ? argb(*text_color) : argb(background[line]);
        }
    }
}
