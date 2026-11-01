/* Gods, the menu program: showing the screens and the colours */
#ifndef MENU_DISPLAY_H
#define MENU_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

void set_scroll(uint8_t *copper_list, uint8_t x);              /* orig $004D3E */
void wait_vblank(void);                                        /* orig $01D05C */
void screens_flip(void);                                       /* orig $01D302 */
void copper_fade_up(void);                                     /* orig $01D168 */
void copper_fade_down(void);                                   /* orig $01D1C2 */
void copper_colors_save(uint16_t mask);                        /* orig $01D1FE */
void copper_colors_restore(void);                              /* orig $01D216 */

#endif
