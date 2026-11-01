/* Gods, the game: the routines at $8000-$FFFF and $16800 and up */
#ifndef GAME_B_H
#define GAME_B_H

#include <stdint.h>

#include "menu_symbols.h"

void do_nothing_b(void);                    /* orig $00BA34 (and eleven more) */
void step_836c(void);                       /* orig $00836C */
void draw_sprite_8472(void);                /* orig $008452 */
void update_choice_888e(void);              /* orig $008742 */
void step_d898(void);                       /* orig $00D898 */
void unit0_update(void);                    /* orig $00E486 */
void unit1_update(void);                    /* orig $00E76C */
void unit2_update(void);                    /* orig $00EA52 */
void unit3_update(void);                    /* orig $00ED38 */
void unit4_update(void);                    /* orig $00F01E */
void step_16aee(void);                      /* orig $016AEE */
void step_1843e(void);                      /* orig $01843E */
void step_190f2(void);                      /* orig $0190F2 */
int16_t damage_player_at(int16_t x, int16_t y, int16_t strength);   /* orig $00D270 */
uint16_t *clear_72_words(uint16_t *p);      /* orig $00FB20 */
void reset_object_slot(int16_t index, int16_t other);   /* orig $00FB2E */

void draw_marks_316(void);                  /* orig $018D14 */
void blitter_grab_wait(void);               /* orig $01E372 */
void blit_tile(int16_t src_step, int16_t dst_step, int16_t rows, const uint8_t *src, uint8_t *dst);   /* orig $017082 */
const uint8_t *draw_background_tile(const uint8_t *map, const uint8_t *tiles, uint8_t *dst, int16_t y, int16_t src_step, int16_t dst_step);   /* orig $017972 */
void blitter_setup_copy(void);              /* orig $017044 */
void draw_background(void);                 /* orig $0178C6 */
void step_18072(const uint8_t *a1, const uint8_t *a2);   /* orig $018072 */
void draw_record_image(int16_t x, int16_t y, const uint8_t *a2);   /* orig $017FA0 */
struct SavedByte *mark_record_cells(int16_t x, int16_t y, const uint8_t *a2, struct SavedByte *list);   /* orig $018014 */
void take_record_to_18740(uint16_t *a0);       /* orig $018900 */
void take_record_to_199ac(uint16_t *a0);    /* orig $018890 */
void stop_sound(uint16_t id);               /* orig $01FA8C */
void spawn_debris_at_cell(int16_t x, int16_t y, const uint8_t *record, int16_t *flag);   /* orig $01695E */
void redraw_record_images(void);            /* orig $017E9C */

int16_t check_hit_and_walls(int16_t x, int16_t y, int16_t strength);   /* orig $00D00A and $00D114 */
void finish_record_1895a(uint16_t *a0);        /* orig $01895A */

uint16_t claim_free_entries(void);   /* orig $00FDBA */

#endif
