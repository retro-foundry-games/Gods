/* Gods, the game: the routines at $0000-$7FFF */
#ifndef GAME_A_H
#define GAME_A_H

#include <stdint.h>

#include "menu_symbols.h"

void game_nothing(void);                                   /* orig $003436, $0077E6 and $007906 */
void game_step_4070(void);                                 /* orig $004070 */
void draw_shutter(void);                               /* orig $0054C4 */
void game_step_5496(void);                                 /* orig $005496 */
void game_step_73d2(void);                                 /* orig $0073D2 */
void update_item_panel(void);                               /* orig $005608 */
uint8_t *map_cell(int16_t x, int16_t y);                   /* orig $007E90 */
void check_high_scores(void);                              /* orig $005AF6 */
void score_add(int16_t points);                            /* orig $004718 */
void spawn_puff(int16_t *object);                          /* orig $0071E6 */
void drop_thing(int16_t x, int16_t y, int16_t kind);       /* orig $007F88 */
void update_falling_thing(void);                           /* orig $007FAA */
void draw_energy_bar(void);                                /* orig $005D76 */

int16_t walk_pace_x(const uint8_t *type, int16_t *obj, int16_t x, int16_t y);       /* orig $007BA8 (d0) */
int16_t walk_pace_frame(const uint8_t *type, int16_t *obj, int16_t x, int16_t y);   /* the same, d2 */

void creature_step_717e(const uint8_t *type, int16_t *object);   /* orig $00717E */

#endif
