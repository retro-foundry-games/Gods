/* Gods, the menu program: the screens and the colours */
#ifndef MENU_DISPLAY_H
#define MENU_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "amiga/hw.h"
#include "game.h"
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
void shop_message_frame(void);                                 /* orig $013E3A (game.c: the message of the shop while the frame tick runs) */
uint16_t shop_message_request(int16_t priority);               /* orig $013DF2 (game.c) */

extern uint16_t fade_keep_screens;                             /* not in the original: 1 while its fade-out does not clear the screens (the shop makes $1CFC0 an rts for it) */
void fade_out(uint16_t delay, uint16_t arg);                  /* orig $01D23E */

void clear_screen_buffer(uint32_t *buffer);                              /* orig $01CFAC */
void copy_screen_buffer(const uint32_t *from, uint32_t *to);             /* orig $01CFE4 */
const uint16_t *find_color01_move(const uint16_t *list);                 /* orig $01D22A */
void shadow_set(uint16_t index, uint16_t value);                         /* orig $01D2D8 */
uint16_t shadow_get(uint16_t index);                                     /* orig $01D2EA */
void clear_both_screens(void);                                           /* orig $01CFC0 */
void copy_other_screen(void);                                            /* orig $01CFCE */
void blitter_wait(void);                                                 /* orig $01DFD8 */
void display_show(const uint8_t *list);                             /* amiga/display_hw.c: the copper runs this list (sdl/display_hw_sdl.c) */

void blitter_grab_wait(void);               /* orig $01E372 */
void blit_tile(int16_t src_step, int16_t dst_step, int16_t rows, const uint8_t *src, uint8_t *dst);   /* orig $017082 */
void blitter_setup_copy(void);              /* orig $017044 */
void blit_image_5wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows);   /* orig $017100 */
void blit_masked(const uint8_t *source, uint8_t *dest, int16_t modulo_ab, uint16_t words, int16_t modulo_cd, uint32_t plane_size,
                  uint16_t rows);                              /* orig $01F54E, $01F4B6: an image through the mask of its planes */
void blit_clipped(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *source, const int16_t *clip);   /* orig $01F0AA */
void copy_screen(const uint8_t *from, uint8_t *to);             /* orig $017814: the whole screen (4 planes of 192 rows) from one place to another */

void vbl_irq(void);                                            /* orig $01CFF8, amiga.asm: the level 3 interrupt */

#endif
