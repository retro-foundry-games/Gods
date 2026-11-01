/* Gods, the menu program: drawing images and text on the screen */
#ifndef MENU_DRAW_H
#define MENU_DRAW_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

/* menu_amiga.asm: the original's masked blit (orig $0171E4 and $0171F6): draw an image of `rows` rows, one word (two words)
 * wide, with its planes `plane` bytes apart at `src`, to the screen at `dst`, `shift` pixels to the right */
void blit_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows);
void blit_image_wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows);

void glyph_draw(int16_t x, int16_t y, uint16_t index);                       /* orig $017686 */
void sprite_draw(int16_t x, int16_t y, uint16_t index);                      /* orig $0173C4 */
void text_draw_at(const char *text, int16_t x, int16_t y);                   /* orig $003BF8 */
void text_draw_centered(const char *text);                                   /* orig $012D90 */
int16_t draw_digit(int16_t x, int16_t y, uint16_t index);                    /* orig $006A3A */
void draw_string_small(const char *text, int16_t x, int16_t y);              /* orig $0069E8 */
int16_t draw_hex_bytes(const uint8_t *bytes, int16_t x, int16_t y);          /* orig $006A1C */
int16_t draw_string_16(const uint8_t *text, int16_t x, int16_t y);           /* orig $006A52 */

#endif
