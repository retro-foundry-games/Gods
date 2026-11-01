/* Gods, the game: what runs while a level is played (the loop of $2C20 and what it calls) */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#include "menu_symbols.h"

void game_read_input(void);                      /* orig $00363A */
void count_if_visible(int16_t x, int16_t y);     /* orig $004952 */

struct Slot12 *find_record_113c(void);                     /* orig $00D314 */
struct Slot8 *find_record_19334(void);                     /* orig $01924C */
struct Slot8 *find_free_record_19b14(void);                /* orig $018EF6 */
int words_all_zero(const uint16_t *record);                /* orig $00FF1E */
struct SavedByte *restore_saved_bytes(struct SavedByte *list, int16_t count);   /* orig $017DDC */
void call_by_kind(int16_t kind, int16_t arg, int16_t *result);                          /* orig $018A10 */
uint8_t *next_message_slot(void);                          /* orig $012A80 */

void game_loop(void);                                      /* menu_amiga.asm: the original's loop at $2C20 is sent here by game_loop_install */
void game_loop_install(void);                              /* the original's game loop at $2C20 gets a jump to game_loop: its body is game_frame */
void game_frame(void);                                     /* orig $002C20 */

void fill_buffer(void);                                    /* orig $006002 */
void activate_records(void);                               /* orig $003EA4 and $003E8C */
void count_zones(void);                                    /* orig $009912 */

void draw_effects(void);                                   /* orig $003F8E */
void update_energy_display(void);                          /* orig $005E02 */
void draw_item_row(void);                                  /* orig $003806 */

void work_area_shift_left(void);                           /* orig $01700C */
void message_ticker(void);                                 /* orig $016CA4 */

void update_records_12a0(void);                           /* orig $009AE6 */
void update_records_1354(void);                           /* orig $008A54 */
void next_table_entry(void);                              /* orig $00D47E */
void mark_player_cells(void);                             /* orig $003C56 */

void draw_sprites_721e(void);                             /* orig $007292 */
void move_records_3a0a6(void);                            /* orig $003DFC */

void spawn_effect(int16_t x, int16_t y, int16_t kind);       /* orig $003F46 */
void check_pickups(void);                                  /* orig $010470 */
void update_score_display(void);                           /* orig $005842 */

#endif
