/* Gods, the game: the routines at $417C and $44D2 and what they call */
#ifndef GAME_D_H
#define GAME_D_H

#include <stdint.h>

#include "menu_symbols.h"

void update_records(void);                                                 /* orig $00417C */
int16_t player_touch_record(int16_t x, int16_t y, struct Slot8 *record, int16_t counter);   /* orig $0044D2 */
int16_t record_check_walls(int16_t x, int16_t y, int16_t *record);         /* orig $007EAE */
void draw_score_panel(void);                                               /* orig $0057FC */
void draw_records_19334(void);                                             /* orig $01924C (all of it; find_record_19334 is its first part) */
void blit_image_5wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows);   /* orig $017100 */

/* The routines of game_e.c this module needs, with the registers they take in the original.  game_d.c has weak versions that do
 * nothing: the strong ones of game_e.c (or whatever names the lead gives them) replace them when the symbols are the same. */
void item_message_12924(int16_t id);        /* orig $012924 (d2 the id of the thing picked up) */
void object_start_15282(int16_t id);        /* orig $015282 (d2 the id less 192) */

#endif
