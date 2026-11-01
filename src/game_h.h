/* Gods, the game: what Begin Quest does between the fire button of the menu and the first frame of the game loop (the original's
 * code at $651E, $2452 to $2C08 and the routines it calls) */
#ifndef GAME_H_H
#define GAME_H_H

#include <stdint.h>

#include "menu_symbols.h"
#include "menu_types.h"

void begin_quest(void);                 /* orig $00651E, which jumps to $002452: sets the game up for level 1 and goes on in the game loop */
void draw_start_frame(void);            /* orig $00FC32: a game frame without the checks of the loop (the set-up draws two before the first fade-in) */
void level_state_init(void);            /* orig $00F526 */
void hud_digits_load(void);             /* orig $005E50 */
void image_draw_clipped(int16_t x, int16_t y, uint16_t image);   /* orig $01EFCC (the clipped, masked draw of image `image` at (x, y) with the clip list of ptr_0de0) */

void start_record_path(uint16_t *entry, int16_t variant);   /* orig $017E0A, $017E0E, $017E12 and $017E16 (the variant 0 to 3), which go on at $017E18 */
void path_continue(uint8_t *record, const uint8_t *description);       /* orig $01808E: the record's line is walked on */

void player_tile_scan(void);             /* orig $012710 */

/* the routines Begin Quest is made of (each says its original address in game_h.c) */
void fill_cce_ones(void);
void clear_records_12a0(void);
void clear_records_1354(void);
void clear_records_113c(void);
void clear_records_1036(void);
void reset_slots_10(int16_t *base);
void reset_slots_fd0(void);
void reset_slots_e392(void);
void reset_slots_de26(void);
void fill_bytes_10b0(void);
void fill_flags_39fde(void);
void set_pickup_glyph(void);
void clear_effects(void);
void clear_records_737e(void);
void fill_table_1a30c(void);
void clear_markers(void);
void clear_records_1a154(void);
void clear_records_721e(void);
void reset_records_7544(void);
void clear_creature_state(void);
void clear_shot_lists(void);
void stop_game_sounds(void);
void copy_block_16bb4(void);
void clear_cell_records(void);
void widen_bytes_to_words(void);
void sort_records_19b14(void);
uint16_t random_next(void);
void random_table_make(void);
void scan_cells_d5ee(void);
void records_113c_reset(void);
void load_pattern_data(const uint8_t *names);
void check_flag_286(void);
void read_text_table_14852(const uint16_t *table, uint16_t *to);
const uint8_t *read_text_table_14814(const uint8_t *table);
void load_objects_file(const uint8_t *names);
void load_weapons_file(const uint8_t *names);
void load_level_objects(const uint8_t *names);
void place_pickups(void);
void load_level_map(void);
void copper_list_make(void);
void load_picture_to_screen(const uint8_t *name);
void load_background_tiles(const uint8_t *name);
void scroll_to_player(int16_t x, int16_t y);
void mark_cells_19170(void);
void make_pairs_fb9a(void);
uint8_t *find_flag_entry(int16_t x, int16_t y);
void fix_records_3d86(void);
void find_duplicate_records(void);
void item_panels_make(void);

#endif
