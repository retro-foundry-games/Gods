/* Gods, the game: the routines at $10000-$167FF (the player, the shots, the tile helpers) */
#ifndef GAME_C_H
#define GAME_C_H

#include <stdint.h>

#include "menu_symbols.h"

uint8_t * player_tile_ptr(void);                            /* orig $0112F6 */
int16_t tile_one_three_rows_down(void);                     /* orig $011346 */
int16_t tile_one_at_player(void);                           /* orig $01136C */
void fn_15cc4_c(void);                                      /* orig $015CC4 */
void fn_16aee_c(void);                                      /* orig $016AEE */
void draw_player_sprite(uint16_t index);                    /* orig $01290C */
void run_object_list(struct ShotList *list);                /* orig $015B36 */
void run_object_lists(void);                                /* orig $015B04 */
void spawn_player_shots(void);                              /* orig $0153A2 */
void player_control(void);                                  /* orig $010368 */

#endif
