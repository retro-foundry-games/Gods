/* Gods, the game: what runs while a level is played (the loop of $2C20 and what it calls) */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#include "menu_symbols.h"

void game_read_input(void);                      /* orig $00363A */
void count_if_visible(int16_t x, int16_t y);     /* orig $004952 */

struct Slot8 *find_record_cce(void);                       /* orig $006136 */
struct Slot12 *find_record_113c(void);                     /* orig $00D314 */
struct Slot8 *find_record_19334(void);                     /* orig $01924C */
struct Slot8 *find_free_record_19b14(void);                /* orig $018EF6 */
int words_all_zero(const uint16_t *record);                /* orig $00FF1E */
struct SavedByte *restore_saved_bytes(struct SavedByte *list, int16_t count);   /* orig $017DDC */
void call_by_kind(int16_t kind);                           /* orig $018A10 */
uint8_t *next_message_slot(void);                          /* orig $012A80 */

void game_frame(void);                                     /* orig $002C20 */

#endif
