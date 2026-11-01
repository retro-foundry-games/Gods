/* Gods, the menu program: the sound system (sound_amiga.asm is Amiga hardware, generated in menu_amiga.asm) */
#ifndef MENU_SOUND_H
#define MENU_SOUND_H

#include <stdint.h>

void play_sound(uint16_t id, uint16_t priority);   /* orig $01FAF6: d0 the id, d1 the priority (a voice with a higher one is not taken) */
void sound_tick(void);               /* orig $01FCB8 */
uint16_t random_word(void);          /* orig $00633E */

#endif
