#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "menu_symbols.h"
#include "menu_types.h"
/* Gods, the game: what runs while a level is played (the loop of $2C20 and what it calls) */



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
void activate_records_now(void);                           /* orig $003EB4 */
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


/* ---- from game_a.h ---- */
/* Gods, the game: the routines at $0000-$7FFF */



void game_nothing(void);                                   /* orig $003436, $0077E6 and $007906 */
void game_step_4070(void);                                 /* orig $004070 */
void draw_shutter(void);                               /* orig $0054C4 */
void game_step_5496(void);                                 /* orig $005496 */
void game_step_73d2(void);                                 /* orig $0073D2 */
void update_item_panel(void);                               /* orig $005608 */
void panel_blank(uint8_t *to);                             /* orig $0057E0 */
void panel_slot_plain(uint8_t *to, int16_t item);          /* orig $0056BE */
void panel_slot_cut(uint8_t *to, int16_t item);            /* orig $0056D8 */
uint8_t *map_cell(int16_t x, int16_t y);                   /* orig $007E90 */
void check_high_scores(void);                              /* orig $005AF6 */
void score_add_carry(int16_t points, int carry_in);      /* orig $004718, the carry (X flag) the caller left is carry_in */
void score_add(int16_t points);                            /* orig $004718 */
void spawn_puff(int16_t *object);                          /* orig $0071E6 */
void drop_thing(int16_t x, int16_t y, int16_t kind);       /* orig $007F88 */
void update_falling_thing(void);                           /* orig $007FAA */
void draw_energy_bar(void);                                /* orig $005D76 */

int16_t walk_pace_x(const uint8_t *type, int16_t *obj, int16_t x, int16_t y);       /* orig $007BA8 (d0) */
int16_t walk_pace_frame(const uint8_t *type, int16_t *obj, int16_t x, int16_t y);   /* the same, d2 */

void creature_step_717e(const uint8_t *type, int16_t *object);   /* orig $00717E */


/* ---- from game_b.h ---- */
/* Gods, the game: the routines at $8000-$FFFF and $16800 and up */



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
void step_18072(uint8_t *a1, const uint8_t *a2);   /* orig $018072 */
void draw_record_image(int16_t x, int16_t y, const uint8_t *a2);   /* orig $017FA0 */
struct SavedByte *mark_record_cells(int16_t x, int16_t y, const uint8_t *a2, struct SavedByte *list);   /* orig $018014 */
void take_record_to_18740(uint16_t *a0);       /* orig $018900 */
void take_record_to_199ac(uint16_t *a0);    /* orig $018890 */
void stop_sound(uint16_t id);               /* orig $01FA8C */
void mark_shot_cells(int16_t x, int16_t y, const int16_t *flag);                /* orig $016A08 */
void spawn_debris_at_cell(int16_t x, int16_t y, const uint8_t *record, int16_t *flag);   /* orig $01695E */
void redraw_record_images(void);            /* orig $017E9C */
void end_flag_8890(void);                   /* orig $008822 */

int16_t check_hit_and_walls(int16_t x, int16_t y, int16_t strength);   /* orig $00D00A and $00D114 */
void finish_record_1895a(uint16_t *a0);        /* orig $01895A */

uint16_t claim_free_entries(void);   /* orig $00FDBA */


/* ---- from game_c.h ---- */
/* Gods, the game: the routines at $10000-$167FF (the player, the shots, the tile helpers) */



uint8_t * player_tile_ptr(void);                            /* orig $0112F6 */
int16_t tile_one_three_rows_down(void);                     /* orig $011346 */
int16_t tile_one_at_player(void);                           /* orig $01136C */
void fn_15cc4_c(void);                                      /* orig $015CC4 */
void fn_16aee_c(void);                                      /* orig $016AEE */
void draw_player_sprite(uint16_t index);                    /* orig $01290C */
void run_object_list(struct ShotList *list);                /* orig $015B36 */
void run_object_lists(void);                                /* orig $015B04 */
void spawn_player_shots(void);                              /* orig $0153A2 */



/* ---- from game_d.h ---- */
/* Gods, the game: the routines at $417C and $44D2 and what they call */



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


/* ---- from game_e.h ---- */
/* Gods, the game: the routines at $6ED0, $12924, $12A9C, $12796, $10976 and $15282 and what they call */



struct Object;

void update_creatures(void);                         /* orig $006ED0 and $0070C8 (the loop over the creatures; fn_6ed0) */
void item_message(int16_t item);                     /* orig $012924 (d2 the item) */
void object_message(const struct Object *object);   /* orig $012A9C (a1 the object's record) */
void remember_visited_spot(void);                      /* orig $0187B8 */
void run_cell_entry(int16_t index);                    /* orig $0127A0 (d0 the index) */
void cell_entry_runs(const uint8_t *cell);           /* orig $012796 (a0 the map cell) */
int16_t hint_message(void);                          /* orig $010976 (returns d3, 0) */
void pick_up_object(int16_t item);                   /* orig $015282 (d2 the item) */


/* ---- from game_f.h ---- */
/* Gods, the game: the player's state machine at $10368 and what its states go on with */



void player_control(void);                                  /* orig $010368 */


/* ---- from game_g.h ---- */
/* Gods, the game: the shot and object-list routines at $168AE and $16A66 (the continuations of game_c.c's update_shots) */



void shot_hit_tile(const struct Object *object, struct Shot *shot, int16_t x, int16_t y);   /* orig $01695E */
void mark_shot_cells_3x3(int16_t x, int16_t y, const struct Shot *shot);        /* orig $016A66 */
void update_shots_sliding(struct ShotList *list, struct Object *object);       /* orig $0168AE */

void update_bolts(void);                                                       /* orig $006136 */


/* ---- from game_h.h ---- */
/* Gods, the game: what Begin Quest does between the fire button of the menu and the first frame of the game loop (the original's
 * code at $651E, $2452 to $2C08 and the routines it calls) */



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
