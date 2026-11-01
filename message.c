/* Gods, the menu program: the line of text that fades in at the top of the screen and fades out again */
#include "menu.h"

/* orig $016DF8: show the next message: it starts as the current one, and its colours begin to fade in (message_arg is kept) */
void message_start(uint16_t arg)
{
    if (message_state <= 0)
        message_state = 1;
    message_current = message_next;
    message_arg = arg;
    message_timer = 10;
}

/* orig $016E42: one frame of the message.  Fading out (state 1) the four colours go down; when they are gone the text that was
 * waiting is drawn in the work area, 40 characters at most, centred, and the colours fade in again (state -1) */
void message_tick(void)
{
    if (message_state == 0)
        return;

    if (message_state > 0) {
        int16_t floor_green = 32;

        for (int i = 0; i < 4; i++) {
            uint16_t *a = (uint16_t *)(uintptr_t)message_pairs[2 * i] + 1;
            uint16_t *b = (uint16_t *)(uintptr_t)message_pairs[2 * i + 1] + 1;
            int16_t blue = *a & 0x00f;
            int16_t green = *a & 0x0f0;
            int16_t red = *a & 0xf00;

            blue -= 2;
            if (blue < 0)
                blue = 0;
            green -= 32;
            if (green < floor_green)
                green = floor_green;
            red -= 512;
            if (red < 0)
                red = 0;
            *a = *b = blue | green | red;
            floor_green += 32;
        }
        if (--message_timer >= 0)
            return;

        message_state = 0;
        message_shown = 0;
        message_wide = 0;
        clear_work_area();
        do_nothing();
        message_text = message_current;
        message_current = 0;

        const uint8_t *text = message_text;
        int16_t length = 0;

        while (*text++ != 0)
            length++;
        message_wide = -1;

        int16_t start = (40 - length) >> 1;

        message_x = start - 8;
        if (start < 0) {
            start = 0;
            message_x = 0;
            message_wide = 320;
            length = 40;
        }
        uint8_t *to = (uint8_t *)work_area + 40 + start;

        text = message_text;
        for (int i = 0; i < length; i++) {
            const uint8_t *glyph = font_8x8 + (*text++ << 3);

            for (int row = 0; row < 7; row++)
                to[40 * row] = glyph[row];
            to++;
        }
        message_shown = 1;
        message_words[8] = 0x0320;
        message_words[9] = 0x0532;
        message_words[10] = 0x0754;
        message_words[11] = 0x0765;
        message_words[0] = message_words[4];
        message_words[1] = message_words[5];
        message_words[2] = message_words[6];
        message_words[3] = message_words[7];
        message_state = -1;
        message_timer = 10;
        return;
    }

    for (int i = 0; i < 4; i++) {
        uint16_t *a = (uint16_t *)(uintptr_t)message_pairs[2 * i] + 1;
        uint16_t *b = (uint16_t *)(uintptr_t)message_pairs[2 * i + 1] + 1;
        uint16_t want = message_words[8 + i] << 1;
        int16_t blue = *a & 0x00f;
        int16_t green = *a & 0x0f0;
        int16_t red = *a & 0xf00;

        if (blue < (int16_t)(want & 0x00f))
            blue += 2;
        if (green < (int16_t)(want & 0x0f0))
            green += 32;
        else if (green != (int16_t)(want & 0x0f0))
            green -= 32;
        if (red < (int16_t)(want & 0xf00))
            red += 512;
        *a = *b = blue | green | red;
    }
    message_timer--;
}
