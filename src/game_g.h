/* Gods, the game: the shot and object-list routines at $168AE and $16A66 (the continuations of game_c.c's update_shots) */
#ifndef GAME_G_H
#define GAME_G_H

#include <stdint.h>

#include "menu_symbols.h"
#include "menu_types.h"

void shot_hit_tile(const struct Object *object, struct Shot *shot, int16_t x, int16_t y);   /* orig $01695E */
void mark_shot_cells_3x3(int16_t x, int16_t y, const struct Shot *shot);        /* orig $016A66 */
void update_shots_sliding(struct ShotList *list, struct Object *object);       /* orig $0168AE */

void update_bolts(void);                                                       /* orig $006136 */

#endif
