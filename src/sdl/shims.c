/* Gods SDL port: routines of the program whose behaviour depends on the Amiga's memory, replaced here (this object is linked before
 * the program's, whose own copies are not used).  Each is the program's routine with the one difference said.  Compiled with the
 * program's headers and flags. */
#include <stdio.h>

#include "menu.h"

/* orig $016ECA (message.c): draw message_text in the work area, 40 characters at most, centred, and start the colours fading in.
 * The program calls it with message_text 0 when it fades a message out with nothing to follow: on the Amiga the text is then read
 * from address 0 (the first bytes of the memory, which are 0: an empty text); here it is an empty text. */
void message_draw(void)
{
    static const uint8_t empty[1] = { 0 };
    const uint8_t *text = message_text ? message_text : empty;
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

    text = message_text ? message_text : empty;
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
}

/* the Amiga's game_loop is the original's loop at $2C20 sent to game_frame (begin_quest ends by entering it): on the host it is the
 * loop of game_frame, for ever, with the interrupts on */
extern void sr_set(uint16_t value);

void game_loop(void)
{
    sr_set(0x2000);
    for (;;)
        game_frame();
}

/* ---- Begin Quest on the host -------------------------------------------------------------------------------------------------------
 * The menu's lines call the original's code (original_begin, original_continue, original_after_password), which has no C yet.  On the
 * host they call host_begin_quest(): with --level1-image the level is started from that RAM image of the original (a test) and the game
 * frame runs for ever; without it the call is logged and the menu goes on.  The functions are weak: the C ones, when they exist and
 * keep these names, take their place; when the menu calls another name this hook is not used. */
const char *host_level_image;   /* set by main.c: --level1-image */
extern int guest_load_level_image(const char *path);
extern void sr_set(uint16_t value);

void host_begin_quest(void)
{
    if (!host_level_image) {
        fprintf(stderr, "gods-sdl: Begin Quest: the C is not there yet (try --level1-image)\n");
        return;
    }
    if (guest_load_level_image(host_level_image) != 0) {
        fprintf(stderr, "gods-sdl: cannot read %s\n", host_level_image);
        return;
    }
    sr_set(0x2000);   /* the interrupts on */
    for (;;)
        game_frame();
}

__attribute__((weak)) void original_begin(void)
{
    host_begin_quest();
}

__attribute__((weak)) void original_continue(void)
{
    host_begin_quest();
}

__attribute__((weak)) void original_after_password(void)
{
    host_begin_quest();
}
