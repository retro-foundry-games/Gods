/* Gods SDL port, the intro: what intro_host.c (the program's side, the 68000's struct layout) and intro_sdl.c (SDL) say to each other */
#ifndef GODS_INTRO_HOST_H
#define GODS_INTRO_HOST_H

#include <stdint.h>

void ih_vbl(void);                                         /* the vertical blank interrupt */
void ih_set_fire(int down);                                /* the fire button (space, control) */
const uint16_t *ih_shown_screen(void);                     /* the screen on show: five bitplanes of 200 rows of 20 words, 8000 bytes apart */
uint16_t ih_color(int index);                              /* the colour registers */
void ih_music_mix(int16_t *out, int frames, int rate);     /* the music: stereo, signed 16 bit */
void ih_start_menu(void);                                  /* main.c: the intro is over: the menu program is next (does not return) */
void game_main(void);                                      /* intro.c: the program (never returns) */

#endif
