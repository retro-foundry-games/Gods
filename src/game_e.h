/* Gods, the game: the routines at $6ED0, $12924, $12A9C, $12796, $10976 and $15282 and what they call */
#ifndef GAME_E_H
#define GAME_E_H

#include <stdint.h>

#include "menu_symbols.h"

struct Object;

void update_creatures(void);                         /* orig $006ED0 and $0070C8 (the loop over the creatures; fn_6ed0) */
void item_message(int16_t item);                     /* orig $012924 (d2 the item) */
void object_message(const struct Object *object);   /* orig $012A9C (a1 the object's record) */
void remember_visited_spot(void);                      /* orig $0187B8 */
void run_cell_entry(int16_t index);                    /* orig $0127A0 (d0 the index) */
void cell_entry_runs(const uint8_t *cell);           /* orig $012796 (a0 the map cell) */
int16_t hint_message(void);                          /* orig $010976 (returns d3, 0) */
void pick_up_object(int16_t item);                   /* orig $015282 (d2 the item) */

#endif
