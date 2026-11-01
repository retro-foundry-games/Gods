/* Gods, the menu program: screen buffers, the colour shadow table, the copper list and the blitter */
#ifndef MENU_SCREEN_H
#define MENU_SCREEN_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

void clear_screen_buffer(uint32_t *buffer);                              /* orig $01CFAC */
void copy_screen_buffer(const uint32_t *from, uint32_t *to);             /* orig $01CFE4 */
const uint16_t *find_color01_move(const uint16_t *list);                 /* orig $01D22A */
void shadow_set(uint16_t index, uint16_t value);                         /* orig $01D2D8 */
uint16_t shadow_get(uint16_t index);                                     /* orig $01D2EA */
void clear_both_screens(void);                                           /* orig $01CFC0 */
void copy_other_screen(void);                                            /* orig $01CFCE */
void blitter_wait(void);                                                 /* orig $01DFD8 */

#endif
