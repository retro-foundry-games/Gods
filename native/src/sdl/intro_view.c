/* Gods SDL port, the intro: the picture (the screen on show and its colours, from the intro's own state) */
#include <stdint.h>

#include "intro_host.h"

#define WIDTH 320
#define HEIGHT 256
#define FIRST_LINE 44
#define SCREEN_TOP 60

static uint32_t argb(uint16_t color)
{
    return 0xff000000u | (uint32_t)((color >> 8 & 15) * 17) << 16 | (uint32_t)((color >> 4 & 15) * 17) << 8 | (uint32_t)((color & 15) * 17);
}

/* the picture: 320 x 200, five bitplanes (32 colours), from line 60 on */
void intro_render(uint32_t *pixels)
{
    const uint16_t *screen = ih_shown_screen();
    uint32_t palette[32];

    for (int i = 0; i < 32; i++)
        palette[i] = argb(ih_color(i));
    for (int i = 0; i < WIDTH * HEIGHT; i++)
        pixels[i] = palette[0];
    for (int row = 0; row < 200; row++)
        for (int x = 0; x < WIDTH; x++) {
            int index = 0;

            for (int plane = 0; plane < 5; plane++)
                index |= (screen[plane * 4000 + row * 20 + (x >> 4)] >> (15 - (x & 15)) & 1) << plane;
            pixels[(SCREEN_TOP - FIRST_LINE + row) * WIDTH + x] = palette[index];
        }
}

