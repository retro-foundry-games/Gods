/* Gods, the menu program: showing the screens and the colours */
#ifndef MENU_DISPLAY_H
#define MENU_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "amiga/hw.h"
#include "menu_symbols.h"
#include "sound.h"

void set_scroll(uint8_t *copper_list, uint8_t x);              /* orig $004D3E */
void wait_vblank(void);                                        /* orig $01D05C */
void screens_flip(void);                                       /* orig $01D302 */
void copper_fade_up(void);                                     /* orig $01D168 */
void copper_fade_down(void);                                   /* orig $01D1C2 */
void copper_colors_save(uint16_t mask);                        /* orig $01D1FE */
void copper_colors_restore(void);                              /* orig $01D216 */

void fade_in(const uint16_t *target, uint16_t delay);         /* orig $01D0C0 */

void frame_tick(void);                                         /* orig $016C30 */

void fade_out(uint16_t delay, uint16_t arg);                  /* orig $01D23E */

void vbl_irq(void);                                            /* orig $01CFF8, menu_amiga.asm: the level 3 interrupt */

#endif
