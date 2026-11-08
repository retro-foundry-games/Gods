/* Gods, the game: what runs while a level is played.  The loop that calls it all is at $2C20; the routines are written in the
 * order they were found, named by what they do (a provisional name is `fn_` and the address). */
#include "menu.h"
#include "sound.h"

static void update_falling_slots(void);
static void update_archers(void);
static void update_thrown(void);
static void archer_thing(struct Slot8 *record, int16_t x, int16_t y, int16_t glyph);
static int bat_spawn(int16_t x, int16_t y);
static void score_add_packed(uint8_t packed);
static void pod_spawn(int16_t x, int16_t y);
static void pods_update(void);
static void bats_run(void);
static void aimed_shot(int16_t x, int16_t y, int16_t speed, int16_t kind);
static void update_crumbles(void);
static void crumble_start(const uint8_t *cell, const int16_t *slot);
static void spawn_shot_ball(struct Shot *slot, int16_t id, int16_t x, int16_t y, int right);
static void update_shots_ball(struct ShotList *list, struct Object *object);
static void update_shots_hunter(struct ShotList *list, struct Object *object);

/* orig $00363A: this frame's input: the joystick byte and the button bits are kept for the game; when the input is being recorded
 * (demo_mode positive) one byte of them is put in the record (the buttons in bits 6 and 7, the stick below) */
void game_read_input(void)
{
    guest.game_stick = guest.joystick;
    guest.game_buttons = guest.button_bits[0];
    if (guest.demo_mode == 0)
        return;
    if (guest.demo_mode < 0)
        return;   /* playing a demo back: the original goes on at $3668, which did not run */
    *guest.game_record_ptr++ = (uint8_t)((guest.game_buttons << 6) | guest.game_stick);
}

/* orig $004952: a thing at the screen position (x, y) is counted when it is near the screen: x from -16 to 320 and y, less the
 * scroll, above -32 and below 192 */
void count_if_visible(int16_t x, int16_t y)
{
    y -= guest.scroll_y;
    if (x > 320 || x < -16 || y <= -32 || y >= 192)
        return;
    guest.onscreen_count++;
}

/* orig $00D314: the first of the ten records of 12 bytes at $113C whose third word is not 0, or 0 (the original goes on at $D32A) */
struct Slot12 *find_record_113c(void)
{
    for (int i = 0; i < 10; i++)
        if (guest.records_113c[i].w[2] != 0)
            return &guest.records_113c[i];
    return 0;
}

/* orig $01924C: the first of the ten records of 8 bytes at $19334 whose third word is not negative, or 0 (the original goes on at $19262) */
struct Slot8 *find_record_19334(void)
{
    for (int i = 0; i < 10; i++)
        if (guest.records_19334[i].w[2] >= 0)
            return &guest.records_19334[i];
    return 0;
}

/* orig $018EF6: the first free (third word negative) of the two hundred records of 8 bytes at $19B14, or 0 when all are in use */
struct Slot8 *find_free_record_19b14(void)
{
    for (int i = 0; i < 200; i++)
        if (guest.records_19b14[i].w[2] < 0)
            return &guest.records_19b14[i];
    return 0;
}

/* orig $00FF1E: whether the words at +0, +8 and +16 of the record are all 0 (otherwise the original goes on at $FF30) */
int words_all_zero(const uint16_t *record)
{
    return record[0] == 0 && record[4] == 0 && record[8] == 0;
}

/* orig $017DDC: `count` + 1 entries of 6 bytes (an address and a word) are undone: where the address is not 0 the low byte of the
 * word is put there, and the entry is cleared; returns where the list ends */
struct SavedByte *restore_saved_bytes(struct SavedByte *list, int16_t count)
{
    for (int i = 0; i <= count; i++, list++) {
        if (list->address != 0) {
            *list->address = (uint8_t)list->value;
            list->address = 0;
            list->value = 0;
        }
    }
    return list;
}

/* orig $018A10 (d5 the kind, d6 the argument, a3 the result word `result`; the routines of dispatch_18be0, $18C24 to $1906C, are
 * written in here): a condition of a record is looked at: when it does not hold, *result is cleared (it is left alone when it
 * holds).  Kind 0 is no condition; 1 and 2: the argument is not / is an item of the four slots (item_slots); 3 and 4: it is not /
 * is one of the three slot_refs; 5 and 6: the entry (4 bytes) of the table at ptr_0cbe numbered argument - 1 has a state that is
 * not / is negative; 7 and 8: the argument is at least / at most the player's energy; 9 and 10: five times the argument is at
 * least / at most the seconds (tick_count / 50, a quotient that does not fit a word leaves the low word of tick_count); 11 and 12:
 * bit 0 of the byte +5 of the 6-byte entry numbered argument of the table at $1C056 is set / not set; 13 and 14: five thousand
 * times the argument (as digits, kept in bcd_346, which sets score_redraw as score_add does) is at least / at most the score; 15 and
 * 16: the argument is at least / at most item_count.  Kinds 13 and 14 are only done for the arguments 0 to 65 (the original
 * divides the number in a word) */
void call_by_kind(int16_t kind, int16_t arg, int16_t *result)
{
    int clear = 0;

    switch (kind) {
    case 0:
        break;
    case 1:
    case 2: {
        int16_t found = arg == guest.item_slots[0] || arg == guest.item_slots[1] || arg == guest.item_slots[2] || arg == guest.item_slots[3];

        clear = kind == 1 ? !found : found;
        break;
    }
    case 3:
    case 4: {
        int16_t found = arg == guest.slot_ref_6b6 || arg == guest.slot_ref_748 || arg == guest.slot_ref_7da;

        clear = kind == 3 ? !found : found;
        break;
    }
    case 5:
    case 6: {
        int16_t state = (int16_t)guest.ptr_0cbe[arg - 1].state;

        clear = kind == 5 ? state >= 0 : state < 0;
        break;
    }
    case 7:
        clear = arg >= (int16_t)guest.player_energy;
        break;
    case 8:
        clear = arg <= (int16_t)guest.player_energy;
        break;
    case 9:
    case 10: {
        uint32_t ticks = guest.tick_count;
        uint32_t seconds = 0;
        uint16_t rest = 0;

        for (int shift = 24; shift >= 0; shift -= 8) {   /* ticks / 50 a byte at a time: the compiler has no 32-bit division */
            rest = (uint16_t)((rest << 8) | ((ticks >> shift) & 255));
            seconds = (seconds << 8) | (uint16_t)(rest / 50);
            rest = (uint16_t)(rest % 50);
        }
        int16_t low = (seconds <= 32767) ? (int16_t)seconds : (int16_t)ticks;   /* (ticks are not negative) */
        int16_t five = (int16_t)(arg + (int16_t)(arg << 2));

        clear = kind == 9 ? five >= low : five <= low;
        break;
    }
    case 11:
    case 12: {
        int set = guest.lever_flags[(uint16_t)arg].state & 1;

        clear = kind == 11 ? !set : set;
        break;
    }
    case 13:
    case 14: {
        int16_t m5 = (int16_t)(arg * 5);   /* the number is m5 * 1000 */
        int cmp = 0;

        guest.score_redraw = 1;
        guest.bcd_346[0] = 0;
        guest.bcd_346[1] = (uint16_t)((m5 % 10) << 4);
        guest.bcd_346[2] = (uint16_t)(((m5 / 10) % 10) | (((m5 / 100) % 10) << 4));
        guest.bcd_346[3] = (uint16_t)(((m5 / 1000) % 10) | (((m5 / 10000) % 10) << 4));
        for (int i = 3; i >= 0 && cmp == 0; i--)
            cmp = (int16_t)guest.bcd_346[i] < (int16_t)guest.score_words[i] ? -1 : (int16_t)guest.bcd_346[i] > (int16_t)guest.score_words[i] ? 1 : 0;
        clear = kind == 13 ? cmp >= 0 : cmp <= 0;
        break;
    }
    case 15:
        clear = arg >= guest.item_count;
        break;
    case 16:
        clear = arg <= guest.item_count;
        break;
    }
    if (clear)
        *result = 0;
}

/* orig $012A80: the first message line, or the one after it when it is the one on show (message_text or message_current) */
uint8_t *next_message_slot(void)
{
    uint8_t *slot = guest.message_slots;

    if (slot == guest.message_text || slot == guest.message_current)
        slot += 88;
    return slot;
}

static void level_end_screen(void);   /* orig $012E08 (below) */
static int shop_door(void);   /* orig $003438 (below) */
static void update_records_113c(void);   /* orig $00D32A (below) */
static void spawn_stone_enemy(int16_t *entry);   /* orig $00D4E2 (below) */
static void shop_run(void);   /* orig $013690 (below) */
static void world_end_countdown(void);   /* orig $0032B4 (below) */
static void boss_start(struct EntryRef *entry);   /* orig $00AFD6 (below) */
static void boss_rts(void);
static void boss_load_30da(void);   /* orig $0030DA (below) */
static void player_state_run(int16_t state, int16_t d7);   /* orig $010368's table (below) */
uint16_t fade_keep_screens;

/* orig $001D2FC: wait until more than `frames` frames have passed since the last flip (vbl_saturating counts them), then flip */
static void wait_frames_then_flip(uint16_t frames)
{
    host_iteration_cpu(2);   /* (the work of the frame, which took the original two frames; one for the first after a level end: host_iteration_cpu_next) */
    while ((int16_t)(frames - guest.vbl_saturating) >= 0)
        host_spin();
    host_spin_end();
    screens_flip();
}

/* not in the original: the loop of the original at $2C20 is sent to game_loop (a jump over its first six bytes), which runs game_frame in
 * its place and goes back to $2C20.  This is called before the original's Begin Quest, Continue and the code after the password
 * screen start the game */
void game_loop_install(void)
{
    *(volatile uint16_t *)guest.game_loop_jump = 0x4ef9;
    *(volatile uint32_t *)(guest.game_loop_jump + 2) = (uint32_t)(uintptr_t)game_loop;
}

/* orig $0030C6: the screen is pushed to the right, 4 pixels a frame (the toggle word shifted twice, or 4 when it is 0), and flag_26e (the
 * frames left) counts down.  The recording never reaches it (flag_26e is 0 there) */
static void scroll_push(void)
{
    int16_t step = (int16_t)(guest.frame_toggle << 2);

    if (step == 0)
        step = 4;
    guest.scroll_x += step;
    guest.flag_26e--;
}

static void player_death_frame(void);
/* orig $0016AC8: the list of message lines is set again: $A9A is 255, the four words at $A9E are 0, 16, 32 and 48, the cursor's frame is 0 and flag_aa8 is 1 */
static void messages_reset(void)
{
    guest.word_a9a = 0xFF;
    guest.words_a9e[0] = 0;
    guest.words_a9e[1] = 0x10;
    guest.words_a9e[2] = 0x20;
    guest.words_a9e[3] = 0x30;
    guest.cursor_frame = 0;
    guest.flag_aa8 = 1;
}

/* orig $0038BE (from $0039EE when the player has things left, word item_count not negative): the player comes back where he last was: the screen fades out, the energy is full,
 * the cell he was at (previous_player_pos, the place remember_visited_spot kept) is looked at for a free one near it (the fifteen offsets of respawn_search; the cell that is $FF), he is put there
 * and the scroll follows, he is standing, the records on the screen are made again, a frame is drawn, the sounds are stopped, and the screen fades in; then what the start of a level
 * does ($2BD6) */
static void player_respawn(void)
{
    host_fade_lag(4);   /* (the original shows the steps of this fade a frame after the registers have them when its first step began late in a frame, as they are when it began early: host_fade_first) */
    fade_out(0, 0xFFFF);
    guest.player_energy = 0x18;
    guest.energy_shown = 1;
    guest.word_2a6 = 0xFFFF;
    messages_reset();
    guest.word_a9a = (int16_t)(guest.word_a9a - 0xAA);
    if (guest.flag_23a != 0)
        guest.callback_514();

    int16_t x = (int16_t)((guest.previous_player_pos[0] + 0x10) & -32);
    int16_t y = (int16_t)((guest.previous_player_pos[1] + 8) & -16);
    const uint8_t *cell = guest.ptr_0b86 + 0x2000 + (x >> 5) + (int16_t)(y << 3);
    int16_t dx = 0, dy = 0;

    for (int i = 0; i < 15; i++) {
        int16_t offset = (int16_t)respawn_search[i * 3];

        dx = (int16_t)respawn_search[i * 3 + 1];
        dy = (int16_t)respawn_search[i * 3 + 2];
        if (cell[offset] == 0xFF)
            break;
    }
    guest.player_x = (int16_t)(x + dx);
    guest.player_y = (int16_t)(y + dy);
    if (guest.word_037c == 0) {
        int16_t sx = (int16_t)(guest.player_x - 0x90);
        int16_t sy = (int16_t)(guest.player_y - 0x48);

        if (sx < 0)
            sx = 0;
        if (sy < 0)
            sy = 0;
        if (sx >= 0xEC0)
            sx = 0xEC0;
        if (sy >= 0x340)
            sy = 0x340;
        guest.scroll_x = sx;
        guest.scroll_y = sy;
    }
    guest.player_state = 4;
    guest.word_19a = 0;
    guest.fall_distance = 0;
    guest.word_370 = 0;
    guest.word_1f2 = 3;
    activate_records_now();
    host_iteration_mark();
    host_cpu_frames(2);   /* (the original's drawing of that frame: its sounds are started two frames in; the flip comes then or a frame later, by what is on the screen: in the lockstep it waits for the original's) */
    draw_start_frame();
    host_flip_bind();
    screens_flip();
    host_cpu_frames(1);   /* (the original's flip routine waits for a blank before it returns, and then stops the sounds, which stop_game_sounds waits for) */
    stop_game_sounds();
    play_sound(60, 2);
    play_sound(9, 0x7F);
    guest.palette_words[0] = 0;
    host_fade_lag(2);   /* (the colours as they are, the copper list's a frame late) */
    fade_in(guest.palette_words, 0);
    host_fade_lag(0);
    drives_idle();
    if (guest.flag_aa8 != 0)
        play_sound(60, 2);
    else
        stop_sound(60);
    if (guest.param_index == 3)
        play_sound(66, 1);
}

/* orig $0039EE: the death is over: a life is taken (subq.w #1,item_count: our disk dump has a tst.w there but every other disk 1 dump has the subq, taken as the original: the lockstep puts the subq back); with
 * things left (item_count not negative) the player comes back; otherwise the game is over (game_over) */
static int player_came_back;   /* the death frame ends in $39EE: the player has come back and the original goes on at $2C20, the start of a frame, not at the end of this one ($2D54) */

/* orig $012D6E: `text` in the middle of the screen at row 70 (the sounds are stopped, the other screen is copied, the text is drawn, the screens
 * flip and the other screen is copied again; the original's copy of a screen takes about a frame) */
static void game_over_message(const char *text)
{
    guest.text_y = 70;
    stop_game_sounds();
    copy_other_screen();
    host_cpu_frames(1);
    text_draw_centered(text);
    host_cpu_frames(3);
    screens_flip();
    copy_other_screen();
    host_cpu_frames(1);
}

/* orig $0039F6: the lives are gone: "GAME  OVER" is shown with sound 42 (the original plays it twice), the work area is cleared, the sounds are
 * stopped, it waits 151 frames and then up to 349 frames or a button; the score is entered in the top ten, which is shown, and the menu
 * starts again.  A demo that ends (demo_mode not 0) goes back to $0036AA instead, which is not here yet */
static void game_over(void)
{
    game_over_message((const char *)text_game_over);
    play_sound(42, 100);
    play_sound(42, 100);
    clear_work_area();
    stop_game_sounds();
    for (int i = 0; i < 151; i++)
        wait_vblank();
    if (wait_frames_or_button(350))
        host_disk_extra(1);   /* (the fade after a button shows a frame later than the one after the time ran out: what the frame's phase in the original makes of it) */
    if (guest.demo_mode != 0)
        host_missing_routine(0x36AA);
    guest.has_continue = 0;
    high_score_entry();
    top_ten_screen();
    menu_start();
}

static void player_death_over(void)
{
    guest.item_count--;
    if (guest.item_count >= 0) {
        player_respawn();
        player_came_back = 1;
        return;
    }
    game_over();
}

/* orig $003B04 (to $003BD0; it returns into the end of the frame): the frames of the player's death.  When it begins (word_2a6 is negative) the sounds 0 and 4 are played and the
 * seven pieces start at the player; then each frame the pieces move by their steps (the y step grows by one a frame) and are drawn as the sprites 91 + the frame number; at
 * frame 6 a sound is played; at frame 17 they are no longer moved; at frame 18 the death is over ($39EE: the player comes back, or the game is over) */
static void player_death_step(void)
{
    if ((int16_t)guest.word_2a6 < 0) {
        play_sound(0, 0);
        play_sound(4, 0x7F);
        int16_t x = (int16_t)(guest.player_x - guest.scroll_x);
        int16_t y = (int16_t)(guest.player_y - guest.scroll_y + 8);

        for (int i = 0; i < 7; i++) {
            guest.death_particles[i * 4] = x;
            guest.death_particles[i * 4 + 1] = y;
            guest.death_particles[i * 4 + 2] = death_steps[i * 2];
            guest.death_particles[i * 4 + 3] = death_steps[i * 2 + 1];
        }
        guest.word_2a6 = 0;
    } else if (guest.word_2a6 == 0x12) {
        player_death_over();
        return;
    }
    if (guest.word_2a6 != 0x11) {
        for (int i = 0; i < 7; i++) {
            int16_t *piece = &guest.death_particles[i * 4];

            piece[0] = (int16_t)(piece[0] + piece[2]);
            piece[1] = (int16_t)(piece[1] + piece[3]);
            piece[3]++;
            sprite_draw(piece[0], piece[1], (uint16_t)(0x5B + guest.word_2a6));
        }
    }
    guest.word_2a6++;
    if (guest.word_2a6 == 6)
        play_sound(0x15, 0x7F);
}

/* orig $003A6C: a frame of the game when the player is dead (energy_shown is not above 0): the things of the level are run and drawn as in the frame of the living, but not the player
 * and his hits; the frame goes on at the end of the common frame ($2D54) */
static void player_death_frame(void)
{
    fill_buffer();
    draw_background();
    draw_pickups();
    do_nothing_b();   /* $194D0 */
    do_nothing_b();   /* $194D0 */
    update_crumbles();   /* $17CC0 */
    redraw_record_images();
    if (guest.flag_23a == 0) {
        activate_records();
        update_records();
        chest_pieces_move();
    }
    run_object_lists();
    fn_15cc4_c();
    falcon_seek_target();
    guest.value_c6c = 3030;
    guest.flag_20e = -1;
    update_creatures();
    if (guest.flag_23a == 0) {
        move_shards();
        draw_effects();
        update_falling_thing();
        draw_marks_316();
    }
    guest.scroll_x_a = guest.scroll_x;
    guest.scroll_y_a = guest.scroll_y;
    player_death_step();
}

/* orig $00374A (from $0035D2 when word_200 is 1): the player is put where word_202 says (the high byte times 32 plus 16, the low byte, signed, times 16),
 * the scroll goes there (as $37D2: 144 left and 72 above, kept in the level), the records of the screen are made again, the player stands (state 4),
 * the screen fades out, a frame is drawn, the screens flip and the screen fades in (delay 1) */
static void teleport_player(void)
{
    int16_t x = (int16_t)(((guest.word_202 >> 8) << 5) + 0x10);
    int16_t y = (int16_t)((int8_t)(guest.word_202 & 0xFF) << 4);

    guest.player_x = x;
    guest.player_y = y;
    if (guest.falcon_energy >= 0) {
        guest.word_8882 = x;
        guest.word_8884 = (int16_t)(y - 0x28);
    }

    int16_t sx = (int16_t)(x - 0x90);
    int16_t sy = (int16_t)(y - 0x48);

    if (sx < 0)
        sx = 0;
    if (sy < 0)
        sy = 0;
    if (sx >= 0xEC0)
        sx = 0xEC0;
    if (sy >= 0x340)
        sy = 0x340;
    guest.scroll_x = sx;
    guest.scroll_y = sy;
    clear_monsters();
    activate_records_now();
    guest.word_19a = 0;
    guest.flag_1f0 = 0;
    guest.player_state = 4;
    guest.records_scroll_y = guest.scroll_y;
    host_fade_lag(1);   /* (the lockstep: the colours of the registers and the list's bands both a frame after the steps) */
    fade_out(1, 0xFFFF);
    host_fade_lag(0);
    draw_start_frame();
    screens_flip();
    guest.copper_setup_done = 0;
    host_fade_bind();   /* (the lockstep: the original calls its fade-in a frame after its flip, which comes 12 or 13 frames after the fade-out by what it draws) */
    host_fade_lag(2);   /* (the colours as they are, the copper list's a frame late: as the fade-out above, the original's list is written at the top of the frame) */
    fade_in(guest.palette_words, 1);
    host_fade_lag(0);
    host_cpu_frames(1);   /* (the original goes on 31 frames after the call of the fade) */
}

static void boss_sparks_move(void);
static void boss_shots_move(void);
static void boss2_shots_move(void);
static void boss_death_score(void);
static void boss_quickload(void);
static void start_debris_moving(int16_t x, int16_t y, int16_t speed_x, int16_t speed_y);
static void chest_burst_at(int16_t x, int16_t y, const uint16_t *sprites);

static void level_end_for_door(void);
static void level_begin(void);
static int level_next_wait;
static void game_won_screen(void);
static void password_start_level(void);
static int shop_from_password;   /* the shop is entered at $136C8 (a level's password): no first fade-out, the saved tick count is 0 and the saved word_025c 1 (what the caller pushed) */
static int level_next_fade;   /* (the fade-in of the next level's start is bound to the original's) */
/* orig $002D54 on: what ends a frame of the game, after the player has been run (or, when he is dead, after the frame of his death: $3A6C): the units and the other things
 * that are run, the panels, the energy, the keys of the pause and the like, and the wait for the next frame */
static void game_frame_tail(void)
{
    guest.scroll_x_b = guest.scroll_x;
    guest.scroll_y_b = guest.scroll_y;
    guest.scroll_x = guest.scroll_x_a;
    guest.scroll_y = guest.scroll_y_a;
    step_16aee();
    if (guest.flag_23a == 0) {
        bats_run();
        dropped_sparkle_update();
        update_thrown();
        update_archers();
        update_walker_groups();
        if (shop_door()) {   /* $3514: the shop; the rest of the frame is not run */
            host_cpu_frames(1);   /* (the work of the frame that the original has done to here) */
            shop_run();
            return;
        }
        mark_player_cells();
        update_monsters();
        draw_sprites_721e();
        update_falling_slots();
        unit0_update();
        unit1_update();
        unit2_update();
        unit3_update();
        unit4_update();
        next_table_entry();
        update_records_113c();
        pods_update();
        spores_update();
    } else {
        guest.boss_frame();
        boss_sparks_move();
        boss2_shots_move();
        boss_shots_move();
    }
    boss3_death_fall();   /* $BC1A: a bare return unless the level 3 guardian is dead */
    move_records_3a0a6();
    falcon_draw();
    update_bolts();
    bolts_update();
    boss_death_score();
    update_score_display();
    draw_score_panel();
    update_item_panel();
    draw_shutter();
    update_energy_display();
    draw_energy_bar();
    draw_item_row();
    if (guest.flag_268) {
        int16_t d = (int16_t)guest.energy_old;

        if (d >= (int16_t)guest.player_energy) {
            d -= (int16_t)guest.player_energy;
            guest.player_energy += d >> 1;
        }
    }
    guest.scroll_x = guest.scroll_x_b;
    guest.scroll_y = guest.scroll_y_b;

    int16_t request = (int16_t)take_pending_request();

    if (guest.flag_23a == 0 && request == 27) {   /* Escape: $3A54 */
        static const char text_quit[] = "QUIT GAME";

        game_over_message(text_quit);
        guest.has_continue = 1;
        menu_start();
        return;
    }
    if (request == 32) {   /* the space bar: the game waits for a request or a button, and the clock goes on from where it was */
        uint32_t saved_ticks = guest.tick_count;

        guest.tick_hold_26c = 1;
        while ((uint16_t)take_pending_request() != 0)
            ;
        for (;;) {
            request = (int16_t)take_pending_request();
            if (request != 0 || guest.button_bits[0] != 0)
                break;
        }
        guest.tick_hold_26c = 0;
        guest.tick_count = saved_ticks;
    }
    if (guest.flag_2b0 == 18)
        request = 98;
    if (request == 70)
        return;   /* goes on at $39F6 */
    if (guest.level_ended)
        level_end_screen();
    else
        wait_frames_then_flip(guest.frame_wait);
    if ((int16_t)guest.flag_290 >= 0)
        world_end_countdown();   /* $32B4 (it goes on at $2EF4: the tests below) */
    if (guest.flag_258) {
        boss_load_30da();   /* $30DA */
        return;
    }
    if (guest.flag_1f0) {   /* $35D2 */
        if (guest.word_200 == 1)
            teleport_player();
        else
            level_end_for_door();
        return;
    }
    if (guest.flags_3b0 & 0x40) {
        game_won_screen();   /* $2F12 */
        return;
    }
}

/* orig $002C20: one frame of the game.  The routines it calls are the original's (fn_ and the address) until they are written
 * in C.  Where the original goes on at an address that did not run in the recorded play ($3A36, $3A6C, $3A54, $39F6, $32B4,
 * $30DA, $35D2, $2F12) the frame ends here: those are the ends of a level, a death, the pause menu and the like */
void game_frame(void)
{
    host_iteration_mark();
    message_tick();
    host_read_wait();   /* (the lockstep: the original's read of this iteration is done: the keys are there) */
    game_read_input();
    guest.frame_toggle = ~guest.frame_toggle;
    guest.frame_phase++;
    if ((int16_t)guest.frame_phase >= 2)
        guest.frame_phase = 0;
    guest.timer_306--;
    if ((int16_t)guest.timer_306 < 0)
        guest.timer_306 = 0;
    if ((int16_t)guest.demo_mode < 0 && (guest.button_bits[0] & 1)) {
        guest.demo_mode = 0;
        return;   /* goes on at $3A36 */
    }
    if (!(guest.game_buttons & 1))
        guest.fire_latch = 0;
    guest.energy_old = guest.player_energy;
    if ((int16_t)guest.energy_shown <= 0) {   /* the player is dead: $3A6C */
        player_death_frame();
        if (player_came_back)
            player_came_back = 0;
        else
            game_frame_tail();
        return;
    }
    fill_buffer();
    if (guest.flag_26e)
        scroll_push();
    draw_background();
    draw_pickups();
    do_nothing_b();   /* $194D0 */
    update_crumbles();   /* $17CC0 */
    boss4_death_wait();   /* $BA34: a bare return unless the level 4 guardian is dead */
    if (guest.flag_23a)
        guest.callback_4e4();
    redraw_record_images();
    if (guest.param_index == 2 || guest.flag_23a != 1) {
        activate_records();
        update_records();
        update_falling_thing();
        chest_pieces_move();
    }
    run_object_lists();
    fn_15cc4_c();
    falcon_seek_target();
    guest.value_c6c = 3030;
    guest.flag_20e = -1;
    update_creatures();
    if (guest.flag_23a == 0) {
        move_shards();
        face_bolts_move();
        face_bolt_pieces_move();
        draw_effects();
        draw_marks_316();
    }
    guest.scroll_x_a = guest.scroll_x;
    guest.scroll_y_a = guest.scroll_y;
    if (guest.flag_290 >= 0)
        world_end_auto_walk();
    else
        player_control();
    count_zones();
    game_frame_tail();
}

/* orig $006002: the 1600 bytes line_text points at are filled with a pattern of 56 bytes (fourteen longs): the words of fill_words
 * made longs, or the longs of fill_longs when flag_290 is not negative (then flag_210 is cleared as well) */
void fill_buffer(void)
{
    uint32_t pattern[14];

    for (int i = 0; i < 14; i++)
        pattern[i] = (uint32_t)(int32_t)(int16_t)fill_words[i];
    if ((int16_t)guest.flag_290 >= 0) {
        for (int i = 0; i < 14; i++)
            pattern[i] = fill_longs[i];
        guest.flag_210 = 0;
    }

    uint32_t *end = (uint32_t *)((uint8_t *)guest.line_text + 1600);

    for (int k = 0; k < 28; k++) {
        end -= 14;
        for (int i = 0; i < 14; i++)
            end[i] = pattern[i];
    }
    end -= 8;
    for (int i = 0; i < 8; i++)
        end[i] = pattern[i];
}

/* orig $003EA4 (and $003E8C): when the scroll has gone on 32 pixels sideways (or a flag says so) and 32 or more rows since the
 * last time, the records of the things of the level are looked at: a record that is in use (third word not negative) and inside
 * the screen with 64 to the left, 48 above, 384 to the right and 240 below is marked (bit 0 of its last byte; near_flags says
 * for the new ones), the others lose their flags word */
void activate_records(void)
{
    int16_t d0;

    if ((guest.scroll_x & 31) != 0) {
        guest.flag_b24 = 0;
    } else if (guest.flag_b24 == 0) {
        activate_records_now();
        return;
    }
    d0 = guest.scroll_y - guest.records_scroll_y;
    if (d0 < 0)
        d0 = -d0;
    if (!(d0 & 0x20))
        return;
    activate_records_now();
}

/* orig $003EB4: the part of activate_records that marks the records (also called at the start of a level) */
void activate_records_now(void)
{
    guest.flag_b24 = 1;
    guest.records_scroll_y = guest.scroll_y;

    int16_t x0 = (guest.scroll_x & -32) - 64;
    int16_t y0 = (guest.scroll_y & -16) - 48;
    int16_t x1 = (guest.scroll_x & -32) + 384;
    int16_t y1 = (guest.scroll_y & -16) + 240;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[2] < 0 || record->w[0] < x0 || record->w[0] > x1 || record->w[1] < y0 || record->w[1] > y1) {
            record->w[3] = 0;
            continue;
        }
        if (!(record->w[3] & 1)) {
            record->w[3] |= 1;
            guest.near_flags[i] = 0xff;
        }
    }
}

/* orig $009912: which of the seven zones the player's box is in (a box 32 by 48 from player_x, player_y less the scroll against
 * the limits of zone_limits) and how many zones of each group of three that is: zone_counts */
void count_zones(void)
{
    uint8_t *counts = guest.zone_counts;
    int16_t x = guest.player_x - guest.scroll_x;
    int16_t y = guest.player_y - guest.scroll_y;
    uint16_t mask = 0;

    *(uint32_t *)(guest.zone_counts) = 0;   /* $4B6 and $4BA: two longs */
    *(uint32_t *)(guest.zone_counts + 4) = 0;
    for (int zone = 0; zone < 7; zone++) {
        const uint16_t *limit = &zone_limits[zone * 4];

        if (!(x + 32 < (int16_t)limit[0] || y + 48 < (int16_t)limit[1] || x > (int16_t)limit[2] || y > (int16_t)limit[3]))
            mask |= 1;
        mask <<= 1;
    }
    counts++;   /* the counters begin at $4B7 */
    for (int group = 0; group < 6; group++, counts++) {
        for (int n = 0; n < 3; n++)
            if (mask & (1 << zone_groups[group * 3 + n]))
                (*counts)++;
    }
}

/* orig $003F8E: the ten effects at $3A196 (records of five words: x, y, ?, a count, ?) that are not free (x not negative) are drawn
 * where they are on the screen with the glyph 80 more than their count, and the count goes down; below 0 the effect is free
 * again.  effect_alive says whether any is alive */
void draw_effects(void)
{
    if (guest.effect_alive == 0)
        return;
    guest.effect_alive = 0;
    for (int i = 0; i < 10; i++) {
        int16_t *effect = &guest.effects[i * 5];

        if (effect[0] < 0)
            continue;
        guest.effect_alive = 1;
        glyph_draw(effect[0] - guest.scroll_x, effect[1] - guest.scroll_y, effect[3] + 80);
        if (--effect[3] < 0)
            effect[0] = -1;
    }
}

/* orig $005E02: the energy as shown goes a step a frame toward the player's: down (the sound is played once as it begins) or up
 * (to 24 at most) */
void update_energy_display(void)
{
    int16_t shown = (int16_t)guest.energy_shown;

    if (shown > (int16_t)guest.player_energy) {
        guest.energy_shown--;
        if (guest.flag_2e0 == 0) {
            play_sound(41, 14);
            guest.flag_2e0++;
        }
    } else if (shown < (int16_t)guest.player_energy) {
        guest.flag_2e0 = 0;
        shown++;
        if (shown > 24)
            shown = 24;
        guest.energy_shown = shown;
    } else {
        guest.flag_2e0 = 0;
    }
}

/* orig $003806: a row of glyphs (212) at the top of the screen, 16 pixels apart, one for each of the things the player has, at most 10 */
void draw_item_row(void)
{
    int16_t n = guest.item_count;

    if (n <= 0)
        return;
    if (n > 10)
        n = 10;
    for (int i = 0; i < n; i++)
        glyph_draw(16 * i, 0, 212);
}

/* orig $01700C: the seven rows of 40 bytes before work_area + 320 (the rows of the text of the message) are moved one pixel to the left,
 * each from its right end: the last bit that was shifted out is lost and a 0 comes in */
void work_area_shift_left(void)
{
    uint16_t *p = (uint16_t *)((uint8_t *)guest.work_area + 320);

    for (int row = 0; row < 7; row++) {
        uint16_t carry = 0;

        for (int word = 0; word < 20; word++) {
            uint16_t value = *--p;

            *p = (uint16_t)((value << 1) | carry);
            carry = value >> 15;
        }
    }
}

/* orig $016CA4: the message scrolls through the work area, a pixel column at a time: the next column of the text (message_wide counts
 * the columns, the characters are those of message_text, the font is 8 by 8) is or'ed in at the right end of the seven rows.  When
 * the text ends (a 0 character) or while message_wide is negative a count of frames goes on instead (message_x) and at 328 the
 * message is over */
void message_ticker(void)
{
    int16_t column = guest.message_wide;

    if (column < 0) {
        if (++guest.message_x >= 328) {
            guest.message_x = 0;
            guest.message_wide = 0;
            guest.message_arg = 0;
            guest.message_shown = 0;
        }
        return;
    }

    int16_t glyph = (int8_t)guest.message_text[(column & -8) >> 3];

    if (glyph == 0) {
        guest.message_x = 0;
        guest.message_wide = -1;
        guest.message_arg = 0;
        guest.flag_236 = 0;
        return;
    }

    int shift = 7 - (column & 7);
    uint16_t mask = (uint16_t)(0x80 >> (column & 7));
    const uint8_t *bits = font_8x8 + (glyph << 3);
    uint16_t *to = (uint16_t *)((uint8_t *)guest.work_area + 40 + 38);

    for (int row = 0; row < 7; row++) {
        *to |= (uint16_t)((bits[row] & mask) >> shift);
        to = (uint16_t *)((uint8_t *)to + 40);
    }
    guest.message_wide++;
}

/* orig $009F50: the cell of the level's second map (ptr_0b86 + 8192: a byte for each 32 by 16 pixels, 128 to a row; 1 is a wall) that holds the member's place (x, y): the same as map_cell */
uint8_t *walk_member_cell(const struct WalkMember *member)
{
    return map_cell(member->x, member->y);
}

/* orig $00A264: whether there is a wall at the member's place, `cell` being its cell (walk_member_cell): 1 when the cell is a wall, or when the member is 8 pixels or more into its
 * column of 32 and the cell to the right is one; else 0 */
int16_t walk_cell_wall(const struct WalkMember *member, const uint8_t *cell)
{
    if (cell[0] == 1)
        return 1;
    if ((member->x & 31) >= 8 && cell[1] == 1)
        return 1;
    return 0;
}

/* orig $00A242: the same for the row two cells lower (the floor under the member's feet): cell[$100] and cell[$101] */
int16_t walk_cell_wall_below(const struct WalkMember *member, const uint8_t *cell)
{
    if (cell[0x100] == 1)
        return 1;
    if ((member->x & 31) >= 8 && cell[0x101] == 1)
        return 1;
    return 0;
}

/* orig $00A6EA: in the map of the targets ptr_03ac (20 bytes a row, 400 bytes) the cell of the screen point (x, y) and the one under it are marked with $FF and bit 3 of flags_3b0 says something was
 * marked; the cell is counted from the point less 8, with 5 columns and 4 rows before the map's own first cell; a place outside the map is left alone */
void mark_target_cells(int16_t x, int16_t y)
{
    int16_t across = (int16_t)(((int16_t)(x - 8) >> 5) + 5);
    int16_t row = (int16_t)((int16_t)(y - 8) >> 4);
    int16_t offset = (int16_t)(across + (int16_t)(row << 2));

    offset = (int16_t)(offset + (int16_t)(row << 4));
    offset = (int16_t)(offset + 0x50);

    uint8_t *cell = guest.ptr_03ac + offset;

    if (cell < guest.ptr_03ac || cell >= guest.ptr_03ac + 0x17C)
        return;
    cell[0] = 0xFF;
    cell[0x14] = 0xFF;
    guest.flags_3b0 |= 8;
}

/* orig $00D47E: with param_index 1 the entry of table_ed0 index_1ea names (four words) is looked at: the one whose third word is
 * not 0 or that is not in the window of 9 by 11 cells (scroll in cells of 32 by 16) at the scroll is passed by; one in the window (third
 * word 0) may become a stone creature (spawn_stone_enemy, $D4E2); index_1ea goes on 8, back to 0 at 256 or when the first long of the table
 * is negative */
void next_table_entry(void)
{
    if (guest.param_index != 1) {
        guest.index_1ea = 0;
        return;
    }
    int16_t *entry = (int16_t *)((uint8_t *)guest.table_ed0 + (int16_t)guest.index_1ea);

    if (entry[2] == 0) {
        int16_t cell_x = guest.scroll_x >> 5;
        int16_t cell_y = guest.scroll_y >> 4;

        if (entry[0] > cell_x && entry[1] > cell_y && entry[0] < cell_x + 9 && entry[1] < cell_y + 11)
            spawn_stone_enemy(entry);   /* $D4E2 (and on at $D4BE) */
    }
    int16_t next = (int16_t)guest.index_1ea + 8;

    if (next >= 256 || ((int32_t)((guest.table_ed0[0] << 16) | (uint16_t)guest.table_ed0[1])) < 0) {
        guest.index_1ea = 0;
        return;
    }
    guest.index_1ea = next;
}

/* orig $003C56: the player's place in the buffer line_text points at, 101 bytes in: four bytes of $FD around where the player is
 * on the screen (a cell of 8 pixels, 12 bytes of the buffer a row of cells) */
void mark_player_cells(void)
{
    int16_t x = (guest.player_x - guest.scroll_x) >> 3;
    int16_t y = (int16_t)(guest.player_y - guest.scroll_y) & -8;
    uint8_t *p = (uint8_t *)guest.line_text + 100 + 1;

    p += x;
    y = (int16_t)(y << 1);
    p += y;
    y = (int16_t)(y << 1);
    p += y;
    p[0] = 0xfd;
    p[3] = 0xfd;
    p[288] = 0xfd;
    p[291] = 0xfd;
}

/* orig $007292: the ten records of three words at $721E that are in use (the third word not negative) are drawn as sprites (their count
 * plus 100) where they are on the screen, and their counts go down; when the last has run out flag_721c is cleared */
void draw_sprites_721e(void)
{
    int16_t alive = 0;

    if (guest.flag_721c == 0)
        return;
    for (int i = 0; i < 10; i++) {
        int16_t *record = &guest.records_721e[i * 3];

        if (record[2] < 0)
            continue;
        alive++;
        sprite_draw(record[0] - guest.scroll_x, record[1] - guest.scroll_y, record[2] + 100);
        if (--record[2] < 0) {
            record[2] = -1;
            alive--;
        }
    }
    if (alive == 0)
        guest.flag_721c = 0;
}

/* orig $003DFC: when records_3a0a6_in_use says so, the twenty records of six words at $3A0A6 that are in use (first word not negative) move
 * (the third and fourth words are added to the second and third: x and y) and are drawn as glyphs of the sixth word where they are on
 * the screen; the sixth word goes up and at 69 the record is free again (and records_3a0a6_in_use down) */
void move_records_3a0a6(void)
{
    if (guest.records_3a0a6_in_use == 0)
        return;
    for (int i = 0; i < 20; i++) {
        int16_t *record = &guest.records_3a0a6[i * 6];

        if (record[0] < 0)
            continue;
        record[1] += record[3];
        record[2] += record[4];
        glyph_draw(record[1] - guest.scroll_x, record[2] - guest.scroll_y, record[5]);
        record[5]++;
        if (record[5] >= 69) {
            record[0] = -1;
            guest.records_3a0a6_in_use--;
        }
    }
}

/* orig $003F46: an effect is started where (x, y) is on the screen (x and y are the screen's, the scroll is added): in the first free
 * record of the ten at `effects` (the first if none is alive), with the kind (`d2`), a count of 6 and -12; none if all are in use */
void spawn_effect(int16_t x, int16_t y, int16_t kind)
{
    int16_t *effect = guest.effects;

    if (guest.effect_alive != 0) {
        int i;

        for (i = 0; i < 10; i++, effect += 5)
            if (effect[0] < 0)
                break;
        if (i == 10)
            return;
    }
    effect[0] = x + guest.scroll_x;
    effect[1] = y + guest.scroll_y;
    effect[2] = kind;
    effect[3] = 6;
    effect[4] = -12;
    guest.effect_alive = 1;
}

/* orig $010470: the player (a box 32 wide and 16 high at the player) against the twenty records of 8 bytes at $19334: the first that
 * is in use (third word above 0) and inside it sets flag_1f0, copies its third and fourth words and plays sound 5 with priority 25 */
void check_pickups(void)
{
    int16_t x0 = guest.player_x - 16, y0 = guest.player_y - 8, x1 = guest.player_x + 16, y1 = guest.player_y + 8;

    for (int i = 0; i < 20; i++) {
        struct Slot8 *record = &guest.records_19334[i];

        if (record->w[2] <= 0)
            continue;
        if (record->w[0] < x0 || record->w[0] > x1 || record->w[1] < y0 || record->w[1] > y1)
            continue;
        guest.flag_1f0 = 1;
        guest.word_200 = record->w[2];
        guest.word_202 = record->w[3];
        play_sound(5, 25);
        return;
    }
}

/* the eight pixels at a byte address of a bitplane (the planes are words: the first of the two bytes of a word is the left eight pixels, its high half) */
static uint8_t plane_byte(const uint8_t *p)
{
    const uint16_t *word = (const uint16_t *)((uintptr_t)p & ~(uintptr_t)1);

    return ((uintptr_t)p & 1) ? (uint8_t)(*word & 0xff) : (uint8_t)(*word >> 8);
}

static void set_plane_byte(uint8_t *p, uint8_t pixels)
{
    uint16_t *word = (uint16_t *)((uintptr_t)p & ~(uintptr_t)1);

    *word = ((uintptr_t)p & 1) ? (uint16_t)((*word & 0xff00) | pixels) : (uint16_t)((*word & 0x00ff) | (pixels << 8));
}

/* orig $0058C8: one digit (0 to 15) of digit_font is put in the four planes (150 bytes apart) of the score image, 8 rows of a byte, a
 * row 10 bytes on */
static void draw_score_digit(uint8_t *to, uint16_t digit)
{
    const uint8_t *from = guest.digit_font + (digit << 5);

    for (int row = 0; row < 8; row++) {
        set_plane_byte(to, *from++);
        set_plane_byte(to + 150, *from++);
        set_plane_byte(to + 300, *from++);
        set_plane_byte(to + 450, *from++);
        to += 10;
    }
}

/* orig $005842: when score_redraw says so the eight digits of the score are drawn in the score image (image 121 of the image table, 48
 * bytes in, from the right: the low digit of the first word first), then the routine at $5AF6 compares it with the high scores */
void update_score_display(void)
{
    if (guest.score_redraw == 0)
        return;
    guest.score_redraw = 0;

    uint8_t *to = *(uint8_t **)((uint8_t *)guest.image_table + 968 + 4) + 48;

    for (int w = 0; w < 4; w++) {
        draw_score_digit(to, guest.score_words[w] & 15);
        to--;
        draw_score_digit(to, guest.score_words[w] >> 4);
        to--;
    }
    check_high_scores();
}


/* ======== from game_a.c ======== */
/* Gods, the game: the routines at $0000-$7FFF (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */

/* orig $003436, $0077E6 and $007906: routines that only return (the game loop calls them; there is nothing in them) */
void game_nothing(void)
{
}

/* the ten pieces a chest bursts into (orig $3A1FA: the second half of effects, ten of five words: x, y, the sideways speed, the fall speed (1 to 4), the sprite less 170, negative when the
 * piece is not there) */
static int16_t *chest_pieces(void)
{
    return guest.effects + 50;
}

/* orig $00401A (the record: x, y and the number of an open chest): ten pieces start at the chest's place, each with a sideways speed of 1 to 8 (left when bit 7 of the random number is set)
 * and a fall speed of -3 to -10 (the glyph from words_3fde, 16 bytes on for each chest); word_2ae is 1 */
static void chest_burst(const struct Slot8 *record)
{
    int16_t chest = (int16_t)(((record->w[2] - 12) >> 1) << 3);   /* (the original: 16 bytes for each chest of two numbers) */

    chest_burst_at(record->w[0], record->w[1], words_3fde + chest);
}

/* orig $004034 (inside chest_burst): the ten pieces start at (x, y) with the glyphs `sprites` */
static void chest_burst_at(int16_t x, int16_t y, const uint16_t *sprites)
{
    int16_t *piece = chest_pieces();

    for (int i = 0; i < 10; i++, piece += 5) {
        uint16_t random = random_word();
        int16_t speed = (int16_t)((random & 7) + 1);

        piece[0] = x;
        piece[1] = y;
        piece[2] = (random & 0x80) ? (int16_t)-speed : speed;
        piece[3] = (int16_t)-(int16_t)((random_word() & 7) + 3);
        piece[4] = (int16_t)*sprites++;
    }
    guest.word_2ae = 1;
}

/* orig $004070: while word_2ae is set the pieces of a chest are moved by their speeds and drawn (glyph + 170); the fall speed goes up by one to 4 and the sideways speed
 * towards 0; a piece that has both ends there (4 and 0) is a thing of its own on the ground: the first free of records_19b14 gets its place, its sprite number as the kind and 1, its byte in
 * near_flags is set, and the piece is gone (no free record: it stays) */
void chest_pieces_move(void)
{
    if (guest.word_2ae == 0)
        return;
    guest.word_2ae = 0;

    int16_t *piece = chest_pieces();

    for (int i = 0; i < 10; i++, piece += 5) {
        if (piece[4] < 0)
            continue;

        int16_t x = (int16_t)(piece[0] + piece[2]);
        int16_t y = (int16_t)(piece[1] + piece[3]);
        int16_t sideways = piece[2];
        int16_t fall = piece[3];

        piece[0] = x;
        piece[1] = y;
        glyph_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)(piece[4] + 170));
        fall++;
        if (fall >= 4)
            fall = 4;
        if (sideways >= 0) {
            sideways--;
            if (sideways < 0)
                sideways = 0;
        } else {
            sideways++;
            if (sideways >= 0)
                sideways = 0;
        }
        if (fall == 4 && sideways == 0) {
            struct Slot8 *record = find_free_record_19b14();

            if (record == 0)
                continue;
            record->w[0] = x;
            record->w[1] = y;
            record->w[2] = piece[4];
            record->w[3] = 1;
            piece[4] = -1;
            guest.near_flags[record - guest.records_19b14] = 0xff;
            continue;
        }
        piece[2] = sideways;
        piece[3] = fall;
        guest.word_2ae = 1;
    }
}

/* the shutter's rows: for each of the 12 steps (shutter_row) a word with a bit for each of the 24 rows of its columns (12 from the top, then 12
 * from the bottom: the bits are read from the high end for the first half and from bit 4 up for the second) */
static int shutter_rows(const uint8_t *from, uint8_t *to, uint16_t mask)
{
    int count = 0;
    uint16_t bits = mask;

    for (int i = 0; i < 12; i++) {
        int set = (bits & 0x8000) != 0;

        bits <<= 1;
        if (set) {
            for (int plane = 0; plane < 4; plane++)
                for (int b = 0; b < 4; b++)
                    to[plane * 96 + b] = from[plane * 96 + b];
            to += 4;
            count++;
        }
        from += 4;
    }
    bits = mask >> 4;
    for (int i = 0; i < 12; i++) {
        int set = bits & 1;

        bits >>= 1;
        if (set) {
            for (int plane = 0; plane < 4; plane++)
                for (int b = 0; b < 4; b++)
                    to[plane * 96 + b] = from[plane * 96 + b];
            to += 4;
            count++;
        }
        from += 4;
    }
    return count;
}

/* orig $0054C4 (and inlined: $0055EC): the shutter that opens and closes over the picture at the foot of the screen (image 115, four
 * columns of 32 pixels and 24 rows each, 384 bytes a column) is drawn while shutter_row is not negative.  shutter_row is the step (0 to 11,
 * 12 is open); shutter_direction, added to it, says which way it goes.  At a step under 12 the rows named by the step's word in
 * shutter_masks (shutter_rows) are packed into ptr_0e44 and blitted, a column at a time, the part moving down a row of 44
 * bytes a step; at 12 the picture is drawn whole.  A step to 11 plays sound 22; at 12 and with shutter_direction 0 the step goes back by 1
 * (or shutter_direction turns to -1 when word_2bc was set) */
void draw_shutter(void)
{
    if (guest.shutter_row < 0)
        return;

    const uint8_t *image = (const uint8_t *)guest.image_table[115].data;
    uint8_t *buffer = guest.ptr_0e44;
    uint8_t *screen = guest.screen_buffer + 7492 + (((guest.x_offset & 31) & -16) >> 3);
    int16_t shift = (guest.x_offset & 31) & 15;

    if (guest.shutter_row == 12) {
        screen -= 484;
        for (int column = 0; column < 4; column++) {
            blit_image_wide(screen, image, 96, shift, 24);
            screen += 4;
            image += 384;
        }
        if (guest.shutter_direction != 0) {
            guest.shutter_row--;
            return;
        }
        if (guest.word_2bc != 0) {
            guest.shutter_direction = -1;
            guest.word_2bc = 0;
        }
        return;
    }
    screen -= (uint16_t)guest.shutter_row * 44;
    for (int column = 0; column < 4; column++) {
        int rows = shutter_rows(image, buffer, shutter_masks[guest.shutter_row]);

        if (rows != 0)
            blit_image_wide(screen, buffer, 96, shift, rows);
        image += 384;
        screen += 4;
    }
    guest.shutter_row += guest.shutter_direction;
    if (guest.shutter_direction < 0)
        return;
    if (guest.shutter_row != 11)
        return;
    play_sound(22, 2);
}

/* orig $005496: word_2bc is set to 1 unless shutter_row is 12 (then sound 57, priority 40, is played and the shutter's way shutter_direction is -1) */
void game_step_5496(void)
{
    if (guest.shutter_row != 12) {
        guest.word_2bc = 1;
        return;
    }
    play_sound(57, 40);
    guest.shutter_direction = -1;
}

static void aimed_bolt_hits_player(uint32_t *record, int16_t x, int16_t y);

/* orig $0073D2: the shards the stone faces spit (records_737e, ten of struct Shard; word_73ce says there are some): each falls 2 pixels (not into a wall: a cell of the second map that is 1 under it
 * sends it back up), its picture goes on (every other frame) through the glyphs 529, 530, 531, 530, it hits the player ($0184EE: the energy goes down, the shard is over) and is drawn at its place,
 * and its life runs down; at under 0 it is free */
void move_shards(void)
{
    struct Shard *shards = (struct Shard *)guest.records_737e;
    int16_t alive = 0;

    if (guest.word_73ce == 0)
        return;
    for (int i = 0; i < 10; i++) {
        struct Shard *shard = &shards[i];
        int16_t x = shard->x;
        int16_t y;
        int16_t frame = shard->frame;

        if (x < 0)
            continue;
        y = (int16_t)(shard->y + 2);
        shard->y = y;
        if (guest.ptr_0b86[0x2000 + (((int16_t)(x & 0xFFE0)) >> 5) + (((int16_t)((y + 8) & 0xFFF0)) << 3)] == 1) {
            y = (int16_t)(y - 2);
            shard->y = y;
        }
        uint16_t picture = pictures_747e[frame & 3];

        if (guest.frame_toggle != 0) {
            frame = (int16_t)((frame + 1) & 3);
            shard->frame = frame;
        }
        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);

        aimed_bolt_hits_player((uint32_t *)shard, (int16_t)(sx - 6), (int16_t)(sy + 2));
        glyph_draw(sx, sy, picture);
        alive = 1;
        shard->life--;
        if (shard->life < 0)
            shard->x = -1;
    }
    guest.word_73ce = (uint16_t)alive;
}

static void start_debris_moving(int16_t x, int16_t y, int16_t speed_x, int16_t speed_y);
int16_t damage_player_at(int16_t x, int16_t y, int16_t strength);

/* orig $0076E8 to $0076FB: the speeds (x, y) the bolts of the spitting faces and the pieces a bolt bursts into start with */
static const int16_t bolt_speeds_76e8[10] = { 4, 4, -4, 4, -1, -6, 0, -7, 1, -6 };

/* orig $00773E (with $0074CA, $007752 to $00777C): `count` records (five words: x, y, picture step, speed x, speed y) are put at the first free ones (step negative) of the `records` records from `first` on (the list at
 * records_7544: 12 of the bolts, then 30 of the pieces), at (x + 3, y + 11), the speeds from the table */
static void bolt_records_put(int first, int records, int count, int16_t x, int16_t y, const int16_t *speeds)
{
    for (int i = first; i < first + records && count > 0; i++) {
        int16_t *r = &guest.records_7544[i * 5];

        if (r[2] >= 0)
            continue;
        guest.code_77e6 = 0x4E71;
        guest.code_7906 = 0x4E71;
        r[0] = (int16_t)(x + 3);
        r[1] = (int16_t)(y + 11);
        r[2] = 0;
        r[3] = speeds[0];
        r[4] = speeds[1];
        speeds += 2;
        count--;
    }
}

/* orig $0076FC (the face's step 3 of the movement routine $74F0): two bolts are put at the face's place, with the sound 58 when the face is on the screen (within 24 pixels outside it) */
static void face_spits_bolts(int16_t x, int16_t y)
{
    int16_t sx = (int16_t)(x - guest.scroll_x);
    int16_t sy = (int16_t)(y - guest.scroll_y);

    if (sx > -24 && sx < 320 && sy > -24 && sy < 192)
        play_sound(0x3A, 3);
    bolt_records_put(0, 12, 2, x, y, bolt_speeds_76e8);
}

/* orig $0077E6 (which is an rts until a bolt is put there): the bolts of the faces (the first 12 records): each moves by its speed, off the screen (16 or more outside it) it is over, else it is drawn as the glyph 529 + its
 * step; every other frame (frame_phase 0) its step goes on (modulo 4), it falls faster (speed y + 1) and its sideways speed runs down by 1.  A wall cell (the byte $80 on in the second map is 1) under it makes it burst into three pieces
 * ($77A0, the speeds from $76F0) with the sounds 58 off and 59 on; the player's box hit ($00D270, a 5 by 5 box) bursts it with a spark moving as it did */
void face_bolts_move(void)
{
    if (guest.code_77e6 != 0x4E71)
        return;
    int alive = 0;

    for (int i = 0; i < 12; i++) {
        int16_t *r = &guest.records_7544[i * 5];

        if (r[2] < 0)
            continue;
        alive++;

        int16_t step = r[2];
        int16_t x = (int16_t)(r[0] + r[3]);
        int16_t y = (int16_t)(r[1] + r[4]);

        r[0] = x;
        r[1] = y;

        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);

        if (sx <= -16 || sy <= -16 || sx >= 320 || sy >= 192) {
            r[2] = -1;
            continue;
        }
        glyph_draw(sx, sy, (uint16_t)(step + 0x211));
        if (guest.frame_phase == 0) {
            r[2] = (int16_t)((r[2] + 1) & 3);
            r[4]++;
            if (r[3] != 0)
                r[3] = (int16_t)(r[3] < 0 ? r[3] + 1 : r[3] - 1);
        }
        if (guest.ptr_0b86[0x2000 + 0x80 + (((int16_t)(x + 8)) >> 5) + ((((int16_t)(y + 4)) & 0xFFF0) << 3)] == 1) {
            stop_sound(0x3A);
            play_sound(0x3B, 3);
            bolt_records_put(12, 30, 3, (int16_t)(x + 2), (int16_t)(y + 2), &bolt_speeds_76e8[4]);
            r[2] = -1;
            continue;
        }
        guest.hit_box_extra_width = 5;
        guest.hit_box_extra_height = 5;
        if (damage_player_at(x, y, 0) < 0) {
            r[2] = -1;
            start_debris_moving(sx, sy, r[3], r[4]);
        }
    }
    if (alive == 0)
        guest.code_77e6 = 0x4E75;
}

/* orig $007908 (a rts, $007906, until a piece is put there): the pieces the bolts burst into (the 30 records after the first 12): each moves by its speed, off the screen it is over, is drawn as the glyph 533 + its step, every other frame
 * its step goes on and it falls faster; a wall cell (the first byte of the cell is 1) bursts it with a spark, the player's box ($0184EE) takes 2 of the energy */
void face_bolt_pieces_move(void)
{
    if (guest.code_7906 != 0x4E71)
        return;
    int alive = 0;

    for (int i = 12; i < 42; i++) {
        int16_t *r = &guest.records_7544[i * 5];

        if (r[2] < 0)
            continue;
        alive++;

        int16_t step = r[2];
        int16_t x = (int16_t)(r[0] + r[3]);
        int16_t y = (int16_t)(r[1] + r[4]);

        r[0] = x;
        r[1] = y;

        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);
        int burst = sx <= -16 || sy <= -16 || sx >= 320 || sy >= 192;

        if (!burst) {
            glyph_draw(sx, sy, (uint16_t)(step + 0x215));
            if (guest.frame_phase == 0) {
                r[2] = (int16_t)((r[2] + 1) & 3);
                r[4]++;
            }
            burst = guest.ptr_0b86[0x2000 + (((int16_t)(x + 5)) >> 5) + ((((int16_t)(y - 2)) & 0xFFF0) << 3)] == 1;
            if (!burst) {
                aimed_bolt_hits_player((uint32_t *)&r[2], (int16_t)(sx + 2), (int16_t)(sy + 2));
                continue;
            }
        }
        r[2] = -1;
        start_debris_moving((int16_t)(sx - 2), (int16_t)(sy - 2), r[3], r[4]);
    }
    if (alive == 0)
        guest.code_7906 = 0x4E75;
}

/* orig $0057E0: a blank slot picture (image 119, 384 bytes: 24 rows, four planes of 96 bytes... as four longs a row) is copied to `to` */
void panel_blank(uint8_t *to)
{
    const uint8_t *from = (const uint8_t *)guest.image_table[119].data;

    for (int i = 0; i < 384; i++)
        to[i] = from[i];
}

static uint32_t ror4(uint32_t value)
{
    return (value >> 4) | (value << 28);
}

/* orig $00570A: the glyph (the picture of the item; 279 and 280 are the small pictures 204 and 183) is put into the slot picture `to` (24 rows
 * of 32 pixels, four planes 96 bytes apart), 16 pixels in and centred in height, moved 4 pixels right: the pixels of the
 * glyph are cut in over the blank (its mask is all four planes or'ed).  The original keeps the words of its four colour planes
 * in registers whose upper halves stay from the row before (the move 4 to the right rotates the whole long), and so does this */
static void panel_item(uint8_t *to, int16_t glyph)
{
    uint32_t d0 = 0, d1 = 0, d2 = 0, d3 = 0;

    if (glyph >= 279 && glyph <= 280)
        glyph = (glyph == 280) ? 183 : 204;

    const struct Image *image = &guest.image_table[glyph];
    int16_t height = (int16_t)image->height;
    const uint8_t *source = (const uint8_t *)image->data;
    int16_t stride = (int16_t)((height + 1) * 2);
    uint16_t *dst = (uint16_t *)(to + 16 + (((16 - height) >> 1) << 2));
    const uint16_t *p0 = (const uint16_t *)source;
    const uint16_t *p1 = (const uint16_t *)(source + stride);
    const uint16_t *p2 = (const uint16_t *)(source + 2 * stride);
    const uint16_t *p3 = (const uint16_t *)(source + 3 * stride);
    const uint32_t keep = 0xf0000fff;   /* the original's long $0000FFFF rotated right by 4 */

    for (int row = 0; row <= height; row++) {
        d0 = (d0 & 0xffff0000) | *p0++;
        d1 = (d1 & 0xffff0000) | *p1++;
        d2 = (d2 & 0xffff0000) | *p2++;
        d3 = (d3 & 0xffff0000) | *p3++;
        d0 = ror4(d0) & keep;
        d1 = ror4(d1) & keep;
        d2 = ror4(d2) & keep;
        d3 = ror4(d3) & keep;

        uint32_t d4 = ~(d0 | d1 | d2 | d3);

        dst[0] = (uint16_t)((dst[0] & d4) | d0);
        dst[48] = (uint16_t)((dst[48] & d4) | d1);
        dst[96] = (uint16_t)((dst[96] & d4) | d2);
        dst[144] = (uint16_t)((dst[144] & d4) | d3);
        d0 = (d0 << 16) | (d0 >> 16);
        d1 = (d1 << 16) | (d1 >> 16);
        d2 = (d2 << 16) | (d2 >> 16);
        d3 = (d3 << 16) | (d3 >> 16);
        d4 = (d4 << 16) | (d4 >> 16);
        dst[1] = (uint16_t)((dst[1] & d4) | d0);
        dst[49] = (uint16_t)((dst[49] & d4) | d1);
        dst[97] = (uint16_t)((dst[97] & d4) | d2);
        dst[145] = (uint16_t)((dst[145] & d4) | d3);
        dst += 2;
    }
}

/* orig $0056BE: a slot picture of item `item` (index of table_14cf8; negative: none) */
void panel_slot_plain(uint8_t *to, int16_t item)
{
    panel_blank(to);
    if ((int16_t)(item << 1) < 0)
        return;
    panel_item(to, (int16_t)guest.table_14cf8[item].w[0]);
}

/* orig $0056D8: the same with the slot picture's part cut away first (the words at +96+12 and on, 18 rows: all but the three top bits of the
 * first word and bits 8 to 10 of the second) */
void panel_slot_cut(uint8_t *to, int16_t item)
{
    panel_blank(to);
    for (int i = 0; i < 18; i++) {
        *(uint16_t *)(to + 12 + 4 * i + 96) &= 0xe000;
        *(uint16_t *)(to + 12 + 4 * i + 98) &= 0x0700;
    }
    if ((int16_t)(item << 1) < 0)
        return;
    panel_item(to, (int16_t)guest.table_14cf8[item].w[0]);
}

/* orig $005608 (with $0056BE, $0056D8, $00570A and $0057E0 inlined): when the slot that is chosen (item_chosen) is another than the one
 * shown (item_shown) the picture of the old one (image 115 and the slot's number) is made plain and the new one's cut, and the new
 * is the one shown */
void update_item_panel(void)
{
    int16_t chosen = (int16_t)guest.item_chosen;

    if (chosen == (int16_t)guest.item_shown)
        return;
    panel_slot_plain((uint8_t *)guest.image_table[115 + (int16_t)guest.item_shown].data, guest.item_slots[(int16_t)guest.item_shown]);
    panel_slot_cut((uint8_t *)guest.image_table[115 + chosen].data, guest.item_slots[chosen]);
    guest.item_shown = guest.item_chosen;
}

/* orig $007E90: where in the map of the level (ptr_0b86 + 8192, a byte for each cell of 32 by 16 pixels, 128 bytes a row of
 * cells... 8 bytes a 16 pixel step of y) the point (x, y) is */
uint8_t *map_cell(int16_t x, int16_t y)
{
    int16_t across = (int16_t)(x & -32) >> 5;
    int16_t down = (int16_t)((y & -16) << 3);

    return guest.ptr_0b86 + 8192 + across + down;
}

/* the BCD add (abcd) of two bytes of two decimal digits, with the carry */
static uint8_t bcd_add(uint8_t a, uint8_t b, int *carry)
{
    int low = (a & 15) + (b & 15) + *carry;
    int high = (a >> 4) + (b >> 4);

    if (low > 9) {
        low -= 10;
        high++;
    }
    *carry = 0;
    if (high > 9) {
        high -= 10;
        *carry = 1;
    }
    return (uint8_t)((high << 4) | low);
}

/* the texts of the two messages (in the original's data, where the digits of the high score are put in: $5C52 and $5C79) */
static uint8_t text_extra_life[] = "\x11\x11 Extra life for    50,000  points \x11\x11";
static uint8_t text_bonus_gem[] = "\x11\x11 Bonus Gem for     0,000 points \x11\x11";

/* orig $005C1A: the four digits of the high word of `value` (the thousands) are written to `to`, the leading zeros as blanks */
static void high_score_digits(uint8_t *to, uint32_t value)
{
    uint16_t word = (uint16_t)(value >> 16);
    int seen = 0;

    for (int shift = 12; shift >= 0; shift -= 4) {
        uint8_t digit = (uint8_t)((word >> shift) & 15);

        if (digit == 0 && !seen) {
            *to++ = ' ';
        } else {
            *to++ = (uint8_t)('0' + digit);
            seen = 1;
        }
    }
}

/* orig $005B76 (from $005B98 and $005B72): the high score that follows when the table of them has ended: the two bytes of the high word of `current` get 20 added as decimal digits */
static int32_t high_score_after(int32_t current)
{
    uint16_t word = (uint16_t)((uint32_t)current >> 16);
    int carry = 0;
    uint8_t low = bcd_add((uint8_t)(word & 0xFF), 0x20, &carry);
    uint8_t high = bcd_add((uint8_t)(word >> 8), 0, &carry);

    return (int32_t)((uint32_t)((uint16_t)((high << 8) | low)) << 16);
}

/* the entry `index` (a count of bytes, 4 a score) of a table of four high scores that the original ends with $FFFFFFFF */
static int32_t level_score_at(const uint32_t *table, uint16_t index)
{
    return (index >> 2) < 4 ? (int32_t)table[index >> 2] : -1;
}

/* orig $005AF6 (and inlined: $006882): the score as one long (the low byte of each of its four words, the low digits in the lowest byte) is compared with the two
 * high scores of the level.  At or over the first ($5B04) the next high score of the table (or 20 more than this one when the table is at its end) becomes the first, a life is
 * added ($5AC2: the sound 31, and the message "Extra life for N points" with the score that was passed) ; then, while the level is not ending, at or over the second ($5B46)
 * the same with its table: the message "Bonus Gem for N points", a bonus gem (kind 18) dropped at the player and the sound 49 */
void check_high_scores(void)
{
    uint32_t score = ((uint32_t)guest.score_words[3] << 24) | ((uint32_t)guest.score_words[2] << 16) | ((uint32_t)guest.score_words[1] << 8) |
                     guest.score_words[0];

    if ((int32_t)score >= guest.high_score_baa) {
        int32_t next;
        int32_t passed;

        guest.word_27c = (uint16_t)(guest.word_27c + 4);
        next = level_score_at(table_5b9e, guest.word_27c);
        if (next < 0) {
            guest.word_27c = (uint16_t)(guest.word_27c - 4);
            next = high_score_after(guest.high_score_baa);
        }
        passed = guest.high_score_baa;
        guest.high_score_baa = next;
        high_score_digits(text_extra_life + 0x12, (uint32_t)passed);
        guest.item_count++;   /* $5AC2: a life */
        play_sound(0x1F, 0x37);
        guest.message_next = text_extra_life;
        message_request(0x32);
    }
    if (guest.level_ended != 0)
        return;
    if ((int32_t)score < guest.high_score_bae)
        return;

    int32_t next;
    int32_t passed;

    guest.word_27e = (uint16_t)(guest.word_27e + 4);
    next = level_score_at(table_5bb2, guest.word_27e);
    if (next < 0) {
        guest.word_27e = (uint16_t)(guest.word_27e - 4);
        next = high_score_after(guest.high_score_bae);
    }
    passed = guest.high_score_bae;
    guest.high_score_bae = next;
    high_score_digits(text_bonus_gem + 0x11, (uint32_t)passed);   /* $5BC6 */
    guest.message_next = text_bonus_gem;
    message_request(0x31);
    drop_thing((int16_t)(guest.player_x + 8), (int16_t)(guest.player_y - 4), 0x12);
    play_sound(0x31, 0x1E);
}

/* orig $004718 (and inlined: $0047C8, $0047CC, $0047D0, $0047D4): `points` is added to the score, a decimal digit at a time (the
 * low digit of the remainder of division by 10 to the low digit of the first word, the next to its high digit, then the second
 * word and so on, four words of two digits) and score_redraw says the score is to be drawn again.  The original begins with the carry
 * (X flag) the caller left: the pick-up at $4654 comes from `asr.w #3,d0`, so it is the last bit shifted out */
void score_add_carry(int16_t points, int carry_in)
{
    int carry = carry_in;

    for (int i = 0; i < 8; i++) {
        int digit = (points % 10) << ((i & 1) ? 4 : 0);   /* the odd digits are the high digits of a byte */

        points = points / 10;
        for (int word = i >> 1; word < 4; word++) {
            uint8_t sum = bcd_add((uint8_t)guest.score_words[word], word == (i >> 1) ? (uint8_t)digit : 0, &carry);

            guest.score_words[word] = (uint16_t)((guest.score_words[word] & 0xff00) | sum);
        }
        guest.score_redraw = 1;
        if (points == 0)
            return;
    }
}

/* ---- the end of a level (or of one of its parts): the screen with the bonus, orig $012E08 ---- */


/* the original's divs.w #10: the quotient of the long and the word, the remainder as the word of the result that swap brings down.  When the quotient does
 * not fit in a word the long is left as it is: the digit is its upper word and the next quotient its lower word as a long */
static uint16_t level_end_divide10(int32_t *value)
{
    uint32_t magnitude = *value < 0 ? (uint32_t)(-*value) : (uint32_t)*value;
    uint32_t quotient = 0;
    uint16_t rest = 0;

    for (int shift = 24; shift >= 0; shift -= 8) {   /* a byte at a time: the 68000 divides no long, and the compiler has no routine for it here */
        rest = (uint16_t)((rest << 8) | ((magnitude >> shift) & 255));
        quotient = (quotient << 8) | (uint16_t)(rest / 10);
        rest = (uint16_t)(rest % 10);
    }
    int32_t signed_quotient = *value < 0 ? -(int32_t)quotient : (int32_t)quotient;

    if (signed_quotient > 32767 || signed_quotient < -32768) {
        uint16_t upper = (uint16_t)((uint32_t)*value >> 16);

        *value = (int16_t)(uint16_t)*value;
        return upper;
    }
    *value = signed_quotient;
    return (uint16_t)(*value < 0 || signed_quotient < 0 ? -(int16_t)rest : rest);
}

/* orig $01461C (and $014622 without the first line): the number `value` is drawn from (x, y) in glyphs 156 on, 16 pixels each: the six digits from the
 * left, those before the first digit that is not 0 left out, then the last digit always.  The digits are left in the words the original keeps them in */
static void draw_level_end_number(int16_t x, int16_t y, int32_t value, int clear_started)
{
    uint16_t last, next[5];

    if (clear_started)
        guest.digits_started = 0;
    last = level_end_divide10(&value);   /* $1026A */
    next[0] = level_end_divide10(&value);
    guest.word_234 = next[0];
    next[1] = level_end_divide10(&value);
    guest.word_232 = next[1];
    next[2] = level_end_divide10(&value);
    guest.word_2fe = next[2];
    next[3] = level_end_divide10(&value);
    guest.word_2fc = next[3];
    uint16_t top = level_end_divide10(&value);
    uint16_t order[5] = { top, guest.word_2fc, guest.word_2fe, guest.word_232, guest.word_234 };

    for (int i = 0; i < 5; i++) {
        uint16_t digit = order[i];

        if (digit != 0)
            guest.digits_started = (uint8_t)digit;
        if (digit != 0 || guest.digits_started != 0)
            glyph_draw(x, y, (uint16_t)(digit + 0x9C));
        x += 16;
    }
    glyph_draw(x, y, (uint16_t)(last + 0x9C));
    guest.digits_started = 1;
}

/* orig $01D7E8 and $01D7C4: the 16 rows of 24 bytes in four planes at the screen_buffer's row 40 (+$1774) are copied to the buffer line_text points at (one long of
 * each plane after the other) and back */
static void level_end_save_rect(void)
{
    const uint8_t *from = guest.screen_buffer + 0x1774;
    uint32_t *to = (uint32_t *)(uintptr_t)guest.line_text;

    for (int row = 0; row < 16; row++, from += 44)
        for (int i = 0; i < 6; i++)
            for (int plane = 0; plane < 4; plane++)
                *to++ = *(const uint32_t *)(from + i * 4 + plane * 8448);
}

static void level_end_restore_rect(void)
{
    uint8_t *to = guest.screen_buffer + 0x1774;
    const uint32_t *from = (const uint32_t *)(uintptr_t)guest.line_text;

    for (int row = 0; row < 16; row++, to += 44)
        for (int i = 0; i < 6; i++)
            for (int plane = 0; plane < 4; plane++)
                *(uint32_t *)(to + i * 4 + plane * 8448) = *from++;
}

/* orig $013186: the picture under the bonus is put back and the bonus counted so far is drawn over it */
static void level_end_draw_counter(void)
{
    level_end_restore_rect();
    draw_level_end_number(0xAC, 0x88, (int32_t)guest.level_end_counter, 0);
}

/* orig $0134E6: the sounds of the level are stopped and the sound system waited for until it has nothing to play (at most 1000 frames) */
static int level_end_wait_rule(const struct LevelEnd *record)
{
    /* the original waits a frame for the voices of the game's sounds on most of the end screens (the 2500 of the first level's, the "WORLD TWO" of the second's: seen in recordings); on the first
     * of the second level's ("WORLD ONE", the bonus 500) it does not: what sound was playing then is not modelled (the port's wait for the sound system takes the same blanks in both) */
    if (record->spots == 6 && record->end_effects == 1408)
        return 0;
    /* the "WORLD ONE" of the third level ($1450C: 100 spots, the effects 1921) has none either: the second's and the third's are the first of a list whose sound is the level's ambient one */
    if (guest.param_index >= 2) {
        static const char world_one[20] = "     WORLD  ONE     ";
        int same = 1;

        for (int i = 0; i < 20; i++)
            if (record->text[i] != (uint8_t)world_one[i])
                same = 0;
        if (same)
            return 0;
    }
    return 1;
}

static void level_end_quiet(void)
{
    static const uint8_t sounds[6] = { 0x13, 0x17, 0x34, 0x3C, 0x3A, 0x42 };

    for (int i = 0; i < 6; i++)
        stop_sound(sounds[i]);
    int16_t frames = 0x3E8;

    for (;;) {
        wait_vblank();
        if (--frames < 0)
            break;
        if (guest.sound_busy_207a0 == 0)
            break;
    }
}

/* orig $01CF38: wait for `frames` video frames, not for longer than a key (or the like: pending_request) is not waiting */
static void wait_frames_or_request(uint16_t frames)
{
    guest.pending_request = 0;
    for (;;) {
        if (take_pending_request() != 0)
            return;
        wait_vblank();
        if (--frames == 0)
            return;
    }
}

/* orig $01CFCE and the time it takes the original: copying the 33792 bytes of a screen with a loop of move.l takes about a frame */
static void level_end_copy(void)
{
    copy_other_screen();
    host_cpu_frames(1);
}

/* orig $0130AC: `count * unit` is added to the counted bonus, the counter is drawn, sound 54 is played, the row of things is drawn and the screen shows it */
static void level_end_count(uint16_t count, uint16_t unit)
{
    guest.level_end_counter += (uint32_t)count * unit;
    level_end_draw_counter();
    play_sound(0x36, 5);
    draw_item_row();
    host_cpu_frames(1);   /* (the numbers and the row of things take the original most of a frame to draw) */
    screens_flip();
    level_end_copy();
}

/* orig $005AA8: the BCD byte `step` is added to the score's first word's low byte, the carry on through the other three words */
static void level_end_add_score(uint8_t step)
{
    int carry = 0;

    for (int word = 0; word < 4; word++) {
        uint8_t sum = bcd_add((uint8_t)guest.score_words[word], word == 0 ? step : 0, &carry);

        guest.score_words[word] = (uint16_t)((guest.score_words[word] & 0xFF00) | sum);
    }
    guest.score_redraw = 1;
}

static void world_records_clear(void);   /* orig $0135C6 (below begin_quest) */
static void level_load_world(void);   /* orig $013532 (below begin_quest) */

/* orig $01D06C: the shift register of the passwords: the seed word ($1D0AA) is made another (its bits mixed through the carry of the rotates) */
static uint16_t password_seed;

#define ROXR(v) do { unsigned o = (v) & 1; (v) = (uint8_t)(((v) >> 1) | (x << 7)); x = o; } while (0)

static uint16_t password_next_1d06c(void)
{
    uint8_t d7 = (uint8_t)password_seed;
    uint8_t d6 = (uint8_t)(password_seed >> 8);
    unsigned x = (password_seed >> 7) & 1;   /* (the bit that the lsr.w #8 shifted out last) */
    uint8_t d0 = d6;

    ROXR(d0);
    d6 ^= d0;
    ROXR(d0);
    ROXR(d0);
    d6 ^= d0;
    ROXR(d0);
    d7 ^= d0;
    ROXR(d0);
    ROXR(d0);
    d0 &= 1;
    x = d7 >> 7;   /* asl.b #1,d7 */
    d7 = (uint8_t)(d7 << 1);

    unsigned top = d6 >> 7;   /* roxl.b #1,d6 */

    d6 = (uint8_t)((d6 << 1) | x);
    (void)top;
    d0 = (uint8_t)(d0 + d7);
    d7 = d0;
    password_seed = (uint16_t)((d6 << 8) | d7);
    return password_seed;
}

/* orig $013470: a code for the level: three numbers of 1 to 26 (the shift register's words by 2521, one added), kept as the three top bytes of a long, that none of the three saved codes ($D12, $D16, $D1A)
 * has; after a hundred tries the code is 1, 2, 3 and the level */
static uint32_t password_generate(void)
{
    int tries = 0x64;

    for (;;) {
        uint32_t code = 0;

        password_seed = random_word();
        for (int i = 0; i < 3; i++) {
            uint16_t word = password_next_1d06c();

            code = (code & 0xFFFFFF00u) | (uint8_t)(word / 0x9D9 + 1);
            code <<= 8;
        }
        tries--;
        if (tries < 0)
            return 0x1020300u + ((uint32_t)(guest.param_index & 0xFF) << 8);
        if (code == guest.password_slots[0] || code == guest.password_slots[1] || code == guest.password_slots[2])
            continue;
        return code;
    }
}

/* the centre of a text on the screen: 160 less 8 a character (the characters are 16 wide) */
static void password_text(const char *text, int16_t y)
{
    int16_t length = 0;

    while (text[length] != 0)
        length++;
    text_draw_at(text, (int16_t)(0xA0 - 8 * length), y);
}

/* orig $013234 (the effect of the bit 10 of a level end, from the second level on): the level's password is made (once: while the first byte of its code is 0) and shown on the picture of the level with "FIRE TO
 * CONTINUE": the frame is drawn, the title, the three letters ((byte - 1) + 'A') and the line are put on it, the screen fades out and in and the program waits for the fire button to be let go and pressed; the codes
 * are saved ($0067FE).  The frames it takes are not counted in the level's tick ($CE2) */
static void level_password_screen(void)
{
    uint32_t saved_ticks = guest.tick_count;
    int level = (int16_t)guest.param_index;

    if (level == 1)
        return;

    uint32_t *slot = &guest.password_slots[level - 2];

    if ((*slot >> 24) != 0) {
        guest.tick_count = saved_ticks;
        return;
    }
    *slot = password_generate();
    draw_start_frame();

    char title[17] = "LEVEL _ PASSWORD";
    char letters[4];

    title[6] = (char)(0x5D + level);
    for (int i = 0; i < 3; i++)
        letters[i] = (char)(((*slot >> (24 - 8 * i)) & 0xFF) - 1 + 0x41);
    letters[3] = 0;
    password_text(title, 0x14);
    password_text(letters, 0x34);
    password_text("FIRE TO CONTINUE", 0x96);
    host_cpu_frames(2);   /* (the frame the original draws before the fade-out) */
    fade_keep_screens = 1;
    host_fade_lag(1);
    fade_out(1, 0xFFFF);
    host_fade_lag(0);
    fade_keep_screens = 0;
    screens_flip();
    level_end_copy();
    host_fade_bind();
    host_fade_lag(1);
    fade_in(guest.palette_words, 0);
    host_fade_lag(0);
    while (guest.joystick & 0x80)
        wait_vblank();
    while (!(guest.joystick & 0x80))
        wait_vblank();
    scores_save();   /* ($0067FE: the codes xored with the constants of the save block, and the block written) */
    guest.tick_count = saved_ticks;
}

/* orig $0131E2: the routines the level end's bits name run, the bit 10 first (the table at $13234 backwards).  The ones that did not run in the recording are not written */
static void level_end_effects(const struct LevelEnd *record)
{
    static const uint32_t handlers[11] = { 0x13234, 0x135C6, 0x1361C, 0x13614, 0x1366E, 0x1368A, 0x13690, 0x13532, 0x143B6, 0x13646, 0x13532 };

    guest.level_ended = 0;
    for (int bit = 10; bit >= 0; bit--) {
        if (!(record->end_effects & (1u << bit)))
            continue;
        switch (handlers[10 - bit]) {
        case 0x13234:
            level_password_screen();
            break;
        case 0x1361C:   /* the things in the slots that are not kept (their entry's word +4 not 0) are put away */
            for (int i = 0; i < 4; i++) {
                int16_t item = guest.item_slots[i];

                if (item >= 0 && guest.table_14cf8[item].w[2] != 0)
                    guest.item_slots[i] = -1;
            }
            break;
        case 0x1366E:   /* the list of the teleport gems goes on where the level end says */
            guest.ptr_0b76 = table_1077a[(int16_t)(guest.param_index - 1)] + record->list_offset;
            break;
        case 0x135C6:   /* the records of the world that is left are cleared */
            world_records_clear();
            break;
        case 0x13614:   /* $019170: the first three words of each of the first ten records of records_19334 (the things the player picks up) are -1 and the doors of the map shut (mark_cells_19170) */
            mark_cells_19170();
            break;
        case 0x1368A:
            guest.tick_count = 0;
            break;
        case 0x13532:   /* the next world is loaded from the disk */
            level_load_world();
            break;
        case 0x13646:   /* the next level: param_index goes up (5 is 1), the screen fades out and the level is begun again from the files */
            guest.param_index = (uint16_t)(guest.param_index + 1);
            if ((int16_t)guest.param_index >= 5)
                guest.param_index = 1;
            guest.word_0266 = 0;
            guest.menu_active = 0;
            fade_out(1, 1);
            guest.score_redraw = 0xFFFF;
            level_next_wait = 1;
            level_begin();
            return;
        default:
            host_missing_routine(handlers[10 - bit]);
            break;
        }
    }
}

/* orig $0035DC (flag_1f0 set, word_200 not 1): flag_1f0 is cleared and the record of the level end that table_143c8 has for the door (word_202 less 2, 0 to 3: the last record of a level's list) becomes
 * ptr_0bc6, and level_ended starts the level's end screen on the frame that follows */
static void level_end_for_door(void)
{
    guest.flag_1f0 = 0;

    int16_t door = (int16_t)(guest.word_202 - 2);

    if (door < 0 || door >= 4)
        return;
    static const struct LevelEnd *const last[4] = { &level_ends[0].l1[3], &level_ends[0].l2_4, &level_ends[0].l3[2], &level_ends[0].l4[4] };

    guest.ptr_0bc6 = last[door];
    guest.level_ended = 1;
}

/* orig $012E08: the level end (level_ended): the next record of ptr_0bc6 is taken; with a bonus the screen shows what the level's end is called, the bonus of the things
 * the player has (2000 each), the energy (80 each) and the score earned (an eighth of it), counted up in turn in the number at the foot, and the bonus is
 * then counted into the score (90, 10 and 1 at a time) with the score panel drawn each time; then the routines of the record's bits run */
static void level_end_screen(void)
{
    const struct LevelEnd *record = guest.ptr_0bc6;

    guest.ptr_0bc6++;
    host_iteration_cpu(2);   /* (the work of the frame, which took the original two frames, as before the loop's own flip) */
    screens_flip();
    level_end_copy();
    if (record->bonus == 0) {
        level_end_effects(record);
        return;
    }
    guest.flag_268 = 0;
    level_end_quiet();
    if (level_end_wait_rule(record))
        host_cpu_frames(1);   /* (the voices of the game's sounds are free a frame later on the Amiga than the host's note it) */
    play_sound(record->bonus >= 0x3E8 ? 0x24 : 0x25, 0x79);
    guest.level_end_counter = 0;
    text_draw_at((const char *)record->text, 0, 0x12);
    text_draw_at("COMPLETED", 0x58, 0x26);
    text_draw_at("BONUS", 0x30, 0x88);
    ((volatile uint8_t *)&guest.word_ddc)[0] = 0;
    guest.draw_routine_index = 1;
    guest.flags_0ddd |= 2;
    guest.flags_0ddd |= 8;
    guest.flags_0ddd &= (uint8_t)~1;
    level_end_save_rect();
    level_end_draw_counter();
    screens_flip();
    level_end_copy();
    level_end_quiet();

    int16_t saved_energy_shown = guest.energy_shown;

    draw_level_end_number(0x2C, 0x40, guest.item_count, 1);
    sprite_draw(0x30, 0x3B, 0x77);
    glyph_draw(0x34, 0x3F, 0xD4);
    text_draw_at("X  _]]]", 0x9A, 0x40);
    screens_flip();
    level_end_copy();
    if (guest.item_count != 0)
        level_end_count((uint16_t)guest.item_count, 0x7D0);
    wait_frames_or_request(0x1E);

    draw_level_end_number(0x2C, 0x58, guest.player_energy, 1);
    {
        uint8_t *saved = guest.screen_buffer;

        guest.screen_buffer -= 0xD0C;
        update_energy_display();
        draw_energy_bar();
        guest.screen_buffer = saved;
    }
    text_draw_at("X    e]", 0x9A, 0x58);
    screens_flip();
    level_end_copy();
    if (guest.player_energy != 0) {
        level_end_count(guest.player_energy, 0x50);
    }
    wait_frames_or_request(0x1E);

    int32_t earned = (int32_t)guest.score_total_1fc;

    if (earned != 0)
        earned = (int32_t)((uint32_t)earned >> 3);
    earned = (int16_t)earned;
    draw_level_end_number(0xAC, 0x70, earned, 1);
    text_draw_at("^ X", 0x7A, 0x70);
    sprite_draw(0x30, 0x6C, 0x77);
    glyph_draw(0x34, 0x70, 0xAF);
    screens_flip();
    level_end_copy();
    if (earned != 0)
        level_end_count((uint16_t)earned, 1);
    wait_frames_or_request(0x1E);

    int32_t left = (int32_t)guest.level_end_counter;
    uint16_t beat = 0;

    guest.level_end_step = 0x90;
    guest.level_end_unit = 0x5A;
    for (;;) {
        if (left == 0)
            break;
        left -= (int32_t)guest.level_end_unit;
        if (left < 0) {
            left += (int32_t)guest.level_end_unit;
            if ((uint8_t)guest.level_end_unit != 0x0A) {
                guest.level_end_step = 0x10;
                guest.level_end_unit = 0x0A;
            } else {
                guest.level_end_step = 1;
                guest.level_end_unit = 1;
            }
            continue;
        }
        level_end_add_score((uint8_t)guest.level_end_step);
        update_score_display();
        draw_score_panel();
        draw_item_row();
        guest.level_end_counter = (uint32_t)left;
        level_end_draw_counter();
        beat++;
        if (beat >= 3) {
            play_sound(0x37, 5);
            beat = 0;
        }
        draw_item_row();
        screens_flip();
    }
    guest.energy_shown = saved_energy_shown;
    wait_frames_or_request(0x32);
    level_end_quiet();
    level_end_effects(record);
    host_iteration_cpu_next(1);
}

/* the same with the carry clear */
void score_add(int16_t points)
{
    score_add_carry(points, 0);
}

/* orig $0071E6 (and inlined: $007272): a puff is started where the object (five words: x, y, ...; a5) is: sound 18 plays, the first
 * free (third word negative) of the ten records of three words at $721E gets its x and y and a count of 7, the object's fourth word
 * becomes -5 and flag_721c says a record is in use.  Without a free record only the sound plays */
void spawn_puff(int16_t *object)
{
    int16_t *record = guest.records_721e;
    int i;

    play_sound(18, 4);
    for (i = 0; i < 10; i++, record += 3)
        if (record[2] < 0)
            break;
    if (i == 10)
        return;
    record[0] = object[0];
    record[1] = object[1];
    record[2] = 7;
    object[3] = -5;
    guest.flag_721c = 1;
}

/* the end of $007F88 ($008058): the first free of records_19b14 gets x, y, kind less 11 and 1 and its byte in near_flags is set.
 * Kinds below 11 ($8088) are the objects: the kind plus 192 is the record's kind.  The original's result is in a5 and d5 and the condition
 * codes; nothing uses it */
static void drop_record(int16_t x, int16_t y, int16_t kind)
{
    struct Slot8 *record = find_free_record_19b14();

    if (record == 0)
        return;
    record->w[0] = x;
    record->w[1] = y;
    if (kind < 11)
        record->w[2] = kind + 0xC0;   /* $8088 */
    else
        record->w[2] = kind - 11;
    record->w[3] = 1;
    guest.near_flags[record - guest.records_19b14] = 0xff;
}

/* orig $007F88 (with $008058 inlined: drop_record): a thing is dropped at (x, y) with kind `kind` (not 61): when none falls yet
 * it starts to fall (fall_kind); when one falls already a record is made at once */
void drop_thing(int16_t x, int16_t y, int16_t kind)
{
    if (kind == 61) {   /* $7F80: dropped at once, on a multiple of 16 in y */
        drop_record(x, (int16_t)(y & -16), kind);
        return;
    }
    if (guest.fall_kind >= 0) {
        drop_record(x, y, kind);
        return;
    }
    guest.fall_x = x;
    guest.fall_y = y;
    guest.fall_speed = -6;
    guest.fall_kind = kind;
}

/* orig $007FAA: the thing that falls (fall_kind not negative) goes down by fall_speed and is drawn (the glyph of the kind: kinds from
 * 11 on are in table_14cf8, the others are drawn as the glyph kind + 302; the sprites 279 and 280 eight pixels higher); fall_speed
 * goes up by one and at 4 the fall is over: the thing becomes a record (drop_record) */
void update_falling_thing(void)
{
    if (guest.fall_kind < 0)
        return;

    int16_t x = guest.fall_x;
    int16_t y = guest.fall_y + guest.fall_speed;
    int16_t kind = guest.fall_kind;
    int16_t glyph;

    if (kind < 11) {
        glyph = kind + 302;
        guest.fall_y = y;
        glyph_draw(x - guest.scroll_x, y - guest.scroll_y, glyph);
    } else {
        glyph = (int16_t)guest.table_14cf8[kind - 11].w[0];
        guest.fall_y = y;
        if (glyph == 279 || glyph == 280)
            sprite_draw(x - guest.scroll_x, y - guest.scroll_y - 8, glyph);
        else
            glyph_draw(x - guest.scroll_x, y - guest.scroll_y, glyph);
    }
    guest.fall_speed++;
    if (guest.fall_speed < 4)
        return;
    x = guest.fall_x;
    y = guest.fall_y;
    kind = guest.fall_kind;
    guest.fall_kind = -1;
    drop_record(x, y, kind);
}

/* orig $005D76: the picture of the energy (image 126 of the table: 24 rows, two words wide) is drawn at the foot of the screen
 * in two parts: the first rows come from the beginning of the image, the others from further on by the picture (word_2de, 4
 * less it times 384 bytes) the bar is cycled; the number of rows of the first part falls as the energy goes up.  word_2de goes
 * on every other frame (frame_phase 0) */
void draw_energy_bar(void)
{
    if (guest.frame_phase == 0)
        guest.word_2de = (guest.word_2de + 1) & 3;

    int16_t back = (int16_t)((4 - guest.word_2de) * 384);
    const uint8_t *src = (const uint8_t *)guest.image_table[126].data;
    uint8_t *dst = guest.screen_buffer + 7042 + ((guest.x_offset & -16) >> 3);
    int16_t shift = guest.x_offset & 15;
    int16_t energy = guest.energy_shown;

    if (energy < 0)
        energy = 0;

    int16_t rows = (int16_t)(((24 - energy) >> 1) + 6);

    blit_image_wide(dst, src, 96, shift, rows);
    blit_image_wide(dst + 44 * rows, src + 4 * rows - back, 96, shift, 23 - rows);
}

/* orig $007BA8: the walker's step along the ground (the movement routine at +22 of the type record at $199AC, `type`; the object has
 * x, y, the step 0 to 7 (8: just turned) at +4 and the direction 0 or 1 at +10).  x is the object's x, y its y.  Every other
 * frame (frame_toggle) the step goes on and the walker moves 4 pixels in its direction; at a cell border (a multiple of 32) the map
 * is looked at: ground ahead or none below turns it round.  Results: the new x (*x_io) and the picture (the return value: walker_picture_base,
 * the first picture, plus the step, plus the direction times 9).  The entry of the routine sets walker_picture_base (walker_first_picture).  Where it goes on at $7CB0
 * (type byte 18 set and a wall ahead walking left) did not run */
static int16_t walk_pace(const uint8_t *type, int16_t *obj, int16_t *x_io, int16_t y)
{
    int16_t x = *x_io;
    int16_t step;

    step = obj[2];
    if (step > 7) {
        obj[2]--;
        step = 8;
        goto done;
    }
    if (guest.frame_toggle == 0)
        goto done;
    step = (step + 1) & 7;
    obj[2] = step;
    if (obj[5] == 0) {   /* walking left */
        x -= 4;
        if ((x & 31) != 0)
            goto done;

        const uint8_t *cell = map_cell(x, y);

        if (cell[-1] == 1 || cell[127] == 1)
            goto turn;
        if (cell[255] == 1)
            goto done;   /* with type[18] set the original goes on at $7CB0 */
        goto turn;
    }
    x += 4;   /* walking right */
    if ((x & 31) != 0)
        goto done;
    {
        const uint8_t *cell = map_cell(x, y);

        if (cell[1] == 1 || cell[129] == 1)
            goto turn;
        if (cell[257] != 1)
            goto turn;
        if (type[18] == 0)
            goto done;
        /* the original goes on at $7C5A: the walker turns when it comes to the player's cell */
        const uint8_t *player_cell = guest.ptr_0b86 + 8192 + ((int16_t)(guest.player_x & -32) >> 5) +
                                     (int16_t)(((guest.player_y & -16) + 16) << 3);

        cell--;
        if (cell == player_cell)
            goto turn;
        cell--;
        if (cell == player_cell)
            goto turn;
        for (;;) {
            if (cell[0] == 1 || cell[128] == 1)
                goto done;
            if (cell[256] != 1)
                goto done;
            cell--;
            if (cell == player_cell)
                goto turn;
        }
    }
turn:
    obj[5] ^= 1;
done:
    *x_io = x;
    return (int16_t)(step + obj[5] + (obj[5] << 3) + guest.walker_picture_base);
}

/* orig $007BA8: the x that walk_pace works out; the original returns it in d0 (and the picture in d2: walk_pace_frame; the step is made
 * once, call one of the two) */
int16_t walk_pace_x(const uint8_t *type, int16_t *obj, int16_t x, int16_t y)
{
    walk_pace(type, obj, &x, y);
    return x;
}

int16_t walk_pace_frame(const uint8_t *type, int16_t *obj, int16_t x, int16_t y)
{
    return walk_pace(type, obj, &x, y);
}

/* orig $00717E: a routine of a creature's type record (a3) for the object (a5): nothing is done while byte 19 of the type is 0; the original
 * goes on at $7184 otherwise, which did not run */
void creature_step_717e(const uint8_t *type, int16_t *object)
{
    (void)object;
    if (type[19] == 0)
        return;
    /* goes on at $7184 */
}


/* ======== from game_b.c ======== */
/* Gods, the game: the routines at $8000-$FFFF and $16800 and up (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */

/* orig $007C52, $00BA34, $00BC1A, $00C374, $00D6B8, $00DB04, $00DE54, $00E142, $00E3A2, $017CC0, $0194D0, $018C24 and $018C2E: nothing;
 * places of the program where an empty routine is called (the game loop calls most of them every frame) */
void do_nothing_b(void)
{
}

/* orig $008684: of the records at $0BD6 (x, y, strength: the creatures hit this frame; flag_20e + 1 of them) the one with the highest strength is the target: its place (in the level) is given, its strength is cleared and
 * it is returned; -1 when there is none */
static int16_t falcon_pick_target(int16_t *x, int16_t *y)
{
    int16_t best = -1;
    int16_t *chosen = 0;
    int16_t *list = guest.hit_list;

    for (int i = 0; i <= guest.flag_20e; i++) {
        int16_t *record = &list[i * 3];

        if (best >= record[2])
            continue;
        chosen = record;
        *x = (int16_t)(record[0] + guest.scroll_x);
        *y = (int16_t)(record[1] + guest.scroll_y);
        best = record[2];
    }
    if (best >= 0)
        chosen[2] = 0;
    return best;
}

/* orig $0084BA: the falcon goes after the point (x, y): its speeds (across, down) go one step toward the point's side (up to 2 * word_8894 + 3 across, + 2 down), a place level with the point's is a toss-up, and the way it faces is the one it goes */
static void falcon_chase_point(int16_t x, int16_t y)
{
    int16_t fx = guest.word_8882;
    int16_t fy = guest.word_8884;
    int16_t speed = (int16_t)(guest.word_8894 * 2);
    int right;

    if (fx == x)
        right = random_word() & 1;
    else
        right = fx < x;
    if (right) {
        int16_t limit = (int16_t)(speed + 3);
        int16_t v = guest.falcon_speed_x;

        if (v != limit) {
            v++;
            if (v > limit)
                v = limit;
        }
        guest.falcon_speed_x = v;
        guest.falcon_facing = 1;
    } else {
        int16_t limit = (int16_t)-(speed + 3);
        int16_t v = guest.falcon_speed_x;

        if (v != limit) {
            v--;
            if (v < limit)
                v = limit;
        }
        guest.falcon_speed_x = v;
        guest.falcon_facing = -1;
    }

    int down;

    if (fy == y)
        down = random_word() & 1;
    else
        down = fy < y;
    if (down) {
        int16_t limit = (int16_t)(speed + 2);
        int16_t v = guest.falcon_speed_y;

        if (v != limit) {
            v++;
            if (v > limit)
                v = limit;
        }
        guest.falcon_speed_y = v;
    } else {
        int16_t limit = (int16_t)-(speed + 2);
        int16_t v = guest.falcon_speed_y;

        if (v != limit) {
            v--;
            if (v < limit)
                v = limit;
        }
        guest.falcon_speed_y = v;
    }
    guest.word_8882 = (int16_t)(fx + guest.falcon_speed_x);
    guest.word_8884 = (int16_t)(fy + guest.falcon_speed_y);
}

/* orig $008626: the four cells of the map under the falcon are marked with $FF (a hit that reaches one takes the falcon's energy: the cell type -1) */
static void falcon_mark_cells(void)
{
    int16_t x = (int16_t)(guest.word_8882 - guest.scroll_x);
    int16_t y = (int16_t)(guest.word_8884 - guest.scroll_y);
    uint8_t *map = (uint8_t *)guest.line_text;
    uint8_t *cell = map + 0x64;
    int16_t d5;

    cell += (int16_t)(x >> 3);
    d5 = (int16_t)(y & (int16_t)0xFFF8);
    d5 = (int16_t)(d5 << 1);
    cell += d5;
    d5 = (int16_t)(d5 << 1);
    cell += d5;
    if (cell < map + 0x30 || cell >= map + 0x5DC)
        return;
    cell[0x31] = 0xFF;
    cell[0x32] = 0xFF;
    cell[0x61] = 0xFF;
    cell[0x62] = 0xFF;
}

/* orig $00836C: the falcon (the pick-up 7; falcon_energy is its energy, negative: it is not there) goes after the player or, with no weapon of its own chosen (falcon_choice negative), after what the player's shots have hit (the target of
 * $8684); it is drawn as the picture 389 to 391 (+4 facing right) by falcon_draw, it turns to face where the player does when it is above the player, and its cells are marked */
void falcon_seek_target(void)
{
    static const int16_t wings[6] = { 0, 1, 2, 3, 2, 1 };
    static const int8_t facing[30] = { -1, 1, 1, -1, 0, 1, -1, 0, -1, 1, 0, -1, 1, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 0, -1, 1, 0 };

    if (guest.falcon_energy < 0)
        return;

    int16_t saved = guest.word_8894;
    int16_t x = 0, y = 0;
    int16_t value = falcon_pick_target(&x, &y);

    if (guest.falcon_choice < 0) {
        if (value < 0) {
            y = (int16_t)(guest.player_y - 0x24);
            guest.word_8894 = 0;
            x = guest.player_x;
        }
    } else {
        if (value < 0)
            y = (int16_t)(guest.player_y - 0x24);
        x = guest.player_x;
    }
    guest.falcon_previous_facing = guest.falcon_facing;
    falcon_chase_point(x, y);

    int16_t sx = (int16_t)(guest.word_8882 - guest.scroll_x);
    int16_t sy = (int16_t)(guest.word_8884 - guest.scroll_y);
    int16_t picture = (int16_t)(0x185 + wings[(guest.wing_step >> 1) % 6]);
    int16_t limit = (int16_t)(guest.player_x - guest.scroll_x - 8);

    if (!(sx < limit) && !(sx > (int16_t)(limit + 0x10))) {
        int8_t turn = facing[guest.word_19c & 31];

        guest.falcon_facing = turn == 0 ? guest.falcon_previous_facing : (int16_t)turn;
    }
    if (guest.falcon_facing >= 0)
        picture += 4;
    guest.falcon_screen_x = sx;
    guest.falcon_screen_y = sy;
    guest.wing_glyph = picture;
    if (guest.frame_toggle != 0) {
        guest.wing_step += 2;
        if (guest.wing_step >= 0xC)
            guest.wing_step = 0;
    }
    falcon_mark_cells();
    guest.word_8894 = saved;
}

/* orig $008452: the glyph wing_glyph (if it is not negative) is drawn at (falcon_screen_x, falcon_screen_y) */
void falcon_draw(void)
{
    if (guest.wing_glyph < 0)
        return;
    sprite_draw(guest.falcon_screen_x, guest.falcon_screen_y, guest.wing_glyph);
}

/* orig $008742: the choice of the falcon's own weapon: of the three weapon slots only those with the object 2 count, and when two or more slots are filled the one with the lowest first value (the
 * first one of a tie) is chosen, if it is one of them (falcon_choice 0 to 2; -1 none) */
void falcon_choose_weapon(void)
{
    if (guest.falcon_energy < 0)
        return;

    const struct Object *objects = (const struct Object *)guest.object_slots;
    int16_t slot[3] = { (int16_t)guest.slot_ref_6b6, (int16_t)guest.slot_ref_748, (int16_t)guest.slot_ref_7da };
    int count = 0;

    for (int i = 0; i < 3; i++)
        if (slot[i] >= 0)
            count++;
    guest.falcon_choice = -1;
    if (count < 2)
        return;

    int16_t d[3];

    for (int i = 0; i < 3; i++) {
        d[i] = slot[i] >= 0 ? objects[slot[i]].first_value : slot[i];
        if (slot[i] != 2)
            d[i] = -1;
    }

    int chosen;

    if (d[0] >= 0 && (d[1] < 0 || d[0] <= d[1]) && (d[2] < 0 || d[0] <= d[2]))
        chosen = 0;
    else if (d[1] < 0)
        chosen = 2;
    else if (d[2] < 0 || d[1] <= d[2])
        chosen = 1;
    else
        chosen = 2;
    if (d[chosen] < 0)
        return;
    guest.falcon_choice = (int16_t)chosen;
}

/* orig $0086D8 (the pick-up 7 with the falcon not there): it starts 160 above the player, still, facing right, with the energy 1000, the damage a cell of it does 4 and the strength word 0; the list of the creatures hit is switched on (word_214 0) */
static void falcon_appear(void)
{
    guest.word_8882 = guest.player_x;
    guest.word_8884 = (int16_t)(guest.player_y - 0xA0);
    guest.falcon_screen_x = (int16_t)(guest.word_8882 - guest.scroll_x);
    guest.falcon_screen_y = (int16_t)(guest.word_8884 - guest.scroll_y);
    guest.wing_glyph = 0x185;
    guest.falcon_speed_x = 0;
    guest.falcon_speed_y = 0;
    guest.falcon_facing = 1;
    guest.wing_step = 0;
    guest.falcon_energy = 0x3E8;
    guest.word_8892 = 4;
    guest.word_8894 = 0;
    guest.word_214 = 0;
    falcon_choose_weapon();
}

/* ======== the pods of level 3 (orig $00D664 to $00D836: ten records of 10 bytes at $FD0) and the spores they spit ($00D838 to $00DA50: ten records of 12 bytes at $1036) ======== */

/* a pod (the thing 266): its place (a negative x: free), hit points (negative: dying), the picture state (0 shut, 1 to 3 opening and spitting, or the count of its death's pictures) and
 * the frames it waits to open */
struct Pod {
    int16_t x;        /* +0 */
    int16_t y;        /* +2 */
    int16_t energy;   /* +4 */
    int16_t state;    /* +6 */
    int16_t timer;    /* +8 */
};

/* a spore (twelve bytes): its place, speeds, the picture frame (negative: it is dying), the hit points as a byte (0: free, negative: it is born, rising) and the frames it goes on the way it is going */
struct Spore {
    int16_t x;        /* +0 */
    int16_t y;        /* +2 */
    int16_t speed_x;  /* +4 */
    int16_t speed_y;  /* +6 */
    int8_t frame;     /* +8 */
    int8_t energy;    /* +9 */
    int16_t count;    /* +10 */
};
_Static_assert(sizeof(struct Spore) == 12, "spore");

static struct Pod *pods(void)
{
    return (struct Pod *)guest.pod_records;
}

static struct Spore *spores(void)
{
    return (struct Spore *)guest.spore_records;
}

/* orig $00D838 (d0 and d1 the place): a spore is born there (hit points -10, rising at 10 up) when fewer than ten are going */
static void spore_spawn(int16_t x, int16_t y)
{
    if (guest.word_10ae == 10)
        return;

    struct Spore *spore = spores();
    int i;

    for (i = 0; i < 10; i++, spore++)
        if (spore->energy == 0)
            break;
    if (i == 10)
        spore--;   /* ($D868 leaves a0 on the last one when none is free) */
    spore->x = x;
    spore->y = y;
    spore->energy = -10;
    spore->speed_y = -10;
    spore->speed_x = 0;
    spore->frame = 0;
    guest.word_10ae++;
}

/* orig $00DA08: a spore is a box of 24 by 24 for the player's shots; its hit points gone it starts to die (frame -1, hit points 1, 4 up and left) and 5 points are added */
static void spore_hit(struct Spore *spore, int16_t x, int16_t y)
{
    guest.hit_box_extra_width = 0x18;
    guest.hit_box_extra_height = 0x18;

    int16_t energy = check_hit_and_walls(x, y, spore->energy);

    if (energy >= 0) {
        spore->energy = (int8_t)energy;
        return;
    }
    spore->frame = -1;
    spore->energy = 1;
    spore->x = (int16_t)(spore->x - 4);
    spore->y = (int16_t)(spore->y - 4);
    score_add_packed(5);
}

/* orig $00D898 (every frame, when word_10ae is not 0): the spores.  One being born rises (the picture 490) and is a spore with 20 hit points when its hit points have counted up to 0; a dying one drifts on in
 * the pieces' pictures (91 on) and is free after 17 frames; a going one turns through four frames, walks a few frames (3 to 10) in a direction toward the player, stands a few (0 to 15) and chooses again;
 * it is hit, drawn (the picture 491 + 4 left, 8 right + its frame) and is free when it is off the screen */
void spores_update(void)
{
    if (guest.word_10ae == 0)
        return;

    struct Spore *spore = spores();

    for (int i = 0; i < 10; i++, spore++) {
        if (spore->energy == 0)
            continue;

        int16_t x, y;

        if (spore->energy < 0) {   /* $D9C2: being born */
            x = spore->x;
            y = (int16_t)(spore->y + spore->speed_y);
            spore->speed_y++;
            spore->energy++;
            if (spore->energy >= 0) {
                spore->energy = 20;
                spore->speed_x = 0;
                spore->speed_y = 0;
                spore->count = 0xF;
            }
            spore->y = y;
            sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), 0x1EA);
            continue;
        }
        if (spore->frame < 0) {   /* $DA52: dying */
            x = (int16_t)(spore->x + spore->speed_x);
            y = (int16_t)(spore->y + spore->speed_y);
            spore->x = x;
            spore->y = y;
            sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)(-(int16_t)spore->frame - 1 + 0x5B));
            spore->frame--;
            if (spore->frame <= (int8_t)0xEE) {
                spore->energy = 0;
                guest.word_10ae--;
            }
            continue;
        }
        spore->frame = (int8_t)((spore->frame + 1) & 3);
        if (guest.timer_306 != 0) {
            x = spore->x;
            y = spore->y;
        } else if (spore->count != 0) {   /* $D922 */
            x = (int16_t)(spore->x + spore->speed_x);
            y = (int16_t)(spore->y + spore->speed_y);
            spore->x = x;
            spore->y = y;
            spore->count--;
            if (spore->count == 0) {
                if ((spore->speed_x | spore->speed_y) == 0)
                    goto choose;
                spore->count = (int16_t)(random_word() & 0xF);
                spore->speed_x = 0;
                spore->speed_y = 0;
                x = spore->x;
            }
        } else {
choose:     /* $D8D4: a new way: 2 to 9 across and up or down, toward the player */
            for (;;) {
                y = spore->y;

                int16_t dx = (int16_t)((random_word() & 7) + 2);
                int16_t dy = (int16_t)((random_word() & 7) + 2);

                if ((int16_t)(spore->x + 4) >= guest.player_x)
                    dx = (int16_t)-dx;
                if ((int16_t)(spore->y + 0xC) >= guest.player_y)
                    dy = (int16_t)-dy;
                spore->speed_x = dx;
                spore->speed_y = dy;
                spore->count = (int16_t)((random_word() & 7) + 3);
                x = spore->x;
                break;
            }
        }

        uint16_t image = 0;

        if (spore->speed_x != 0) {
            if (spore->speed_x > 0)
                image = 4;
            image += 4;
        }
        image = (uint16_t)(image + (uint8_t)spore->frame + 0x1EB);
        spore_hit(spore, x, y);

        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);

        if (sx < -0x20 || sx >= 0x160 || sy < -0x20 || sy >= 0xE0) {
            spore->energy = 0;
            continue;
        }
        sprite_draw(sx, sy, image);
    }
}

/* orig $00D664 (d0 the thing's x on the screen, d1 its y in the level): the first free pod (a negative x) gets the place, 200 hit points, the state 0 and 10 frames to wait, and the routine at $D6B8 is switched on; with none free nothing is
 * made (the thing is taken from the map all the same) */
static void pod_spawn(int16_t x, int16_t y)
{
    struct Pod *pod = pods();

    for (int i = 0; i < 10; i++, pod++) {
        if (pod->x >= 0)
            continue;
        pod->x = (int16_t)(x + guest.scroll_x);
        pod->y = y;
        pod->energy = 0xC8;
        pod->state = 0;
        pod->timer = 10;
        guest.code_d6b8 = 0x4E71;
        return;
    }
}

/* orig $00D6B8 (a bare return unless a pod was started): the pods.  A dying one is drawn in the pictures 108 on and is free after seven; one that lives counts its wait down (then 40 to 71 frames) and opens, spits (the third frame:
 * a spore from 10 above it when it is on the screen) and shuts; it is a box of 32 by 40 for the player's shots (the fight's flag on while it is looked at) and, when it is killed, 10 points are added and it dies; it is drawn
 * as the picture 266, 445, 446 or 447 by its state */
static void pods_update(void)
{
    static const uint16_t pictures[4] = { 0x10A, 0x1BD, 0x1BE, 0x1BF };

    if (guest.code_d6b8 != 0x4E71)
        return;

    struct Pod *pod = pods();

    guest.thrown_things_count = 0;
    for (int i = 0; i < 10; i++, pod++) {
        if (pod->x < 0)
            continue;
        guest.thrown_things_count++;
        if (pod->energy < 0) {
            sprite_draw((int16_t)(pod->x - 4 - guest.scroll_x), (int16_t)(pod->y - 8 - guest.scroll_y), (uint16_t)(pod->state + 0x6C));
            pod->state++;
            if (pod->state >= 7)
                pod->x = -1;
            continue;
        }

        int spit = 0;

        if (guest.timer_306 == 0) {
            if (pod->state == 0) {
                pod->timer--;
                if (pod->timer < 0) {
                    pod->timer = (int16_t)(0x28 + (random_word() & 0x1F));
                    pod->state++;
                }
            } else {
                pod->state++;
                if (pod->state == 3)
                    spit = 1;
                else if (pod->state > 3)
                    pod->state = 0;
            }
        }
        if (spit) {
            int16_t sx = (int16_t)(pod->x - guest.scroll_x);
            int16_t sy = (int16_t)(pod->y - guest.scroll_y);

            if (!(sx > 0x140 || sx < -0x20 || sy < -8 || sy > 0xC8))
                spore_spawn(pod->x, (int16_t)(pod->y - 10));
        }

        uint16_t saved_23a = guest.flag_23a;

        guest.hit_box_extra_width = 0x20;
        guest.hit_box_extra_height = 0x28;
        guest.flag_23a = 1;

        int16_t energy = check_hit_and_walls((int16_t)(pod->x - 8), (int16_t)(pod->y - 8), pod->energy);

        guest.flag_23a = saved_23a;
        pod->energy = energy;
        if (energy < 0) {
            pod->state = 0;
            score_add_packed(0x10);
        }
        sprite_draw((int16_t)(pod->x - guest.scroll_x), (int16_t)(pod->y - guest.scroll_y), pictures[pod->state & 3]);
    }
    if (guest.thrown_things_count == 0)
        guest.code_d6b8 = 0x4E75;
}

/* orig $016AEE: nothing is done unless flag_aa8 is 1; the original then goes on at $16AF6, which did not run */
void step_16aee(void)
{
    fn_16aee_c();
}

static void start_debris_at(int16_t x, int16_t y);

/* orig $0185A8 and the loops $0185E2, $018614, $018660 and $018692 (which the record keeps the address of): the bolt goes on along its line by `speed` steps.  The line goes in
 * x (length_x the longer) or in y; error is the Bresenham error, count what the original counts down without looking at it */
static void aimed_bolt_steps(struct Bolt *bolt)
{
    int16_t x = bolt->x, y = bolt->y;
    int16_t error = bolt->error;
    uint16_t steps = (uint16_t)bolt->speed;

    do {
        switch (bolt->routine) {
        case 0x185E2:
            error = (int16_t)(error - bolt->length_y);
            if (error < 0) {
                error = (int16_t)(error + bolt->length_x);
                y = (int16_t)(y + bolt->dir);
            }
            x++;
            break;
        case 0x18614:
            y = (int16_t)(y + bolt->dir);
            error = (int16_t)(error - bolt->length_x);
            if (error < 0) {
                error = (int16_t)(error + bolt->length_y);
                x++;
            }
            break;
        case 0x18660:
            error = (int16_t)(error - bolt->length_y);
            if (error < 0) {
                error = (int16_t)(error + bolt->length_x);
                y = (int16_t)(y + bolt->dir);
            }
            x--;
            break;
        default:   /* $18692 */
            y = (int16_t)(y + bolt->dir);
            error = (int16_t)(error - bolt->length_x);
            if (error < 0) {
                error = (int16_t)(error + bolt->length_y);
                x--;
            }
            break;
        }
    } while (--steps != 0);
    bolt->x = x;
    bolt->y = y;
    bolt->error = error;
    bolt->count--;
}

/* orig $0183C8 (with $0185BE and $018640): a bolt is shot from (x, y) at (target_x, target_y) with `speed` steps a frame and the kind `kind` (bit 0): it takes the first free of the
 * twenty records, goes its first steps at once and word_186be says there are bolts.  Without a free record nothing happens */
static void aimed_bolt_start(int16_t x, int16_t y, int16_t target_x, int16_t target_y, int16_t speed, int16_t kind)
{
    for (int i = 0; i < 20; i++) {
        struct Bolt *bolt = &guest.bolt_records[i];

        if ((int32_t)bolt->routine >= 0)
            continue;
        bolt->speed = speed;
        bolt->kind = (int16_t)(kind & 1);

        int left = x > target_x;
        int16_t length_x = (int16_t)(left ? x - target_x : target_x - x);
        int16_t length_y = (int16_t)(target_y - y);

        bolt->dir = 1;
        if (length_y < 0) {
            length_y = (int16_t)-length_y;
            bolt->dir = -1;
        }
        bolt->x = x;
        bolt->y = y;
        bolt->length_x = length_x;
        bolt->length_y = length_y;
        if ((uint16_t)length_x < (uint16_t)length_y) {
            bolt->count = length_y;
            bolt->error = (int16_t)((uint16_t)length_y >> 1);
            bolt->routine = left ? 0x18692 : 0x18614;
        } else {
            bolt->count = length_x;
            bolt->error = (int16_t)((uint16_t)length_x >> 1);
            bolt->routine = left ? 0x18660 : 0x185E2;
        }
        aimed_bolt_steps(bolt);
        guest.word_186be = 1;
        return;
    }
}

/* orig $0184EE: a bolt at the screen place (x, y) that is in the box of the player (from 6 pixels right of the bolt's place; the player's box is 32 by 48, from 24 lower when the
 * player is in the states 18 and 19) and the player not invulnerable (flag_aa8): the energy goes down by 2 and the bolt is over, with a spark */
static void aimed_bolt_hits_player(uint32_t *record, int16_t x, int16_t y)
{
    int16_t hit_x = (int16_t)(x + 4 + guest.scroll_x + 2);
    int16_t hit_y = (int16_t)(y + 4 + guest.scroll_y);
    int16_t top = guest.player_y;

    if (guest.word_19c >= 0x12 && guest.word_19c <= 0x13)
        top = (int16_t)(top + 0x18);
    if (hit_y < top || hit_x < guest.player_x)
        return;
    if (hit_x > (int16_t)(guest.player_x + 0x20) || hit_y > (int16_t)(guest.player_y + 0x30))
        return;
    if (guest.flag_aa8 != 0)
        return;
    guest.player_energy -= 2;
    *record = 0xFFFFFFFF;
    start_debris_at((int16_t)(x - 2), (int16_t)(y - 2));
}

/* orig $01843E: the bolts (while word_186be says there are some) go on by their steps; one that is in a wall (a cell of the second map, 32 by 16, that is 1) or that has left the
 * screen is over (the wall with a spark), the others hit the player (aimed_bolt_hits_player) and are drawn as the glyph 75 + (frame_toggle & 1) */
void bolts_update(void)
{
    if (guest.word_186be == 0)
        return;
    guest.word_186be = 0;
    for (int i = 0; i < 20; i++) {
        struct Bolt *bolt = &guest.bolt_records[i];

        if ((int32_t)bolt->routine < 0)
            continue;
        guest.word_186be = 1;
        aimed_bolt_steps(bolt);

        int16_t x = bolt->x, y = bolt->y;
        int16_t cell = (int16_t)(((x + 3) & 0xFFE0) >> 5);

        cell = (int16_t)(cell + (int16_t)(((y + 3) & 0xFFF0) << 3));
        if (guest.ptr_0b86[0x2000 + cell] == 1) {
            bolt->routine = 0xFFFFFFFF;
            start_debris_at((int16_t)(x - 3 - guest.scroll_x), (int16_t)(y - 3 - guest.scroll_y));
            continue;
        }
        x = (int16_t)(x - guest.scroll_x);
        y = (int16_t)(y - guest.scroll_y);
        if (x < 0 || y < 0 || x >= 0x13C || y >= 0xBC) {
            bolt->routine = 0xFFFFFFFF;
            continue;
        }
        aimed_bolt_hits_player(&bolt->routine, x, y);
        glyph_draw(x, y, (uint16_t)((guest.frame_toggle & 1) + 0x4B));
    }
}

/* orig $0190F2: the sparkle of a thing that was dropped (last_drop: where, and the number), while last_drop_frame is not negative: the glyph
 * 80 + last_drop_frame (a number from 192 on is an object: the glyph is its base frame) is drawn on the middle of the thing's picture
 * (16 less its width and height, halved, are taken off the place) and last_drop_frame goes up; at 7 it is -1 (over) */
void dropped_sparkle_update(void)
{
    if (guest.last_drop_frame < 0)
        return;

    int16_t x = guest.last_drop[0];
    int16_t y = guest.last_drop[1];
    int16_t number = guest.last_drop[2];
    int16_t glyph;

    if (number >= 0xC0)
        glyph = ((const struct Object *)guest.object_slots)[number - 0xC0].base_frame;
    else
        glyph = (int16_t)guest.table_14cf8[number].w[0];

    const struct Image *image = &guest.image_table[glyph];
    int16_t across = (int16_t)((16 + image->neg_width) >> 1);
    int16_t down = (int16_t)((16 - (int16_t)image->height) >> 1);

    x = (int16_t)(x - across);
    y = (int16_t)(y - down);
    glyph_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)(guest.last_drop_frame + 0x50));
    guest.last_drop_frame++;
    if (guest.last_drop_frame >= 7)
        guest.last_drop_frame = -1;
}

/* orig $00D270: a hit at (x, y) of the given strength: when the player is alive and the point is inside the box around the player
 * (widened by half of hit_box_extra_width and hit_box_extra_height) the energy goes down by strength/32 + 5 (unless flag_aa8), and the result is -1
 * unless flag_23a (the strength is the result otherwise); the sound 4 is played when the routine at $3436 is only an rts */
int16_t damage_player_at(int16_t x, int16_t y, int16_t strength)
{
    if (guest.energy_shown < 0)
        return strength;

    int16_t left = guest.player_x + 10, right = guest.player_x + 22;
    int16_t top = guest.player_y + 8, bottom = guest.player_y + 38;

    if (guest.word_19c >= 18 && guest.word_19c <= 19)
        top += 20;
    int16_t half = guest.hit_box_extra_width >> 1;

    x += half;
    left -= half;
    right += half;
    half = guest.hit_box_extra_height >> 1;
    y += half;
    top -= half;
    bottom += half;
    if (x < left || x > right || y < top || y > bottom)
        return strength;

    int16_t loss = ((int16_t)(strength + 1) >> 5) + 5;

    if (guest.flag_aa8 == 0)
        guest.player_energy -= loss;
    if (guest.flag_23a == 0)
        strength = -1;
    if (guest.word_3436 == 0x4E75)
        play_sound(4, 120);
    return strength;
}

/* orig $00FB20: 72 words from p on are cleared; returns where that ends */
uint16_t *clear_72_words(uint16_t *p)
{
    for (int i = 0; i < 72; i++)
        *p++ = 0;
    return p;
}

/* orig $00FB2E: the record `index` of object_slots is made ready: its word +4 is cleared, its word +8 gets the word +6 and a
 * 3 in word +2 becomes 0; word_214 is -1.  Ends there when falcon_energy is negative and none of the three slot_refs is 8;
 * otherwise the original goes on at $FB94 (word_214 0) */
void reset_object_slot(int16_t index, int16_t other)
{
    uint16_t *r = (uint16_t *)&guest.object_slots[index];

    (void)other;   /* the original also forms an address from it (in a0) that it throws away */
    r[2] = 0;
    r[4] = r[3];
    if (r[1] == 3)
        r[1] = 0;
    guest.word_214 = -1;
    if (guest.falcon_energy >= 0 || guest.slot_ref_6b6 == 8 || guest.slot_ref_748 == 8 || guest.slot_ref_7da == 8)
        guest.word_214 = 0;   /* $FB94: the list of the creatures hit stays on while the falcon is there (or a weapon 8 is) */
}

/* orig $018D14: the four records of records_316 that are in use (the first word not negative) are drawn as the glyph 108 plus
 * the state, at their position less the scroll, and the state counts up; at 7 the record is free again */
void draw_marks_316(void)
{
    for (int i = 0; i < 4; i++) {
        int16_t *r = &guest.records_316[i * 3];

        if (r[0] < 0)
            continue;
        sprite_draw(r[1] - guest.scroll_x, r[2] - guest.scroll_y, r[0] + 108);
        r[0]++;
        if (r[0] >= 7)
            r[0] = -1;
    }
}

/* orig $017972: one tile of the background: the byte `*map` names the tile (256 bytes of background_tile_graphics each), and the blits of
 * blit_tile copy it to dst; y is its row on the screen (negative: the tile begins above it, positive: it may reach below the
 * last line and is cut).  Returns the map pointer after the byte; nothing is drawn when nothing of the tile is visible */
const uint8_t *draw_background_tile(const uint8_t *map, const uint8_t *tiles, uint8_t *dst, int16_t y, int16_t src_step, int16_t dst_step)
{
    const uint8_t *src = tiles + ((uint32_t)*map++ << 8);
    int16_t rows = 15;   /* d7: rows less one */

    if (y >= 0) {
        int16_t cut = y - (191 - rows);

        if (cut > 0) {
            rows -= cut;
            if (rows < 0)
                return map;
        }
    } else {
        int16_t skip = -y;

        rows -= skip;
        if (rows < 0)
            return map;
        src += skip * 4;
        dst += skip * 4 + skip * 8 + skip * 32;
    }
    blit_tile(src_step, dst_step, rows, src, dst);
    return map;
}

/* orig $0178C6: the background of the level is drawn into the screen buffer from the map ptr_0b86 with blits of tiles: eleven
 * tiles of 32 pixels across and up to twelve rows of 16 lines, from the scroll position (blitter_setup_copy first; the steps
 * between the four blits of a tile are 64 and $2100).  Does nothing more when word_037c is above 2 (the original goes on at $17814 then) and the original goes on at $1785C
 * when it is 2 (neither ran) */
void draw_background(void)
{
    if (guest.word_037c > 2) {   /* the background is kept: it is copied to the screen */
        copy_screen(guest.background, guest.screen_buffer);
        return;
    }
    guest.x_offset = guest.scroll_x & 31;
    blitter_setup_copy();

    const uint8_t *map = guest.ptr_0b86 + ((int16_t)(guest.scroll_x & -16) >> 5) + (int16_t)((guest.scroll_y & -16) << 3);
    uint8_t *dst = (uint8_t *)guest.screen_buffer;
    int16_t y = 0;
    int16_t into = guest.scroll_y & 15;

    y -= into;
    dst -= into * 4 + into * 8 + into * 32;
    for (;;) {
        const uint8_t *row_map = map;
        uint8_t *row_dst = dst;

        for (int16_t x = 0; x < 352; x += 32) {
            map = draw_background_tile(map, guest.background_tile_graphics, dst, y, 64, 0x2100);
            dst += 4;
        }
        map = row_map + 128;
        dst = row_dst + 704;
        y += 16;
        if ((int16_t)(y + guest.scroll_y) >= 1024)
            break;
        if (y >= 192)
            break;
    }
    if (guest.word_037c != 2)
        return;
    guest.background = guest.memory_base + 256;   /* the screen drawn is kept to be copied the next times */
    copy_screen(guest.screen_buffer, guest.background);
    guest.word_037c = 4;
}

/* orig $018072: the entry a2 is looked at (word_185bc gets its byte at +5, plus one); when the record a1 has its byte at +5
 * negative that is all; otherwise it goes on along its line (path_continue, $01808E) */
void step_18072(struct PathRecord *a1, const struct PathDescription *a2)
{
    guest.word_185bc = (int16_t)a2->speed + 1;
    if (a1->waypoint < 0)
        return;
    path_continue(a1, a2);
}

/* orig $017FA0: the picture of a record (a2: byte +4 is the number of a 256-byte block of the background, +26 and +27 the size
 * in 32 pixel columns and 16 line rows, less one) is made an image at the end of the image table and drawn with sprite_draw at
 * (x, y), less the scroll, in columns and rows */
void draw_record_image(int16_t x, int16_t y, const struct PathDescription *a2)
{
    struct Image *image = (struct Image *)guest.asset_table_end;
    int16_t columns;
    int16_t rows = a2->rows;

    image->height = 15;
    image->data = (const uint16_t *)(guest.background + ((int32_t)a2->block << 8));
    uint16_t index = (uint16_t)((int16_t)((uint32_t)guest.asset_table_end - (uint32_t)guest.image_table) >> 3);

    x -= guest.scroll_x;
    y -= guest.scroll_y;
    int16_t first_x = x;

    for (int16_t r = rows; r >= 0; r--) {
        columns = a2->columns & 7;
        for (int16_t c = columns; c >= 0; c--) {
            sprite_draw(x, y, index);
            x += 32;
        }
        x = first_x;
        y += 16;
    }
}

/* orig $018014: the cells the record a2 covers in the tile map (ptr_0b86 + 8192) at (x + 16, y + 8) are marked: for each cell
 * an entry (its address, and its old value) is put in the list at `list` and the cell is set to 1.  Not for a record whose
 * byte +26 has bit 7 set.  Returns the end of the list */
struct SavedByte *mark_record_cells(int16_t x, int16_t y, const struct PathDescription *a2, struct SavedByte *list)
{
    if (a2->columns & 0x80)
        return list;
    int16_t col = (int16_t)(x + 16) & -32;
    int16_t row = (int16_t)(y + 8) & -16;
    uint8_t *cell = guest.ptr_0b86 + 8192 + (col >> 5) + (int16_t)(row << 3);
    uint8_t *line = cell;
    int16_t lines = a2->rows;

    for (int16_t r = lines; r >= 0; r--) {
        uint8_t *p = line;

        for (int16_t c = a2->columns & 7; c >= 0; c--) {
            list->address = p;
            list->value = *p;
            list++;
            *p++ = 1;
        }
        line += 128;
    }
    return list;
}

/* orig $018890: the record `a0` (its word +2 numbers an entry of the table creature_entries, 16 bytes each) is taken out of use (its
 * first word -1) and a free one of the ten records of 36 bytes of table_199ac is filled from the entry (a state 2, the entry's
 * words and bytes and then 14 bytes copied from the table ptr_0b82 + 2, 16 bytes for each of the entry's word +10); when none is
 * free that is all */
void take_record_to_199ac(struct EntryRef *a0)
{
    a0->state = 0xFFFF;
    const struct CreatureEntry *entry = &guest.creature_entries[a0->number];
    struct CreatureType *type = (struct CreatureType *)guest.table_199ac;
    int i;

    for (i = 0; i < 10; i++, type++)
        if (type->status == 0)
            break;
    if (i == 10)
        return;
    const struct CreatureTemplate *template = (const struct CreatureTemplate *)guest.ptr_0b82 + entry->template_index;

    type->status = 2;
    type->spawn_x = entry->spawn_x;
    type->spawn_y = entry->spawn_y;
    type->direction = entry->direction;
    type->bounds = entry->bounds;
    type->delay_step = entry->delay_step;
    type->number = entry->number;
    type->value = entry->value;
    type->flag_18 = entry->flag_18;
    type->speed = entry->speed;
    type->draw_kind = template->draw_kind;
    type->move_routine = template->move_routine;
    type->box_width = template->box_width;
    type->box_height = template->box_height;
    type->step_routine = template->step_routine;
    type->drop = entry->drop;
}


/* orig $018900: like take_record_to_199ac, but the entry's record (its word +2 numbers 8 bytes of unit_scripts) is made into the first
 * free one of the five records of 24 bytes of `units` (a state 0): state 2, the 8 bytes (the byte +6 split into two
 * nibbles), and 14 bytes copied from the table level_creature_templates + 2 (16 bytes for each of the signed byte +5); when none is free that is
 * all */
void take_unit_record(struct EntryRef *a0)
{
    a0->state = 0xFFFF;

    const struct UnitScript *script = &((const struct UnitScript *)guest.unit_scripts)[a0->number];
    struct Unit *unit = guest.units;
    int i;

    for (i = 0; i < 5; i++, unit++)
        if (unit->state == 0)
            break;
    if (i == 5)
        return;
    unit->state = 2;
    unit->count = script->count;
    unit->path = script->path;
    unit->spacing = script->spacing;
    unit->life = script->life;
    unit->aim = script->speeds >> 4;
    unit->rate = script->speeds & 15;

    const struct CreatureTemplate *template = &((const struct CreatureTemplate *)guest.level_creature_templates)[script->template_index];
    unit->draw = (uint8_t)template->draw_kind;
    unit->move = template->move_routine;
    unit->box_width = (int16_t)template->box_width;
    unit->box_height = (int16_t)template->box_height;
    unit->step = template->step_routine;
    unit->drop = script->drop;
}

/* orig $01FA8C: the sound `id` is stopped when a voice plays it: the voice whose entry in voice_ids_207a6 equals the byte +1 of the
 * sound's record in sound_records.  When none does that is all; when one does its entry is cleared and $1FE3E runs
 * (d0 = 22, d1 = the voice) */
void stop_sound(uint16_t id)
{
    uint8_t wanted = sound_records[id].track[0];

    for (int i = 0; i < 4; i++)
        if (guest.voice_ids_207a6[i] == wanted) {
            guest.voice_ids_207a6[i] = 0;
            voice_start(22, (uint16_t)i);
            return;
        }
}

/* orig $016A08: the cells of the map at line_text + 100 (48 columns of 8 pixels, rows of 8) under the point (x, y) are marked with the
 * low byte of *flag when the first of them is in the map (from line_text + 48 to + 1500): two cells side by side, or when bit 2 of y is set
 * the two cells a row down and the two a row further.  Used where a shot did not end in a tile */
void mark_shot_cells(int16_t x, int16_t y, const int16_t *flag)
{
    const uint8_t *map = guest.line_text;
    uint8_t *cell = (uint8_t *)(map + 100) + (x >> 3) + (((y & -8) * 2) + ((y & -8) * 4));
    uint8_t value = (uint8_t)*flag;

    if (cell < map + 48 || cell >= map + 1500)
        return;
    if ((y & 4) == 0) {
        cell[0] = value;
        cell[48] = value;
        cell[1] = value;
        cell[49] = value;
    } else {
        cell[48] = value;
        cell[96] = value;
        cell[49] = value;
        cell[97] = value;
    }
}

/* orig $01695E: a hit at (x, y) on the screen (record: an object record whose byte +72 is not 0): the cell of the tile map
 * (ptr_0b86 + 8192) under the point is looked at; when it is 1 the sound 3 is played and a free one of the twenty records of
 * records_3a0a6 is started there (a flag 0, the position in the level, no speed and a count of 58), and *flag is cleared.  The
 * original goes on at $16A08 when the record's byte is 0 or the cell is not 1 (neither ran) and calls $17B98 first when the
 * tile under the cell is 109 and param_index is 3 or more (also not run) */
void spawn_debris_at_cell(int16_t x, int16_t y, const uint8_t *record, int16_t *flag)
{
    if (record[72] == 0) {
        mark_shot_cells(x, y, flag);
        return;
    }
    int16_t col = (int16_t)(x + guest.scroll_x + 8) & -32;
    int16_t row = (int16_t)(y + guest.scroll_y + 8) & -16;
    const uint8_t *cell = guest.ptr_0b86 + 8192 + (col >> 5) + (int16_t)(row << 3);

    if (*cell != 1) {
        mark_shot_cells(x, y, flag);
        return;
    }
    play_sound(3, 4);
    if (cell[-8192] == 109 && (int16_t)guest.param_index >= 3)
        crumble_start(cell, (const int16_t *)record);   /* $17B98, and the original goes on */
    int16_t *debris = guest.records_3a0a6;
    int i;

    for (i = 0; i < 20; i++, debris += 6)
        if (debris[0] < 0)
            break;
    if (i < 20) {
        debris[0] = 0;
        debris[1] = x + guest.scroll_x;
        debris[2] = y + guest.scroll_y;
        debris[3] = 0;
        debris[4] = 0;
        debris[5] = 58;
        guest.records_3a0a6_in_use++;
    }
    *flag = 0;
}

/* orig $017F46: a record that moved is looked at for being near the screen: on it (the screen's terms: -32 to 320 across and -16 to 192
 * down) or, when it is to the left of it or above, with its image's size (a2: the columns +26 and the rows +27) reaching it: bit 7 of
 * flags_3b0 is set */
static void record_seen(const struct PathRecord *a1, const struct PathDescription *a2)
{
    int16_t sx = (int16_t)(a1->x - guest.scroll_x);
    int16_t sy = (int16_t)(a1->y - guest.scroll_y);

    if (sx > -32) {
        if (sx > 320)
            return;
        if (sy > -16) {
            if (sy <= 192)
                guest.flags_3b0 |= 0x80;
            return;
        }
    }
    int16_t wide = (int16_t)((a2->rows << 5) + sx);
    int16_t high = (int16_t)(((a2->columns & 7) << 4) + sy);

    if (wide > -32 && high > -16)
        guest.flags_3b0 |= 0x80;
}

/* orig $017E9C (with $017E9E and $017F46): the pictures of the 25 records of ptr_0c9e (24 bytes) and ptr_0c9a (28 bytes) are redrawn: the cells marked
 * last frame are given back (entry_table), and each record that is not turned off (byte +4 not negative) is looked at
 * ($18072), has its cells marked in the tile map (entry_table) and its image drawn; one that moved (its first long changed) is
 * looked at once more for being on the screen or within 32 by 16 pixels of it (bit 7 of flags_3b0, $17F46).  The sound 52 is played
 * (priority 8) at the end when bit 7 of flags_3b0 got set, otherwise stopped.  The routine patches itself into an rts when word_4a2 is
 * 0 (the first word of it is then $4E75) */
void redraw_record_images(void)
{
    if (guest.word_17e9c == 0x4E75)
        return;
    if (guest.word_4a2 == 0) {
        guest.word_17e9c = 0x4E75;
        stop_sound(52);
        return;
    }
    guest.flags_3b0 &= 0x7F;
    restore_saved_bytes((struct SavedByte *)guest.entry_table, 49);

    struct SavedByte *list = (struct SavedByte *)guest.entry_table;
    struct PathRecord *a1 = guest.ptr_0c9e;
    const struct PathDescription *a2 = guest.ptr_0c9a;
    int16_t x;
    int16_t y;

    for (int16_t n = 24; n >= 0; n--, a1++, a2++) {
        if (a1->kind < 0)
            continue;
        uint32_t before = *(uint32_t *)a1;

        step_18072(a1, a2);
        if (*(uint32_t *)a1 != before) {
            record_seen(a1, a2);
        }
        x = a1->x;
        y = a1->y;

        list = mark_record_cells(x, y, a2, list);
        draw_record_image(x, y, a2);
    }
    if (guest.flags_3b0 & 0x80)
        play_sound(52, 8);
    else
        stop_sound(52);
}

/* orig $015C54: up to eight particles of a burst at (x, y) take the free records (first word negative) of particle_records, each with
 * the next speeds of particle_speeds_15b5c; the sound 34 (priority 45) is played for each.  Nothing is done while the burst
 * flag_a4c is set and particle_burst_counter is 190 or more.  The first one started sets flag_a4c to -1 (when it was not negative), particle_burst_counter to 0 and
 * word_a54 to -10 */
static void spawn_particle_burst(int16_t x, int16_t y)
{
    if (guest.flag_a4c != 0 && guest.particle_burst_counter >= 0xBE)
        return;

    int16_t *record = guest.particle_records;
    const uint16_t *speeds = particle_speeds_15b5c;
    int started = 0;

    for (int n = 0; n < 80; n++, record += 3) {
        if (record[0] >= 0)
            continue;
        play_sound(0x22, 0x2D);
        record[0] = x;
        record[1] = y;
        record[2] = (int16_t)*speeds++;
        if (guest.flag_a4c >= 0) {
            guest.particle_burst_counter = 0;
            guest.flag_a4c = -1;
            guest.word_a54 = -10;
        }
        if (++started == 8)
            return;
    }
}

/* orig $008822: the end of what falcon_energy counts: when it is negative a burst of particles is started at the place
 * (word_8882 + 4, word_8884 + 4) unless word_0248 is set.  Then falcon_energy, falcon_choice and wing_glyph become -1, and word_214
 * becomes -1 unless one of the three slots holds the object 8 */
void falcon_end(void)
{
    if (guest.falcon_energy < 0 && guest.word_0248 == 0)
        spawn_particle_burst((int16_t)(guest.word_8882 + 4), (int16_t)(guest.word_8884 + 4));
    guest.falcon_energy = -1;
    guest.falcon_choice = -1;
    guest.wing_glyph = -1;
    if (guest.slot_ref_6b6 != 8 && guest.slot_ref_748 != 8 && guest.slot_ref_7da != 8)
        guest.word_214 = -1;
}

/* orig $0159FC (inlined): what the map cell before `after` (a byte not 0) does when a hit reaches it.  A positive byte names a shot
 * (1 to 9 of the first list of shots, 10 to 18 of the second, then the third): the damage word_1cc is the last_value of the
 * list's object (plus one eighth of word_272 - slot_value_sum when that is positive), the sound 3 is played when flag_23a, and the shot
 * is cleared when the object's byte +73 is set; returns 0.  The byte -1 does word_8892 and returns 1; -2 does 10000 (0 with
 * flag_23a), -3 does 0, and a byte below it does 10000 and (as the original goes on at $15AC2) makes a burst of particles at the
 * cell (or sets the damage to 50 with flag_23a); these return 0 */
static int16_t hit_cell_effect(const uint8_t *after)
{
    int16_t type = (int8_t)after[-1];

    if (type < 0) {
        if (type == -1) {
            guest.hit_damage_1cc = guest.word_8892;
            return 1;
        }
        if (type == -2) {
            guest.hit_damage_1cc = guest.flag_23a != 0 ? 0 : 10000;
            return 0;
        }
        if (type == -3) {
            guest.hit_damage_1cc = 0;
            return 0;
        }
        guest.hit_damage_1cc = 10000;

        int32_t offset = (int32_t)(after - guest.line_text) - 100;
        int16_t row = 0;
        int16_t column;

        while (offset >= 48) {   /* divs.w #48 (no division in the C runtime) */
            offset -= 48;
            row++;
        }
        column = (int16_t)offset;
        int16_t x = (int16_t)((column << 3) + guest.scroll_x);
        int16_t y = (int16_t)((row << 3) + guest.scroll_y);

        if (guest.flag_23a != 0)
            guest.hit_damage_1cc = 50;
        else
            spawn_particle_burst(x, y);
        return 0;
    }

    int list_number = 0;

    if (type > 9) {
        list_number = 1;
        type -= 9;
        if (type > 9) {
            list_number = 2;
            type -= 9;
        }
    }

    struct ShotList *list = &((struct ShotList *)guest.shot_lists_raw)[list_number];
    const struct Object *object = &((const struct Object *)guest.object_slots)[list->object];

    guest.hit_damage_1cc = object->last_value;

    int16_t more = (int16_t)(guest.word_272 - guest.slot_value_sum);

    if (more > 0)
        guest.hit_damage_1cc = (int16_t)(guest.hit_damage_1cc + (more >> 3));
    if (guest.flag_23a != 0)
        play_sound(3, 4);
    if (((const uint8_t *)object)[73] != 0)
        *(int16_t *)((uint8_t *)list + 2 + (int16_t)((type - 1) * 8)) = 0;
    return 0;
}

/* orig $018560 (with d2 = d3 = 0): the first free record of records_3a0a6 (first word negative) is started at the place (x, y)
 * of the level with no speed and a count of 58 */
static void start_debris(int16_t x, int16_t y)
{
    for (int i = 0; i < 20; i++) {
        int16_t *record = &guest.records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x;
            record[2] = y;
            record[3] = 0;
            record[4] = 0;
            record[5] = 58;
            guest.records_3a0a6_in_use++;
            return;
        }
    }
}

/* the part of check_hit_and_walls at $D130: the cell before `after` is not 0, so its effect (hit_cell_effect) is looked at.  The
 * type -1 takes the damage off falcon_energy (falcon_end when it goes negative).  With armour_level the damage is 1, less its
 * eighth, quarter ... by armour_level (a shift 3 - armour_level), otherwise the cell's damage; when nothing is left of the strength the
 * player is hit (the sounds 4/44) and -1 comes back.  Otherwise hit_cell_strength is the strength left; with damage left the debris
 * flies off near the hit at (x, y) (levels coordinates) and hit_cell_strength comes back, without damage the strength as it was */
static int16_t hit_marked_cell(const uint8_t *after, int16_t x, int16_t y, int16_t strength)
{
    int16_t flag = hit_cell_effect(after);
    int16_t damage = guest.hit_damage_1cc;
    int16_t left;

    if (flag != 0) {
        guest.falcon_energy = (int16_t)(guest.falcon_energy - strength);
        if (guest.falcon_energy < 0)
            falcon_end();
    }
    if (guest.armour_level != 0) {
        /* the original loads the long $4B79C278 ($D156) and does moveq #1 when it is not 0: that is what the copylock leaves when it
         * fails (the pirated game: -DPIRATED); when it passes the long is 0 and d3 stays the cell's damage */
#ifdef PIRATED
        uint16_t one = 1;
#else
        uint16_t one = (uint16_t)damage;
#endif
        int16_t shift = (int16_t)(3 - guest.armour_level);
        uint16_t reduced;

        left = (int16_t)(strength - one);
        if (shift < 0)
            shift = 0;
        shift &= 63;
        reduced = shift >= 16 ? 0 : (uint16_t)(one >> shift);
        if (reduced == one) {
            int16_t half = (int16_t)(reduced >> 1);
            int16_t rest = (int16_t)(reduced - half);

            reduced = (uint16_t)(half + (rest >> 1));
        }
        left = (int16_t)(left + reduced);
        if (left > 0)
            damage = (int16_t)(strength - left);
    } else {
        left = (int16_t)(strength - damage);
    }
    if (left <= 0) {
        if (guest.word_3a6 == 0) {
            play_sound(4, 15);
        } else {
            play_sound(44, 15);
            play_sound(4, 14);
        }
        return -1;
    }
    guest.hit_cell_strength = left;
    if (damage == 0)
        return strength;

    int16_t half_width = (int16_t)(guest.uncut_hit_box_width >> 1);
    int16_t spread = (int16_t)(half_width & 0xFFF0);
    int16_t random;

    x = (int16_t)(x + half_width);
    spread = (int16_t)((spread != 0 ? spread : 16) - 1);
    random = (int16_t)random_word();
    x = (int16_t)(random < 0 ? x - (random & spread) : x + (random & spread));

    int16_t half_height = (int16_t)(guest.uncut_hit_box_height >> 1);

    spread = (int16_t)(half_height & 0xFFF0);
    y = (int16_t)(y + half_height);
    spread = (int16_t)((spread != 0 ? spread : 16) - 1);
    random = (int16_t)random_word();
    y = (int16_t)(random < 0 ? y - (random & spread) : y + (random & spread));
    start_debris((int16_t)(x - 8), (int16_t)(y - 8));
    return guest.hit_cell_strength;
}

/* orig $00D00A and $00D114 (inside the unrolled loop) and the continuation at $D130: a hit at (x, y) of the given strength.
 * damage_player_at first (the result -1 means the player was hit: the sound 4 is played, or 44 when word_3a6 is not 0, and
 * that is all).  Otherwise uncut_hit_box_width and uncut_hit_box_height keep the size of the box (hit_box_extra_width, hit_box_extra_height) and the box is cut to the
 * screen's cells (8 pixels) in x and y; when nothing is left that is all.  The cells of the map (line_text + 100, 48 columns)
 * that the box covers, less a column, are looked at; the first cell that is not 0 is dealt with by hit_marked_cell and ends the
 * routine.  Returns the strength (or -1, or the strength left after a hit on a cell) */
/* where the original's a4 is after $007EAE / $00D270 wrote a record of the list of hits ($0BD6, while the falcon is there): the next free record.  The routines that give the score of a monster killed ($9DDA) read the byte
 * at +1 of a4 as the monster's value, which is the template's when nothing was written and the low byte of the x of that free record (what an earlier frame left there) when the falcon made the list */
/* the record of the list of the creatures hit that value_c6c (the original's address of it, $BD6 on) names */
static int16_t *hit_list_at(uint32_t address)
{
    return (int16_t *)((uint8_t *)guest.hit_list + (address - 0xBD6));
}

static const uint8_t *hit_list_a4;

static uint8_t kill_value_9dda(uint8_t life)
{
    return hit_list_a4 != 0 ? hit_list_a4[0] : life;
}

int16_t check_hit_and_walls(int16_t x, int16_t y, int16_t strength)
{
    int16_t hit_x = x;
    int16_t hit_y = y;

    hit_list_a4 = 0;

    strength = damage_player_at(x, y, strength);
    if (strength < 0) {
        play_sound(guest.word_3a6 == 0 ? 4 : 44, 15);
        return strength;
    }
    guest.uncut_hit_box_width = guest.hit_box_extra_width;
    guest.uncut_hit_box_height = guest.hit_box_extra_height;
    x = (int16_t)((x - guest.scroll_x + 4) & -8);
    y = (int16_t)((y - guest.scroll_y + 4) & -8);
    if (y > -16) {
        int16_t over = guest.hit_box_extra_height + y;

        if (over >= 208) {
            over -= 208;
            guest.hit_box_extra_height -= over;
            if (guest.hit_box_extra_height <= 0)
                return strength;
        }
    } else {
        y += 16;
        guest.hit_box_extra_height += y;
        if (guest.hit_box_extra_height <= 0)
            return strength;
        y = -16;
    }
    if (x > -16) {
        int16_t over = guest.hit_box_extra_width + x;

        if (x >= 336) {
            over -= 336;
            guest.hit_box_extra_width -= over;
            if (guest.hit_box_extra_width <= 0)
                return strength;
        }
    } else {
        x += 16;
        guest.hit_box_extra_width += x;
        if (guest.hit_box_extra_width <= 0)
            return strength;
        x = -16;
    }
    if (guest.word_214 >= 0) {
        int16_t *list = hit_list_at(guest.value_c6c);

        list[0] = x;
        list[1] = y;
        list[2] = strength;
        guest.value_c6c += 6;
        guest.flag_20e++;
        hit_list_a4 = (const uint8_t *)hit_list_at(guest.value_c6c);
    }

    const uint8_t *cell = guest.line_text + 100 + (x >> 3);

    y &= -8;
    cell += y * 6;
    int16_t rows = (guest.hit_box_extra_height >> 3) - 1;
    int16_t columns = (guest.hit_box_extra_width >> 3) - 1;

    if (rows < 0)
        rows = 0;
    if (columns <= 0)
        columns = 1;
    for (int16_t r = 0; r <= rows; r++) {
        for (int16_t c = 0; c < columns; c++)
            if (*cell++ != 0)
                return hit_marked_cell(cell, hit_x, hit_y, strength);
        cell += 48 - columns;
    }
    return strength;
}

/* orig $018B6C: the slot of item_slots that holds the item is emptied (-1); the first one only */
static void empty_first_slot_with(int16_t item)
{
    for (int i = 0; i < 4; i++)
        if (guest.item_slots[i] == item) {
            guest.item_slots[i] = -1;
            return;
        }
}

/* orig $018AAE and $018AD6: a record whose kind (the word +16 without the top bit) is 2, 3, 4, 7 or 8 empties the slot that holds
 * the item each of its three pairs of words names, when the pair's first word is 1 and the item is 18 to 23 or 127 to 129 */
static void empty_slots_of_record(const struct ScriptRecord *a1)
{
    uint16_t kind = a1->effect & 0x7FFF;

    if (!(kind == 2 || kind == 3 || kind == 4 || kind == 7 || kind == 8))
        return;
    for (int p = 0; p < 3; p++) {
        int16_t item = a1->condition[p].argument;

        if (a1->condition[p].kind == 1 && ((item >= 0x12 && item <= 0x17) || (item >= 0x7F && item <= 0x81)))
            empty_first_slot_with(item);
    }
}

/* orig $018A22, $018A2C and $018A5C: a record whose word +16 has the top bit set empties every slot of item_slots that holds the
 * item each of its pairs of words names (the pair's first word 1) when the item's table_14cf8 entry has the word +4 at 0 or 2 */
static void empty_slots_of_flagged_record(const struct ScriptRecord *a1)
{
    if (!(a1->effect & 0x8000))
        return;
    for (int p = 0; p < 3; p++) {
        if (a1->condition[p].kind != 1)
            continue;
        int16_t item = a1->condition[p].argument;
        uint16_t state = guest.table_14cf8[item].w[2];

        if (state != 2 && state != 0)
            continue;
        for (int i = 0; i < 4; i++)
            if (guest.item_slots[i] == item)
                guest.item_slots[i] = -1;
    }
}

/* the first of the `count` records of 8 bytes in `list` whose word +4 is negative (free), or 0 */
static struct Slot8 *first_free_slot8(struct Slot8 *list, int count)
{
    for (int i = 0; i < count; i++)
        if (list[i].w[2] < 0)
            return &list[i];
    return 0;
}

/* orig $018E38 (kind 0 of dispatch_18ba4; a1 the record): a free record of records_19b14 gets the position (words +12, +14) and the
 * number in the record's word +18 and the state 1; its byte in near_flags is set.  For the numbers 64 to 68 that is all; 83 sets
 * bit 2 of flags_3b0; the position and number are kept in last_drop (last_drop_frame cleared) and the sound 48 is played (priority 30)
 * unless offsets_4cba says 4 for the number */
static void kind_drop_item(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)a1->x, y = (int16_t)a1->y, number = (int16_t)a1->effect_argument;
    struct Slot8 *record = first_free_slot8(guest.records_19b14, 200);

    if (record == 0)
        return;
    record->w[0] = x;
    record->w[1] = y;
    record->w[2] = number;
    record->w[3] = 1;
    guest.near_flags[record - guest.records_19b14] = 0xFF;
    if (number >= 0x40 && number <= 0x44)
        return;
    if (number == 0x53)
        guest.flags_3b0 |= 4;
    guest.last_drop[0] = x;
    guest.last_drop[1] = y;
    guest.last_drop[2] = number;
    guest.last_drop_frame = 0;
    if (offsets_4cba[number] != 4)
        play_sound(0x30, 0x1E);
}

/* orig $018EB6 (kind 1): as kind 0 but the number is the word +18 plus 192 and no sound is played */
static void kind_drop_192(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)a1->x, y = (int16_t)a1->y, number = (int16_t)(a1->effect_argument + 0xC0);
    struct Slot8 *record = first_free_slot8(guest.records_19b14, 200);

    if (record == 0)
        return;
    record->w[0] = x;
    record->w[1] = y;
    record->w[2] = number;
    record->w[3] = 1;
    guest.near_flags[record - guest.records_19b14] = 0xFF;
    guest.last_drop[0] = x;
    guest.last_drop[1] = y;
    guest.last_drop[2] = number;
    guest.last_drop_frame = 0;
}

/* orig $019280: whether one of the first ten records of records_19334 is at (x, y) already */
static int record_19334_at(int16_t x, int16_t y)
{
    for (int i = 0; i < 10; i++)
        if (guest.records_19334[i].w[0] == x && guest.records_19334[i].w[1] == y)
            return 1;
    return 0;
}

/* orig $018D64 (kind 4: looks in the first ten records of records_19334 for a free one) and $018DE4 (kind 3: in all twenty, and
 * then plays the sound 7, priority 5): a free record gets the position of the record a1 (word +12 less 16 and rounded down to 32,
 * word +14 plus 8 rounded down to 16), the state 3 (kind 4) or 1 (kind 3) and the number of the word +18.  When a record is at the
 * position already ($019280) nothing is done */
static void kind_place_19334(const struct ScriptRecord *a1, int twenty)
{
    struct Slot8 *record = first_free_slot8(guest.records_19334, twenty ? 20 : 10);

    if (record == 0)
        return;
    int16_t x = (int16_t)((a1->x & 0xFFE0) - 0x10);
    int16_t y = (int16_t)((a1->y + 8) & 0xFFF0);

    if (record_19334_at(x, y))
        return;
    record->w[0] = x;
    record->w[1] = y;
    record->w[2] = twenty ? 1 : 3;
    record->w[3] = (int16_t)a1->effect_argument;
    if (twenty)
        play_sound(7, 5);
}

/* orig $0192A4 and $0192D6 (kind 2): the block of the tile map (ptr_0b86) at the record a1's position (x rounded to 32, y 8
 * further and rounded to 16) is set to six tiles counted up from the byte at $24D (param_a >> 16) and the three cells of the second plane (+8192)
 * cleared; the sound 7 is played (priority 5) */
static void kind_set_tiles(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)(((a1->x & 0xFFE0) + 12) & 0xFFE0);
    int16_t y = (int16_t)((a1->y + 8) & 0xFFF0);
    uint8_t *cell = guest.ptr_0b86 + (int16_t)(x >> 5) + (int16_t)(y << 3);
    uint8_t base = (uint8_t)(guest.param_a >> 16);   /* the byte at $24D: the long param_a is the machine's, its second byte from the top is not the one at that address here */

    cell[0] = base;
    cell[1] = (uint8_t)(base + 1);
    cell[0x80] = (uint8_t)(base + 10);
    cell[0x81] = (uint8_t)(base + 11);
    cell[0x100] = (uint8_t)(base + 20);
    cell[0x101] = (uint8_t)(base + 21);
    cell += 0x2000;
    cell[0] = 0;
    cell[0x80] = 0;
    cell[0x100] = 0;
    play_sound(7, 5);
}

/* orig $0193D4 (kind 9): the block of the tile map at the record a1's position (as kind 2 finds it) is closed again: the six tiles are the byte of
 * param_a's low word, and 10 and 20 more under it, with the byte of param_c next to them, and the three cells of the second plane are 1; the sound 43
 * is played (priority 5) */
static void kind_close_tiles(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)(((a1->x & 0xFFE0) + 12) & 0xFFE0);
    int16_t y = (int16_t)((a1->y + 8) & 0xFFF0);
    uint8_t *cell = guest.ptr_0b86 + (int16_t)(x >> 5) + (int16_t)(y << 3);
    uint8_t tile = (uint8_t)guest.param_a;
    uint8_t other = (uint8_t)guest.param_c;

    cell[0] = tile;
    cell[0x80] = (uint8_t)(tile + 10);
    cell[0x100] = (uint8_t)(tile + 20);
    cell[1] = other;
    cell[0x81] = other;
    cell[0x101] = other;
    cell += 0x2000;
    cell[0] = 1;
    cell[0x80] = 1;
    cell[0x100] = 1;
    play_sound(0x2B, 5);
}

/* orig $0195DA: the entry of table_19456 (twenty of three words: x, y, a state) whose place is (x, y), or 0 */
static uint16_t *find_entry_19456(int16_t x, int16_t y)
{
    for (int i = 0; i < 20; i++)
        if ((int16_t)guest.table_19456[i * 3] == x && (int16_t)guest.table_19456[i * 3 + 1] == y)
            return &guest.table_19456[i * 3];
    return 0;
}

/* orig $019618 (kind 7: state 1, the tile of param_b's third byte, the second map 0, sound 7) and $019658 (kind 8: state 0, param_b's low byte, the second map 1, sound 43): the
 * entry of table_19456 at the record a1's place (x, y) gets its state, and the cell of the map there (x / 32 across, y / 16 down) its tile and the cell of the second map its
 * byte; the sound has priority 5.  Nothing is done when no entry is there */
static void kind_set_entry(const struct ScriptRecord *a1, int on)
{
    uint16_t *entry = find_entry_19456(a1->x, a1->y);

    if (entry == 0)
        return;
    entry[2] = on ? 1 : 0;

    uint8_t *cell = guest.ptr_0b86 + (int16_t)(a1->x >> 5) + (int16_t)(a1->y << 3);

    cell[0] = on ? (uint8_t)(guest.param_b >> 16) : (uint8_t)guest.param_b;   /* ($251 and $253: bytes of the long at $250) */
    cell[0x2000] = on ? 0 : 1;
    play_sound(on ? 7 : 0x2B, 5);
}

/* orig $018C50 (the box d0 to d2 across and d1 to d3 down: the entry of $018C30, kind 6, with the box 12 round the record a1's position (words +12, +14)): every record of records_19b14 within 12 of the record a1's position (words +12, +14) whose number's
 * table_14cf8 entry has the word +4 at 4 is taken out (word +4 -1, +6 0) and makes a mark in records_316 (state 0, x less 8, y
 * (8 less for the number 96)), at most two, each with the sound 4 (priority 15); the marks go to the first free entry and on
 * from there (this may pass the end of the four, as in the original) */
static void thing_burst_box(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t *mark = guest.records_316;
    int16_t hits = 0;

    guest.finish_mark_count = 0;
    for (int i = 0; i < 4 && mark[0] >= 0; i++)
        mark += 3;
    if (mark >= guest.records_316 + 12)
        mark = guest.records_316 + 9;
    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[2] < 0)
            continue;
        if (guest.table_14cf8[record->w[2]].w[2] != 4)
            continue;
        int16_t x = record->w[0], y = record->w[1];

        if (x0 > x || x1 < x || y0 > y || y1 < y)
            continue;
        int is_96 = record->w[2] == 0x60;

        record->w[2] = -1;
        record->w[3] = 0;
        mark[0] = 0;
        mark[1] = (int16_t)(x - 8);
        mark[2] = is_96 ? (int16_t)(y - 8) : y;
        mark += 3;
        play_sound(4, 15);
        guest.finish_mark_count = ++hits;
        if (hits == 2)
            return;
    }
}

static void kind_burst(const struct ScriptRecord *a1)
{
    thing_burst_box((int16_t)(a1->x - 12), (int16_t)(a1->y - 12), (int16_t)(a1->x + 12), (int16_t)(a1->y + 12));
}

/* orig $018F16 (kind 10): the slot_ref (6b6, 748, 7da) that equals the word +18 of the record a1 becomes -1; the first only */
static void kind_free_slot_ref(const struct ScriptRecord *a1)
{
    int16_t number = (int16_t)a1->effect_argument;

    if (guest.slot_ref_6b6 == number)
        guest.slot_ref_6b6 = -1;
    else if (guest.slot_ref_748 == number)
        guest.slot_ref_748 = -1;
    else if (guest.slot_ref_7da == number)
        guest.slot_ref_7da = -1;
}

/* orig $01895A: the record a0 (its word +2 numbers a record of 24 bytes of script_records) is looked at by call_by_kind (kind and
 * argument: its three pairs of words; the result of each into words_300); when all three results (words_300) are still -1 the
 * record a0 is finished: its first word becomes -1, a message of the record's word +20 is requested when it has one (priority
 * 50), the slots its items are in are emptied (empty_slots_of_record, empty_slots_of_flagged_record) and the routine its kind
 * (word +16 without the top bit) names in dispatch_18ba4 does its part (written in here: 0 to 6, 10 to 15; 11 to 13 and 15 do
 * nothing, 14 clears tick_count, 9 closes a block of tiles; 7 and 8 ($19618, $19658) did not run and are left out).  Nothing is done while
 * level_ended */
void finish_record_1895a(struct EntryRef *a0)
{
    if (guest.level_ended)
        return;

    const struct ScriptRecord *a1 = &guest.script_records[a0->number];

    *(int32_t *)guest.words_300 = -1;
    guest.words_300[2] = -1;
    call_by_kind(a1->condition[0].kind, a1->condition[0].argument, &guest.words_300[0]);
    call_by_kind(a1->condition[1].kind, a1->condition[1].argument, &guest.words_300[1]);
    call_by_kind(a1->condition[2].kind, a1->condition[2].argument, &guest.words_300[2]);
    if ((int16_t)(guest.words_300[0] & guest.words_300[1] & guest.words_300[2]) >= 0)
        return;
    a0->state = 0xFFFF;

    const int16_t *offset = (const int16_t *)(guest.message_offsets + (uint16_t)(a1->message * 2));

    if (*offset >= 0) {
        guest.message_next = guest.message_base + *offset;
        if (*guest.message_next != 0)
            message_request(50);
    }
    empty_slots_of_record(a1);
    empty_slots_of_flagged_record(a1);
    int effect = a1->effect & 0x7FFF;

    switch (effect) {
    case 0:
        kind_drop_item(a1);
        break;
    case 1:
        kind_drop_192(a1);
        break;
    case 2:
        kind_set_tiles(a1);
        break;
    case 3:
        kind_place_19334(a1, 1);
        break;
    case 4:
        kind_place_19334(a1, 0);
        break;
    case 5:
        run_cell_entry((int16_t)(a1->effect_argument - 1));
        break;
    case 6:
        kind_burst(a1);
        break;
    case 10:
        kind_free_slot_ref(a1);
        break;
    case 9:
        kind_close_tiles(a1);
        break;
    case 14:
        guest.tick_count = 0;
        break;
    case 7:
        kind_set_entry(a1, 1);
        break;
    case 8:
        kind_set_entry(a1, 0);
        break;
    default:
        break;
    }
}

/* (the three copies of one piece of $00FDBA) the first of up to three entries of 24 bytes of a unit (the first at `entry`,
 * `*number` the number of the first) that words_all_zero says is free, looking at no more than `count` of them (the third is
 * always looked at); returns it, or 0 */
static uint16_t *first_free_entry(uint16_t *entry, int16_t count, int16_t *number)
{
    for (int i = 0; i < 3; i++, entry += 12, *number += 3) {
        if (words_all_zero(entry))
            return entry;
        if (i < 2 && --count == 0)
            return 0;
    }
    return 0;
}

/* orig $00FDBA: each of the three units (the records at slot_ref_6b6, _748 and _7da, 146 bytes apart; a negative first word means
 * it is not there) that is there gets a free entry of its (the first of three entries of 24 bytes after the word), as many as
 * the count in the word at +10 of its object_slots record allows: the entry is kept in entry_b92 and its number in entry_id_2ea.
 * The first unit sets word_2f0 when its object has the count 1 and the others then leave it alone.  Returns 0 when some
 * unit got an entry, else 1 (also 1 and nothing done when fire_latch is not 0; fire_latch is set) */
uint16_t claim_free_entries(void)
{
    int16_t *refs[3] = { &guest.slot_ref_6b6, &guest.slot_ref_748, &guest.slot_ref_7da };
    int16_t first_id[3] = { 1, 10, 19 };
    uint16_t result = 1;

    if (guest.fire_latch != 0)
        return 1;
    guest.fire_latch = 1;
    guest.word_2f0 = 0;
    guest.entry_b92[0] = 0;
    guest.entry_b92[1] = 0;
    guest.entry_b92[2] = 0;
    for (int u = 0; u < 3; u++) {
        if (*refs[u] < 0)
            continue;
        int16_t count = ((uint16_t *)&guest.object_slots[*refs[u]])[5];   /* the word at +10 of the object's record ($A(a2,d4)): how many fans of shots it may have at once */
        int16_t number = first_id[u];

        if (u > 0 && count < 0)
            continue;
        uint16_t *entry = first_free_entry((uint16_t *)refs[u] + 1, count, &number);

        if (entry == 0)
            continue;
        if (u > 0 && count == 1 && guest.word_2f0 != 0)
            continue;
        guest.entry_b92[u] = entry;
        guest.entry_id_2ea[u] = number;
        result = 0;
        if (u == 0 && count == 1)
            guest.word_2f0 = 1;
    }
    return result;
}


/* ======== from game_c.c ======== */
/* Gods, the game: the routines at $10000-$167FF (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */

#define OBJECT(n) (&((struct Object *)guest.object_slots)[(n)])
#define shot_lists ((struct ShotList *)guest.shot_lists_raw)

/* orig $0112F6: where in the level's second map (8192 bytes after ptr_0b86; a tile is 32 by 16 pixels, a row of them 128 bytes) the
 * tile at the player's place is */
uint8_t *player_tile_ptr(void)
{
    int16_t x = guest.player_x & -32;
    int16_t y = guest.player_y & -16;
    uint8_t *tile = guest.ptr_0b86 + 8192;

    tile += x >> 5;
    tile += (int16_t)(y << 3);
    return tile;
}

/* orig $011346: 1 when the tile at the player's place has the type 1 three rows down, or the next tile does while the player is 8 or
 * more pixels into his tile; else 0 */
int16_t tile_one_three_rows_down(void)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] == 1)
        return 1;
    if ((guest.player_x & 31) >= 8 && tile[385] == 1)
        return 1;
    return 0;
}

/* orig $01136C: the same for the row of the player's tile itself */
int16_t tile_one_at_player(void)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[0] == 1)
        return 1;
    if ((guest.player_x & 31) >= 8 && tile[1] == 1)
        return 1;
    return 0;
}

/* orig $015CC4: while a burst of particles goes on (flag_a4c negative): the counters particle_burst_counter (by 2), word_a54, particle_glyph_step (0 to 2: the picture) and word_a50 (0 to 7) count on; each of
 * the 80 records of particle_records in use (x, y, and a word of two signed bytes: the speed across and down) is moved by its speeds (the speed down goes up by one when word_a50 is 0), drawn as
 * the glyph 77 + particle_glyph_step with its cells marked (mark_shot_cells with the byte $FC) when it is in the screen (8 outside it to the left, above, and 319 to the right, 191 down) and is
 * freed when it is not; while word_a54 is not negative a random number at most twice (190 - particle_burst_counter) starts a new burst of particles at the particle (word_a54 -10).  When none was in
 * use flag_a4c is 0 */
void fn_15cc4_c(void)
{
    static const int16_t shot_mark = (int16_t)0xFCFC;   /* ($15CB8: the byte $FC is the one it marks with) */

    if (guest.flag_a4c >= 0)
        return;
    guest.particle_burst_counter += 2;
    guest.word_a54++;
    guest.particle_glyph_step++;
    if (guest.particle_glyph_step >= 3)
        guest.particle_glyph_step = 0;
    guest.word_a50 = (int16_t)((guest.word_a50 + 1) & 7);

    int16_t *record = guest.particle_records;
    int16_t live = 0;

    for (int n = 0; n < 80; n++, record += 3) {
        if (record[0] < 0)
            continue;
        live++;

        int16_t x = (int16_t)(record[0] + (int8_t)(record[2] >> 8));
        int16_t y = (int16_t)(record[1] + (int8_t)(record[2] & 0xFF));

        record[0] = x;
        record[1] = y;
        if (guest.word_a50 == 0)
            record[2] = (int16_t)((record[2] & 0xFF00) | ((record[2] + 1) & 0xFF));

        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);

        if (sx > 0x13F || sx < -8 || sy < -8 || sy > 0xBF) {
            record[0] = -1;
            continue;
        }
        mark_shot_cells(sx, sy, &shot_mark);
        glyph_draw(sx, sy, (uint16_t)(0x4D + guest.particle_glyph_step));
        if (guest.word_a54 < 0)
            continue;
        if (random_word() > (uint16_t)((0xBE - guest.particle_burst_counter) << 1))
            continue;
        guest.word_a54 = -10;
        spawn_particle_burst(record[0], record[1]);
    }
    if (live == 0)
        guest.flag_a4c = 0;
}

/* orig $016B6E: the cells of the map at line_text + 100 (48 columns of 8 pixels) under the screen point (x, y) are marked with $FE: the cell 48 bytes (a row) before it when bit 3 of y is 0,
 * the one a row after when it is 1; nothing when the cell is outside the map (before line_text + 48 or from line_text + 1500) */
static void mark_map_cell(int16_t x, int16_t y)
{
    const uint8_t *map = guest.line_text;
    int16_t offset = (int16_t)(y & (int16_t)0xFFF8);
    uint8_t *cell = (uint8_t *)(map + 0x64) + (int16_t)(x >> 3);

    offset = (int16_t)(offset << 1);
    cell += offset;
    offset = (int16_t)(offset << 1);
    cell += offset;
    if (cell < map + 0x30 || cell >= map + 0x5DC)
        return;
    if ((y & 8) == 0)
        cell[-0x30] = 0xFE;
    else
        cell[0x30] = 0xFE;
}

/* orig $016AEE: the invulnerability (flag_aa8 is 1): four sparkles go round the player, each at a place of the circle orbit_offsets (the four places are the words words_a9e, going on by 2 a frame,
 * modulo 64); the cells under each are marked ($16B6E) and the sparkle is drawn as the glyph 87 + ((its number * 2 + cursor_frame) & 3); cursor_frame goes on every other frame (when frame_toggle is 0);
 * word_a9a counts down and, below 0, the invulnerability is over (flag_aa8 0, the sound 60 stopped) */
void fn_16aee_c(void)
{
    if (guest.flag_aa8 != 1)
        return;
    /* (the original's bytes at $FBF2: thirty-two places of a circle, x then y) */
    static const int8_t orbit_offsets[64] = {
        11, 50, 15, 50, 20, 48, 24, 45, 27, 41, 30, 36, 33, 31, 34, 25,
        34, 19, 34, 12, 33, 6, 30, 1, 27, -4, 24, -8, 20, -11, 15, -13,
        11, -13, 7, -13, 2, -11, -2, -8, -5, -4, -8, 1, -11, 6, -12, 12,
        -12, 19, -12, 25, -11, 31, -8, 36, -5, 41, -2, 45, 2, 48, 7, 50,
    };
    uint16_t *place = guest.words_a9e;

    for (int n = 3; n >= 0; n--, place++) {
        int16_t x = (int16_t)(orbit_offsets[*place] + guest.player_x - guest.scroll_x);
        int16_t y = (int16_t)(orbit_offsets[*place + 1] + guest.player_y - guest.scroll_y);

        mark_map_cell(x, y);
        glyph_draw(x, y, (uint16_t)(((n * 2 + guest.cursor_frame) & 3) + 0x57));
        *place = (uint16_t)((*place + 2) & 0x3F);
    }
    if (guest.frame_toggle == 0)
        guest.cursor_frame = (uint16_t)((guest.cursor_frame + 1) & 3);
    guest.word_a9a--;
    if (guest.word_a9a < 0) {
        guest.flag_aa8 = 0;
        stop_sound(0x3C);
    }
}

/* orig $01290C: the sprite of index `index` is drawn where the player is, relative to the scroll kept while the loop runs */
void draw_player_sprite(uint16_t index)
{
    sprite_draw(guest.player_x - guest.scroll_x_a, guest.player_y - guest.scroll_y_a, index);
}

/* orig $01695E: a shot of an object (`object`) that is at (x, y) on the screen: when the object hits tiles (byte +72) and the tile of the
 * second map there (the position, plus the scroll, plus 8, rounded to a tile) has the type 1 the shot ends: sound 3 is played, a record of
 * records_3a0a6 that is free (first word negative) gets the place of the hit (no speed, count 58) and the shot is cleared.  When the tile
 * is 109 on the first map and param_index is 3 or more the original calls $17B98, which did not run. */
void shot_hit_tile(const struct Object *object, struct Shot *shot, int16_t x, int16_t y)
{
    if (object->hits_tiles == 0) {
        mark_shot_cells(x, y, &shot->active);
        return;
    }

    int16_t tx = (int16_t)((x + guest.scroll_x + 8) & -32);
    int16_t ty = (int16_t)((y + guest.scroll_y + 8) & -16);
    uint8_t *tile = guest.ptr_0b86 + 8192;

    tile += tx >> 5;
    tile += (int16_t)(ty << 3);
    if (*tile != 1) {
        mark_shot_cells(x, y, &shot->active);
        return;
    }
    play_sound(3, 4);
    if (tile[-8192] == 109 && guest.param_index >= 3)
        crumble_start(tile, (const int16_t *)object);   /* $169BE: $17B98, and the original goes on */
    for (int i = 0; i < 20; i++) {
        int16_t *record = &guest.records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x + guest.scroll_x;
            record[2] = y + guest.scroll_y;
            record[3] = 0;
            record[4] = 0;
            record[5] = 58;
            guest.records_3a0a6_in_use++;
            shot->active = 0;
            return;
        }
    }
    shot->active = 0;   /* the original's $16A04 (no free record) */
}

/* how a shot of a direction moves: the first twelve words of the table are what the power-ups change (block_16bb4, the same memory as the table on the Amiga; on the host the table
 * the C reads is a copy, so the words the program changes are read from the memory it changes) */
static int16_t shot_delta(int index)
{
    if (index < 12)
        return (int16_t)((const uint16_t *)guest.block_16bb4)[index];
    return (int16_t)shot_deltas[index];
}

/* orig $01673E and $0167CE: the nine shots of a list of an object (handler 0 of the table at $15260) are moved by the delta of their
 * direction, and the ones that are on the screen are drawn as the object's draw type says (type 0: a glyph, after the test against the
 * tiles); one that has left the screen (32 or more outside to the left or up, 320 or 192 on) is cleared.  Then the object's frame goes on
 * (back to 0 after max_frame) unless it has a negative anim_mode.  The draw types 1, 2 and 3 go on at $167E0, $167F2 and $16858, which
 * did not run. */
static void update_shots(struct ShotList *list, struct Object *object)
{
    struct Shot *shot = list->shot;

    for (int n = 0; n < 9; n++, shot++) {
        if (shot->active == 0)
            continue;
        int16_t x = shot->x;
        int16_t y = shot->y;

        guest.shot_prev_x = x;
        guest.shot_prev_y = y;
        x += shot_delta(shot->direction * 2);
        y += shot_delta(shot->direction * 2 + 1);
        shot->x = x;
        shot->y = y;
        x -= guest.scroll_x;
        y -= guest.scroll_y;
        if (x <= -32 || y <= -32 || x >= 320 || y >= 192) {
            shot->active = 0;   /* orig $016898 */
            continue;
        }

        int16_t frame;

        if (object->anim_mode < 0 && object->alt_frame != 0)
            frame = object->alt_frame + object->frame;
        else
            frame = object->frame + object->base_frame;
        if (object->draw_type == 0) {
            shot_hit_tile(object, shot, x, y);
            glyph_draw(x, y, frame);
        } else if (object->draw_type == 1) {
            /* orig $0167E0: the cells under the shot are marked (3 by 3), a two word image is drawn */
            mark_shot_cells_3x3(x, y, shot);
            sprite_draw(x, y, frame);
        } else if (object->draw_type == 2) {
            /* orig $0167F2: a glyph and an image side by side, the image first (at y + 2) with a positive anim_mode */
            mark_shot_cells_3x3(x, y - 16, shot);
            if (object->anim_mode >= 0) {
                sprite_draw(x, y + 2, frame);
                glyph_draw(x + 32, y, frame + 1);
            } else {
                glyph_draw(x, y, frame);
                sprite_draw(x + 16, y + 2, frame + 1);
            }
        } else {
            /* orig $016858: the tile test, then a glyph while the shot lives */
            shot_hit_tile(object, shot, x, y);
            if (shot->active != 0)
                glyph_draw(x, y, frame);
        }
    }

    int16_t frame = object->frame;

    if (object->anim_mode >= 0) {
        frame++;
        if (frame > object->max_frame)
            frame = 0;
    } else {
        frame--;   /* orig $01688E */
        if (frame < 0)
            frame = object->max_frame;
    }
    object->frame = frame;
}

/* the nine shots of the magic axe have each three words at $66E (the table of the handler 5): the sideways step, the vertical step and the frames left (the spawn gives 4, a step of the table of the
 * power-ups and 50); the words are the host's own, a quickload makes them so */
static int16_t *axe_table(void)
{
    return guest.axe_steps;
}

/* orig $016592, $0165C6 and $0165FA: the cell of the second map (32 by 16, a 1 is a wall) that the point (x, y) is in, as the three tests of the axe look at it */
static const uint8_t *axe_cell(int16_t x, int16_t y)
{
    return guest.ptr_0b86 + 0x2000 + ((int16_t)(x & 0xFFE0) >> 5) + (int16_t)(uint16_t)((y & 0xFFF0) << 3);
}

/* orig $016592 (the wall to the right: the cell next to it, and the one under that when the point is not on a row's line) and $0165C6 (to the left) */
static int axe_wall_ahead(int16_t x, int16_t y, int right)
{
    const uint8_t *cell = axe_cell(x, y);

    if ((y & 0xF) != 0) {
        if (right ? cell[0x81] == 1 : cell[0x7F] == 1)
            return 1;
        return right ? cell[1] == 1 : cell[-1] == 1;
    }
    return cell[1] == 1;   /* (on a row's line the original's left test jumps into the right one: $0165E8 to $0165BE) */
}

/* orig $01661E (handler 5 of the table at $15260: the magic axe): each of the nine shots that is in use goes on by its table's steps: it turns at a wall (the step -4 or 4, when it is in the far part of
 * the cell: more than 12 of 32 to the right, less than 4 to the left), it falls with a step that goes up by 2 to 8 and bounces off a floor with it negated (a cell of the first map under it, not in a wall: the
 * y goes to the row's line; a step of 0 becomes -4); one that is off the screen is cleared, one whose frames have run out ends with a spark of the table's steps, the others are drawn (the glyph 426 + 1 + the
 * object's frame and the shot's place from 6, or the first one on the bounce) after the tile test.  The object's frame goes up to 5 */
static void update_shots_axe(struct ShotList *list, struct Object *object)
{
    struct Shot *shot = list->shot;
    int16_t *table = axe_table();

    for (int n = 0; n < 9; n++, shot++, table += 3) {
        int16_t d7 = (int16_t)(8 - n);

        if (shot->active == 0)
            continue;

        int16_t x = shot->x;
        int16_t y = shot->y;
        int16_t step_x = table[0];
        int16_t step_y = table[1];

        if (step_x >= 0) {
            if ((x & 31) > 12 && axe_wall_ahead(x, y, 1)) {
                step_x = -4;
                table[0] = step_x;
            }
        } else if ((x & 31) < 4 && axe_wall_ahead(x, y, 0)) {
            step_x = 4;
            table[0] = step_x;
        }

        int landed = 0;

        x = (int16_t)(x + step_x);
        y = (int16_t)(y + step_y);
        shot->x = x;
        if (step_y >= 0 && axe_cell(x, y)[0x80] == 1 && axe_cell(x, y)[0] != 1) {
            y = (int16_t)(y & 0xFFF0);
            landed = 1;
            step_y = (int16_t)-step_y;
            table[1] = step_y;
            if (step_y == 0)
                table[1] = -4;
        } else {
            step_y = (int16_t)(step_y + 2);
            if (step_y > 8)
                step_y = (int16_t)(step_y - 2);
            table[1] = step_y;
        }
        shot->y = y;

        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);

        if (sx <= -32 || sy <= -32 || sx >= 320 || sy >= 192) {
            shot->active = 0;
            continue;
        }
        if (--table[2] < 0) {
            start_debris_moving(sx, sy, table[0], table[1]);
            shot->active = 0;
            continue;
        }

        int16_t glyph = 0x1AA;

        if (!landed) {
            int16_t frame = (int16_t)(object->frame + d7);

            if (frame >= 6) {
                frame = (int16_t)(frame - 6);
                if (frame >= 6)
                    frame = (int16_t)(frame - 6);
            }
            glyph = (int16_t)(glyph + frame + 1);
        }
        shot_hit_tile(object, shot, sx, sy);
        glyph_draw(sx, sy, (uint16_t)glyph);
    }
    object->frame++;
    if (object->frame >= 6)
        object->frame = 0;
}

/* ======== the bouncing ball (the fire kinds 8 and 9, orig $015870 and $015898, the object handler 3, $015DB2 to $01634C) ======== */

/* orig $015870 (to the left) and $015898 (to the right, 16 on): sound 2 (priority 4); the ball goes at 4 across (right) or -4 (left), no fall, no jump ($206, $208 and $20A), and the list's first shot is the number,
 * the place (the y 32 up, a multiple of 4) and 47 frames of life */
static void spawn_shot_ball(struct Shot *slot, int16_t id, int16_t x, int16_t y, int right)
{
    int16_t *speed_x = &guest.ball_values[0];
    int16_t *speed_y = &guest.ball_values[1];
    int16_t *jump = &guest.ball_values[2];

    play_sound(2, 4);
    if (right) {
        x = (int16_t)(x + 0x10);
        *speed_x = 4;
    } else {
        *speed_x = -4;
    }
    *speed_y = 0;
    *jump = -1;
    y = (int16_t)((y - 0x20) & 0xFFFC);
    slot[0].active = id;
    slot[0].x = x;
    slot[0].y = y;
    slot[0].direction = 0x2F;
}

/* the ten pieces of the ball's burst ($E6C: x, y, the steps across and down, the picture count; the count negative: free) and the seven pillars of fire ($A6E: x, y, the count) */
struct BallPiece {
    int16_t x;
    int16_t y;
    int16_t speed_x;
    int16_t speed_y;
    int16_t count;
};

struct BallPillar {
    int16_t x;
    int16_t y;
    int16_t count;
};

/* the collision map's cell (ptr_0b86 + 8192) under (x, y) */
static uint8_t *ball_cell(int16_t x, int16_t y)
{
    return guest.ptr_0b86 + 0x2000 + (int16_t)((int16_t)(x & 0xFFE0) >> 5) + (int16_t)((int16_t)(y & 0xFFF0) << 3);
}

/* orig $0160FC: the ball is at (x, y): the shot's x is kept, its life counts down and it is drawn as the glyph 339 + the low two bits of the count it had */
static void ball_shot_draw(struct Shot *shot, int16_t x, int16_t y, int16_t counter)
{
    shot->x = x;
    shot->direction--;
    glyph_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)((counter & 3) + 0x153));
}

/* orig $01634E: a pillar of fire (every fourth picture) that has a corner of the player's box (5 and 27 across, 4 and 44 down from the screen place of the player) in its box of 32 by 32 takes an energy off */
static void ball_pillar_hurts(int16_t x, int16_t y, int16_t image)
{
    if ((image & 3) != 0)
        return;

    int16_t x2 = (int16_t)(x + 0x20);
    int16_t y2 = (int16_t)(y + 0x20);
    int16_t px = (int16_t)(guest.player_x - guest.scroll_x);
    int16_t py = (int16_t)(guest.player_y - guest.scroll_y);
    int16_t left = (int16_t)(px + 5), right = (int16_t)(px + 0x1B), top = (int16_t)(py + 4), bottom = (int16_t)(py + 0x2C);
    const int16_t corners[4][2] = { { left, top }, { right, top }, { left, bottom }, { right, bottom } };

    for (int i = 0; i < 4; i++) {
        if (corners[i][0] >= x && corners[i][0] <= x2 && corners[i][1] >= y && corners[i][1] <= y2) {
            guest.player_energy--;
            return;
        }
    }
}

/* orig $01611E and $016274 (the life of the ball is below 0): at -1 it bursts (sound 4, priority 16: pieces fly up and sideways, as many pairs (at most five) as the object's last value less its first,
 * the pillars of fire start staggered, the things of kind 4 in the box are hit: thing_burst_box); every frame the pieces move (the step down grows every third frame), are drawn as the glyph 77 + their count when they are on
 * the screen (the tiles are looked at: a piece that hits one is over) and the pillars go through their seven pictures (the sprites 108 to 114), mark the cells and hurt the player; when nothing is left the shot is over */
static void ball_burst(struct Object *object, struct Shot *shot, int16_t x, int16_t y, int16_t counter)
{
    struct BallPiece *pieces = (struct BallPiece *)guest.ball_pieces;
    struct BallPillar *pillars = (struct BallPillar *)guest.ball_pillars;

    if (counter == -1) {
        static const int16_t steps[5] = { -8, -6, -4, -2, -10 };

        play_sound(4, 0x10);
        for (int i = 0; i < 10; i++)
            pieces[i].count = -1;

        int16_t pairs = (int16_t)(object->last_value - object->first_value);

        if (pairs != 0) {
            for (int n = 0; n < 5; n++) {
                pieces[2 * n] = (struct BallPiece){ x, y, 5, steps[n], 0 };
                pieces[2 * n + 1] = (struct BallPiece){ x, y, -5, steps[n], 0 };
                pairs--;
                if (pairs == 0)
                    break;
            }
        }

        int16_t px = (int16_t)(x - 8);
        int16_t py = (int16_t)(y - 0x10 - 0x7E);
        int16_t count = -12;

        for (int k = 0; k < 7; k++) {
            pillars[k] = (struct BallPillar){ px, py, count };
            py = (int16_t)(py + 0x15);
            count = (int16_t)(count + 2);
        }
        shot->direction--;

        int16_t valid = (int16_t)(object->last_value - object->first_value + 2);

        for (int m = 1; m <= 7; m++) {
            valid--;
            if (valid < 0)
                pillars[7 - m].count = (int16_t)0xD8F0;
        }
        thing_burst_box((int16_t)(x - 0x10), (int16_t)(y - 0x14), (int16_t)(x + 0x10), (int16_t)(y + 4));
    }

    int16_t alive = 0;

    for (int i = 0; i < 10; i++) {
        struct BallPiece *piece = &pieces[i];

        if (piece->count < 0)
            continue;
        alive++;
        piece->count++;
        if (piece->count >= 3) {
            piece->count = 0;
            piece->speed_y++;
        }
        piece->x = (int16_t)(piece->x + piece->speed_x);
        piece->y = (int16_t)(piece->y + piece->speed_y);

        int16_t sx = (int16_t)(piece->x - guest.scroll_x);
        int16_t sy = (int16_t)(piece->y - guest.scroll_y);

        if (sx > 0x13F || sx < -8 || sy < -8 || sy > 0xBF) {
            piece->count = -1;
            continue;
        }

        uint16_t image = (uint16_t)(piece->count + 0x4D);

        shot_hit_tile(object, shot, sx, sy);
        if (shot->active != 0) {
            glyph_draw(sx, sy, image);
        } else {
            shot->active = 1;
            piece->count = -1;
        }
    }
    for (int k = 0; k < 7; k++) {
        struct BallPillar *pillar = &pillars[k];

        if (pillar->count >= 0) {
            alive++;

            int16_t sx = (int16_t)(pillar->x - guest.scroll_x);
            int16_t sy = (int16_t)(pillar->y - guest.scroll_y);
            int16_t image = (int16_t)(pillar->count + 0x6C);

            mark_shot_cells_3x3(sx, sy, shot);
            if (guest.flag_aa8 == 0)
                ball_pillar_hurts(sx, sy, image);
            sprite_draw(sx, sy, (uint16_t)image);
        }
        pillar->count++;
        if (pillar->count >= 7)
            pillar->count = -100;
    }
    if (alive == 0)
        shot->active = 0;
}

/* orig $015DB2 (the handler 3 of the table at $15260: the bouncing ball): the list's first shot goes on, its words the number, x, y and the frames it has left.  Going across ($206 not 0) it falls (the step $208 up to 4,
 * every fourth frame), lands on a floor (the cells two rows down: the speed is 0 and the sound 11) or turns at a wall (the sound 11 and the speed negated); with no speed across it jumps along the table at $F2
 * (the sounds at $10A, the count in $20A; 100 is the landing) or falls ($20A negative) until it lands.  When its life is below 0 it bursts (ball_burst) */
static void update_shots_ball(struct ShotList *list, struct Object *object)
{
    struct Shot *shot = &list->shot[0];
    int16_t *speed_x = &guest.ball_values[0];
    int16_t *speed_y = &guest.ball_values[1];
    int16_t *jump = &guest.ball_values[2];

    if (shot->active == 0)
        return;

    int16_t x = shot->x;
    int16_t y = shot->y;
    int16_t counter = shot->direction;

    if (counter < 0) {
        ball_burst(object, shot, x, y, counter);
        return;
    }

    uint8_t *cell = ball_cell(x, y);

    if (*speed_x != 0) {
        if (((counter + 2) & 3) == 0) {
            (*speed_y)++;
            if (*speed_y > 4)
                *speed_y = 4;
        }
        if ((int16_t)((y + *speed_y) & 0xFFF0) != (int16_t)(y & 0xFFF0)) {
            int land = (cell[0x100] == 1 && cell[0x80] != 1) || ((x & 0x1F) >= 0x10 && cell[0x101] == 1 && cell[0x81] != 1);

            if (land) {
                y = (int16_t)((y + *speed_y) & 0xFFF0);
                shot->y = y;
                *speed_x = 0;
                *speed_y = 0;
                *jump = 0;
                play_sound(0xB, 6);
                ball_shot_draw(shot, x, y, counter);
                return;
            }
        }
        y = (int16_t)(y + *speed_y);
        x = (int16_t)(x + *speed_x);
        if (*speed_x < 0) {
            if ((counter & 3) == 0)
                (*speed_x)++;

            const uint8_t *side = ball_cell(x, y);

            if (side[0] == 1 || side[0x80] == 1) {
                play_sound(0xB, 6);
                *speed_x = (int16_t)-*speed_x;
            }
        } else {
            if ((counter & 3) == 0)
                (*speed_x)--;

            const uint8_t *side = ball_cell((int16_t)(x + 0xF), y);

            if (side[0] == 1 || side[0x80] == 1) {
                play_sound(0xB, 6);
                *speed_x = (int16_t)-*speed_x;
            }
        }
        shot->y = y;
        ball_shot_draw(shot, x, y, counter);
        return;
    }
    if (*jump < 0) {   /* $15FB6: falling */
        y = (int16_t)(y + *speed_y);

        const uint8_t *below = ball_cell(x, y);
        int land = (below[0x80] == 1 && below[0] != 1) || ((x & 0x1F) > 0xF && below[0x81] == 1 && below[1] != 1);

        if (land) {
            y = (int16_t)(y & 0xFFF0);
            shot->y = y;
            *speed_y = 0;
            *jump = 0x64;
        } else {
            shot->y = y;
        }
        ball_shot_draw(shot, x, y, counter);
        return;
    }
    if (*jump != 0) {   /* $15F48: the jump */
        int16_t step = *jump;

        if (step == 0x64) {
            play_sound(0xB, 6);
            y = (int16_t)(y - 4);
            shot->y = y;
            *jump = 1;
        } else {
            y = (int16_t)(y + (int8_t)fall_steps_f2[step]);
            if (fall_steps_f2[0x18 + step] != 0)
                play_sound(0xB, 6);
            shot->y = y;
            *jump = (int16_t)(step + 1);
            if (step >= 0x17)
                *jump = 0;
        }
    }
    ball_shot_draw(shot, x, y, counter);
}

/* orig $01645C (handler 4 of the table at $15260: the hunter): each of the nine shots that is in use ($0): a negative number is a shot that is launching: it goes by its speeds (the high byte across, the low
 * down) that slow by one a frame; when both are 0 the speed word is $0F00 and goes up by $100 a frame until it reaches $0F00 + 1024 * ((8 - its place + 2) mod 4), when the number turns positive.  A positive
 * one hunts: the target of $8684 (the strongest creature that was hit this frame; with none the shot bursts with its own speeds and is free) is chased, the speeds going toward it by the object's
 * (last_value - first_value - 2, 1 to 2) a frame up to 6 across and down.  Each is checked against the tiles ($1695E) when it is on the screen and drawn as the glyph of the object's frame + base_frame; the
 * object's frame goes up to 3 */
static void update_shots_hunter(struct ShotList *list, struct Object *object)
{
    struct Shot *shot = list->shot;

    for (int n = 0; n < 9; n++, shot++) {
        int16_t d7 = (int16_t)(8 - n);

        if (shot->active == 0)
            continue;

        int16_t x, y;

        if (shot->active < 0) {
            int16_t speed = shot->direction;

            if (speed >= 0x0F00) {   /* $163E4: waiting */
                speed = (int16_t)(speed + 0x100);
                shot->direction = speed;

                int16_t wait = (int16_t)(((((d7 + 2) & 3) << 2) << 8) + 0x0F00);

                if (speed >= wait) {
                    shot->direction = 0;
                    shot->active = (int16_t)-shot->active;
                }
            } else {
                int16_t vx = (int8_t)(speed >> 8);
                int16_t vy = (int8_t)(speed & 0xFF);

                shot->x = (int16_t)(shot->x + vx);
                shot->y = (int16_t)(shot->y + vy);
                if (vx > 0)
                    vx--;
                else if (vx < 0)
                    vx++;
                if (vy > 0)
                    vy--;
                else if (vy < 0)
                    vy++;
                shot->direction = (int16_t)(((vx & 0xFF) << 8) | (vy & 0xFF));
                if (((vx | vy) & 0xFF) == 0)
                    shot->direction = 0x0F00;
            }
        } else {
            int16_t tx, ty;
            int16_t value = falcon_pick_target(&tx, &ty);
            int16_t speed = shot->direction;
            int16_t vx = (int8_t)(speed >> 8);
            int16_t vy = (int8_t)(speed & 0xFF);

            if (value < 0) {   /* $16554: no target */
                start_debris_moving((int16_t)(shot->x - guest.scroll_x), (int16_t)(shot->y - guest.scroll_y), vx, vy);
                shot->active = 0;
                continue;
            }

            int16_t acc = (int16_t)(object->last_value - object->first_value - 2);

            if (acc <= 1)
                acc = 1;
            else if (acc > 2)
                acc = 2;
            x = (int16_t)(shot->x + vx);
            y = (int16_t)(shot->y + vy);
            tx = (int16_t)(tx + 6);
            ty = (int16_t)(ty + 6);
            if (x < tx) {
                vx = (int16_t)(vx + acc);
                if (vx > 6)
                    vx = 6;
            } else {
                vx = (int16_t)(vx - acc);
                if (vx < -6) {
                    vx = (int16_t)(-6 + acc);
                    if (vx > 6)
                        vx = 6;
                }
            }
            if (y < ty) {
                vy = (int16_t)(vy + acc);
                if (vy > 6)
                    vy = 6;
            } else {
                vy = (int16_t)(vy - acc);
                if (vy < -6) {
                    vy = (int16_t)(-6 + acc);
                    if (vy > 6)
                        vy = 6;
                }
            }
            shot->x = x;
            shot->y = y;
            shot->direction = (int16_t)(((vx & 0xFF) << 8) | (vy & 0xFF));
        }

        int16_t sx = (int16_t)(shot->x - guest.scroll_x);
        int16_t sy = (int16_t)(shot->y - guest.scroll_y);

        if (sx <= 0x140 && sx >= -16 && sy <= 0xC0 && sy >= -16) {
            if (shot->active < 0) {
                shot->active = (int16_t)-shot->active;
                shot_hit_tile(object, shot, sx, sy);
                shot->active = (int16_t)-shot->active;
            } else {
                shot_hit_tile(object, shot, sx, sy);
            }
        }
        glyph_draw(sx, sy, (uint16_t)(object->frame + object->base_frame));
    }
    object->frame = (int16_t)((object->frame + 1) & 3);
}

/* orig $015B36: the list of shots of an object (its first word is the number of the object, negative: none) is run by the routine the
 * object's handler number names in the table at $15260: 0 is $1673E (update_shots), 2 is a bare return; 1, 3, 4 and 5 ($168AE, $15DB2,
 * $1645C and $1661E) did not run */
void run_object_list(struct ShotList *list)
{
    if (list->object < 0)
        return;

    struct Object *object = OBJECT(list->object);

    switch (object->handler) {
    case 0:
        update_shots(list, object);
        break;
    case 1:
        update_shots_sliding(list, object);
        break;
    case 2:
        break;
    case 3:
        update_shots_ball(list, object);
        break;
    case 4:
        update_shots_hunter(list, object);
        break;
    case 5:
        update_shots_axe(list, object);
        break;
    default:
        break;
    }
}

/* orig $015B04: the three lists of shots are run, list_index_a98 saying which */
void run_object_lists(void)
{
    for (int i = 0; i < 3; i++) {
        guest.list_index_a98 = i;
        run_object_list(&shot_lists[i]);
    }
}

/* orig $01590E: the handler 1 of the table at $15220 (the object's fire_kind): the object's list gets its first shots at (x + 16, y - 12):
 * as many as the object's last_value less its first_value, plus one, at most three, the first with the direction 1, the next 0 and 2, and
 * the object's anim_mode is set to 1.  `slot` is where the first shot goes (they are `id`, `id` + 1 and `id` + 2 in the first word). */
static void spawn_shots_right_triple(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y)
{
    play_sound(2, 4);
    x += 16;
    y -= 12;

    int16_t count = object->last_value - object->first_value + 1;

    slot[0].active = id;
    slot[0].x = x;
    slot[0].y = y;
    slot[0].direction = 1;
    object->anim_mode = 1;
    if (--count == 0)
        return;
    id++;
    slot[1].active = id;
    slot[1].x = x;
    slot[1].y = y;
    slot[1].direction = 0;
    if (--count == 0)
        return;
    id++;
    slot[2].active = id;
    slot[2].x = x;
    slot[2].y = y;
    slot[2].direction = 2;
}

/* orig $01592C (to the right) and $015982 (to the left): the object's list gets its first shots at (x, y): as many as the object's last_value less its first_value, plus one, at most three
 * (`id`, `id` + 1 and `id` + 2 in the first word), the directions 1, 0 and 2 and anim_mode 1 to the right, 4, 3 and 5 and anim_mode -1 to the left */
static void spawn_shot_fan(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y, int right)
{
    static const int16_t directions[2][3] = { { 4, 3, 5 }, { 1, 0, 2 } };
    int16_t count = (int16_t)(object->last_value - object->first_value + 1);

    object->anim_mode = right ? 1 : -1;
    for (int i = 0; i < 3; i++) {
        slot[i].active = (int16_t)(id + i);
        slot[i].x = x;
        slot[i].y = y;
        slot[i].direction = directions[right][i];
        if (--count == 0)
            return;
    }
}

/* orig $0158D6 (fire kind 7, the spear to the right: sound 35, from 4 above y) and $0158F0 (fire kind 6, to the left: sound 35, 24 to the left of x and 4 above y) */
static void spawn_shots_spear(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y, int right)
{
    play_sound(0x23, 4);
    y -= 4;
    if (!right)
        x -= 0x18;
    spawn_shot_fan(slot, id, object, x, y, right);
}

/* orig $01557C: the handler 3 of the table at $15220 (the fireball): as the first handler, but at (x, y - 6) and a start counter of -6 in shot_offsets_6a4 for each shot's column (the shot's number less
 * word_fb2c, less one, and the next ones), the shots with the directions 1, 0 and 2 */
static void spawn_shots_fireball_right(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y)
{
    static const int16_t directions[3] = { 1, 0, 2 };

    play_sound(2, 4);
    y -= 6;

    int16_t column = (int16_t)(id - guest.word_fb2c - 1);
    int16_t count = (int16_t)(object->last_value - object->first_value + 1);

    object->anim_mode = 1;
    for (int i = 0; i < 3; i++) {
        slot[i].active = (int16_t)(id + i);
        slot[i].x = x;
        slot[i].y = y;
        slot[i].direction = directions[i];
        guest.shot_offsets_6a4[column + i] = -6;
        if (--count == 0)
            return;
    }
}

/* orig $015660 and, to the left, $015714 (the handlers 15 and 14 of the table at $15220: the magic axe): sound 2 (priority 4); the shots start at (x + 16, y - 16), then 3 left and 5 higher, then 6 right and 10 lower than that, with the directions 1, 0
 * and 2 (as many as the object has: its last_value less its first_value, plus one, at most three), and each has its three words in the table at $66E: the column (the number less 9, twice at most, less 1) of
 * them, the step 4, a step of the power-ups' table (the words 3, 1 and 5) and 50 frames */
static void spawn_shots_axe(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y, int left)
{
    static const int16_t directions[3] = { 1, 0, 2 };
    static const int16_t deltas[2][3] = { { 3, 1, 5 }, { 9, 7, 11 } };   /* (the words of $16BBA, $16BB6, $16BBE and $16BC6, $16BC2, $16BCA from $16BB4) */
    static const int16_t step_x[2][3] = { { 0, -3, 6 }, { 0, -3, -6 } };
    static const int16_t step_y[3] = { 0, -5, 10 };

    play_sound(2, 4);
    x = (int16_t)(x + 0x10);
    y = (int16_t)(y - 0x10);

    int16_t column = id;

    if (column >= 10) {
        column = (int16_t)(column - 9);
        if (column >= 10)
            column = (int16_t)(column - 9);
    }
    column--;

    int16_t *table = axe_table() + 3 * column;
    int16_t count = (int16_t)(object->last_value - object->first_value + 1);

    for (int i = 0; i < 3; i++, table += 3) {
        x = (int16_t)(x + step_x[left][i]);
        y = (int16_t)(y + step_y[i]);
        slot[i].active = (int16_t)(id + i);
        slot[i].x = x;
        slot[i].y = y;
        slot[i].direction = directions[i];
        table[0] = left ? -4 : 4;
        table[1] = shot_delta(deltas[left][i]);
        table[2] = 50;
        if (--count == 0)
            return;
    }
}

/* orig $0157C8 (fire kind 11, to the right) and $01581A (kind 10, to the left: 16 more to the right and the sideways speeds negative): sound 2 (priority 4); up to three hunters start at (x, 12 above y) with the negative
 * numbers -id, -id - 1 and -id - 2 (they are launching: first they fly out with a speed of the high byte across and the low byte down, 11 across and 0, 7 and -7 down, and slow by one a frame, then they wait and
 * then hunt) */
static void spawn_shots_hunter(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y, int left)
{
    static const int16_t right_speeds[3] = { 0x0B00, 0x0B07, 0x0BF9 };
    static const int16_t left_speeds[3] = { (int16_t)0xF500, (int16_t)0xF507, (int16_t)0xF5F9 };
    int16_t count = (int16_t)(object->last_value - object->first_value + 1);
    int16_t number = (int16_t)-id;

    play_sound(2, 4);
    if (left)
        x = (int16_t)(x + 0x10);
    y = (int16_t)(y - 12);
    for (int i = 0; i < 3; i++) {
        slot[i].active = (int16_t)(number - i);
        slot[i].x = x;
        slot[i].y = y;
        slot[i].direction = left ? left_speeds[i] : right_speeds[i];
        if (--count == 0)
            return;
    }
}

/* orig $015510: an object's list `list` gets a shot at (x, y): sound 2 (priority 4) and the routine the object's fire_kind names in the
 * table at $15220: 1 is $1590E (spawn_shots_right_triple), 4 and 5 are bare returns; 0, 2, 3, 6 to 15 ($15968, $155EC, $1557C, $158F0 ...) did
 * not run */
static void spawn_shots(struct Shot *slot, int16_t id, struct ShotList *list, int16_t x, int16_t y)
{
    struct Object *object = OBJECT(list->object);

    play_sound(2, 4);
    switch (object->fire_kind) {
    case 1:
        spawn_shots_right_triple(slot, id, object, x, y);
        break;
    case 3:
        spawn_shots_fireball_right(slot, id, object, x, y);
        break;
    case 7:
        spawn_shots_spear(slot, id, object, x, y, 1);
        break;
    case 8:
    case 9:
        spawn_shot_ball(slot, id, x, y, object->fire_kind == 9);
        break;
    case 11:
        spawn_shots_hunter(slot, id, object, x, y, 0);
        break;
    case 15:
        spawn_shots_axe(slot, id, object, x, y, 0);
        break;
    default:
        break;   /* 4 and 5 return; the others ($15968, $155EC ...) did not run */
    }
}

/* orig $0153A2: each of the three lists that has a free place for a shot (shot_slot_b92, b96 and b9a, not 0) gets one, at the player (a
 * little below him) or, when falcon_choice is the list's number, at the place word_8882 and word_8884 give (8 and 16 on) */
void spawn_player_shots(void)
{
    struct Shot *slots[3] = { guest.shot_slot_b92, guest.shot_slot_b96, guest.shot_slot_b9a };
    int16_t ids[3] = { guest.word_2ea, guest.word_2ec, guest.word_2ee };

    for (int i = 0; i < 3; i++) {
        if (slots[i] == 0)
            continue;

        int16_t x = guest.player_x;
        int16_t y = guest.player_y + 24;

        if (guest.falcon_choice == i) {
            x = guest.word_8882 + 8;
            y = guest.word_8884 + 16;
        }
        spawn_shots(slots[i], ids[i], &shot_lists[i], x, y);
    }
}


/* ======== from game_d.c ======== */
/* Gods, the game: the routines at $417C and $44D2 and what they call.  Each function says where the original has it (`orig $ADDR`). */

/* a shot of a guardian (ten bytes, fifteen of them at $518): its place, the fall speed (up by one each frame), the picture 0 to 3 (-1: the record is free) and the sideways speed */
struct BossShot {
    int16_t x;
    int16_t y;
    int16_t speed_y;
    int16_t frame;
    int16_t speed_x;
};

/* the variables of a guardian, in the original's places ($4E8 to $518): its place, the picture it is in, its energy (a long: negative when it is dead; kept as the two words of the guest
 * memory), where it is on its path (the address of a row of two signed bytes, a picture and a step in x, ended by a negative byte) and its shots */
struct BossState {
    uint16_t x;                       /* $4E8 */
    uint16_t y;                       /* $4EA */
    uint8_t other[0x508 - 0x4EC];
    uint16_t picture;                 /* $508 */
    uint16_t energy_high;             /* $50A */
    uint16_t energy_low;              /* $50C */
    uint16_t unused_50e;
    uint16_t path_high;               /* $510 */
    uint16_t path_low;                /* $512 */
    uint16_t routine_514[2];          /* $514: callback_514 */
    struct BossShot shots[15];        /* $518 */
};

/* a place of the guardians' variables by the address the original keeps it at ($4E8 to $66E: boss_area) */
static uint8_t *boss_at(uint16_t address)
{
    return guest.boss_area + (address - 0x4E8);
}

static struct BossState *boss_state(void)
{
    return (struct BossState *)guest.boss_area;
}


/* orig $005CD2: during a guardian's fight the panel is the guardian's energy: the picture of the table at $5CC2 (by the level:
 * 484 and 1500 for the first: a number whose entry the picture's data is read four entries on from (`$24(a0)`), and the divisor) is drawn like the player's energy (draw_energy_bar) 32
 * bytes further right in the screen buffer; the energy is the word of the long at $50A, the rows fall as it grows */
static void draw_guardian_bar(void)
{
    static const uint16_t bar_table[8] = {484, 1500, 490, 4400, 464, 2250, 467, 6000};
    int idx = (guest.param_index - 1) & 3;
    int16_t back = (int16_t)((4 - guest.word_2de) * 384);
    const uint8_t *src = (const uint8_t *)guest.image_table[bar_table[idx * 2] + 4].data;
    uint8_t *dst = guest.screen_buffer + 7074 + ((guest.x_offset & -16) >> 3);
    int16_t shift = guest.x_offset & 15;
    int32_t scaled = (int32_t)(int16_t)boss_state()->energy_low * 24;
    int32_t acc = 0;
    int16_t w = 0;   /* scaled over the divisor, by subtraction (the Amiga build has no 32-bit division) */

    while (w < 24 && scaled > 0) {
        acc += bar_table[idx * 2 + 1];
        if (acc > scaled)
            break;
        w++;
    }

    int16_t rows = (int16_t)(((24 - w) >> 1) + 6);

    blit_image_wide(dst, src, 96, shift, rows);
    blit_image_wide(dst + 44 * rows, src + 4 * rows - back, 96, shift, 23 - rows);
}

/* orig $0057FC: the score panel (image 121 of the table, 5 words wide and 16 rows, its planes 150 bytes apart) is blitted to the screen
 * buffer, 7420 bytes in and at the column x_offset says (in steps of 16 pixels, shifted by the low four bits).  When menu_state_25a is
 * 2 and flag_23a is not 0 the original goes on at $5CD2, which did not run */
void draw_score_panel(void)
{
    if (guest.menu_state_25a == 2 && guest.flag_23a != 0) {
        draw_guardian_bar();
        return;
    }

    uint8_t *dst = guest.screen_buffer + 7420;
    const uint8_t *src = (const uint8_t *)guest.image_table[121].data;
    uint16_t x = (uint16_t)guest.x_offset;

    dst += (int16_t)(x & 240) >> 3;
    blit_image_5wide(dst, src, 150, x & 15, 15);
}

/* orig $007EAE: the record (a5: x at +0, y at +2, a word at +4 and the strength at +8) is checked against the walls at the point
 * (x + scroll_x, y + scroll_y) with check_hit_and_walls; the strength word is replaced by the result, and when that is negative the
 * word at +4 becomes -1.  Returns the result (the original's N flag is its sign) */
int16_t record_check_walls(int16_t x, int16_t y, int16_t *record)
{
    int16_t result = check_hit_and_walls((int16_t)(x + guest.scroll_x), (int16_t)(y + guest.scroll_y), record[4]);

    record[4] = result;
    if (result < 0)
        record[2] = -1;
    return result;
}

/* ---- the pick-up handlers of the table at $1067A (one for each of the 24 kinds of thing the player can pick up: the index is the
 * number of zero bytes in offsets_4cba before the thing's id).  Those that ran are written; they leave the original's d3 (0 here). ---- */

/* orig $01263A as the pick-up handler 11 (a copy of player_end/player_after of game_c.c, whose functions are static): the animation
 * frame word_19a becomes `frame` (the original has the loop counter of $417C in d7 there), the tiles around the player are looked at,
 * the scroll follows the player and his sprite is drawn */
static void pickup_player_end(int16_t frame)
{
    guest.word_19a = frame;
    player_tile_scan();

    int16_t x = guest.player_x - guest.scroll_x;
    int16_t y = guest.player_y - guest.scroll_y;

    if (guest.word_037c != 0)
        return;   /* goes on at $1265C */
    if (x <= (int16_t)frame_map_125ac[5]) {
        guest.scroll_x -= 4;
        if (guest.scroll_x < 0)
            guest.scroll_x = 0;
    } else if (x >= 320 - (int16_t)frame_map_125ac[5] - 32) {
        guest.scroll_x += 4;
        if (guest.scroll_x >= 3776)
            guest.scroll_x = 3776;
    }
    if (y > 112) {
        int16_t step = (int16_t)((y - 112) >> 1);

        if (step == 0)
            guest.scroll_y++;
        guest.scroll_y += step;
        if (guest.scroll_y >= 832)
            guest.scroll_y = 832;
    } else if (y < 32) {
        int16_t step = (int16_t)((32 - y) >> 1);

        if (step == 0)
            guest.scroll_y--;
        guest.scroll_y -= step;
        if (guest.scroll_y < 0)
            guest.scroll_y = 0;
    }

    int16_t sprite = (int16_t)(state_table[guest.player_state * 2] + frame);

    if (sprite == 52)
        sprite = 25;
    else if (sprite == 54)
        sprite = 35;
    else if (sprite == 55)
        sprite = 38;
    draw_player_sprite(sprite);
}

/* the energy pick-ups (handlers 0, 3, 19, 20 and 21; orig $01088E, $0108C4, $01086C, $010910 and $010930): unless the player is dead (energy_shown
 * negative) the energy goes up by `amount` (handlers 0 and 3 play sound 29 or 30 first, priority 55); what is over 24 is cut off and
 * adds 32 for each point to falcon_energy (when that is not negative) ($0108F8) */
static void energy_pickup(int16_t amount, int16_t sound)
{
    if (guest.energy_shown < 0)
        return;
    if (sound != 0)
        play_sound(sound, 55);
    guest.player_energy += amount;
    if ((int16_t)guest.player_energy > 24) {
        int16_t over = (int16_t)guest.player_energy - 24;

        guest.player_energy = 24;
        if (guest.falcon_energy >= 0)
            guest.falcon_energy += over << 5;
    }
}

/* orig $0106F0: the teleport gem (the thing viewed_record is at, its x the first word): where it sends the player is word_202.  In the
 * third level row (param_index 2, word_266 0) a gem with x from 1472 to 1600 sends to $202E.  Otherwise the thirty entries of
 * table_1c1d6 are looked at for the gem's x: its two other words give (x / 32) * 256 + y / 16; with no entry the next word of the list at
 * ptr_0b76 is taken (the list goes on by two bytes).  flag_1f0 and word_200 are set and sound 5 is played (priority 25) */
static void teleport_gem(void)
{
    int16_t x = guest.viewed_record->w[0];
    int found = 0;

    if (guest.param_index == 2 && guest.word_0266 == 0 && x >= 0x5c0 && x <= 0x640) {
        guest.word_202 = 0x202e;
        found = 1;
    }
    for (int i = 0; i < 30 && !found; i++) {
        if (guest.table_1c1d6[i * 3] == x) {
            guest.word_202 = (uint16_t)(((int16_t)(guest.table_1c1d6[i * 3 + 1] >> 5) << 8) | (uint16_t)(guest.table_1c1d6[i * 3 + 2] >> 4));
            found = 1;
        }
    }
    if (!found) {
        guest.word_202 = (uint16_t)((guest.ptr_0b76[0] << 8) | guest.ptr_0b76[1]);
        guest.ptr_0b76 += 2;
    }
    guest.flag_1f0 = 1;
    guest.word_200 = 1;
    play_sound(5, 25);
}

static void count_slots(void);   /* orig $0152D8 (below) */

/* orig $003E5C (with $003E76): the last_value of the objects the three slot_refs name goes up by one.  A ref of -1 names the 80 bytes before the table,
 * which the original changes in the same way (a word of the machine's byte order there) */
static void raise_unit_counts(void)
{
    int16_t refs[3] = { guest.slot_ref_6b6, guest.slot_ref_748, guest.slot_ref_7da };

    for (int i = 0; i < 3; i++) {
        if (refs[i] >= 0) {
            ((struct Object *)guest.object_slots)[refs[i]].last_value++;
        } else {
            uint8_t *word = (uint8_t *)guest.object_slots + refs[i] * 80 + 8;
            uint16_t value = (uint16_t)((word[0] << 8) | word[1]) + 1;

            word[0] = (uint8_t)(value >> 8);
            word[1] = (uint8_t)value;
        }
    }
}

/* orig $0106D0..: the pick-ups of the things that are not energy (the handlers of the table at $1067A); each returns what the original leaves in d3 */
static void load_block_16bb4(const uint16_t *words)
{
    for (int i = 0; i < 12; i++)
        ((uint16_t *)guest.block_16bb4)[i] = words[i];
}

/* the handlers of the table at $1067A, called with the index of the thing's kind (the number of zero bytes of offsets_4cba before its id)
 * and the loop counter of $417C (d7); returns the d3 they leave (0: nothing more to do; 1: the message of the thing; 2 or more: the
 * message without the line of word_2f8 being cleared first) or -1 when the handler is not written (it did not run) */
static int16_t pickup_handler(int16_t index, int16_t counter)
{
    switch (index) {
    case 0:
        energy_pickup(12, 29);   /* orig $01088E */
        return 1;
    case 3:
        energy_pickup(24, 30);   /* orig $0108C4 */
        return 1;
    case 5:
        guest.flag_268 = 1;            /* orig $01082A */
        return 1;
    case 11:
        pickup_player_end(counter);   /* orig $01263A */
        return 0;
    case 12:
        guest.timer_306 = 0xaa;        /* orig $0106D6 */
        return 1;
    case 13:
        guest.countdown_220 = 1000;    /* orig $010802 */
        return 1;
    case 1:                      /* orig $010954 */
        raise_unit_counts();
        count_slots();
        play_sound(0x38, 0xA);
        return 1;
    case 2:                      /* orig $010950 */
        raise_unit_counts();
        raise_unit_counts();
        count_slots();
        play_sound(0x38, 0xA);
        return 1;
    case 4:                      /* orig $010852 */
        spawn_particle_burst((int16_t)(guest.player_x + 8), (int16_t)(guest.player_y + 0x20));
        return 1;
    case 6:                      /* orig $010834 */
        messages_reset();
        play_sound(0x3C, 2);
        return 1;
    case 7:                      /* orig $01080C: $0086C0 */
        if (guest.falcon_energy >= 0) {
            guest.falcon_energy = (int16_t)(guest.falcon_energy + 0x3E8);
            guest.word_8892++;
        } else {
            falcon_appear();   /* the rest of $0086C0 (falcon_energy negative) */
        }
        return 1;
    case 8:                      /* orig $010816 */
        guest.word_8894++;
        return 1;
    case 9:                      /* orig $010820 */
        guest.word_8892 = (int16_t)(guest.word_8892 + 2);
        return 1;
    case 10:                     /* orig $0107DC: the extra life.  It adds a life when the long $4B79C278 at $107F2 is 0, which is what a
                                  * passing copylock leaves; the failed one (-DPIRATED) leaves it not 0 and item_count stays */
        play_sound(0x1F, 0x37);
#ifndef PIRATED
        guest.item_count++;
#endif
        return 1;
    case 14:                     /* orig $016C14 */
        copy_block_16bb4();
        return 1;
    case 15: {                   /* orig $0107C0: the twelve words at $16BE4 */
        static const uint16_t words[12] = { 0x000C, 0xFFFF, 0x000C, 0x0000, 0x000C, 0x0001, 0xFFF4, 0xFFFF, 0xFFF4, 0x0000, 0xFFF4, 0x0001 };

        load_block_16bb4(words);
        return 1;
    }
    case 16: {                   /* orig $0107CE: the twelve words at $16BFC */
        static const uint16_t words[12] = { 0x000A, 0xFFF6, 0x000A, 0x0000, 0x000A, 0x000A, 0xFFF6, 0xFFF6, 0xFFF6, 0x0000, 0xFFF6, 0x000A };

        load_block_16bb4(words);
        return 1;
    }
    case 22:                     /* orig $0106E0 */
        guest.flag_290 = 0x41;
        guest.flags_3b1 &= (uint8_t)~4;   /* bclr #10 on a byte: bit 2 */
        return 2;
    case 17:
        teleport_gem();          /* orig $0106F0 */
        return 1;
    case 18:
        hint_message();          /* orig $010976 */
        return 0;
    case 19:
        energy_pickup(2, 0);     /* orig $01086C */
        return 1;
    case 20:
        energy_pickup(4, 0);     /* orig $010910 */
        return 1;
    case 21:
        energy_pickup(3, 0);     /* orig $010930 */
        return 1;
    default:
        return -1;
    }
}

/* orig $0044D2: the thing `record` (of records_19b14), drawn at (x, y) on the screen, against the player.  The thing counts as touched
 * when the point (x + 8, y + 8) is inside the box from 4 left of the player to 36 right and from 4 above to 52 below him (not when
 * energy_shown is negative).  `counter` is the loop counter of $417C (199 down to 0), which a pick-up handler gets in d7.
 * A thing of kind 2 or more (table_14cf8, word +4) is only touched (the original's N flag: returns 1); kind 0 and 1 are picked up:
 * the record is freed, and kind 1 gives points (and a message) and kind 0 runs its handler; ids from 192 on start an object.
 * Returns the original's N flag at its rtr (the result is on the stack): 1 when the thing is touched and not picked up, 0 otherwise.
 * The things that did not run are marked: the handlers other than 11 and 18, the keys (ids 12 to 17, $45DC) and the gem ($4676) */
int16_t player_touch_record(int16_t x, int16_t y, struct Slot8 *record, int16_t counter)
{
    if (guest.energy_shown < 0)
        return 0;

    int16_t left = guest.player_x - guest.scroll_x, top = guest.player_y - guest.scroll_y;
    int16_t right = left + 36;
    int16_t px = x + 8, py = y + 8;

    left -= 4;
    if (px < left || px > right)
        return 0;
    int16_t bottom = top + 52;

    top -= 4;
    if (py < top || py > bottom)
        return 0;

    int16_t id = record->w[2];

    if (id >= 192) {
        record->w[2] = -1;
        record->w[3] = 0;
        id -= 192;
        guest.touched_thing_id = id;
        pick_up_object(id);
        play_sound(6, 6);
        spawn_effect(x, y, id);
        return 0;
    }
    guest.touched_thing_id = id;

    const struct Rec10 *info = &guest.table_14cf8[id];
    int16_t kind = (int16_t)info->w[2];

    if (kind > 1)
        return 1;
    if (kind == 1) {
        if (id >= 12 && id < 18) {   /* $45D6: the keys (even) and the chests they open (odd: open) */
            if (id & 1) {
                record->w[3] = 1;
                return 0;
            }

            int16_t wanted = (int16_t)(((id - 12) >> 1) + 0x15);

            for (int i = 0; i < 4; i++) {
                if (guest.item_slots[i] != wanted)
                    continue;
                play_sound(0x20, 5);   /* $4676 */
                guest.item_slots[i] = -1;
                record->w[2] = (int16_t)(id | 1);
                record->w[3] = 1;
                item_message(id);
                int16_t points = (int16_t)info->w[3];

                guest.score_total_1fc += points;
                score_add_carry(points >> 3, (points >> 2) & 1);
                chest_burst(record);
                return 0;
            }
            return 0;
        }
        play_sound(6, 6);
        record->w[2] = -1;
        record->w[3] = 0;
        item_message(id);
        int16_t points = (int16_t)info->w[3];

        guest.score_total_1fc += points;
        score_add_carry(points >> 3, (points >> 2) & 1);   /* asr.w #3 leaves the last bit out in X */
        spawn_effect(x, y, guest.touched_thing_id + 170);
        return 0;
    }

    /* kind 0 */
    play_sound(6, 6);
    record->w[2] = -1;
    record->w[3] = 0;

    int16_t index = 0;

    for (int i = 0; i < id; i++)
        if (offsets_4cba[i] == 0)
            index++;
    play_sound(6, 6);

    int16_t d3 = pickup_handler(index, counter);

    if (d3 < 0)
        return 0;   /* the handler did not run: the original goes on at $45B0 */
    if (d3 != 0) {
        if (d3 != 1)
            guest.message_arg = 0;
        item_message(id);
    }
    spawn_effect(x, y, guest.touched_thing_id + 170);
    return 0;
}

/* The player's energy goes down by 2 unless flag_aa8 (inlined: the tests of $49C2 and $4A50) */
static void hurt_player(void)
{
    if (guest.flag_aa8 == 0)
        guest.player_energy -= 2;
}

/* orig $004978: the thing 300 (a bat): counted when near the screen, its glyph by counter_262; it hurts the player when its box (x + 16,
 * y + 16) is inside the box 4 left of the player and 36 right, 8 above and 56 below; drawn as a sprite 2 words wide */
static void draw_thing_300(int16_t x, int16_t y)
{
    count_if_visible(x, y);

    uint16_t glyph = frames_4b08[guest.counter_262];
    int16_t bx = x + 16, by = y + 16;
    int16_t left = guest.player_x - guest.scroll_x, top = guest.player_y;
    int16_t right = left + 36;

    left -= 4;
    if (!(bx < left || bx > right)) {
        int16_t bottom = top + 56;

        top -= 8;
        if (!(by < top || by > bottom))
            hurt_player();
    }
    sprite_draw(x, y - guest.scroll_y, glyph);
}

/* orig $0049F8: the thing 287 or 220 (a flying thing whose image is cut to a height that counts 0 to 15 and back with counter_204;
 * the image of the glyph is changed for it): counted, it hurts the player when its box is on him, drawn in two parts (the lower with
 * glyph_plane_size 64, from 32 bytes further in the image) and then its head, by counter_262 (287) or counter_20c (220) */
static void draw_thing_flyer(int16_t x, int16_t y, int16_t glyph)
{
    struct Image *image = &guest.image_table[glyph];
    int16_t height = (guest.counter_204 & 16) ? 15 - (guest.counter_204 & 15) : (int16_t)guest.counter_204;

    count_if_visible(x, y);
    image->height = (uint16_t)height;
    y = y + 31 - height - 16;

    int16_t left = guest.player_x - guest.scroll_x;
    int16_t right = left + 32;
    int16_t bx = x + 8;

    if (!(bx < left || bx > right)) {
        int16_t top = guest.player_y;

        if (!(y < top || y > top + 40))
            hurt_player();
    }
    y += 16;
    image->data = (const uint16_t *)((const uint8_t *)image->data + 32);
    y -= guest.scroll_y;
    guest.glyph_plane_size = 64;
    glyph_draw(x, y, glyph);
    guest.glyph_plane_size = 0;
    image->data = (const uint16_t *)((const uint8_t *)image->data - 32);
    y -= 16;
    image->height = 16;

    uint16_t head;

    if (glyph == 287) {
        head = guest.counter_262;
        if (head == 0)
            guest.glyph_plane_size = 64;
        head = frames_4afc[head];
    } else {
        head = guest.counter_20c;
        if (head == 0)
            guest.glyph_plane_size = 64;
        head = frames_4af4[head];
    }
    glyph_draw(x, y, head);
    guest.glyph_plane_size = 0;
}

/* orig $004BD0 (a thing of kind 3): drawn as a glyph at (x, y - scroll_y).  When word_2ba is not negative and the glyph is one of
 * 232, 288, 290 and 292 and the record ptr_0ba2 points at is at (x, y) on the screen, the glyph is the next of a row of five
 * (frames_4ba8, by param_index), word_2ba counts on, at 3 the record's third word is toggled, at 5 word_2ba is -1 again */
static void draw_kind3(int16_t x, int16_t y, int16_t glyph)
{
    y -= guest.scroll_y;
    if (guest.word_2ba >= 0 && (glyph == 232 || glyph == 288 || glyph == 290 || glyph == 292)) {
        uint16_t *other = guest.ptr_0ba2;

        if ((int16_t)(other[0] - guest.scroll_x) == x && (int16_t)(other[1] - guest.scroll_y) == y) {
            glyph = (int16_t)frames_4ba8[(guest.param_index - 1) * 5 + guest.word_2ba];
            guest.word_2ba++;
            if (guest.word_2ba == 3)
                other[2] ^= 1;
            if (guest.word_2ba == 5)
                guest.word_2ba = -1;
        }
    }
    glyph_draw(x, y, glyph);
}

/* orig $0047E2 (a thing of kind 5 or more): by the glyph the thing is drawn at (x, y - scroll_y) as a glyph (the default), a sprite
 * (the glyphs 219, 265, 267, 268, 269, 270, 282 and 301 of the table at $47FE), with a second glyph 16 higher by the column (225 and
 * 251) or changing with the column (252).  The glyphs 283 and 284 (the original goes on at $4862) did not run */
static void draw_kind5(int16_t x, int16_t y, int16_t glyph)
{
    y -= guest.scroll_y;
    switch (glyph) {
    case 0xdb: case 0x119: case 0x109: case 0x114: case 0x115: case 0x116: case 0x11a: case 0x12d:
        sprite_draw(x, y, glyph);
        break;
    case 0xfc:
        glyph_draw(x, y, frames_494a[((x >> 5) + guest.glyph_frame) & 3]);
        break;
    case 0xe1:
        glyph_draw(x, y, glyph);
        glyph_draw(x, y - 16, frames_493a[((x >> 5) + guest.glyph_frame) & 3]);
        break;
    case 0xfb:
        glyph_draw(x, y, glyph);
        glyph_draw(x, y - 16, frames_4942[((x >> 5) + guest.glyph_frame) & 3]);
        break;
    case 0x11b: case 0x11c:   /* $4862: the table's legs (283 and 284): draw_routine_index is 1, flags_0ddd becomes $0A (the clipped, masked draw: the original leaves them so), the image is drawn at x with the draw offset */
        guest.draw_routine_index = 1;
        *(volatile uint8_t *)&guest.flags_0ddd = 0x0A;
        image_draw_clipped((int16_t)(x + guest.x_offset), y, (uint16_t)glyph);
        break;
    default:
        glyph_draw(x, y, glyph);
        break;
    }
}

/* orig $00413A, $4266 and $42F6: what happens to the thing before it is drawn.  A thing whose flag byte is not negative falls: y goes
 * up by fall_steps_f2[flag] (a sound at some steps) and the flag byte counts on to 37 (at 24 it is looked at like a negative one: $4174,
 * which did not run).  One with a negative flag byte looks at the map cells under its two halves: where there is ground (cell 1) it
 * stays (y + 4, then to a multiple of 4; flag -1; from y 1024 on the record is freed), otherwise it lands (flag 0, sound 24) when
 * y is a multiple of 4 */
static void thing_fall(struct Slot8 *record, uint8_t *flag)
{
    int8_t state = (int8_t)*flag;

    if (state >= 0 && state != 24) {
        int16_t step = (int8_t)fall_steps_f2[state];

        if (fall_steps_f2[24 + state] != 0)
            play_sound((uint16_t)((int8_t)fall_steps_f2[24 + state] + 24), 5);
        record->w[1] += step;
        state++;
        if (state >= 37)
            state = 37;
        *flag = (uint8_t)state;
        return;
    }

    int16_t x = record->w[0], y = record->w[1];
    const uint8_t *map = guest.ptr_0b86 + 8192;
    int16_t row = (int16_t)(((y + 16) & -16) << 3);
    int16_t left = (int16_t)(((x & -32) >> 5) + row);
    int16_t right = (int16_t)(((((int16_t)(x + 15)) & -32) >> 5) + row);
    int ground;

    if (map[left] == 1)
        ground = map[(int16_t)(left - 128)] == 1 || map[(int16_t)(right - 128)] == 1;
    else
        ground = map[right] != 1 || map[(int16_t)(right - 128)] == 1;
    if (!ground && ((record->w[1] & 12) <= 0)) {
        record->w[1] &= -4;
        if (*flag != 24) {
            *flag = 0;
            play_sound(24, 5);
        }
        return;
    }
    record->w[1] += 4;
    *flag = 0xff;
    record->w[1] &= -4;
    if (record->w[1] >= 1024) {
        record->w[2] = -1;
        record->w[3] = 0;
    }
}

/* orig $00424E: one thing of the two hundred: it falls or stands (thing_fall), is drawn and tested against the player.  `counter` is
 * the loop counter (d7 in the original) */
static void record_update(struct Slot8 *record, uint8_t *flag, int16_t counter)
{
    guest.viewed_record = record;
    int16_t y0 = record->w[1];
    int16_t id = record->w[2];

    guest.byte_2b6 = *flag;
    thing_fall(record, flag);

    int16_t x = (int16_t)(record->w[0] - guest.scroll_x);
    int16_t y = record->w[1];

    if (id >= 192) {
        /* $4C66: an object (the list is filled, and drawn as the glyph id + 110) */
        y -= guest.scroll_y;
        guest.list_count_1294++;
        if (guest.list_count_1294 <= 20) {
            int16_t *list = (int16_t *)guest.value_1296;

            *list++ = x;
            *list++ = y;
            *list++ = id;
            guest.value_1296 = (uint32_t)(uintptr_t)list;
        }
        player_touch_record(x, y, record, counter);
        glyph_draw(x, y, id + 110);
        return;
    }

    const struct Rec10 *info = &guest.table_14cf8[id];
    int16_t kind = (int16_t)info->w[2];
    int16_t glyph = (int16_t)info->w[0];

    if (kind >= 3) {
        record->w[1] = y0;
        y = y0;
        if (kind == 4) {
            if (glyph == 300)
                draw_thing_300(x, y);
            else if (glyph == 287 || glyph == 220)
                draw_thing_flyer(x, y, glyph);
            else if (glyph == 285)
                archer_thing(record, x, y, glyph);
            else if (glyph == 266) {
                pod_spawn(x, y);   /* ($4B1C: the thing is taken from the map: a pod) */
                record->w[2] = 0xFFFF, record->w[3] = 0;
            }
            else if (bat_spawn(x, y))
                record->w[2] = 0xFFFF, record->w[3] = 0;   /* ($4B7C: the thing is taken from the map and a bat is made) */
            else
                sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)glyph);   /* ($4B8A: with no bat free) */
        } else if (kind > 4) {
            draw_kind5(x, y, glyph);
        } else {
            draw_kind3(x, y, glyph);
        }
        return;
    }

    y -= guest.scroll_y;
    guest.list_count_1294++;
    if (guest.list_count_1294 <= 20) {
        if (id == 45) {
            guest.value_1296 = (uint32_t)(uintptr_t)guest.list_121c;
            guest.list_count_1294 = 20;
            guest.flag_25e = 1;
        }
        int16_t *list = (int16_t *)guest.value_1296;

        *list++ = x;
        *list++ = y;
        *list++ = id;
        guest.value_1296 = (uint32_t)(uintptr_t)list;
    }
    switch (glyph) {
    case 253:
        glyph = (int16_t)frames_44c4[guest.counter_24a];
        break;
    case 224:
        if (guest.counter_260 != 0)
            glyph = guest.counter_260 + 402;
        break;
    case 279:
        sprite_draw(x, y - 8, frames_44bc[guest.glyph_frame]);
        return;
    case 280:
        return;   /* goes on at $4400 (which drops things; did not run) */
    }
    player_touch_record(x, y, record, counter);
    glyph_draw(x, y, glyph);
}

/* orig $00417C: one frame of the things of the level.  The counters of the animations go on, then the two hundred records of
 * records_19b14 (a thing whose fourth word is not 0 and third not negative) are done with record_update; at the end the sound 19 is
 * played (priority 1) while things were counted on the screen (onscreen_count) and word_3436 is the rts of the sound routine,
 * otherwise stopped */
void update_records(void)
{
    guest.list_count_1294 = 0;
    guest.flag_25e = 0;
    guest.onscreen_count = 0;
    guest.value_1296 = (uint32_t)(uintptr_t)guest.list_121c;
    guest.counter_204 = (guest.counter_204 + 1) & 31;
    if (guest.frame_toggle != 0)
        guest.counter_20c = (guest.counter_20c + 1) & 3;
    if (guest.frame_toggle != 0) {
        guest.glyph_frame = (guest.glyph_frame + 1) & 3;
    } else {
        guest.counter_24a++;
        if ((int16_t)guest.counter_24a >= 5)
            guest.counter_24a = 0;
    }
    guest.counter_262++;
    if ((int16_t)guest.counter_262 >= 6)
        guest.counter_262 = 0;
    guest.counter_260 = (guest.counter_260 + 1) & 7;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[3] == 0 || record->w[2] < 0)
            continue;
        record_update(record, &guest.near_flags[i], (int16_t)(199 - i));
    }
    if (guest.flag_25e != 0)
        guest.list_count_1294 = 1;
    if (guest.onscreen_count != 0 && guest.word_3436 == 0x4e75 && guest.tick_hold_38a == 0) {
        play_sound(19, 1);
        return;
    }
    stop_sound(19);
}

/* orig $01924C (the whole of the function whose first part is find_record_19334): every one of the ten records of 8 bytes at $19334
 * whose third word is not negative is drawn as the sprite pickup_glyph at its position less the scroll */
void draw_pickups(void)
{
    for (int i = 0; i < 10; i++) {
        const struct Slot8 *record = &guest.records_19334[i];

        if (record->w[2] >= 0)
            sprite_draw(record->w[0] - guest.scroll_x, record->w[1] - guest.scroll_y, guest.pickup_glyph);
    }
}


/* ======== from game_e.c ======== */
/* Gods, the game: the routines at $6ED0, $12924, $12A9C, $12796, $10976 and $15282 and what they call.  Each function says where the original has it (`orig $ADDR`). */

/* the BCD add of a byte (abcd src, dst) with the extend bit x, which it sets */
static uint8_t abcd(uint8_t dst, uint8_t src, int *x)
{
    uint16_t low = (src & 0x0F) + (dst & 0x0F) + *x;
    uint16_t high = (src & 0xF0) + (dst & 0xF0);

    if (low > 9)
        low += 6;

    uint16_t sum = high + low;
    int carry = (sum & 0x1F0) > 0x90;

    if (carry)
        sum += 0x60;
    *x = carry;
    return (uint8_t)sum;
}

/* orig $009DEA (and inlined: $005AAC): a creature that is hit to death adds its score: value is halved and one added (the type's
 * byte 17), divided by 10 gives the digits of hundreds (the quotient) and tens... as the original does: the packed result
 * (quotient in the high digit, remainder in the low) is added to the second word of the score, with the carry on into the other two;
 * a quotient above 9 adds the rest in the high digit of the second word's next add.  score_redraw says the score is to be drawn again */
static void add_kill_score(uint8_t value)
{
    uint16_t points = (uint16_t)((value >> 1) + 1);
    uint16_t quotient = points / 10;
    uint16_t remainder = points % 10;
    uint16_t extra = quotient > 9 ? (uint16_t)(quotient - 9) : 0;
    uint8_t packed = (uint8_t)((quotient << 4) | remainder);
    int x = 0;

    for (int round = 0; round < 2; round++) {
        uint8_t b1 = abcd((uint8_t)guest.score_words[1], packed, &x);
        uint8_t b2 = abcd((uint8_t)guest.score_words[2], 0, &x);
        uint8_t b3 = abcd((uint8_t)guest.score_words[3], 0, &x);

        guest.score_words[1] = (uint16_t)((guest.score_words[1] & 0xFF00) | b1);
        guest.score_words[2] = (uint16_t)((guest.score_words[2] & 0xFF00) | b2);
        guest.score_words[3] = (uint16_t)((guest.score_words[3] & 0xFF00) | b3);
        guest.score_redraw = 1;
        if (round == 0) {
            if (extra == 0)
                return;
            packed = (uint8_t)(extra << 4);
            x = 0;
        }
    }
}

/* orig $007B44 to $007BA8 (eleven entries of 10 bytes, each sets the first picture, walker_picture_base, and goes on at $7BC0: the walk, walk_pace): the first picture of the walker whose
 * movement routine is at `routine`, or 0 when it is not one of them */
static int16_t walker_first_picture(uint32_t routine)
{
    switch (routine) {
    case 0x7B44:
    case 0x7B58:
    case 0x7B9E:
        return 0x1D2;
    case 0x7B4E:
    case 0x7B6C:
    case 0x7B76:
    case 0x7B8A:
        return 0x1C0;
    case 0x7B62:
        return 0x1E4;
    case 0x7B80:
    case 0x7B94:
        return 0x1CC;
    case 0x7BA8:
        return 0x1F0;
    default:
        return 0;
    }
}

/* orig $0074A6 (the step routine of the stone faces): a face whose type has a speed counts its timer down; at 0 it opens its mouth (its step 1) and the timer is set again (16 less the speed, times 4) */
static void stone_face_step(const struct CreatureType *type, struct Creature *creature)
{
    if (type->speed == 0)
        return;
    if (--creature->timer != 0)
        return;
    creature->state = 1;
    creature->timer = (int16_t)((int16_t)(int8_t)(16 - type->speed) << 2);
}

/* orig $007310: a shard is put at the first free record of records_737e, 10 to the right and 18 under the face, with the sound 20 when the face is on the screen (the point's place less the scroll is within
 * 24 pixels outside it: $0074CA); without a free record nothing is done */
static void face_spits(const struct Creature *creature)
{
    struct Shard *shards = (struct Shard *)guest.records_737e;

    for (int i = 0; i < 10; i++) {
        struct Shard *shard = &shards[i];

        if (shard->x >= 0)
            continue;
        int16_t sx = (int16_t)(creature->x - guest.scroll_x);
        int16_t sy = (int16_t)(creature->y - guest.scroll_y);

        if (sx > -24 && sx < 320 && sy > -24 && sy < 192)
            play_sound(20, 3);
        shard->x = (int16_t)(creature->x + 10);
        shard->y = (int16_t)(creature->y + 0x12);
        shard->frame = 1;
        shard->life = 0x37;
        guest.word_73ce = 1;
        return;
    }
}

/* orig $0072E8 (the movement routine of the stone faces): the picture is 519 plus its step; while it has a step (not 0) it goes on every other frame (frame_toggle), at 3 a shard goes out
 * (face_spits) and after that it is 0 again.  It does not move */
static int16_t stone_face_move(struct Creature *creature)
{
    int16_t picture = (int16_t)(creature->state + 0x207);

    if (guest.frame_toggle != 0 && creature->state != 0) {
        creature->state++;
        if (creature->state == 3)
            face_spits(creature);
        else if (creature->state > 3)
            creature->state = 0;
    }
    return picture;
}

/* orig $0074F0 (the movement routine of the spitting faces, the table at $74E8: the pictures 514, 515, 516, 515 by its step): the picture is taken from the step first; while the step is not 0 (and frame_toggle is set) it goes on, at 3 the
 * face spits two bolts ($76FC: records at $7544, not ported) and at 4 it is 0 again */
static int16_t spitting_face_move(struct Creature *creature)
{
    static const int16_t pictures[4] = { 0x202, 0x203, 0x204, 0x203 };
    int16_t picture = pictures[creature->state & 3];

    if (guest.frame_toggle != 0 && creature->state != 0) {
        creature->state++;
        if (creature->state == 3)
            face_spits_bolts(creature->x, creature->y);
        else if (creature->state > 3)
            creature->state = 0;
    }
    return picture;
}

/* orig $007A36 (the movement routine of a creature that stands where it is: the sprites 523 to 525, the table at $7A6E, 3 more when the type's direction is 0): the step of its animation goes on while it is not 0, and
 * from 0 when the type's speed is not 0 and its timer is 1 (the step of $7AB0 is about to shoot) */
static int16_t standing_creature_slow_move(const struct CreatureType *type, struct Creature *creature);
static int16_t walker_ten_steps_move(struct Creature *creature, int16_t *x, int16_t y);

static int16_t standing_creature_move(const struct CreatureType *type, struct Creature *creature)
{
    int16_t picture = (int16_t)guest.table_7a6e[creature->state & 3];   /* ($7A6E: the pictures the level start put there: 530, 531, 532, 531 in level 1, 523, 524, 525, 524 in the others) */

    if (type->direction == 0)
        picture += 3;
    if (creature->state != 0 || (type->speed != 0 && creature->timer == 1))
        creature->state = (int16_t)((creature->state + 1) & 3);
    return picture;
}

/* orig $007A0E (the movement routine of a creature that stands where it is, the sprites 545 to 548, 4 more when the type's direction is 0): the step of its animation goes on (modulo 4) every other frame (frame_phase 0) while it is not 0 */
static int16_t standing_creature_slow_move(const struct CreatureType *type, struct Creature *creature)
{
    int16_t picture = (int16_t)(creature->state + 0x221);

    if (type->direction == 0)
        picture += 4;
    if (creature->state != 0 && guest.frame_phase == 0)
        creature->state = (int16_t)((creature->state + 1) & 3);
    return picture;
}

/* orig $007D08 (the movement routine of a walker of ten steps: the pictures 484 + its step): the screen stopped (timer_306) it is only drawn.  To the left (direction 0) the step goes down by one a frame; below 0, off a
 * cell border (x not a multiple of 32) it goes to 9 and 16 to the left, on one it looks at the cells: a wall ahead (the byte before it or the one under that is 1) or no floor turns it (to the step 1), floor goes
 * to 9 and 16 to the left.  To the right the step goes up by one; at 10, off a border it goes to 0 and 16 to the right, on one a wall ahead (the next cell or the one under) or no floor turns it, and either way it goes
 * to 0 and 16 to the right */
static int16_t walker_ten_steps_move(struct Creature *creature, int16_t *x_io, int16_t y)
{
    int16_t state = creature->state;
    int16_t x = *x_io;

    if (guest.timer_306 != 0)
        return (int16_t)(state + 0x1E4);
    if (creature->direction == 0) {
        state--;
        if (state < 0) {
            if ((x & 31) != 0) {
                state = 9;
                x = (int16_t)(x - 16);
            } else {
                const uint8_t *cell = map_cell(x, y);

                if (cell[-1] == 1 || cell[0x7F] == 1) {
                    creature->direction ^= 1;
                    state = 1;
                } else if (cell[0xFF] == 1) {
                    state = 9;
                    x = (int16_t)(x - 16);
                } else {
                    creature->direction ^= 1;
                    state = 1;
                }
            }
        }
    } else {
        state++;
        if (state >= 10) {
            if ((x & 31) != 0) {
                state = 0;
                x = (int16_t)(x + 16);
            } else {
                const uint8_t *cell = map_cell(x, y);

                if (cell[1] == 1 || cell[0x81] == 1 || cell[0x101] != 1)
                    creature->direction ^= 1;
                state = 0;
                x = (int16_t)(x + 16);
            }
        }
    }
    creature->state = state;
    *x_io = x;
    return (int16_t)(state + 0x1E4);
}

/* orig $007B44 to $007BA8 as the movement routine of a creature's record (the one at +22): the new x (d0) and the picture (d2).  Only the
 * walkers ran.  The step of walk_pace_frame moves it 4 pixels in its direction (as the direction is before a turn) when its step
 * is at most 7 and frame_toggle is set; the x is worked out here for that reason */
static int call_move(const struct CreatureType *type, struct Creature *creature, int16_t *x, int16_t y)
{
    int16_t first = walker_first_picture(type->move_routine);

    if (first != 0) {
        int16_t old_x = *x;

        guest.walker_picture_base = (uint16_t)first;
        if (creature->state <= 7 && guest.frame_toggle != 0)
            *x += creature->direction == 0 ? -4 : 4;
        return walk_pace_frame((const uint8_t *)type, (int16_t *)creature, old_x, y);
    }
    if (type->move_routine == 0x72E8)
        return stone_face_move(creature);
    if (type->move_routine == 0x74F0)
        return spitting_face_move(creature);
    if (type->move_routine == 0x7A36)
        return standing_creature_move(type, creature);
    if (type->move_routine == 0x7A0E)
        return standing_creature_slow_move(type, creature);
    if (type->move_routine == 0x7D08)
        return walker_ten_steps_move(creature, x, y);
    /* goes on at the routine the record names ($7BB2, $7A0E, ...): none of those ran */
    return 0;
}

/* orig $0060AE (right false) and $0060F2 (right true): an enemy shot at (x, y) with the speed shot_speed is put in the first free
 * (fourth word negative) of the twenty records of 8 bytes at ptr_0cce, when the place is near enough to the screen */
static void enemy_shot(int16_t x, int16_t y, int right)
{
    int16_t sx = (int16_t)(x - guest.scroll_x);
    int16_t sy = (int16_t)(y - guest.scroll_y);

    if (sy >= 192 || sy < -4)
        return;
    if (right ? (sx > 312 || sx < -8) : (sx < 8 || sx > 328))
        return;

    struct Slot8 *record = (struct Slot8 *)guest.ptr_0cce;

    for (int i = 0; i < 20; i++, record++) {
        if (record->w[3] < 0) {
            record->w[0] = x;
            record->w[1] = y;
            record->w[2] = guest.shot_speed;
            record->w[3] = 0;
            return;
        }
    }
}

/* orig $00717E: the step routine of the shooting creatures: its timer counts down (nothing is done while the type's speed is 0); at
 * 0 it is set again (from the speed) and a shot goes out sideways (to the left from 8 pixels before the creature, to the right from
 * 32 pixels after) with a speed that goes up with the type's speed.  A type with byte 18 set aims its shot ($7154, with the line
 * drawing of $183BC) and the original goes on there: that did not run */
static void shooting_creature_step(const struct CreatureType *type, struct Creature *creature)
{
    if (type->speed == 0)
        return;
    creature->timer--;
    if (creature->timer != 0)
        return;
    creature->timer = (int16_t)((int16_t)(int8_t)(16 - type->speed) << 2);
    if (type->flag_18 != 0) {   /* $7154: a shot aimed at the player, from 16 to the right, of the speed half of the type's plus 2 (the kind 1) */
        aimed_shot((int16_t)(creature->x + 0x10), creature->y, (int16_t)(((int16_t)(int8_t)type->speed >> 1) + 2), 1);
        return;
    }

    int16_t speed = (int16_t)(((int16_t)(int8_t)type->speed >> 2) + 4);

    if (creature->direction == 0) {
        guest.shot_speed = (int16_t)-speed;
        enemy_shot((int16_t)(creature->x - 8), creature->y, 0);
    } else {
        guest.shot_speed = speed;
        enemy_shot((int16_t)(creature->x + 32), creature->y, 1);
    }
}

/* orig $007AB0: the step routine of the creatures that stand and shoot (the movement routine $7A36): nothing is done while the type's speed is 0; the timer counts down, at 0 (in the world 4 the
 * step of the animation goes on) it is set again from the speed and a shot goes out: one aimed at the player (the type's byte 18: from 22 to the right of the creature, or 6 to the left of it when its
 * direction is not 1, 13 down, the kind 1, the speed half of the type's plus 2), or a plain one to the left (from 10 to the right, 15 down) or to the right (from 14, 15 down) with a speed that goes up
 * with the type's speed (the world 4: 7 higher and the right one 4 more to the right) */
static void standing_shooter_step(const struct CreatureType *type, struct Creature *creature)
{
    if (type->speed == 0)
        return;
    creature->timer--;
    if (creature->timer != 0)
        return;
    if (guest.param_index == 4)
        creature->state++;
    creature->timer = (int16_t)((int16_t)(int8_t)(16 - type->speed) << 2);
    if (type->flag_18 != 0) {
        int16_t x = (int16_t)(creature->x + 0x16);

        if (creature->direction != 1)
            x = (int16_t)(x - 0x1C);
        aimed_shot(x, (int16_t)(creature->y + 0xD), (int16_t)(((int16_t)(int8_t)type->speed >> 1) + 2), 1);
        return;
    }

    int16_t speed = (int16_t)(((int16_t)(int8_t)type->speed >> 2) + 4);
    int16_t x, y = (int16_t)(creature->y + 0xF);

    if (guest.param_index == 4)
        y -= 7;
    if (creature->direction == 0) {
        guest.shot_speed = (int16_t)-speed;
        x = (int16_t)(creature->x + 0xA);
        enemy_shot(x, y, 0);
    } else {
        guest.shot_speed = speed;
        x = (int16_t)(creature->x + 0xE);
        if (guest.param_index == 4)
            x += 4;
        enemy_shot(x, y, 1);
    }
}

/* the step routine of a creature's record (+30), called with the screen not frozen (timer_306 0): only $717E ran */
static void stone_face_step(const struct CreatureType *type, struct Creature *creature);

static void call_step(const struct CreatureType *type, struct Creature *creature)
{
    if (type->step_routine == 0x717E)
        shooting_creature_step(type, creature);
    else if (type->step_routine == 0x74A6 || type->step_routine == 0x7520)   /* $7520 is the same code as $74A6 */
        stone_face_step(type, creature);
    else if (type->step_routine == 0x7AB0)
        standing_shooter_step(type, creature);
    /* goes on at $74A6, $7520, $7AB0, $7EE2, $80E8, $8158: none of those ran */
}

/* orig $006EEC to $006F68: the group starts: each of its creatures is put at the group's start with its delay (the delays go
 * up by delay_step), the first one makes a puff, the group's count and status (3) are set */
static void creatures_start(struct CreatureType *type)
{
    struct Creature *creature = type->creatures;
    int16_t delay = 0;
    int number = 0;

    do {
        creature->x = type->spawn_x;
        creature->y = type->spawn_y;
        creature->state = 0;
        creature->delay = delay;
        if (delay == 0)
            spawn_puff((int16_t *)creature);
        creature->hp = type->value;
        creature->direction = type->direction;
        delay = (int16_t)(delay + type->delay_step);
        creature->timer = (int16_t)((int16_t)(int8_t)(16 - type->speed) << 2);
        creature++;
        number++;
    } while ((uint8_t)number != type->number);
    type->count = (int8_t)type->number;
    type->status = 3;
}

/* orig $0070F8: a dying creature (hp negative) goes through seven pictures (sprites 108 to 114); the first time its place is kept
 * in last_death_x and last_death_y (where the group's drop goes); at the end its hp is 0 */
static void creature_dies(struct Creature *creature)
{
    guest.dying_count++;
    if (creature->state < 0) {
        guest.last_death_x = creature->x;
        guest.last_death_y = creature->y;
    }
    creature->state++;
    if (creature->state >= 7) {
        creature->hp = 0;
        creature->delay = 0;
        creature->direction = 0;
        return;
    }
    sprite_draw((int16_t)(creature->x - guest.scroll_x), (int16_t)(creature->y - guest.scroll_y), (uint16_t)(creature->state + 108));
}

/* orig $006F74 to $007070: one creature of a started group (not gone, not dying): the delay runs down, the step routine and the
 * movement routine are called, the position is kept, the way out of the screen the group's bounds name is looked at, the box is
 * put in hit_box_extra_width / hit_box_extra_height and the creature is looked at by check_hit_and_walls (its hp is kept; a negative one starts the
 * dying), a creature just killed adds its score, and it is drawn.  The return value 0: go on with the next creature; 1: it left
 * the screen (it is counted as gone) */
static int creature_runs(struct CreatureType *type, struct Creature *creature)
{
    if (creature->delay != 0) {
        if (creature->delay < 0) {
            creature->delay++;
            return 0;
        }
        creature->delay--;
        if (creature->delay == 0)
            spawn_puff((int16_t *)creature);
        return 0;
    }

    int16_t x = creature->x;
    int16_t y = creature->y;

    guest.creature_x = x;
    if (guest.timer_306 == 0)
        call_step(type, creature);

    int16_t frame = (int16_t)call_move(type, creature, &x, y);

    creature->x = x;
    creature->y = y;
    x = (int16_t)(x - guest.scroll_x);
    y = (int16_t)(y - guest.scroll_y);

    int gone = 0;

    switch (type->bounds) {   /* $7074 */
    case 2:
        gone = x < -112;
        break;
    case 3:
        gone = x > 400;
        break;
    case 4:
        gone = y < -72;
        break;
    case 5:
        gone = y > 232;
        break;
    default:
        break;
    }
    if (gone) {
        creature->timer = 0;
        type->drop = 0xFF;
        return 1;
    }

    guest.hit_box_extra_width = type->box_width;
    guest.hit_box_extra_height = type->box_height;

    /* orig $007EAE */
    creature->hp = check_hit_and_walls((int16_t)(x + guest.scroll_x), (int16_t)(y + guest.scroll_y), creature->hp);
    if (creature->hp < 0)
        creature->state = -1;
    if (creature->hp < 0)
        add_kill_score(type->value);   /* $70DC */

    switch (type->draw_kind) {   /* the table at $6EA8 */
    case 0:
        glyph_draw(x, y, (uint16_t)frame);
        break;
    case 1:
    case 2:
        sprite_draw(x, y, (uint16_t)frame);
        break;
    default:
        break;   /* goes on at $1EFCC and $7DB4: neither ran */
    }
    return 0;
}

/* a group that has been started: every creature is looked at; when all of them are gone the thing the group drops falls where the
 * last one died */
static void creatures_run(struct CreatureType *type)
{
    struct Creature *creature = type->creatures;
    int16_t gone = 0;
    int16_t number = 0;

    guest.dying_count = 0;
    do {
        if (creature->hp == 0)
            gone++;
        else if (creature->hp < 0)
            creature_dies(creature);
        else if (creature_runs(type, creature))
            gone++;
        number++;
        creature++;
    } while (number < type->count);

    if ((int16_t)(gone + guest.dying_count) == type->count) {
        if (type->drop != 0xFF)
            drop_thing((int16_t)(guest.last_death_x + 8), (int16_t)(guest.last_death_y + 8), type->drop);
        type->drop = 0xFF;
    }
    if (gone >= type->count)
        type->status = 0;
}


/* orig $00E352 (d0 and d1 the screen place): a thing falls from there: the first free of the ten records of five words at falling_things (the first word negative) gets the
 * place (the x eleven to the right, in the level), no picture count yet and a speed of 4, and the routine at $E3A2 is switched on */
static void drop_falling_slot(int16_t x, int16_t y)
{
    for (int i = 0; i < 10; i++) {
        int16_t *slot = &guest.falling_things[i * 5];

        if (slot[0] < 0) {
            guest.code_e3a2 = 0x4E71;
            slot[0] = (int16_t)(x + guest.scroll_x + 11);
            slot[1] = (int16_t)(y + guest.scroll_y);
            slot[2] = 0;
            slot[3] = 4;
            return;
        }
    }
}

/* orig $017BE2 and $017C1E: the cell `cell` of the collision map (the second half of the map) has an entry of the table of forty crumbling cells (table_1a30c, three words:
 * x, y, the state) when the place of the cell is in one whose state is negative (just hit): then what the thing that hit it does to it (its word +8 of `slot`, more
 * for the weapon 10) is taken off the state; at less than -200 the cell begins to crumble (state 0).  Returns 1 then: the caller is done */
static int crumble_hit_existing(uint16_t x, uint16_t y, const int16_t *slot)
{
    uint16_t *entry = guest.table_1a30c;

    for (int i = 0; i < 40; i++, entry += 3) {
        if (entry[0] != x || entry[1] != y || (int16_t)entry[2] >= 0)
            continue;
        int16_t hit = slot[4];
        int16_t weapon = (int16_t)y;   /* d5 as it is when the list number is not 0 to 2 */

        if (guest.list_index_a98 == 0)
            weapon = guest.slot_ref_6b6;
        if (guest.list_index_a98 == 1)
            weapon = guest.slot_ref_748;
        if (guest.list_index_a98 == 2)
            weapon = guest.slot_ref_7da;
        if (weapon == 10)
            hit = (int16_t)(hit + 0x32);
        entry[2] = (uint16_t)(entry[2] - hit);
        if ((int16_t)entry[2] < -200)
            entry[2] = 0;
        return 1;
    }
    return 0;
}

/* orig $017B98: a thing (`slot`) has hit the cell `cell` of the collision map: when the cell is not yet crumbling a free entry of the table of forty is made for it (its place,
 * the state -1) and the routine at $17CC0 is switched on */
static void crumble_start(const uint8_t *cell, const int16_t *slot)
{
    uint32_t index = (uint32_t)(cell - 0x2000 - guest.ptr_0b86);
    uint16_t x = (uint16_t)((index & 0x7F) << 5);
    uint16_t y = (uint16_t)((index & 0xFFFF80) >> 3);

    if (crumble_hit_existing(x, y, slot))
        return;
    uint16_t *entry = guest.table_1a30c;

    for (int i = 0; i < 40; i++, entry += 3) {
        if ((int16_t)entry[0] < 0) {
            entry[0] = x;
            entry[1] = y;
            entry[2] = 0xFFFF;
            guest.crumbling_cell_count++;
            guest.code_17cc0 = 0x4E71;
            return;
        }
    }
}

/* orig $017D28: the crumbling entry `entry` is done: its cell becomes ground no more: a free one of the hundred markers (marker_table) is made for the cell of the collision
 * map, which gets 0, and the cell of the map gets 1 */
static void crumble_remove(const uint16_t *entry)
{
    int16_t x = (int16_t)(entry[0] & 0xFFE0);
    int16_t y = (int16_t)(entry[1] & 0xFFF0);
    uint8_t *cell = guest.ptr_0b86 + 0x2000 + (x >> 5) + (int16_t)(y << 3);

    for (int i = 0; i < 100; i++) {
        if ((int32_t)guest.marker_table[i] < 0) {
            guest.marker_table[i] = (uint32_t)(uintptr_t)cell;
            *cell = 0;
            *(cell - 0x2000) = 1;
            return;
        }
    }
}

/* orig $017CC2 (the routine at $17CC0 is a bare return until a cell crumbles): the cells that crumble are drawn (the sprite param_d + the state), the state goes up every frame
 * the screen is on and at 4 the cell is removed */
static void update_crumbles(void)
{
    if (guest.code_17cc0 != 0x4E71)
        return;
    if (guest.crumbling_cell_count == 0)
        guest.code_17cc0 = 0x4E75;
    uint16_t *entry = guest.table_1a30c;

    for (int i = 0; i < 40; i++, entry += 3) {
        if ((int32_t)(((uint32_t)entry[0] << 16) | entry[1]) < 0)
            continue;
        if ((int16_t)entry[2] < 0)
            continue;
        sprite_draw((int16_t)(entry[0] - guest.scroll_x), (int16_t)(entry[1] - guest.scroll_y), (uint16_t)(entry[2] + guest.param_d));
        guest.crumbling_cell_count++;
        if (guest.frame_toggle == 0)
            continue;
        entry[2]++;
        if ((int16_t)entry[2] > 3) {
            crumble_remove(entry);
            entry[0] = 0xFFFF;
            entry[1] = 0xFFFF;
            guest.crumbling_cell_count--;
        }
    }
}

/* orig $00E3A4 (the routine at $E3A2 is a bare return until something is dropped): the ten falling things: each falls by its speed, which goes up to 16 every other
 * frame; the cell two rows under it is looked at (a cell of 109 is hit: the crumbling of $17B98 starts); the player is hit by it (damage_player_at) or it is drawn as the
 * glyph 541 + its picture count and goes when it is off the bottom of the screen */
static void update_falling_slots(void)
{
    if (guest.code_e3a2 != 0x4E71)
        return;
    guest.falling_things_count = 0;
    for (int i = 0; i < 10; i++) {
        int16_t *slot = &guest.falling_things[i * 5];
        int16_t x = slot[0];

        if (x < 0)
            continue;
        guest.falling_things_count++;
        int16_t y = (int16_t)(slot[1] + slot[3]);

        slot[1] = y;
        if (guest.frame_phase == 0) {
            slot[3]++;
            if (slot[3] > 0x10)
                slot[3] = 0x10;
        }
        const uint8_t *cell = guest.ptr_0b86 + (int16_t)(((x + 5) & 0xFFE0) >> 5) + (int16_t)((y & 0xFFF0) << 3) + 0x100;

        if (*cell == 0x6D) {
            slot[4] = 0xFA;
            guest.list_index_a98 = 0;
            crumble_start(cell + 0x2000, slot);
        }
        guest.hit_box_extra_width = 5;
        guest.hit_box_extra_height = 0x10;
        if ((int16_t)damage_player_at(x, (int16_t)(y + 8), slot[2]) < 0) {
            slot[0] = -1;
            continue;
        }
        int16_t sx = (int16_t)(x - guest.scroll_x);
        int16_t sy = (int16_t)(y - guest.scroll_y);

        if (sy >= 0xC0) {
            slot[0] = -1;
            continue;
        }
        glyph_draw(sx, sy, (uint16_t)(slot[2] + 0x21D));
    }
    if (guest.falling_things_count == 0)
        guest.code_e3a2 = 0x4E75;
}

/* ======== the archers of world 2 ($DDD8, $DDF6, $DE56, $DFC8, $DFE0, $E006, $E0CE) and what they throw ($E144) ======== */

/* orig $005AAC (one add): the packed decimal `packed` (two digits) is added to the score, and the score is to be drawn again */
static void score_add_packed(uint8_t packed)
{
    int x = 0;
    uint8_t b1 = abcd((uint8_t)guest.score_words[1], packed, &x);
    uint8_t b2 = abcd((uint8_t)guest.score_words[2], 0, &x);
    uint8_t b3 = abcd((uint8_t)guest.score_words[3], 0, &x);

    guest.score_words[1] = (uint16_t)((guest.score_words[1] & 0xFF00) | b1);
    guest.score_words[2] = (uint16_t)((guest.score_words[2] & 0xFF00) | b2);
    guest.score_words[3] = (uint16_t)((guest.score_words[3] & 0xFF00) | b3);
    guest.score_redraw = 1;
}

/* orig $00D4E2 to $00D59A (the entry `entry` of table_ed0 - the cell x and y and its state - is in the window of the screen): when the player is not in the box of 48 by 48 round it (the test is
 * the damage routine's, with the energy put back and the sound of $3436 off when the player is in it), a random word of at most $3E80 (one in four) and one of the ten records of records_113c free, the
 * statue's cell and the one under it become the tile 13, the entry's state -1 and the creature is made there: a statue of the tile 35 comes out to the left (hit points 32, the picture 0, the speed
 * -4 across), any other to the right (one pixel on, the picture 2, the speed 4) */
static void spawn_stone_enemy(int16_t *entry)
{
    int16_t saved_energy = (int16_t)guest.player_energy;
    uint16_t saved_3436 = guest.word_3436;

    guest.hit_box_extra_width = 0x30;
    guest.hit_box_extra_height = 0x30;
    guest.word_3436 = 0x4E71;
    int16_t result = damage_player_at((int16_t)((entry[0] << 5) - 8), (int16_t)((entry[1] << 4) - 8), 0x32);

    guest.word_3436 = saved_3436;
    if (result < 0) {
        guest.player_energy = (uint16_t)saved_energy;
        return;
    }
    if ((uint16_t)random_word() > 0x3E80)
        return;

    struct Slot12 *record = 0;

    for (int i = 0; i < 10; i++) {
        if (guest.records_113c[i].w[2] == 0) {
            record = &guest.records_113c[i];
            break;
        }
    }
    if (record == 0)
        return;

    int16_t x = (int16_t)(entry[0] << 5);
    int16_t y = (int16_t)(entry[1] << 4);
    uint8_t *cell = guest.ptr_0b86 + (int16_t)(entry[1] << 7) + entry[0];
    uint8_t old = cell[0];

    cell[0] = 0x0D;
    cell[0x80] = 0x0D;
    entry[2] = -1;
    if (old != 0x23) {
        record->w[0] = (int16_t)(x + 1);
        record->w[1] = y;
        record->w[2] = 0x20;
        record->w[3] = 2;
        record->w[4] = 4;
        record->w[5] = 0;
    } else {
        record->w[0] = x;
        record->w[1] = y;
        record->w[2] = 0x20;
        record->w[3] = 0;
        record->w[4] = -4;
        record->w[5] = 0;
    }
}

/* orig $00D314 to $00D424 (the loop; the pictures $D3F8 and the hit $D426 below it): the ten stone creatures (records_113c: x, y, hit points, step, speed across and down; hit points 0 is a free one).
 * A dying one (negative hit points) goes on by its speed and is drawn as the picture 91 plus its step, which counts to 17, then it is free.  A living one: while its hit points are 16 or less (they
 * count down by one every other frame to 4) and the player is not frozen (timer_306) it goes toward the player, its speed across 1 more or less every frame up to 3 and down up to 4 (-3 and -4 the
 * other way); its picture is by its step (up every other frame once the hit points are 4 or less; the pictures of unit_pictures_82d8, 3 more when it goes to the right).  It is hit as a box of 32 by 32
 * (the damage of the player too: the energy of the player goes down and it dies: -1 comes back); killed it drops the thing 16, the 5 points go to the score and it begins to die */
static void update_records_113c(void)
{
    for (int i = 0; i < 10; i++) {
        struct Slot12 *r = &guest.records_113c[i];

        if (r->w[2] == 0)
            continue;

        int16_t x = r->w[0], y = r->w[1], hp = r->w[2], frame = r->w[3], vx = r->w[4], vy = r->w[5];
        int16_t picture;

        if (hp < 0) {   /* $D3B8 */
            x = (int16_t)(x + vx);
            y = (int16_t)(y + vy);
            r->w[0] = x;
            r->w[1] = y;
            sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)(0x5B + frame));
            if (++r->w[3] >= 0x11) {
                r->w[2] = 0;
                r->w[3] = 0;
            }
            continue;
        }
        if (hp <= 0x10 && guest.timer_306 == 0) {
            if (x > guest.player_x) {
                vx--;
                if (vx <= -4)
                    vx = -3;
            } else {
                vx++;
                if (vx >= 4)
                    vx = 3;
            }
            if (y > guest.player_y) {
                vy--;
                if (vy <= -5)
                    vy = -4;
            } else {
                vy++;
                if (vy >= 5)
                    vy = 4;
            }
            x = (int16_t)(x + vx);
            y = (int16_t)(y + vy);
            r->w[4] = vx;
            r->w[5] = vy;
        }
        r->w[0] = x;
        r->w[1] = y;

        /* $D3F8: the picture */
        if (guest.frame_toggle != 0) {
            if (hp > 4)
                r->w[2]--;
            else
                frame = (int16_t)((frame + 1) & 3);
        }
        picture = (int16_t)unit_pictures_82d8[frame & 3];
        if (vx >= 0)
            picture = (int16_t)(picture + 3);

        /* $D426: the hit */
        int16_t strength = r->w[2];

        guest.hit_box_extra_width = 0x20;
        guest.hit_box_extra_height = 0x20;
        guest.word_3a6 = 0xFFFF;
        strength = check_hit_and_walls(x, y, strength);
        guest.word_3a6 = 0;
        if (strength >= 0) {
            r->w[2] = strength;
            r->w[3] = frame;
        } else {
            drop_thing(x, y, 0x10);
            r->w[2] = -1;
            score_add_packed(5);
            r->w[3] = (int16_t)guest.score_words[3];   /* (the original's d3 has been loaded with the score's last word by then: its step of dying begins at that) */
        }
        sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)picture);
    }
}

/* orig $00E2CC: the cell of the second half of the map at (x, y) (the cells are 32 wide and 16 high) */
static const uint8_t *map_cell_at(int16_t x, int16_t y)
{
    return guest.ptr_0b86 + 0x2000 + (int16_t)((int16_t)(x & 0xFFE0) >> 5) + (int16_t)((int16_t)(y & 0xFFF0) << 3);
}

/* orig $004B36 (a thing of kind 4 with the glyph 285, the archer in the map; $DDD8 and $DDF6): in world 2 the thing is taken from the map and, when one of the ten
 * records of falling_things is free, an archer is made there (three to the right of the thing, 24 above it: waiting, state -4, 60 hit points, a timer of 40 and up to 15 more), and
 * the routine at $DE54 is switched on; in world 1 the thing is drawn as a sprite; in the others it is taken from the map */
static void archer_thing(struct Slot8 *record, int16_t x, int16_t y, int16_t glyph)
{
    if ((int16_t)guest.param_index < 2) {
        sprite_draw(x, (int16_t)(y - guest.scroll_y), (uint16_t)glyph);
        return;
    }
    if ((int16_t)guest.param_index == 2) {
        for (int i = 0; i < 10; i++) {
            int16_t *a = &guest.falling_things[i * 5];

            if (a[0] < 0) {
                a[0] = (int16_t)(x + guest.scroll_x + 3);
                a[1] = (int16_t)(y - 0x18);
                a[2] = -4;
                a[3] = 0x3C;
                a[4] = 0x28;
                a[4] = (int16_t)(a[4] + (random_word() & 0xF));
                guest.code_de54 = 0x4E71;
                break;
            }
        }
    }
    record->w[2] = -1;
    record->w[3] = 0;
}

/* orig $00DFE0: the body of the archer (the sprite 285) under its head */
static void archer_body(const int16_t *a)
{
    sprite_draw((int16_t)(a[0] - guest.scroll_x - 3), (int16_t)(a[1] - guest.scroll_y + 0x18), 0x11D);
}

/* orig $00E0CE: the archer's death throws three or four things (records of table_ed0, 16 words each... eight words: x, y, state, hit points, speed in x, speed in y, falling); returns 0 when there is no
 * free record for one of them (the original returns from the whole of $DE56 then) */
static int archer_throws(const int16_t *a)
{
    int count = 2 + (random_word() & 1);

    for (int n = 0; n <= count; n++) {
        int16_t *t = guest.table_ed0;
        int i;

        for (i = 0; i < 16; i++, t += 8)
            if (t[2] < 0)
                break;
        if (i == 16)
            return 0;
        t[0] = a[0];
        t[1] = (int16_t)(a[1] + 0x18);
        t[5] = (int16_t)(-(random_word() & 3) - 2);
        uint16_t random = random_word();
        int16_t speed = (int16_t)(random & 3);

        t[4] = (int16_t)random < 0 ? (int16_t)-speed : speed;
        t[3] = 0x2B;
        t[2] = 0;
        t[6] = 1;
        guest.code_e142 = 0x4E71;
    }
    return 1;
}

/* ======== the bats of level 3 and on (orig $00DAA4 to $00DDD6): ten records of 14 bytes at $10B0, started by a thing of kind 4 with a glyph other than 266, 285 and 300 ======== */

/* a bat: its place, hit points (negative: the record is free), its state (-1 hanging and looking at the player, 0 to 3 flying, $2345 diving, $1234 dying), the picture it is turned to and its speeds */
struct Bat {
    int16_t x;        /* +0 */
    int16_t y;        /* +2 */
    int16_t energy;   /* +4 */
    int16_t state;    /* +6 */
    int16_t frame;    /* +8 */
    int16_t speed_x;  /* +10 */
    int16_t speed_y;  /* +12 */
};

static struct Bat *bats(void)
{
    return (struct Bat *)guest.bytes_10b0;
}

/* orig $00DAA4 (d0 the thing's x on the screen, d1 its y in the level): the first free bat gets the place, 25 hit points and the state -1 and the routine at $DB04 is switched on; with none
 * free the thing is drawn as a sprite (the original's $4B8A takes the scroll from the x again) */
static int bat_spawn(int16_t x, int16_t y)
{
    struct Bat *bat = bats();

    for (int i = 0; i < 10; i++, bat++) {
        if (bat->energy >= 0)
            continue;
        bat->energy = 0x19;
        bat->x = (int16_t)(x + guest.scroll_x);
        bat->y = y;
        bat->frame = 0;
        bat->speed_x = 0;
        bat->speed_y = 0;
        bat->state = -1;
        guest.code_db04 = 0x4E71;
        return 1;
    }
    return 0;
}

/* orig $00DD3A: the bat is a box of 24 by 24 for the player's shots; when its hit points are gone it is paid (5 points hanging, 25 flying), starts to die (state $1234) and keeps its place */
static void bat_hit_by_shots(struct Bat *bat)
{
    guest.hit_box_extra_width = 0x18;
    guest.hit_box_extra_height = 0x18;

    int16_t energy = check_hit_and_walls(bat->x, bat->y, bat->energy);

    if (energy >= 0) {
        bat->energy = energy;
        return;
    }
    bat->energy = 0;
    bat->frame = 0;
    score_add_packed(bat->state < 0 ? 0x05 : 0x25);
    bat->state = 0x1234;
}

/* orig $00DBB0: a flying bat's speeds go a step toward the player (the sideways one to 3 either way, the one up and down to 2) and it is turned to the side it goes (picture 4 from 2 to the right,
 * 0 from 2 to the left) */
static void bat_steer_to_player(struct Bat *bat)
{
    int16_t vx = bat->speed_x;
    int16_t vy = bat->speed_y;

    if (bat->x < guest.player_x)
        vx++;
    else if (bat->x > guest.player_x)
        vx--;
    if (bat->y < guest.player_y)
        vy++;
    else if (bat->y > guest.player_y)
        vy--;
    if (vx >= 2)
        bat->frame = 4;
    if (vx <= -2)
        bat->frame = 0;
    if (vx > 3)
        vx = 3;
    else if (vx < -3)
        vx = -3;
    if (vy > 2)
        vy = 2;
    else if (vy < -2)
        vy = -2;
    bat->speed_x = vx;
    bat->speed_y = vy;
}

/* orig $00DB36: one bat per frame */
static void bat_update(struct Bat *bat)
{
    if (bat->state == 0x1234) {
        goto dying;
    }
    bat_hit_by_shots(bat);
    if (bat->state == 0x1234) {
dying:
        /* $DD8E: it drifts on by its speeds and is drawn as the pieces (91 + the count, 8 up and left) until the 17th */
        bat->x = (int16_t)(bat->x + bat->speed_x);
        bat->y = (int16_t)(bat->y + bat->speed_y);
        sprite_draw((int16_t)(bat->x - guest.scroll_x - 8), (int16_t)(bat->y - guest.scroll_y - 8), (uint16_t)(0x5B + bat->frame));
        bat->frame++;
        if (bat->frame >= 0x11)
            bat->energy = -1;
        return;
    }
    if (bat->state > 0x1234) {
        /* $DC1E: the dive: it falls by its speed (which grows every other frame), turned to the player, and is flying again when the speed is above 8 */
        if (guest.timer_306 == 0)
            bat->y = (int16_t)(bat->y + bat->speed_y);

        int16_t x = (int16_t)(bat->x - guest.scroll_x);
        int16_t y = (int16_t)(bat->y - guest.scroll_y);
        uint16_t image = 0x1F7;

        bat->frame = 0;
        if (x <= (int16_t)(guest.player_x - guest.scroll_x)) {
            image += 4;
            bat->frame = 4;
        }
        sprite_draw(x, y, image);
        if (guest.frame_toggle != 0 && guest.timer_306 == 0) {
            bat->speed_y++;
            if (bat->speed_y > 8)
                bat->state = 0;
        }
        return;
    }
    if (bat->state < 0) {
        /* $DC80: hanging: turned to the player (the picture 8 to 10 by where he is), it drops on him by chance (the state $2345) when it is on the screen; off it, out of a margin, it goes back
         * into the map as the thing 108 */
        int16_t x = (int16_t)(bat->x - guest.scroll_x);
        int16_t y = (int16_t)(bat->y - guest.scroll_y);
        int16_t left = (int16_t)(guest.player_x - guest.scroll_x);
        int16_t right = (int16_t)(left + 0x40);
        uint16_t image = 0x1F7;

        left = (int16_t)(left - 0x10);
        if (x < left)
            image += 0xA;
        else if (x > right)
            image += 8;
        else
            image += 9;
        sprite_draw(x, y, image);
        x = (int16_t)(bat->x - guest.scroll_x);
        y = (int16_t)(bat->y - guest.scroll_y);
        if (y >= 0 && x >= 0 && x <= 0x120 && y < 0x78) {
            if (random_word() <= 0x9C4 && guest.timer_306 == 0)
                bat->state = 0x2345;
            return;
        }
        if (x < 0x158 && x > -0x30 && y > -0x30 && y < 0xD8)
            return;

        struct Slot8 *record = find_free_record_19b14();

        if (record != 0) {
            record->w[0] = (int16_t)(x + guest.scroll_x);
            record->w[1] = (int16_t)(y + guest.scroll_y);
            record->w[2] = 0x6C - 11;
            record->w[3] = 0;   /* ($8058 sets 1; the clr.w -(a5) after it clears it) */
            guest.near_flags[record - guest.records_19b14] = 0xff;
        }
        bat->energy = -1;
        return;
    }
    /* $DB52: flying */
    if (guest.timer_306 == 0) {
        bat->y = (int16_t)(bat->y + bat->speed_y);
        bat->x = (int16_t)(bat->x + bat->speed_x);
    }

    int16_t x = (int16_t)(bat->x - guest.scroll_x);
    int16_t y = (int16_t)(bat->y - guest.scroll_y);

    if (y < 0)
        return;
    sprite_draw(x, y, (uint16_t)(0x1F7 + bat->frame + bat->state));
    if (guest.timer_306 == 0)
        bat_steer_to_player(bat);
    bat->state = (int16_t)((bat->state + 1) & 3);
}

/* orig $00DB04 (a bare return unless a bat was started): the bats that are in use go on one by one; with none the routine is switched off again */
static void bats_run(void)
{
    if (guest.code_db04 != 0x4E71)
        return;

    struct Bat *bat = bats();
    int16_t used = 0;

    guest.bat_count = 0;
    for (int i = 0; i < 10; i++, bat++) {
        if (bat->energy < 0)
            continue;
        used++;
        guest.bat_count = (uint16_t)used;
        bat_update(bat);
    }
    if (used == 0)
        guest.code_db04 = 0x4E75;
}

/* orig $00DE56: the ten archers (falling_things: x, y, state, hit points, timer).  The state counts -4 (waiting for the screen), -3 to -1 (rising), 0 to 4 (standing: 0 and 1 shooting to the
 * left, 2 under it, 3 and 4 shooting to the right); a negative hit points count is dying (the picture 108 + the state, the things are thrown at 4, gone at 7).  Each is hit as a box
 * of 24 by 24 (8 by 8 while it rises); when killed it drops the thing 45 and 25 points are added */
static void update_archers(void)
{
    if (guest.code_de54 != 0x4E71)
        return;
    guest.falling_things_count = 0;
    for (int i = 0; i < 10; i++) {
        int16_t *a = &guest.falling_things[i * 5];

        if (a[0] < 0)
            continue;
        guest.falling_things_count++;
        if (a[3] < 0) {   /* $DF78: dying */
            int16_t sx = (int16_t)(a[0] - 4 - guest.scroll_x);
            int16_t sy = (int16_t)(a[1] - 8 - guest.scroll_y);

            sprite_draw(sx, sy, (uint16_t)(a[2] + 0x6C));
            sprite_draw(sx, (int16_t)(sy + 0x18), (uint16_t)(a[2] + 0x6C));
            a[2]++;
            if (a[2] == 4) {
                if (!archer_throws(a))
                    return;
            } else if (a[2] >= 7) {
                a[0] = -1;
            }
            continue;
        }

        int standing = 1;   /* the AI of $DE7A runs */

        if (a[2] < 0) {   /* $E006 */
            standing = 0;
            if (a[2] != -4) {
                if (guest.frame_toggle != 0) {
                    a[2]++;
                    if (a[2] == 0)
                        standing = 1;
                }
            } else {
                int16_t sx = (int16_t)(a[0] - guest.scroll_x);
                int16_t sy = (int16_t)(a[1] - guest.scroll_y);

                if (sx > 0x140 || sx < -0x18 || sy < 8 || sy > 0xB0) {
                    a[4] = 0x27;
                } else {
                    a[4]--;
                    if (a[4] < 0) {
                        a[4] = 0x27;
                        a[2]++;
                    }
                }
            }
            if (!standing) {   /* $E05E */
                guest.hit_box_extra_width = 8;
                guest.hit_box_extra_height = 8;
                a[3] = check_hit_and_walls((int16_t)(a[0] + 8), (int16_t)(a[1] + 0xF), a[3]);
                int16_t sx = (int16_t)(a[0] - guest.scroll_x);
                int16_t sy = (int16_t)(a[1] - guest.scroll_y);

                if (a[2] >= -1)
                    sprite_draw(sx, sy, (uint16_t)(a[2] + 0x202));
                else
                    glyph_draw(sx, sy, (uint16_t)(a[2] + 0x202));
                archer_body(a);
            }
        }
        if (standing) {   /* $DE7A */
            if (guest.timer_306 == 0) {
                if (a[4] > 0x28) {
                    a[4]--;
                } else {
                    int16_t px = (int16_t)(guest.player_x + 0x20);

                    if (px < a[0]) {
                        a[2] = 0;
                        a[4]--;
                        if (a[4] < 0) {
                            a[4] = 0x2A;
                            guest.shot_speed = -5;
                            enemy_shot((int16_t)(a[0] - 6), (int16_t)(a[1] + 6), 0);
                            a[2] = 1;
                        }
                    } else if ((int16_t)(px - 0x38) < a[0]) {
                        a[2] = 2;
                    } else {
                        a[2] = 3;
                        a[4]--;
                        if (a[4] < 0) {
                            a[4] = 0x2A;
                            guest.shot_speed = 5;
                            enemy_shot((int16_t)(a[0] + 0x18), (int16_t)(a[1] + 6), 1);
                            a[2] = 4;
                        }
                    }
                }
            }
            guest.hit_box_extra_width = 0x18;
            guest.hit_box_extra_height = 0x18;
            a[3] = check_hit_and_walls(a[0], a[1], a[3]);
            sprite_draw((int16_t)(a[0] - guest.scroll_x), (int16_t)(a[1] - guest.scroll_y), (uint16_t)(a[2] + 0x202));
            archer_body(a);
        }
        if (a[3] < 0) {   /* $DF52: killed: it drops a thing and its points */
            a[2] = 0;
            drop_thing((int16_t)(a[0] + 4), (int16_t)(a[1] + 0x18), 0x2D);
            score_add_packed(0x25);
        }
    }
    if (guest.falling_things_count == 0)
        guest.code_de54 = 0x4E75;
}

/* orig $00E144: the things the archers throw (table_ed0, sixteen records of eight words: x, y, state, hit points, speed in x, speed in y, falling): one that falls goes on by its speeds (the speed
 * in y goes up to 8) until the cell under it is ground, then it walks and turns round at an edge or a wall; it is hit as a box of 24 by 16, and drawn as the glyph 533 to 535 by the state
 * (3 more when it goes right); killed it counts 10 points and then shows the picture 108 + the state (up to 6) */
static void update_thrown(void)
{
    if (guest.code_e142 != 0x4E71)
        return;
    guest.thrown_things_count = 0;
    for (int i = 0; i < 16; i++) {
        int16_t *t = &guest.table_ed0[i * 8];

        if (t[2] < 0)
            continue;
        guest.thrown_things_count++;
        if (t[3] < 0) {   /* $E25E */
            sprite_draw((int16_t)(t[0] - guest.scroll_x - 4), (int16_t)(t[1] - guest.scroll_y - 0x11), (uint16_t)(t[2] + 0x6C));
            t[2]++;
            if (t[2] >= 7)
                t[2] = -1;
            continue;
        }
        if (t[6] != 0) {   /* $E2E6: falls */
            int16_t x = (int16_t)(t[0] + t[4]);
            int16_t y = (int16_t)(t[1] + t[5]);

            t[5]++;
            if (t[5] > 8)
                t[5]--;
            t[0] = x;
            t[1] = y;
            const uint8_t *cell = map_cell_at((int16_t)(x + 0xC), (int16_t)(y + 0xF));

            if (cell[-0x80] != 1 && cell[0] == 1) {
                t[1] = (int16_t)((t[1] & 0xFFF0) + 1);
                t[6] = 0;
                t[5] = 0;
                t[4] = t[4] < 0 ? -2 : 2;
            }
        } else if (guest.timer_306 == 0) {   /* $E29E: walks */
            int16_t speed = t[4];

            t[0] = (int16_t)(t[0] + speed);

            const uint8_t *cell = map_cell_at((int16_t)(speed >= 0 ? t[0] + 0x18 : t[0]), (int16_t)(t[1] + 4));

            if (cell[0] == 1 || cell[0x80] != 1)
                t[4] = (int16_t)-t[4];
        }
        guest.hit_box_extra_width = 0x18;
        guest.hit_box_extra_height = 0x10;
        t[3] = check_hit_and_walls(t[0], t[1], t[3]);

        int16_t glyph = (int16_t)glyphs_e22c[t[2] & 3];

        if (t[4] >= 0)
            glyph += 3;
        int16_t sx = (int16_t)(t[0] - guest.scroll_x);
        int16_t sy = (int16_t)(t[1] - guest.scroll_y);

        if (t[1] >= 0x400 || sx < -250 || sx > 0x23A || sy < -120 || sy > 0x138) {
            t[2] = -1;
            continue;
        }
        sprite_draw(sx, sy, (uint16_t)glyph);
        if (t[3] < 0) {
            score_add_packed(0x10);
            t[2] = 0;
        }
        t[2] = (int16_t)((t[2] + 1) & 3);
    }
    if (guest.thrown_things_count == 0)
        guest.code_e142 = 0x4E75;
}

/* orig $0183BC: a bolt from (x, y) at the player (the place 8 right and 6 below player_x, player_y), `speed` steps a frame and the kind `kind` (aimed_bolt_start) */
static void aimed_shot(int16_t x, int16_t y, int16_t speed, int16_t kind)
{
    aimed_bolt_start(x, y, (int16_t)(guest.player_x + 8), (int16_t)(guest.player_y + 6), speed, kind);
}

/* ======== the monsters: $8A54, with the routines between $891C and $9A00 ======== */

/* orig $00AEB2: the cells (of the map's 48 columns) round a monster that push it away, and $00AF12 the push of each (x and y) */
static const int16_t monster_wall_cells[48] = {
    -49, -48, -47, -46, -45, -44, -1, 47, 95, 143, 4, 52,
    100, 148, 191, 192, 193, 194, 195, 196, -98, -97, -96, -95,
    -94, -93, -92, -91, -50, -2, 46, 94, 142, 190, -43, 5,
    53, 101, 149, 197, 238, 239, 240, 241, 242, 243, 244, 245,
};
static const int16_t monster_wall_push[96] = {
    4, 4, 3, 4, 2, 4, -2, 4, -3, 4, -4, 4,
    4, 3, 4, 2, 4, -2, 4, -3, -4, 3, -4, 2,
    -4, -2, -4, -3, 4, -4, 3, -4, 2, -4, -2, -4,
    -3, -4, -4, -4, 3, 3, 2, 3, 2, 3, 1, 3,
    -1, 3, -2, 3, -2, 3, -3, 3, 3, 2, 3, 2,
    3, 1, 3, -1, 3, -2, 3, -2, -3, 2, -3, 2,
    -3, 1, -3, -1, -3, -2, -3, -2, 3, -3, 2, -3,
    2, -3, 1, -3, -1, -3, -2, -3, -2, -3, -3, -3,
};

/* orig $009498: for 2 to 10 monsters in the chain (a row of ten bytes each) where each is on the circle of 128 */
static const uint8_t monster_circle[90] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 64, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 42, 85, 0, 0, 0, 0, 0, 0, 0,
    0, 32, 64, 96, 0, 0, 0, 0, 0, 0,
    0, 26, 51, 77, 102, 0, 0, 0, 0, 0,
    0, 21, 43, 64, 85, 107, 0, 0, 0, 0,
    0, 18, 37, 55, 73, 91, 110, 0, 0, 0,
    0, 16, 32, 48, 64, 80, 96, 112, 0, 0,
    0, 13, 26, 38, 51, 64, 77, 90, 102, 0,
};

/* orig $0004A4: the pictures a monster of the first level goes through (a word each) */
static const int16_t monster_steps[4] = { 0, 1, 2, 1 };

/* the unsigned division of the original's divu.w (a word by a word, by 0 the quotient is 65535) done by shifts and subtractions: the 68000 has no 32-bit division in the library */
static uint16_t monster_divide(uint32_t value, uint16_t divisor)
{
    uint32_t rest = 0;
    uint32_t quotient = 0;

    if (divisor == 0)
        return 0xFFFF;
    for (int bit = 31; bit >= 0; bit--) {
        rest = (rest << 1) | ((value >> bit) & 1);
        if (rest >= divisor) {
            rest -= divisor;
            quotient |= 1u << bit;
        }
    }
    return (uint16_t)quotient;
}

/* orig $0098C2: which of the seven zones of zone_limits (1 to 7) the monster's box of 32 by 32 is in; 0 when in none */
static int16_t monster_zone(const struct MonsterPart *m)
{
    int16_t x = (int16_t)(m->x - guest.scroll_x);
    int16_t y = (int16_t)(m->y - guest.scroll_y);

    for (int i = 0; i < 7; i++) {
        const uint16_t *limit = &zone_limits[i * 4];

        if ((int16_t)(x + 32) < (int16_t)limit[0] || (int16_t)(y + 32) < (int16_t)limit[1] || x > (int16_t)limit[2] || y > (int16_t)limit[3])
            continue;
        return (int16_t)(i + 1);
    }
    return 0;
}

/* orig $00AE3C: the monster shows up: sound 18, a puff (the first free record of records_721e) where it is, and it waits five frames more (hp -3).  Without a free record nothing else happens */
static void monster_puff(struct MonsterPart *m)
{
    int16_t *record = guest.records_721e;
    int i;

    play_sound(18, 4);
    for (i = 0; i < 10; i++, record += 3)
        if (record[2] < 0)
            break;
    if (i == 10)
        return;
    record[0] = m->x;
    record[1] = m->y;
    record[2] = 7;
    m->timer = 5;
    guest.flag_721c = 1;
    m->hp = -3;
}

/* orig $00AE78: the monster (at the screen place x, y) is hit as a box of 32 by 32: check_hit_and_walls with its hit points gives what is left; below 0 it dies */
static void monster_hit(struct MonsterPart *m, int16_t x, int16_t y)
{
    guest.hit_box_extra_width = 0x20;
    guest.hit_box_extra_height = 0x20;
    m->hp = check_hit_and_walls((int16_t)(x + guest.scroll_x), (int16_t)(y + guest.scroll_y), m->hp);
    if (m->hp < 0) {
        m->frame = -1;
        m->timer = 0;
    }
}

/* orig $008D1E: the picture of the monster: the first word of its template, a flying one (the template's picture $1EE: eight pictures) or the $1E4 (six) goes through its pictures; the
 * count down in anim_timer sets how fast, from the monster's speed.  The other templates have four pictures (the first levels' take them in the order of monster_steps) and
 * the one that looks left is 3 or 4 on */
static int16_t monster_picture(const struct MonsterType *type, struct MonsterPart *m)
{
    const struct CreatureTemplate *template = &((const struct CreatureTemplate *)guest.level_creature_templates)[type->picture];
    int16_t picture = (int16_t)template->unused;

    if (picture == 0x1EE) {
        if ((int8_t)--m->anim_timer < 0) {
            m->frame = (int16_t)((m->frame + 1) & 7);
            m->anim_timer = (uint8_t)((10 - m->speed) >> 2);
        }
        return (int16_t)(picture + m->frame);
    }
    if (picture == 0x1E4) {
        if ((int8_t)--m->anim_timer < 0) {
            m->frame++;
            if (m->frame >= 6)
                m->frame = 0;
            m->anim_timer = (uint8_t)((10 - m->speed) >> 2);
        }
        return (int16_t)(picture + m->frame);
    }
    if ((int8_t)--m->anim_timer < 0) {
        m->frame = (int16_t)((m->frame + 1) & 3);
        m->anim_timer = (uint8_t)((10 - m->speed) >> 2);
    }
    if (guest.param_index < 2) {
        picture = (int16_t)(picture + monster_steps[m->frame]);
        if (m->facing == 0)
            picture += 3;
    } else {
        picture = (int16_t)(picture + m->frame);
        if (m->facing == 0)
            picture += 4;
    }
    return picture;
}

/* the smaller of the monster's speed and the type's byte 12 (the lower limit used by $8ED8) */
static int16_t monster_limit_c(const struct MonsterType *type, const struct MonsterPart *m)
{
    int16_t limit = type->limit_c;

    return m->speed > limit ? limit : m->speed;
}

/* orig $008ED8: the monster's speed changes towards its target (a cell of 32 by 32 pixels it is in is the target's: it slows down there and, with a rank above 1, is
 * at the target when the places are the same to 8 pixels); elsewhere a eighth of the way, up to the limit, is added to the speed */
static void monster_steer(const struct MonsterType *type, struct MonsterPart *m)
{
    int16_t x = m->x, y = m->y;
    int16_t tx = m->target_x, ty = m->target_y;
    int16_t limit = monster_limit_c(type, m);

    if ((int16_t)((tx + 16) & 0xFFE0) != (int16_t)((x + 16) & 0xFFE0) || (int16_t)((ty + 16) & 0xFFE0) != (int16_t)((y + 16) & 0xFFE0)) {
        int16_t dx = (int16_t)((int16_t)(tx - x) >> 3);
        int16_t dy = (int16_t)((int16_t)(ty - y) >> 3);

        if (dx >= 0) {
            if (dx > limit)
                dx = limit;
        } else if (dx < -limit) {
            dx = (int16_t)-limit;
        }
        if (dy >= 0) {
            if (dy > limit)
                dy = limit;
        } else if (dy < -limit) {
            dy = (int16_t)-limit;
        }
        m->vx = (int16_t)(m->vx + dx);
        m->vy = (int16_t)(m->vy + dy);
        return;
    }

    int16_t vx = m->vx, vy = m->vy;

    if (vx >= 0) {
        vx = (int16_t)(vx - limit);
        if (vx < 0)
            vx = 0;
    } else {
        vx = (int16_t)(vx + limit);
        if (vx >= 0)
            vx = 0;
    }
    if (vy >= 0) {
        vy = (int16_t)(vy - limit);
        if (vy < 0)
            vy = 0;
    } else {
        vy = (int16_t)(vy + limit);
        if (vy >= 0)
            vy = 0;
    }
    m->vx = vx;
    m->vy = vy;
    m->new_target = 1;
    if (m->rank != 0 && m->rank != 1) {
        if ((int16_t)((x + 4) & 0xFFF8) == (int16_t)((tx + 4) & 0xFFF8) && (int16_t)((y + 4) & 0xFFF8) == (int16_t)((ty + 4) & 0xFFF8))
            m->reached = 1;
    }
}

/* orig $0090A2: how the cells round the monster (monster_wall_cells from the cell at its place, on the map line_text + 100: a cell that is not 0, and for the monsters that look for
 * what lies about without a goal also $FD) push it: the first two give their push; the steps along the lists are 16 less the type's byte 15 (at most 15) */
static void monster_wall_push_sum(const struct MonsterType *type, const struct MonsterPart *m, int16_t *push_x, int16_t *push_y)
{
    int16_t px = 0, py = 0;
    int16_t step = (int16_t)((int16_t)(int8_t)(0x0F - type->wall) + 1) & 0x0F;
    int16_t x = (int16_t)(m->x - guest.scroll_x);
    int16_t y = (int16_t)(m->y - guest.scroll_y);

    *push_x = 0;
    *push_y = 0;
    if (x >= 0x140 || x <= -32 || y <= -16 || y >= 0xC0)
        return;

    const uint8_t *cell = guest.line_text + 100;

    if (x & 4)
        cell++;
    cell += x >> 3;
    cell += (y & -8) * 6;

    const int16_t *offset = monster_wall_cells;
    const int16_t *push = monster_wall_push;
    int hits = 0;
    int16_t left = 0x2F;

    if (step == 0)
        return;   /* (the original does not end) */

    int every = m->mode != 0 && m->goal != 0;

    do {
        uint8_t value = cell[*offset++];

        if (every ? value != 0 : (value != 0 && value != 0xFD)) {
            if (hits < 2) {
                hits++;
                px = (int16_t)(px + push[0]);
                py = (int16_t)(py + push[1]);
            }
        }
        push += 2 * step;
        left = (int16_t)(left - step);
    } while (left >= 0);
    *push_x = px;
    *push_y = py;
}

/* orig $009040: a monster with a wall push (the type's byte 15) that is pushed away by the cells round it gets that as its speed (twice, within the limit of byte 12, and what it had
 * before the step added), moves at once (speed 10) and is no longer at its target */
static void monster_bump(const struct MonsterType *type, struct MonsterPart *m)
{
    int16_t dx, dy;

    if (type->wall == 0)
        return;
    monster_wall_push_sum(type, m, &dx, &dy);
    if ((dx | dy) == 0)
        return;
    m->speed = 10;
    m->reached = 0;
    dx = (int16_t)(dx * 2);
    dy = (int16_t)(dy * 2);

    int16_t limit = type->limit_c;

    if (dx != 0) {
        if (dx < 0) {
            if (dx < -limit)
                dx = (int16_t)-limit;
        } else if (dx > limit) {
            dx = limit;
        }
    }
    if (dy != 0) {
        if (dy < 0) {
            if (dy < -limit)
                dy = (int16_t)-limit;
        } else if (dy > limit) {
            dy = limit;
        }
    }
    m->vx = (int16_t)(dx + guest.monster_old_vx);
    m->vy = (int16_t)(dy + guest.monster_old_vy);
}

/* orig $009002: the speed is kept within the type's byte 11 (the step is what is left) */
static void monster_limit_speed(const struct MonsterType *type, struct MonsterPart *m, int16_t *dx, int16_t *dy)
{
    int16_t limit = type->limit_b;
    int16_t vx = m->vx, vy = m->vy;

    if (vx >= 0) {
        if (vx > limit)
            vx = limit;
    } else if (vx < -limit) {
        vx = (int16_t)-limit;
    }
    if (vy >= 0) {
        if (vy > limit)
            vy = limit;
    } else if (vy < -limit) {
        vy = (int16_t)-limit;
    }
    m->vx = vx;
    m->vy = vy;
    *dx = vx;
    *dy = vy;
}

/* orig $008FE0: the monster moves by its step (when timer_306 is 0): to its target when it is at it (reached) */
static void monster_move(struct MonsterPart *m, int16_t dx, int16_t dy)
{
    if (guest.timer_306 != 0)
        return;
    if (m->reached != 0) {
        m->x = m->target_x;
        m->y = m->target_y;
        return;
    }
    m->x = (int16_t)(m->x + dx);
    m->y = (int16_t)(m->y + dy);
}

/* orig $0091EE: the monster shoots when its count down (fire_timer) is over: low nibble of the type's byte 6 (how often), high nibble the speed.  Type bits 4 and 5 are how: 0 a shot
 * sideways (to the side it moves, with flag_244 negative one by the monster's mode), 1 a bolt at a place near the player (aimed_bolt_start, $183C8), 2 and 3 a bolt at the player
 * ($183BC: aimed_shot) */
static void monster_fire(const struct MonsterType *type, struct MonsterPart *m)
{
    uint8_t rate = type->fire;

    if (rate == 0 || (rate & 0x0F) == 0)
        return;
    if (--m->fire_timer != 0)
        return;
    m->fire_timer = (int16_t)((int16_t)(int8_t)(16 - (rate & 0x0F)) * 4);

    int16_t kind = (int16_t)((type->flags >> 4) & 3);
    int16_t speed = (int16_t)((rate & 0xF0) >> 4);

    if (kind != 0) {
        if (kind == 1) {
            int16_t x = (int16_t)(guest.player_x + 8);
            int16_t y = (int16_t)(guest.player_y + 6);

            x = (int16_t)(x + (int16_t)(random_word() & 0x3F) - 0x1F);
            y = (int16_t)(y + (int16_t)(random_word() & 0x3F) - 0x1F);
            aimed_bolt_start((int16_t)(m->x + 0x10), m->y, x, y, speed, 0);
            return;
        }
        aimed_shot((int16_t)(m->x + 0x10), m->y, speed, 0);
        return;
    }

    int right;

    if (guest.flag_244 < 0)
        right = ((0x15u >> (m->mode & 63)) & 1) == 0;   /* $9A1C: the long $15 */
    else if (m->vx == 0)
        return;
    else
        right = m->vx > 0;
    if (right) {
        guest.shot_speed = speed;
        enemy_shot((int16_t)(m->x + 0x20), m->y, 1);
    } else {
        guest.shot_speed = (int16_t)-speed;
        enemy_shot((int16_t)(m->x - 8), m->y, 0);
    }
}

/* orig $00985C: the chain is over (its first has died): all the monsters of the record lose their rank and look for what lies about (mode 1) again */
static void monster_end_chain(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *block)
{
    rec->chain = 0;
    rec->mask = 0;
    for (int i = 0; i < type->count; i++) {
        block[i].rank = 0;
        block[i].reached = 0;
        block[i].mode = 1;
        block[i].goal = -1;
    }
}

/* orig $00988C: a monster of the chain has died (its rank, m->rank, is still there): the ones behind it (higher rank) move up one */
static void monster_close_gap(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *m, struct MonsterPart *block)
{
    uint16_t mask = rec->mask;

    if (rec->chain <= 1)
        return;
    for (int i = 0; i < type->count; i++) {
        int carry = mask & 1;

        mask >>= 1;
        if (carry && m->rank < block[i].rank) {
            block[i].rank--;
            block[i].reached = 0;
        }
    }
}

/* orig $0097DE: while there is a chain its phase (round the circle) goes on.  Without one there is a chance (the type's byte 10, of 128) that a monster that is alive (hit points above 0)
 * becomes the first of one: the one the record picked (special), or one by chance */
static void monster_recruit(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *block)
{
    if (rec->chain != 0) {
        rec->phase = (uint8_t)((rec->phase + 1) & 0x7F);
        return;
    }
    if (!((int8_t)(random_word() & 0x7F) < (int8_t)type->chance))
        return;

    int8_t who = rec->special;

    if (who < 0) {
        int16_t count = type->count;
        uint16_t value = random_word();
        uint16_t step = count > 0 ? monster_divide(0xFFFF, (uint16_t)count) : 0xFFFF;
        uint16_t limit = step;
        int16_t k = 0;

        while (!(value < limit)) {
            limit = (uint16_t)(limit + step);
            k++;
        }
        who = (int8_t)k;
    }

    struct MonsterPart *chosen = &block[who];

    if (chosen->hp <= 0)
        return;
    chosen->rank = 1;
    rec->chain = 1;
    rec->mask = (uint16_t)(1u << (who & 31));
    rec->lead = 0;
    rec->index = 0;
}

/* orig $009606: the monster takes up the object of the list (x, y, kind): the first record of records_19b14 (from the last) with that kind and place goes into the monster's hands */
static void monster_take(struct MonsterPart *m, int16_t x, int16_t y, int16_t kind)
{
    for (int i = 199; i >= 0; i--) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[3] == 0 || record->w[2] < 0)
            continue;
        if (record->w[2] != kind || record->w[0] != x || record->w[1] != y)
            continue;
        m->item_x = x;
        m->item_y = y;
        m->item = kind;
        record->w[2] = -1;
        record->w[3] = 0;
        return;
    }
}

/* orig $00957C: a monster without an object that is near (to 48 pixels up and left, 16 right and down) one of the objects in the list at $121C that the type takes (bits 0 and 1 of
 * the type's flags for the common ones, 2 and 3 for the ones from kind $C0 on) takes it up */
static void monster_pickup(const struct MonsterType *type, struct MonsterPart *m)
{
    if (guest.list_count_1294 == 0 || m->item >= 0)
        return;

    const int16_t *list = (const int16_t *)guest.list_121c;
    int16_t count = (int16_t)guest.list_count_1294;
    int16_t x = (int16_t)(m->x - 8);
    int16_t y = (int16_t)(m->y - 8);

    if (count > 20)
        count = 20;
    for (int16_t n = 0; n < count; n++, list += 3) {
        uint16_t takes = type->flags;

        if (list[2] >= 0xC0)
            takes >>= 2;
        if ((takes & 3) == 0)
            continue;

        int16_t ox = (int16_t)(list[0] + 8 + guest.scroll_x);
        int16_t oy = (int16_t)(list[1] + 8 + guest.scroll_y);

        if (x > ox || y > oy)
            continue;
        ox = (int16_t)(ox - 0x30);
        oy = (int16_t)(oy - 0x30);
        if (x < ox || y < oy)
            continue;
        monster_take(m, (int16_t)(list[0] + guest.scroll_x), (int16_t)(list[1] + guest.scroll_y), list[2]);
        return;
    }
}

/* orig $009370 (mode 4): the target is where the monster in front (the rank before) was: in the history of the first, 16 places for each rank less 8 bytes back.  The type's bits 6 and
 * 7: 0 that place, 2 a circle round the place of the first ($942E, monster_circle), 1 and 3 that place turned by the angle of the rank (sine_table) */
static void monster_follow(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *m)
{
    if (m->speed < 2)
        m->speed = 2;

    int16_t back = (int16_t)((m->rank - 1) * 16);
    int16_t at = (int16_t)(rec->index - (back - 8));

    if (at < 0)
        at = (int16_t)(at + 0x190);

    int16_t x = rec->history[at >> 2].x;
    int16_t y = rec->history[at >> 2].y;
    uint16_t how = type->flags & 0xC0;

    if (how == 0) {
        m->target_x = x;
        m->target_y = y;
        return;
    }

    int16_t last = (int16_t)(rec->index - 4);

    if (last < 0)
        last = (int16_t)(last + 0x190);
    if (how == 0x80) {
        int16_t row = (int16_t)(rec->chain - 2);
        uint8_t around = (uint8_t)(monster_circle[row * 10 + (m->rank - 2)] + rec->phase);
        int16_t angle;

        if (around & 0x80)
            around = (uint8_t)(around - 0x80);
        angle = (int16_t)(around * 8);

        int16_t dx = (int16_t)((int16_t)guest.sine_table[angle >> 1] >> 10);
        int16_t dy = (int16_t)((int16_t)guest.sine_table[(0x100 + angle) >> 1] >> 10);

        m->target_x = (int16_t)(rec->history[last >> 2].x + dx);
        m->target_y = (int16_t)(rec->history[last >> 2].y + dy);
        return;
    }

    int16_t px = rec->history[last >> 2].x;
    int16_t py = rec->history[last >> 2].y;
    int16_t dx = (int16_t)(x - px);
    int16_t dy = (int16_t)(y - py);
    int16_t offset = (int16_t)((((m->rank - 2) & 1)) << 7);
    int16_t sine = (int16_t)guest.sine_table[offset >> 1];
    int16_t cosine = (int16_t)guest.sine_table[(0x100 + offset) >> 1];
    int16_t tx = (int16_t)(((int32_t)dy * sine >> 15) + ((int32_t)dx * cosine >> 15));
    int16_t ty = (int16_t)(((int32_t)dy * cosine >> 15) - ((int32_t)dx * sine >> 15));

    m->target_x = (int16_t)(tx + px);
    m->target_y = (int16_t)(ty + py);
}

/* orig $009652 (mode 1): the monster looks for a zone (zone_limits) to go to: by the chance of the type's byte 13 the one most of the group's zones are in (zone_counts, by the
 * player's place), else one by chance (1 to 7); if it is not in it already the zone next to it on the shorter way (through zones where zone_counts is not 0; on a tie the one with more, or
 * one by chance) is its next, and a place by chance in that zone is its target */
static void monster_zone_goal(const struct MonsterType *type, struct MonsterPart *m)
{
    int16_t aim;
    int16_t here;
    int16_t up, down;

    m->goal = -1;
    if (!((int8_t)(random_word() & 0x7F) > (int8_t)type->zone_chance)) {
        int16_t best = 0;

        aim = 1;
        for (int16_t i = 1; i <= 6; i++) {
            if ((int8_t)guest.zone_counts[i] > best) {
                best = (int8_t)guest.zone_counts[i];
                aim = i;
            }
        }
    } else {
        do
            aim = (int16_t)(random_word() & 7);
        while (aim == 0);
    }

    here = monster_zone(m);
    int8_t zone = (int8_t)here;

    if (zone != (int8_t)aim) {
        int16_t z = zone;

        up = 0;
        for (;;) {
            up++;
            z++;
            if (z >= 7)
                z = 1;
            if ((int8_t)aim == (int8_t)z)
                break;
            if (guest.zone_counts[z] == 0) {
                up = -1;
                break;
            }
        }
        z = zone;
        down = 0;
        for (;;) {
            down++;
            z--;
            if (z == 0)
                z = 6;
            if ((int8_t)aim == (int8_t)z)
                break;
            if (z < 0 || guest.zone_counts[z] == 0) {   /* (a monster in no zone goes below 0 in the original, which reads what lies before zone_counts) */
                down = -1;
                break;
            }
        }
        if (up < 0)
            goto back;
        if (down < 0)
            goto forward;
        if (up == down) {
            int16_t next = (int16_t)(zone + 1);
            int16_t before = (int16_t)(zone - 1);

            if (next >= 7)
                next = 1;
            if (before == 0)
                before = 6;
            if ((int8_t)guest.zone_counts[before] > (int8_t)guest.zone_counts[next])
                goto back;
            if ((int8_t)guest.zone_counts[before] < (int8_t)guest.zone_counts[next])
                goto forward;
            if ((random_word() & 1) == 0)
                goto back;
            goto forward;
        }
        if (down > up)
            goto back;
forward:
        zone = (int8_t)(zone + 1);
        if (zone >= 7)
            zone = 1;
        goto chosen;
back:
        if (down < 0)
            goto chosen;
        zone = (int8_t)(zone - 1);
        if (zone == 0)
            zone = 6;
    }
chosen:
    if ((uint8_t)zone == m->zone && m->new_target == 0)
        return;
    m->zone = (uint8_t)zone;

    /* the zone 0 (in none): the original reads the eight bytes before zone_limits, which are the same four words as its last record ($998A: 110, 60, 210, 136), the middle of the screen */
    const uint16_t *limit = zone < 1 ? &zone_limits[24] : &zone_limits[(zone - 1) * 4];
    int16_t x = (int16_t)limit[0], y = (int16_t)limit[1];
    int16_t wide = (int16_t)(limit[2] - x), high = (int16_t)(limit[3] - y);

    x = (int16_t)(x + (int16_t)((int16_t)((random_word() & 0x7F) * wide) >> 7));
    y = (int16_t)(y + (int16_t)((int16_t)((random_word() & 0x7F) * high) >> 7));
    m->target_x = (int16_t)(x - 0x10 + guest.scroll_x);
    m->target_y = (int16_t)(y - 0x10 + guest.scroll_y);
    m->new_target = 0;
    (void)type;
}

/* orig $009270: where the monster goes.  With a goal (0 the player, 4 the chain) that; without one (-1) a monster of the record that is not in the chain may join it (a chance by the chain's
 * length), else the type's byte 8 (two modes, the low nibble first, the swap by the chance of byte 9) says: 0 the player, 1 a zone, 2 what lies about, 3 start a chain, 4 follow it */
static void monster_target(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *m, struct MonsterPart *block)
{
    uint8_t first, second;
    int16_t value;

    if (m->goal >= 0) {
        first = second = (uint8_t)m->goal;
        goto dispatch;
    }
    if (rec->chain != 0 && !((rec->mask >> guest.monster_index) & 1)) {
        int8_t chance = (int8_t)type->chance;

        value = (int16_t)(random_word() & 0x7F);
        if ((int8_t)value < chance) {
            int16_t eight = (int16_t)((int16_t)rec->chain * 2 * 4);

            if (!(eight > rec->lead)) {
                rec->chain++;
                m->rank = rec->chain;
                rec->mask = (uint16_t)(rec->mask | (1u << (guest.monster_index & 31)));
                m->goal = 4;
                monster_follow(rec, type, m);
                return;
            }
        }
    }
    first = type->mode & 0x0F;
    second = (type->mode & 0xF0) >> 4;
    if ((int8_t)type->mode_chance < (int8_t)(random_word() & 0x7F)) {
        uint8_t swap = first;

        first = second;
        second = swap;
    }

dispatch:
    switch ((int8_t)(uint8_t)(first << 2)) {
    case 0:   /* $931E: the player */
        if ((m->goal << 8 | m->reached) != 0 && type->chance != 0) {
            int different;

            if (m->rank == 1) {
                int16_t d4 = rec->alive;
                uint32_t dividend = (uint32_t)(int32_t)(int16_t)((int16_t)rec->chain << 7);
                uint16_t quotient = monster_divide(dividend, (uint16_t)d4);

                different = (int16_t)quotient < (int8_t)type->chance;
            } else {
                different = (int16_t)(random_word() & 0x7F) <= type->chance;   /* (the original compares with a word whose high byte was left from before: taken as 0) */
            }
            if (different) {
                uint8_t swap = first;

                first = second;
                second = swap;
                goto dispatch;
            }
        }
        m->goal = 0;
        m->target_x = guest.player_x;
        m->target_y = (int16_t)(guest.player_y + 8);
        return;
    case 4:   /* $9652: a zone */
        monster_zone_goal(type, m);
        return;
    case 8:   /* $94F2: what lies about */
        if (m->item < 0 && guest.list_count_1294 != 0) {
            const int16_t *entry = (const int16_t *)(uintptr_t)guest.value_1296;
            int16_t x = (int16_t)(entry[0] + guest.scroll_x - 8);
            int16_t y = (int16_t)(entry[1] + guest.scroll_y - 8);
            uint16_t takes = type->flags;

            if (entry[2] >= 0xC0)
                takes >>= 2;
            if ((takes & 3) == 0) {
                x = (int16_t)(x + (int16_t)(random_word() & 0x0F) - 8);
                y = (int16_t)(y + (int16_t)(random_word() & 0x0F) - 8);
            }
            m->target_x = x;
            m->target_y = y;
            m->new_target = 0;
            monster_pickup(type, m);
            return;
        }
        if (first == second) {
            monster_zone_goal(type, m);
            return;
        }
        {
            uint8_t swap = first;

            first = second;
            second = swap;
        }
        goto dispatch;
    case 12:   /* $97DE */
        monster_recruit(rec, type, block);
        return;
    case 16:   /* $9370 */
        monster_follow(rec, type, m);
        return;
    default:
        host_missing_routine(0x930A + (uint32_t)(uint8_t)(first << 2));
        return;
    }
}

/* orig $008B22: one step of a monster that is there (hit points above 0): it chooses its target, steers, moves and shoots; the first of a chain adds its place to the history; it is hit (by
 * the player's shots and the cells) and drawn; the monster the record picked gets two glyphs (the marks 131 and 132) above and below it, and a monster that carries an object draws it
 * (glyph $AA more than its kind, or $12E more from kind $C0) coming after it; the object is dropped when the monster's type lets it (type flags bits 0 to 3, 0 at once, 2 never, else by chance) */
static void monster_step(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *m, struct MonsterPart *block)
{
    guest.monster_old_x = m->x;
    m->speed--;
    if (m->speed < 1)
        m->speed = 1;
    monster_target(rec, type, m, block);
    guest.monster_old_vx = m->vx;
    guest.monster_old_vy = m->vy;
    monster_steer(type, m);
    monster_bump(type, m);

    int16_t dx, dy;

    monster_limit_speed(type, m, &dx, &dy);
    monster_move(m, dx, dy);
    monster_fire(type, m);

    int16_t x = m->x;
    int16_t y = m->y;

    if (m->rank == 1) {
        rec->lead++;
        if (rec->lead >= 100)
            rec->lead = 100;
        rec->history[rec->index >> 2].x = x;
        rec->history[rec->index >> 2].y = y;
        rec->index += 4;
        if (rec->index >= 0x190)
            rec->index = 0;
    }
    int16_t moved = (int16_t)(guest.monster_old_x - x);

    if (moved != 0) {
        if (moved > 0) {
            if (moved >= 2)
                m->facing = 1;
        } else if (moved <= -2) {
            m->facing = 0;
        }
    }
    x = (int16_t)(x - guest.scroll_x);
    y = (int16_t)(y - guest.scroll_y);

    int16_t picture = monster_picture(type, m);

    guest.word_3a6 = 1;
    monster_hit(m, x, y);
    guest.word_3a6 = 0;
    if (m->hp < 0)
        add_kill_score(kill_value_9dda(type->life));   /* $9DDA: the same adding as $9DEA, with the byte the original's a4 points at */
    sprite_draw(x, y, (uint16_t)picture);

    if ((int8_t)guest.monster_index == rec->special) {
        int16_t mx = (int16_t)(m->x - guest.scroll_x + 8);
        int16_t my = (int16_t)(m->y - guest.scroll_y);

        glyph_draw(mx, (int16_t)(my - 10), 0x83);
        glyph_draw(mx, (int16_t)(my + 0x21), 0x84);
    }
    if (m->item < 0)
        return;

    int16_t ix = m->item_x;
    int16_t iy = m->item_y;

    ix = (int16_t)(ix + (int16_t)((int16_t)(m->x + 8 - ix) >> 1));
    iy = (int16_t)(iy + (int16_t)((int16_t)(m->y + 0x18 - iy) >> 1));
    m->item_x = ix;
    m->item_y = iy;

    int16_t sx = (int16_t)(ix - guest.scroll_x);
    int16_t sy = (int16_t)(iy - guest.scroll_y);
    int16_t kind = m->item;

    if (kind < 0xC0) {
        glyph_draw(sx, sy, (uint16_t)(kind + 0xAA));
        switch (type->flags & 3) {
        case 2:
            return;
        case 0:
            break;
        default:
            if ((random_word() & 0x7F) != 0)
                return;
            break;
        }
        drop_record(m->item_x, m->item_y, (int16_t)(m->item + 0x0B));
    } else {
        glyph_draw(sx, sy, (uint16_t)(kind - 0xC0 + 0x12E));
        switch (type->flags & 0x0C) {
        case 8:
            return;
        case 0:
            break;
        default:
            if ((random_word() & 0x7F) != 0)
                return;
            break;
        }
        drop_record(m->item_x, m->item_y, (int16_t)(m->item - 0xC0));
    }
    m->item = -1;
}

/* orig $008E84 (and $008DD8): a monster that is not there.  Hit points -2: it waits (timer) and shows up with a puff; -3: it waits again and then gets its hit points from the
 * type (twice for the one the record picked); any other negative number it is dying: the first time its place is kept (last_death_x, last_death_y), the object it carries dropped and,
 * in a chain, the chain closed up (or ended when it was the first); then its pictures (the glyph 91 on) go on until the 17th, when it is free */
static void monster_dying(struct Monster *rec, const struct MonsterType *type, struct MonsterPart *m, struct MonsterPart *block, int16_t remaining)
{
    if (m->hp == -2) {
        if (--m->timer == 0)
            monster_puff(m);
        return;
    }
    if (m->hp == -3) {
        if (--m->timer != 0)
            return;

        int16_t life = type->life;

        if ((uint8_t)remaining == (uint8_t)rec->special)
            life = (int16_t)(life * 2);
        m->hp = life;
        return;
    }
    guest.dying_count++;
    if (m->frame < 0) {
        guest.last_death_x = m->x;
        guest.last_death_y = m->y;
        if (m->item >= 0) {
            int16_t kind = m->item;

            kind = kind >= 0xC0 ? (int16_t)(kind - 0xC0) : (int16_t)(kind + 0x0B);
            drop_thing(m->item_x, m->item_y, kind);
        }
        if (m->rank != 0) {
            int16_t rank = m->rank;

            if (rank == 1) {
                monster_end_chain(rec, type, block);
            } else {
                rec->chain--;
                rec->mask = (uint16_t)(rec->mask & ~(1u << (rank & 31)));
                monster_close_gap(rec, type, m, block);
            }
        }
    }
    m->frame++;
    if (m->frame >= 0x11) {
        m->hp = 0;
        m->timer = 0;
        return;
    }
    sprite_draw((int16_t)(m->x - guest.scroll_x), (int16_t)(m->y - guest.scroll_y), (uint16_t)(m->frame + 0x5B));
}

/* orig $00891C: a record to start (state 2): the type's bytes are set when the long at $F6C0 (the copylock's) is not 0, which is the failed lock (-DPIRATED: speed limits 10, mode 0, wall push 15; the type keeps its own when the lock has passed), the monsters are
 * put at the start cell one after the other (their timers add the type's delay), the first one shows up with a puff, and the record goes on (state 1); a record whose type has bit 8 picks
 * a monster by chance */
static void monster_start(struct Monster *rec, struct MonsterPart *block)
{
    struct MonsterType *type = rec->type;
    struct MonsterPart *m = block;

#ifdef PIRATED
    type->limit_b = 10;
    type->limit_c = 10;
    type->mode = 0;
    type->wall = 0x0F;
#endif

    int16_t remaining = (int16_t)(type->count - 1);

    if (remaining < 0)
        return;

    int16_t timer = 0;

    do {
        int16_t x = (int16_t)(((type->start >> 8) & 0xFF) << 5 & 0xFFE0);
        int16_t y = (int16_t)((type->start & 0xFF) << 4 & 0xFFF0);

        m->x = x;
        m->y = y;
        m->target_x = x;
        m->target_y = x;
        m->zone = (uint8_t)monster_zone(m);
        m->frame = 0;
        m->anim_timer = 2;
        m->facing = 0;
        m->hp = -2;
        m->timer = timer;
        if (timer == 0)
            monster_puff(m);
        if ((type->fire & 0x0F) != 0)
            m->fire_timer = (int16_t)((int16_t)(int8_t)(16 - (type->fire & 0x0F)) * 4);
        m->mode = 0;
        m->goal = -1;
        m->vx = 0;
        m->vy = 0;
        m->new_target = 1;
        m->unused_24 = 0;
        m->rank = 0;
        m->reached = 0;
        m->speed = 2;
        m->unused_12 = -1;
        m->item = -1;
        timer = (int16_t)(timer + type->spacing);
        m++;
    } while (--remaining != -1);
    rec->state--;
    rec->dropped = 0;
    rec->special = -1;
    if (type->flags & 0x100) {
        int16_t count = type->count;
        uint16_t value = random_word();
        uint16_t step = count > 0 ? monster_divide(0xFFFF, (uint16_t)count) : 0xFFFF;
        uint16_t limit = step;
        int16_t k = 0;

        while (!(value < limit)) {
            limit = (uint16_t)(limit + step);
            k++;
        }
        rec->special = (int8_t)k;
    }
    rec->chain = 0;
    rec->mask = 0;
    rec->phase = 0;
    rec->field_6 = type->delay;
}

/* orig $008A98: a record that runs (state 1): its monsters one after the other (monster_index counts them on): free ones (hit points 0) are counted (free_monster_count), the ones that are not there
 * (negative) are monster_dying, the others monster_step.  Then the chain may be started (monster_recruit); when none is left (free or dying) the group's drop is made where the last one
 * died (once) and a record in which all are free is free again */
static void monster_run(struct Monster *rec, struct MonsterPart *block)
{
    const struct MonsterType *type = rec->type;
    struct MonsterPart *m = block;
    int16_t remaining = (int16_t)(type->count & 0xFF);

    remaining--;
    guest.free_monster_count = 0;
    guest.dying_count = 0;
    guest.monster_index = 0;
    do {
        if (m->hp < 0)
            monster_dying(rec, type, m, block, remaining);
        else if (m->hp == 0)
            guest.free_monster_count++;
        else
            monster_step(rec, type, m, block);
        guest.monster_index++;
        m++;
    } while (--remaining != -1);
    rec->alive = (int8_t)(type->count - guest.free_monster_count - guest.dying_count);
    monster_recruit(rec, type, block);
    if ((uint8_t)(guest.free_monster_count + guest.dying_count) == (uint8_t)type->count && rec->dropped == 0) {
        if (type->item >= 0)
            drop_thing((int16_t)(guest.last_death_x + 8), (int16_t)(guest.last_death_y + 8), type->item);
        rec->dropped = 1;
    }
    if ((int8_t)guest.free_monster_count >= type->count)
        rec->state = 0;
}

/* orig $0088DC (the entry 5 of the table at $18714): the entry `a0` (its word +2 numbers a monster type of the level's file, 22 bytes each from monster_types) is taken out of use (its first word
 * -1) and the first free one of the ten monster records is to start (state 2) with that type; when none is free that is all */
void take_monster_record(struct EntryRef *a0)
{
    struct MonsterType *type = (struct MonsterType *)(guest.monster_types + (int16_t)a0->number * 22);

    a0->state = 0xFFFF;
    for (int i = 0; i < 10; i++) {
        if (guest.monster_records[i].state == 0) {
            guest.monster_records[i].state = 2;
            guest.monster_records[i].type = type;
            return;
        }
    }
}

/* orig $008898 (the entry 4 of the table at $18714): the same for the groups of walkers: the type is 50 on in the level's file (at monster_types + $44C), the record one of the ten at $12A0 */
void take_walker_group_record(struct EntryRef *a0)
{
    struct MonsterType *type = (struct MonsterType *)(guest.monster_types + 0x44c + (int16_t)a0->number * 22);
    struct WalkGroup *group = (struct WalkGroup *)guest.records_12a0;

    a0->state = 0xFFFF;
    for (int i = 0; i < 10; i++, group++) {
        if (group->state == 0) {
            group->state = 2;
            group->type = type;
            return;
        }
    }
}

/* orig $008A54: the ten records of 428 bytes at $1354 (struct Monster), with blocks of 400 bytes (ten struct MonsterPart) at ptr_0cee: a record that is free (state 0) is passed by, one
 * that runs (1) is monster_run, one to start (any other) monster_start; value_1296 is set to $121C first (the list monster_pickup looks at) */
void update_monsters(void)
{
    guest.flag_244 = 0;
    guest.value_1296 = (uint32_t)(uintptr_t)guest.list_121c;
    for (int i = 0; i < 10; i++) {
        struct MonsterPart *block = (struct MonsterPart *)(guest.ptr_0cee + 400 * i);

        guest.temp_bb6 = (uint8_t *)block;
        if (guest.monster_records[i].state == 0)
            continue;
        if (guest.monster_records[i].state == 1)
            monster_run(&guest.monster_records[i], block);
        else
            monster_start(&guest.monster_records[i], block);
    }
}

/* ======== the walking groups: $9AE6, the walkers that follow the player along the floors of the level, and the path planner they ask ($A492) ======== */

/* the step the vertical speed (an index, twice it the byte offset the original uses) goes: $A33A on, 8 down (falling) to $A358 (0) then up (negative) */
static const int16_t walk_steps[31] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 2, 1, 1, 0, -1, -1, -2, -4, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8
};

static int16_t walk_step(int16_t index)
{
    return walk_steps[15 + index];
}

/* orig $009EB2: where the object a walker carries is drawn from its picture (two bytes, x and y, for each of the twenty pictures; forty bytes for each of the levels 2, 3 and 4) */
static struct PlanItem *plan_tail;   /* a6 of the original: where the next place of plan_list goes */

/* orig $00A872: the cell of the planner's map of the places (ptr_03ac, 20 bytes a row) that the level's point (x, y) is in */
static uint8_t *plan_target(int16_t x, int16_t y)
{
    int16_t across = (int16_t)(((int16_t)(x - guest.scroll_x) >> 5) + 5);
    int16_t row = (int16_t)((int16_t)(y - guest.scroll_y) >> 4);

    across = (int16_t)(across + (int16_t)(row << 2));
    across = (int16_t)(across + (int16_t)(row << 4));
    return guest.ptr_03ac + (int16_t)(across + 0x50);
}

/* orig $00A59A: a place that the planner is to look at is put at the first free of the 32 places of plan_queue (one with x and y 0 is free); with 32 waiting nothing is done */
static void plan_push(int16_t x, int16_t y, int16_t dir, int16_t cost)
{
    if (guest.planner.queued >= 32)
        return;
    for (int i = 0; i < 32; i++) {
        if (guest.plan_queue[i].x == 0 && guest.plan_queue[i].y == 0) {
            guest.plan_queue[i].x = x;
            guest.plan_queue[i].y = y;
            guest.plan_queue[i].cost = cost;
            guest.plan_queue[i].dir = dir;
            guest.planner.queued++;
            return;
        }
    }
}

/* orig $00AC74: from (x, y), standing on the floor, the walk to the right along it: each cell of 32 pixels is put in plan_list with what it cost (up to the type's byte 13) until a wall, the
 * edge of the screen or a cell that has been reached for less; a walk that ends in the air is kept in pending[0] (where it falls from), one that reaches a place the planner is looking for
 * is the way to it */
static void plan_right(const struct MonsterType *type, int16_t x, int16_t y)
{
    const uint8_t *cell = map_cell(x, y);
    uint8_t *target = plan_target(x, y) + 0x14;
    int16_t cost = guest.planner.cost;
    int16_t sx;
    int16_t dir;

    if (cell[0x100] != 1)
        return;
    plan_tail = guest.plan_list;
    if ((int8_t)cost > (int8_t)*target)
        return;
    sx = (int16_t)(x - guest.scroll_x);
    dir = guest.planner.dir;
    if (cost == 0)
        dir = 1;
    for (;;) {
        plan_tail->x = x;
        plan_tail->y = y;
        plan_tail->cost = cost;
        plan_tail->dir = dir;
        plan_tail++;
        *target = (uint8_t)cost;
        guest.planner.visited++;
        cost++;
        if ((int8_t)cost >= (int8_t)type->zone_chance)
            return;
        target++;
        x = (int16_t)(x + 32);
        sx = (int16_t)(sx + 32);
        if (sx >= 0x160)
            return;
        cell++;
        if (cell[0] == 1 || cell[0x80] == 1)
            return;
        if (*target != 0) {
            if ((int8_t)*target < 0) {   /* a place the planner looks for */
                int16_t first = guest.planner.dir;

                guest.planner.visited = 0;
                if (guest.planner.cost == 0)
                    first = 1;
                if ((uint16_t)cost < guest.planner.best) {
                    guest.planner.result = first;
                    guest.planner.best = (uint16_t)cost;
                }
                return;
            }
            if ((int8_t)cost >= (int8_t)*target)
                return;
        }
        if (cell[0x100] == 1)
            continue;
        guest.planner.pending[0].x = x;
        guest.planner.pending[0].y = y;
        guest.planner.pending[0].cost = cost;
        guest.planner.pending[0].dir = guest.planner.cost == 0 ? 1 : guest.planner.dir;
        return;
    }
}

/* orig $00AD36: the same to the left (the cell it stands on is not put in the list again) */
static void plan_left(const struct MonsterType *type, int16_t x, int16_t y)
{
    const uint8_t *cell = map_cell(x, y);
    uint8_t *target = plan_target(x, y) + 0x14;
    int16_t cost = guest.planner.cost;
    int16_t sx;
    int16_t dir;
    int first_step = 1;

    (void)type;
    if (cell[0x100] != 1)
        return;
    if ((int8_t)cost > (int8_t)*target)
        return;
    sx = (int16_t)(x - guest.scroll_x);
    dir = guest.planner.dir;
    if (cost == 0)
        dir = 0;
    for (;;) {
        if (!first_step) {
            plan_tail->x = x;
            plan_tail->y = y;
            plan_tail->cost = cost;
            plan_tail->dir = dir;
            plan_tail++;
            *target = (uint8_t)cost;
            guest.planner.visited++;
        }
        first_step = 0;
        cost++;
        target--;
        x = (int16_t)(x - 32);
        sx = (int16_t)(sx - 32);
        if (sx <= -64)
            return;
        cell--;
        if (cell[0] == 1 || cell[0x80] == 1)
            return;
        if (*target != 0) {
            if ((int8_t)*target < 0) {
                int16_t first = guest.planner.dir;

                guest.planner.visited = 0;
                if (guest.planner.cost == 0)
                    first = 0;
                if (guest.planner.result < 0 || cost < (int16_t)guest.planner.best) {
                    guest.planner.result = first;
                    guest.planner.best = (uint16_t)cost;
                }
                return;
            }
            if ((int8_t)cost >= (int8_t)*target)
                return;
        }
        if (cell[0x100] == 1)
            continue;
        guest.planner.pending[1].x = x;
        guest.planner.pending[1].y = y;
        guest.planner.pending[1].cost = cost;
        guest.planner.pending[1].dir = guest.planner.cost == 0 ? 0 : guest.planner.dir;
        return;
    }
}

/* orig $00ABFC: the two walks that ended in the air (pending) go on down to the floor below (16 pixels a cell, a cost each): the place it comes to is put in the queue, or is the way to the
 * player when it is one the planner looks for */
static void plan_falls(const struct MonsterType *type)
{
    for (int i = 0; i < 2; i++) {
        const struct PlanItem *p = &guest.planner.pending[i];
        int16_t x = p->x;
        int16_t y = p->y;
        int16_t cost = p->cost;
        int16_t sy;
        const uint8_t *cell;
        uint8_t *target;

        if (p->x == 0 && p->y == 0)
            continue;
        cell = map_cell(x, y);
        target = plan_target(x, y) + 0x14;
        sy = (int16_t)(y - guest.scroll_y);
        for (;;) {
            sy = (int16_t)(sy + 0x10);
            if (sy >= 0xc0)
                break;
            y = (int16_t)(y + 0x10);
            cost++;
            cell += 0x80;
            target += 0x14;
            if (cell[0x100] == 1)
                break;
        }
        if (sy >= 0xc0)
            continue;
        if ((int8_t)cost > (int8_t)type->zone_chance)
            continue;
        if (*target != 0) {
            if ((int8_t)*target < 0) {
                if ((uint16_t)cost > guest.planner.best)
                    continue;
                guest.planner.result = p->dir;
                guest.planner.best = (uint16_t)cost;
                continue;
            }
            if ((int8_t)cost >= (int8_t)*target)
                continue;
        }
        *target = (uint8_t)cost;
        plan_push(x, y, p->dir, cost);
    }
}

/* orig $00AA74: a place a jump passes (the jump's cost so far `jump`, half of it counted) is the way to the player when it is one the planner looks for and costs less than the best way */
static void plan_probe(const struct PlanItem *entry, const struct MonsterType *type, int16_t x, int16_t y, int16_t jump)
{
    int16_t cost = (int16_t)((jump >> 1) + entry->cost);
    int16_t sx = (int16_t)(x - guest.scroll_x);
    int16_t sy = (int16_t)(y - guest.scroll_y);

    if ((int8_t)cost > (int8_t)type->zone_chance)
        return;
    if (sx <= -64 || sx >= 0x160 || sy <= -64 || sy >= 0xe0)
        return;
    if ((int8_t)*(plan_target(x, y) + 0x14) >= 0)
        return;
    if ((uint16_t)cost > guest.planner.best)
        return;
    guest.planner.result = entry->cost == 0 ? guest.planner.label : entry->dir;
    guest.planner.best = (uint16_t)cost;
}

/* orig $00A898 (to the right) and $00A98A (to the left): a jump from (x, y) with the speed index `speed` (7 or 13, the original's twice it): 4 pixels a step sideways (not into a wall) and up and
 * down by the steps of walk_steps; at the top of the arc the place is looked at (plan_probe), and when it comes to a floor the place is looked at again and the jump's cost (every step a
 * cost, 8 more for each cell it falls on) is kept in planner.jump.  A jump that hits its head goes on falling.  (x, y) is where it comes down */
static void plan_jump(const struct PlanItem *entry, const struct MonsterType *type, int right, int16_t *px, int16_t *py, int16_t speed)
{
    int16_t x = *px;
    int16_t y = *py;
    const uint8_t *cell = map_cell(x, y);
    int16_t sx = (int16_t)(x & 0x1f);
    int16_t sy = (int16_t)(y & 0xf);
    int16_t v = (int16_t)(speed << 1);
    int16_t cost = 0;

    for (;;) {
        cost++;
        if (v >= -10) {
            int blocked;

            if (right)
                blocked = sx == 0 && (cell[1] == 1 || cell[0x81] == 1);
            else
                blocked = sx == 0 && (cell[-1] == 1 || cell[0x7f] == 1);
            if (!blocked) {
                if (right) {
                    x = (int16_t)(x + 4);
                    sx = (int16_t)((sx + 4) & 0x1f);
                    if (sx == 0)
                        cell++;
                } else {
                    x = (int16_t)(x - 4);
                    sx = (int16_t)(sx - 4);
                    if (sx < 0) {
                        cell--;
                        sx = 0x1c;
                    }
                }
            }
        }
        if (v <= 0) {
            if (v == 0)
                plan_probe(entry, type, x, y, cost);
            if (cell[0x100] == 1 || (sx >= 8 && cell[0x101] == 1)) {   /* it is on a floor */
                y = (int16_t)(y & 0xfff0);
                if (right) {
                    if (sx != 0) {
                        x = (int16_t)((x & 0xffe0) + 0x20);
                        cell++;
                    }
                } else if (sx != 0) {
                    x = (int16_t)(x & 0xffe0);
                }
                plan_probe(entry, type, x, y, cost);
                while (cell[0x100] != 1) {
                    cell += 0x80;
                    y = (int16_t)(y + 0x10);
                    cost = (int16_t)(cost + 8);
                }
                guest.planner.jump = cost;
                *px = x;
                *py = y;
                return;
            }
        }
        {   /* in the air */
            int16_t step = walk_step((int16_t)(v >> 1));

            y = (int16_t)(y + step);
            sy = (int16_t)(sy + step);
            if (sy < 0) {
                sy = (int16_t)(sy + 0x10);
                cell -= 0x80;
            } else if (sy > 0xf) {
                sy = (int16_t)(sy - 0x10);
                cell += 0x80;
            }
            if (cell[0] == 1 || (sx >= 8 && cell[1] == 1)) {   /* the head hits something */
                y = (int16_t)(y & 0xfff0);
                sy = 0;
                y = (int16_t)(y + 0x10);
                cell += 0x80;
                cost = (int16_t)(cost + ((v + 0xc) >> 1));
                v = -10;
            }
            v = (int16_t)(v - 2);
            if (v < -12)
                v = -10;
        }
    }
}

/* orig $00AB78: where a jump came down (x, y) is a place of the planner when it costs little enough (the jump's cost a quarter): put in the queue with the first step of the way that led to
 * the jump, or the way to the player when it is one the planner looks for.  The place the walker is at is not */
static void plan_land(const struct PlanItem *entry, const struct MonsterType *type, const struct WalkMember *m, int16_t x, int16_t y)
{
    int16_t cost = (int16_t)((guest.planner.jump >> 2) + entry->cost);
    int16_t sx = (int16_t)(x - guest.scroll_x);
    int16_t sy = (int16_t)(y - guest.scroll_y);
    uint8_t *target;

    if ((int8_t)cost > (int8_t)type->zone_chance)
        return;
    if (sx <= -64 || sx >= 0x160 || sy <= -64 || sy >= 0xe0)
        return;
    target = plan_target(x, y) + 0x14;
    if (*target != 0) {
        if ((int8_t)*target < 0) {
            if ((uint16_t)cost > guest.planner.best)
                return;
            guest.planner.result = entry->cost == 0 ? guest.planner.label : entry->dir;
            guest.planner.best = (uint16_t)cost;
            return;
        }
        if ((int8_t)cost >= (int8_t)*target)
            return;
    }
    if (x == m->x && y == m->y)
        return;
    *target = (uint8_t)cost;
    plan_push(x, y, entry->cost == 0 ? guest.planner.label : entry->dir, cost);
}

/* orig $00AADA: from every place the walks along the floor found, the four jumps (to the right and left; the second pair, with the greater speed, when the type's byte 12 is above 4):
 * not from a place with a wall over it */
static void plan_jumps(const struct MonsterType *type, const struct WalkMember *m)
{
    int16_t count = guest.planner.visited;

    if (count == 0)
        return;
    for (int16_t n = 0; n < count; n++) {
        const struct PlanItem *entry = &guest.plan_list[n];
        int16_t x, y;

        if (entry->x == 0 && entry->y == 0)
            continue;
        guest.planner.label = 3;
        if (map_cell(entry->x, entry->y)[-0x80] == 1)
            continue;
        x = entry->x;
        y = entry->y;
        guest.planner.jump = 0;
        plan_jump(entry, type, 1, &x, &y, 7);
        plan_land(entry, type, m, x, y);
        if ((int8_t)type->limit_c > 4) {
            guest.planner.label = 7;
            x = entry->x;
            y = entry->y;
            guest.planner.jump = 0;
            plan_jump(entry, type, 1, &x, &y, 0xd);
            plan_land(entry, type, m, x, y);
        }
        x = entry->x;
        y = entry->y;
        guest.planner.label = 2;
        guest.planner.jump = 0;
        plan_jump(entry, type, 0, &x, &y, 7);
        plan_land(entry, type, m, x, y);
        if ((int8_t)type->limit_c > 4) {
            guest.planner.label = 6;
            x = entry->x;
            y = entry->y;
            guest.planner.jump = 0;
            plan_jump(entry, type, 0, &x, &y, 0xd);
            plan_land(entry, type, m, x, y);
        }
    }
}

/* orig $00A54A: all the ways from (x, y): along the floor to the right and left, down from where they end, and the jumps from the places along the floor */
static void plan_look(const struct MonsterType *type, const struct WalkMember *m, int16_t x, int16_t y)
{
    guest.planner.visited = 0;
    guest.planner.pending[0].x = guest.planner.pending[0].y = guest.planner.pending[0].cost = guest.planner.pending[0].dir = 0;
    guest.planner.pending[1].x = guest.planner.pending[1].y = 0;
    plan_right(type, x, y);
    plan_left(type, x, y);
    plan_falls(type);
    plan_jumps(type, m);
}

/* orig $00A844: the cells of the planner's map at the screen point (x, y) and the two under it are marked as places to come to */
static void plan_mark_column(int16_t sx, int16_t sy)
{
    int16_t across = (int16_t)((sx >> 5) + 5);
    int16_t row = (int16_t)(sy >> 4);
    uint8_t *cell;

    across = (int16_t)(across + (int16_t)(row << 2));
    across = (int16_t)(across + (int16_t)(row << 4));
    cell = guest.ptr_03ac + (int16_t)(across + 0x50);
    cell[0] = 0xff;
    cell[0x14] = 0xff;
    cell[0x28] = 0xff;
}

/* orig $00A6A2: the things of the list at $121C are marked in the planner's map (the first twenty); true when one was */
static int plan_mark_things(void)
{
    int16_t count = (int16_t)guest.list_count_1294;
    const int16_t *list = (const int16_t *)guest.list_121c;

    if (count == 0)
        return 0;
    if (count > 0x14)
        count = 0x14;
    guest.flags_3b0 &= (uint8_t)~8;
    for (int16_t i = 0; i < count; i++)
        mark_target_cells(list[i * 3], list[i * 3 + 1]);
    guest.planner.flag = 2;
    return (guest.flags_3b0 & 8) != 0;
}

/* orig $00A726 (the lists at $A780: the places of the levels 1, 2, 3 and 4 (the same list), x and y of a cell of 32 by 16 pixels, ended by -1): the first place that is on the screen is the aim */
static void plan_mark_place(void)
{
    static const int16_t places_1[] = { 8, 25, 6, 31, 36, 51, 43, 58, 72, 38, 83, 19, 91, 25, 99, 11, 99, 5, -1, -1 };
    static const int16_t places_2[] = { 121, 6, 125, 17, 125, 55, 86, 58, 100, 56, 36, 43, 35, 52, 45, 59, 50, 59, 52, 59, 55, 59, 55, 25, 73, 36, 72, 49, 75, 49, 73, 59, 68, 59, 85, 12, 40, 4, -1, -1 };
    static const int16_t places_3[] = { 4, 23, 12, 23, 15, 23, 6, 42, 10, 60, 17, 60, 22, 60, 24, 49, 32, 49, 30, 16, 38, 16, 36, 10, 39, 27, -1, -1 };
    static const int16_t *const lists[4] = { places_1, places_2, places_3, places_3 };
    const int16_t *place = lists[(guest.param_index - 1) & 3];

    for (;; place += 2) {
        if (place[1] < 0)
            return;
        int16_t sx = (int16_t)((place[0] << 5) - guest.scroll_x);
        int16_t sy = (int16_t)(((place[1] + 1) << 4) - guest.scroll_y);

        if (sx <= -64 || sx >= 0x160 || sy <= -64 || sy >= 0xe0)
            continue;
        guest.planner.flag = 1;
        plan_mark_column(sx, sy);
        return;
    }
}

/* orig $00A5C4: the planner's map of the places that are the aim is made: what the type's byte 8 says (its two nibbles, the high one in place of the low with the chance of its byte 9 of 128):
 * 0 the player (the cell he is in and the two under it, flag 0), 1 the cells at 64 pixels round him (a row of eight above and below, a column of eight at either side), 2 the things of
 * the list at $121C (flag 2), 3 a place of the level's list (flag 1).  With the flag $25E (things to be taken) set the things come first; when none can be marked the other way goes on */
static void plan_setup(const struct MonsterType *type)
{
    uint8_t ways[2];
    uint8_t first, second;

    for (int i = 0; i < 400; i++)
        guest.ptr_03ac[i] = 0;
    if (guest.flag_25e != 0 && plan_mark_things())
        return;
    first = (uint8_t)(type->mode & 0xf);
    second = (uint8_t)(type->mode >> 4);
    if ((int8_t)type->mode_chance < (int8_t)(random_word() & 0x7f)) {
        uint8_t t = first;

        first = second;
        second = t;
    }
    ways[0] = first;
    ways[1] = second;
    for (int i = 0; i < 4; i++) {
        switch (ways[i & 1]) {
        case 0:
            guest.planner.flag = 0;
            plan_mark_column((int16_t)(guest.player_x - guest.scroll_x), (int16_t)(guest.player_y - guest.scroll_y));
            return;
        case 1: {
            int16_t px = (int16_t)(guest.player_x - guest.scroll_x);
            int16_t py = (int16_t)(guest.player_y - guest.scroll_y);

            for (int16_t d1 = -64; d1 <= 64; d1 = (int16_t)(d1 + 128))
                for (int16_t d0 = -64; d0 < 64; d0 = (int16_t)(d0 + 16))
                    mark_target_cells((int16_t)(d0 + px), (int16_t)(d1 + py));
            for (int16_t d0 = -64; d0 <= 64; d0 = (int16_t)(d0 + 128))
                for (int16_t d1 = -64; d1 < 64; d1 = (int16_t)(d1 + 16))
                    mark_target_cells((int16_t)(d0 + px), (int16_t)(d1 + py));
            return;
        }
        case 2:
            if (plan_mark_things())
                return;
            break;   /* none: the other way (the original swaps the nibbles) */
        default:
            plan_mark_place();
            return;
        }
    }
}

/* orig $00A492: the first step of the best way for the walker m to the aims of the planner's map, in planner.result (negative: none) and what it costs in planner.best: along the floors, down
 * from their ends and by jumps, the places that cost least first, until a way of 16 or less is found */
static void plan_run(const struct MonsterType *type, const struct WalkMember *m)
{
    int16_t sx = (int16_t)(m->x - guest.scroll_x);
    int16_t sy = (int16_t)(m->y - guest.scroll_y);
    struct PlanItem *item = guest.plan_queue;
    int taken = 0;

    guest.planner.result = -1;
    guest.planner.best = 0xffff;
    if (sx <= -64 || sx >= 0x160 || sy <= -64 || sy >= 0xe0)
        return;
    for (int i = 0; i < 32; i++)
        guest.plan_queue[i].x = guest.plan_queue[i].y = guest.plan_queue[i].cost = guest.plan_queue[i].dir = 0;
    guest.planner.queued = 0;
    plan_setup(type);
    guest.planner.cost = 0;
    plan_look(type, m, m->x, m->y);
    if (guest.planner.best <= 0x10)
        return;
    for (;;) {
        if (guest.planner.queued == 0)
            return;
        if (item->x == 0 && item->y == 0) {
            item++;
            taken++;
            if (taken >= 0x20) {
                item = guest.plan_queue;
                taken = 0;
            }
            continue;
        }
        {
            struct PlanItem here = *item;

            item->x = item->y = item->cost = item->dir = 0;
            item++;
            guest.planner.queued--;
            taken++;
            guest.planner.cost = here.cost;
            guest.planner.dir = here.dir;
            plan_look(type, m, here.x, here.y);
        }
        if (guest.planner.best <= 0x10)
            return;
        if (taken >= 0x20) {
            item = guest.plan_queue;
            taken = 0;
        }
    }
}

/* orig $009F2A: the picture of a walker: the template's picture (the first word of the 16 bytes of the type's byte 7, in ptr_0b82), the way of walking's base (walk_modes_99dc) and its step */
static int16_t walker_picture(const struct MonsterType *type, const struct WalkMember *m)
{
    int16_t first = *(const int16_t *)(guest.ptr_0b82 + 16 * (uint16_t)type->picture);

    /* (the table is at its address in the program's memory as the original has it, the longs in the order of the 68000: they are read by their bytes) */
    const uint8_t *base = (const uint8_t *)&guest.walk_modes_99dc[m->mode & 7].picture_base;
    int16_t picture_base = (int16_t)((base[2] << 8) | base[3]);

    return (int16_t)(picture_base + m->frame + first);
}

/* orig $00A122: the way the planner found (planner.result: 0 and 1 walk left and right, 2 and above jump: 2 and 3 to the left and right, 4 and 5 and then 6 and 7 the high ones; the speed of the
 * jump 7, 9 or 13) sets the mode and the vertical speed of the walker */
static void walker_go(struct WalkMember *m)
{
    int16_t way = guest.planner.result;
    int16_t speed;

    if (way <= 1) {
        m->mode = way;
        return;
    }
    way = (int16_t)(way - 2);
    m->mode = (int16_t)((way & 1) + 4);
    way = (int16_t)(way & 0xfffe);
    speed = 7;
    if (way != 0) {
        speed = 9;
        way = (int16_t)(way - 2);
        if (way != 0)
            speed = 0xd;
    }
    m->vy = speed;
    m->frame = 0;
}

/* orig $009F70 (to the left) and $009F7A: a walker that is on a cell's edge (its x with the bits 2 to 4 clear; $9F7A is the entry that does not look at it) and on a floor asks the planner for the
 * way: none that it can take (negative): it paces by the walls (it turns round at one); the way 0 goes on; another is a jump (walker_go).  With no floor under it it falls (mode 2).  Between the
 * edges it keeps walking: a step of 4 pixels every `timer` frames (10 less its speed) and a picture */
static int16_t walker_left(const struct MonsterType *type, struct WalkMember *m, int entry2)
{
    const uint8_t *cell;

    if (!entry2 && (m->x & 0x1c) != 0)
        goto keep_going;
    cell = map_cell(m->x, m->y);
    if (cell[0x100] != 1) {
        m->mode = 2;
        m->vy = 1;
        m->y = (int16_t)(m->y + 1);
        m->frame = 0;
        return walker_picture(type, m);
    }
    plan_run(type, m);
    if ((int16_t)guest.planner.result < 0)
        goto no_way;
    if (guest.planner.result == 0)
        goto keep_going;
    walker_go(m);
    return walker_picture(type, m);
no_way:
    cell = map_cell(m->x, m->y);
    if (cell[-1] != 1 && cell[0x7f] != 1 && cell[0xff] == 1)
        goto keep_going;
    if (cell[1] == 1 || cell[0x81] == 1 || cell[0x101] != 1)
        goto turn;
    m->mode = 1;
    return walker_picture(type, m);
turn:
    m->mode = 0;
    m->vy = 0;
    if (--m->timer >= 0)
        return walker_picture(type, m);
    m->x = (int16_t)(m->x + 4);
    goto step;
keep_going:
    m->mode = 0;
    m->vy = 0;
    if (--m->timer >= 0)
        return walker_picture(type, m);
step:
    m->timer = (int16_t)(int8_t)(10 - type->limit_b);
    m->frame = (int16_t)((m->frame + 1) & 7);
    m->x = (int16_t)(m->x - 4);
    return walker_picture(type, m);
}

/* orig $00A046 (to the right) and $00A050: the same to the right */
static int16_t walker_right(const struct MonsterType *type, struct WalkMember *m, int entry2)
{
    const uint8_t *cell;

    if (!entry2 && (m->x & 0x1c) != 0)
        goto keep_going;
    cell = map_cell(m->x, m->y);
    if (cell[0x100] != 1) {
        m->mode = 3;
        m->vy = 1;
        m->y = (int16_t)(m->y + 1);
        m->frame = 0;
        return walker_picture(type, m);
    }
    plan_run(type, m);
    if ((int16_t)guest.planner.result < 0)
        goto no_way;
    if (guest.planner.result == 1)
        goto keep_going;
    walker_go(m);
    return walker_picture(type, m);
no_way:
    cell = map_cell(m->x, m->y);
    if (cell[1] != 1 && cell[0x81] != 1 && cell[0x101] == 1)
        goto keep_going;
    if (cell[-1] != 1 && cell[0x7f] != 1 && cell[0xff] == 1) {
        m->mode = 0;
        return walker_picture(type, m);
    }
    m->mode = 1;
    m->vy = 0;
    if (--m->timer >= 0)
        return walker_picture(type, m);
    m->x = (int16_t)(m->x - 4);
    goto step;
keep_going:
    m->mode = 1;
    m->vy = 0;
    if (--m->timer >= 0)
        return walker_picture(type, m);
step:
    m->timer = (int16_t)(int8_t)(10 - type->limit_b);
    m->frame = (int16_t)((m->frame + 1) & 7);
    m->x = (int16_t)(m->x + 4);
    return walker_picture(type, m);
}

/* orig $00A194 (to the left, mode 4) and $00A28A (to the right, mode 5): a jump: every few frames (2 less the speed's quarter) the walker goes 4 pixels sideways (not into a wall, on the edge of
 * a cell), and up or down by the step of its vertical speed (m->vy: it counts down); it comes to rest on a floor (mode 0 and 1) and falls when the speed is -6 (mode 2 and 3); against a ceiling it
 * goes down */
static int16_t walker_jump(const struct MonsterType *type, struct WalkMember *m, int right)
{
    const uint8_t *cell;
    int16_t pace;

    if (--m->timer >= 0)
        return walker_picture(type, m);
    pace = (int16_t)((uint8_t)type->limit_b >> 2);
    m->timer = (int16_t)(2 - pace);
    cell = walk_member_cell(m);
    if (right) {
        if ((m->x & 0x1f) != 0 || (cell[1] != 1 && cell[0x81] != 1))
            m->x = (int16_t)(m->x + 4);
    } else {
        if ((m->x & 0x1f) != 0 || (cell[-1] != 1 && cell[0x7f] != 1))
            m->x = (int16_t)(m->x - 4);
    }
    if (m->vy <= 0 && walk_cell_wall_below(m, cell) != 0) {
        m->mode = right ? 1 : 0;
        m->y = (int16_t)(m->y & 0xfff0);
        return walker_picture(type, m);
    }
    m->y = (int16_t)(m->y + walk_step(m->vy));
    cell = walk_member_cell(m);
    if (walk_cell_wall(m, cell) != 0) {
        m->y = (int16_t)((m->y & 0xfff0) + 0x10);
        m->mode = right ? 3 : 2;
        m->vy = 2;
        return walker_picture(type, m);
    }
    m->vy--;
    if (m->vy < -5) {
        m->mode = right ? 3 : 2;
        m->vy = 8;
    }
    return walker_picture(type, m);
}

/* orig $00A378 (falling to the left, mode 2) and $00A3E2 (to the right, mode 3): the picture's step is 0, the fall goes faster by one a frame to 16 pixels; on a floor (or on the edge of a cell over
 * one) the walker stands (mode 0 and 1), turned round at once when a wall is in front of it */
static int16_t walker_fall(const struct MonsterType *type, struct WalkMember *m, int right)
{
    int16_t picture;
    int16_t speed;
    const uint8_t *cell;

    m->frame = 0;
    picture = walker_picture(type, m);
    speed = m->vy;
    if (speed != 0x10)
        m->vy++;
    m->y = (int16_t)(m->y + speed);
    cell = walk_member_cell(m);
    if (cell[0x100] != 1 && !((m->x & 0x1f) >= 8 && cell[0x101] == 1))
        return picture;
    m->y = (int16_t)(m->y & 0xfff0);
    m->mode = right ? 1 : 0;
    if ((m->x & 0x1f) != 0)
        return picture;
    cell = walk_member_cell(m);
    if (right) {
        if (cell[1] == 1 || cell[0x81] == 1)
            m->mode = 0;
    } else if (cell[-1] == 1 || cell[0x7f] == 1) {
        m->mode = 1;
    }
    return picture;
}

/* orig $0099DC (the table): the movement of a walker by its mode (0 and 6 walk left, 1 and 7 walk right, 2 and 3 fall, 4 and 5 jump) */
static int16_t walker_move(const struct MonsterType *type, struct WalkMember *m)
{
    switch (m->mode & 7) {
    case 0:
        return walker_left(type, m, 0);
    case 1:
        return walker_right(type, m, 0);
    case 2:
        return walker_fall(type, m, 0);
    case 3:
        return walker_fall(type, m, 1);
    case 4:
        return walker_jump(type, m, 0);
    case 5:
        return walker_jump(type, m, 1);
    case 6:
        return walker_left(type, m, 1);
    default:
        return walker_right(type, m, 1);
    }
}

/* orig $009CB8: where the object that a walker carries is drawn: from the walker's place by the offsets of its picture (walk_item_offsets: forty bytes for each of the levels 2 to 4) */
static void walker_item_place(const struct MonsterType *type, int16_t picture, int16_t *x, int16_t *y)
{
    int16_t first = *(const int16_t *)(guest.ptr_0b82 + 16 * (uint16_t)type->picture);
    int16_t index = (int16_t)(((int16_t)(picture - first) << 1) + (int16_t)((guest.param_index - 2) * 40));
    const uint8_t *offsets;

    if (index < 0 || index > 118)
        index = 0;   /* (the first level has no such table) */
    offsets = &walk_item_offsets[index];

    *x = (int16_t)(*x + (int8_t)offsets[0]);
    *y = (int16_t)(*y + (int8_t)offsets[1]);
}

/* orig $009CF0: a walker that carries nothing takes up one of the things of the list at $121C (when the planner's flag says things are the aim, 2) that the type takes (bits 0 and 1 of its
 * flags for the common ones, 2 and 3 for the ones from kind $C0 on) when it is near (to 48 pixels up and left, 16 right and down): the record of that thing in records_19b14 is the walker's */
static void walker_pickup(const struct MonsterType *type, struct WalkMember *m)
{
    const int16_t *list = (const int16_t *)guest.list_121c;
    int16_t count = (int16_t)guest.list_count_1294;
    int16_t x, y;

    if (guest.planner.flag != 2 || count == 0 || m->item >= 0)
        return;
    if (count > 0x14)
        count = 0x14;
    x = (int16_t)(m->x - 8);
    y = (int16_t)(m->y - 8);
    for (int16_t n = 0; n < count; n++, list += 3) {
        uint16_t takes = type->flags;
        int16_t ox, oy;

        if (list[2] >= 0xC0)
            takes >>= 2;
        if ((takes & 3) == 0 || list[2] == 0x45)
            continue;
        ox = (int16_t)(list[0] + 8 + guest.scroll_x);
        oy = (int16_t)(list[1] + 8 + guest.scroll_y);
        if (x > ox || y > oy)
            continue;
        ox = (int16_t)(ox - 0x30);
        oy = (int16_t)(oy - 0x30);
        if (x < ox || y < oy)
            continue;
        ox = (int16_t)(list[0] + guest.scroll_x);
        oy = (int16_t)(list[1] + guest.scroll_y);
        for (int i = 199; i >= 0; i--) {   /* the records of the things are looked at from the last one */
            struct Slot8 *record = &guest.records_19b14[i];

            if (record->w[3] == 0 || record->w[2] < 0)
                continue;
            if (record->w[2] != list[2] || record->w[0] != ox || record->w[1] != oy)
                continue;
            m->item_x = ox;
            m->item_y = oy;
            m->item = list[2];
            record->w[2] = -1;
            record->w[3] = 0;
            return;
        }
        return;
    }
}

/* orig $009B90: a walker that is there (hp above 0): it shoots (monster_fire) and moves (walker_move; the picture only while the screen is frozen: timer_306), takes up a thing, is hit
 * (monster_hit: score when it dies) and drawn; and what it carries is drawn at its place (the glyph kind + 170, or kind - 192 + 302 from kind $C0) and put down, as a record, with the chance
 * 1 in 128 a frame (the type's flags bits 0 and 1 (2 and 3 for the high ones) say: 0 at once, 2 never) */
static void walker_step(const struct MonsterType *type, struct WalkMember *m)
{
    int16_t picture;
    int16_t sx, sy;

    if (guest.timer_306 == 0) {
        monster_fire(type, (struct MonsterPart *)m);
        picture = walker_move(type, m);
    } else {
        picture = walker_picture(type, m);
    }
    walker_pickup(type, m);
    sx = (int16_t)(m->x - guest.scroll_x);
    sy = (int16_t)(m->y - guest.scroll_y);
    monster_hit((struct MonsterPart *)m, sx, sy);
    if (m->hp < 0)
        add_kill_score(kill_value_9dda(type->life));
    sprite_draw(sx, sy, (uint16_t)picture);
    if (m->item < 0)
        return;

    int16_t ix = m->x;
    int16_t iy = m->y;

    walker_item_place(type, picture, &ix, &iy);
    m->item_x = ix;
    m->item_y = iy;
    ix = (int16_t)(ix - guest.scroll_x);
    iy = (int16_t)(iy - guest.scroll_y);
    if (m->item < 0xc0) {
        glyph_draw(ix, iy, (uint16_t)(m->item + 0xaa));
        if ((type->flags & 3) == 2)
            return;
        if ((type->flags & 3) != 0 && (random_word() & 0x7f) != 0)
            return;
        drop_record((int16_t)(m->x + 8), m->y, (int16_t)(m->item + 0xb));
    } else {
        glyph_draw(ix, iy, (uint16_t)(m->item - 0xc0 + 0x12e));
        if ((type->flags & 0xc) == 8)
            return;
        if ((type->flags & 0xc) != 0 && (random_word() & 0x7f) != 0)
            return;
        drop_record((int16_t)(m->x + 8), m->y, (int16_t)(m->item - 0xc0));
    }
    m->item = -1;
}

/* orig $009E30: a walker that is hit to death goes through seven pictures (the sprites 108 to 114); the first time its place is kept in last_death_x and last_death_y (where the group's drop goes) and
 * what it carried is dropped; at the end its hp is 0 */
static void walker_dies(struct WalkMember *m)
{
    guest.dying_count++;
    if (m->frame < 0) {
        guest.last_death_x = m->x;
        guest.last_death_y = m->y;
        if (m->item >= 0) {
            int16_t kind = m->item;

            kind = kind >= 0xc0 ? (int16_t)(kind - 0xc0) : (int16_t)(kind + 0x0b);
            drop_thing((int16_t)(m->x + 8), m->y, kind);
        }
    }
    m->frame++;
    if (m->frame >= 7) {
        m->hp = 0;
        m->timer = 0;
        return;
    }
    sprite_draw((int16_t)(m->x - guest.scroll_x), (int16_t)(m->y - guest.scroll_y), (uint16_t)(m->frame + 0x6c));
}

/* orig $009A20: a group that is to start: the type's bytes are set when the long at $F6C0 (the copylock's) is not 0, which is the failed lock (-DPIRATED: speeds 10, mode 0; the type keeps its own when the lock has passed), the walkers are put at the start cell with their
 * delays (they go up by the type's spacing), the first one shows up with a puff, they stand on the floor (mode 0 or 1 by the type's bit 9) or fall (2 or 3) and the group runs (state 1) */
static void walker_group_start(struct WalkGroup *group, struct WalkMember *m)
{
    struct MonsterType *type = group->type;
    int16_t count = (int16_t)type->count;
    int16_t delay = 0;

#ifdef PIRATED
    type->limit_b = 10;
    type->limit_c = 10;
    type->mode = 0;
#endif
    if (count - 1 < 0)
        return;
    for (int16_t k = 0; k < count; k++, m++) {
        int16_t cell_x = (int16_t)((type->start >> 8) & 0xff);
        int16_t cell_y = (int16_t)(type->start & 0xff);
        int16_t rate = (int16_t)(type->fire & 0xf);
        int16_t mode = 0;

        m->x = (int16_t)((cell_x << 5) & 0xffe0);
        m->y = (int16_t)((cell_y << 4) & 0xfff0);
        m->frame = 0;
        m->hp = -2;
        m->timer = delay;
        if (delay == 0)
            monster_puff((struct MonsterPart *)m);
        if (rate != 0)
            m->fire_timer = (int16_t)((int16_t)(int8_t)(0x10 - rate) << 2);
        if (walk_member_cell(m)[0x100] != 1)
            mode = 2;
        m->mode = (int16_t)(((type->flags >> 9) & 1) + mode);
        m->vy = 2;
        m->unused_12 = -1;
        m->item = -1;
        delay = (int16_t)(delay + type->spacing);
    }
    group->state--;
    group->dropped = 0;
}

/* orig $009B24: a group that runs: each walker is looked at (not there yet, waiting for its puff: hp -2 and -3, dying: other negative, there: walker_step); when all are gone or dying the thing the group
 * drops falls where the last one died; when they are all gone the group is free */
static void walker_group_run(struct WalkGroup *group, struct WalkMember *m)
{
    struct MonsterType *type = group->type;
    int16_t count = (int16_t)(uint8_t)type->count;

    guest.free_monster_count = 0;
    guest.dying_count = 0;
    for (int16_t k = 0; k < count; k++, m++) {
        if (m->hp < 0) {
            if (m->hp == -2) {
                if (--m->timer == 0)
                    monster_puff((struct MonsterPart *)m);
            } else if (m->hp == -3) {
                if (--m->timer == 0) {
                    m->hp = type->life;
                    m->timer = (int16_t)(int8_t)(10 - type->limit_b);
                }
            } else {
                walker_dies(m);
            }
        } else if (m->hp == 0) {
            guest.free_monster_count++;
        } else {
            walker_step(type, m);
        }
    }
    if ((int8_t)(guest.free_monster_count + guest.dying_count) == (int8_t)type->count) {
        if (group->dropped == 0) {
            if ((int16_t)type->item >= 0)
                drop_thing((int16_t)(guest.last_death_x + 8), guest.last_death_y, type->item);
            group->dropped = 1;
        }
    }
    if ((int8_t)guest.free_monster_count >= (int8_t)type->count)
        group->state = 0;
}

/* orig $009AE6: the ten groups of walkers (records of 18 bytes at $12A0, each with its walkers at asset_directory + 240 a record): the ones that run are walker_group_run, the others that are
 * not free are started (walker_group_start) */
void update_walker_groups(void)
{
    if (guest.code_9ae6 == 0x4E75)   /* (a guardian's start made the routine's first instruction an rts) */
        return;
    struct WalkGroup *group = (struct WalkGroup *)guest.records_12a0;
    uint8_t *block = guest.asset_directory;

    guest.flag_244 = -1;
    for (int i = 0; i < 10; i++, group++, block += 240) {
        guest.temp_bb6 = block;
        if (group->state == 0)
            continue;
        if (group->state == 1)
            walker_group_run(group, (struct WalkMember *)block);
        else
            walker_group_start(group, (struct WalkMember *)block);
    }
}

/* ======== the waves of enemies ("units"): $E486, with its four copies $E76C, $EA52, $ED38 and $F01E, and the routines the templates name ======== */

/* a routine of a template that is not written here (the address is the original's): the host says which, once */
static void unit_routine_missing(uint32_t address)
{
    host_missing_routine(address);
}

/* orig $008292: a member of a unit goes through its pictures (the low two bits of its picture count on, bit 3 is kept) and turns to face the player: the bit
 * is cleared when the player is to its right and set when he is to its left, one pixel's worth of dead zone either side */
static void unit_face(struct UnitMember *m)
{
    if (guest.frame_toggle == 0)
        return;
    int16_t picture = m->anim;
    int16_t turned = (int16_t)(picture & 8);

    picture = (int16_t)(((picture + 1) & 3) | turned);
    m->anim = picture;

    int16_t x = m->x;

    if (x == guest.creature_x)
        return;
    if (x > guest.creature_x) {   /* $82C8 */
        x--;
        if (x <= guest.creature_x)
            return;
        m->anim |= 8;
        return;
    }
    x++;
    if (x > guest.creature_x)
        return;
    m->anim &= (int16_t)~8;
}

/* orig $008274: the picture is one of four of the table `pictures`, plus 3 when it faces left */
static int16_t unit_move_four_pictures(struct UnitMember *m, const uint16_t *pictures)
{
    unit_face(m);
    int16_t picture = m->anim;
    int16_t left = (int16_t)(picture & 8);
    int16_t d2 = (int16_t)pictures[picture & 3];

    if (left != 0)
        d2 = (int16_t)(d2 + 3);
    return d2;
}

/* orig $008234: eight pictures from `base` on (four more when it faces left) */
static int16_t unit_move_eight_pictures(struct UnitMember *m, int16_t base)
{
    unit_face(m);
    int16_t picture = m->anim;
    int16_t d2 = (int16_t)((picture & 7) + base);

    if ((picture & 8) != 0)
        d2 = (int16_t)(d2 + 4);
    return d2;
}

/* orig $00818C: a shot goes out sideways from the member (to the left from 12 pixels before it, to the right from 27 after), at the speed its rate gives */
static void unit_shot_sideways(struct UnitMember *m)
{
    int16_t x = m->x;
    int16_t y = (int16_t)(m->y + 9);
    int16_t speed = (int16_t)((guest.unit_shot_rate >> 1) + 4);

    if ((m->anim & 8) == 0) {
        x = (int16_t)(x - 12);
        guest.shot_speed = (int16_t)-speed;
        enemy_shot(x, y, 0);
    } else {
        x = (int16_t)(x + 27);
        guest.shot_speed = speed;
        enemy_shot(x, y, 1);
    }
}

/* orig $0081D0: a member that shoots: with bit 2 of its picture set it goes through the pictures 4 to 7 (the shot goes out at the 6th), else as $8218 */
static int16_t unit_move_shooter(struct UnitMember *m)
{
    int16_t d2 = m->anim;

    if ((d2 & 4) != 0) {
        d2 &= 7;
        if (guest.frame_toggle != 0) {
            d2++;
            if (d2 == 6)
                unit_shot_sideways(m);
            if (d2 >= 8) {
                m->anim &= 8;
                goto done;
            }
        }
        m->anim &= 8;
        m->anim |= d2;
        d2 -= 4;
        if ((m->anim & 8) != 0)
            d2 += 4;
        return (int16_t)(d2 + 0x1FE);
    }
    unit_face(m);
done:;
    d2 = m->anim;
    int16_t d3 = d2;

    d2 &= 3;
    d3 &= 8;
    d3 >>= 1;
    d2 = (int16_t)(d2 + d3);
    return (int16_t)(d2 + 0x1F6);
}

/* orig $008116: a member that goes through twelve pictures (the ones from the fourth on once started, a thing is dropped at the ninth) */
static int16_t unit_move_twelve_pictures(struct UnitMember *m, int16_t x, int16_t y)
{
    if (guest.frame_phase == 0) {
        if (m->anim <= 3) {
            m->anim++;
            m->anim &= 3;
        } else {
            m->anim++;
            if (m->anim == 9)
                drop_falling_slot(x, y);   /* $E352 */
            if (m->anim >= 12)
                m->anim = 0;
        }
    }
    return (int16_t)(m->anim + 0x211);
}

/* orig $0082E8, $008308, $008328 and $008348: a member that goes through `pictures` pictures (one a frame_toggle) from `base` on */
static int16_t unit_move_cycle(struct UnitMember *m, int16_t pictures, int16_t base)
{
    if (guest.frame_toggle != 0) {
        m->anim++;
        if (m->anim >= pictures)
            m->anim = 0;
    }
    return (int16_t)(m->anim + base);
}

/* the movement routines of the templates for the members of a unit (the original's addresses): returns the picture */
static int16_t unit_move_routine(uint32_t routine, struct UnitMember *m, int16_t x, int16_t y)
{
    switch (routine) {
    case 0x8116:
        return unit_move_twelve_pictures(m, x, y);
    case 0x81D0:
        return unit_move_shooter(m);
    case 0x8250:
        return unit_move_eight_pictures(m, 0x209);
    case 0x8256:
        return unit_move_eight_pictures(m, 0x1F6);
    case 0x825C:
        return unit_move_eight_pictures(m, 0x1FE);
    case 0x8262:
        return unit_move_four_pictures(m, unit_pictures_82d8);
    case 0x826A:
        return unit_move_four_pictures(m, unit_pictures_82e0);
    case 0x82E8:
        return unit_move_cycle(m, 6, 0x202);
    case 0x8308:
        return unit_move_cycle(m, 6, 0x1E4);
    case 0x8328:
        return unit_move_cycle(m, 8, 0x1EE);
    case 0x8348:
        return unit_move_cycle(m, 7, 0x20A);
    default:
        unit_routine_missing(routine);
        return 0;
    }
}

/* orig $007EE2: the step of a member that shoots at the player: its timer runs down; at 0 it is set again (from the rate) and, when the unit's shots are aimed, a bolt goes
 * out ($183BC, the aim the rate says), otherwise a shot goes sideways at the speed the rate gives, from the side the player is on */
static void unit_step_shoot_player(struct UnitMember *m)
{
    if (guest.unit_shot_rate == 0)
        return;
    m->timer--;
    if (m->timer != 0)
        return;
    if (guest.shot_aim != 0) {
        m->timer = (int16_t)((16 - guest.unit_shot_rate) << 1);
        aimed_shot((int16_t)(m->x + 16), (int16_t)(m->y + 16), (int16_t)((guest.unit_shot_rate >> 1) + 4), 1);
        return;
    }
    m->timer = (int16_t)((16 - guest.unit_shot_rate) << 1);
    int16_t x = m->x;
    int16_t y = (int16_t)(m->y + 12);
    int16_t speed = (int16_t)((guest.unit_shot_rate >> 1) + 4);

    if (x < guest.creature_x) {
        x = (int16_t)(x + 8);
        guest.shot_speed = (int16_t)-speed;
        enemy_shot(x, y, 0);
    } else {
        x = (int16_t)(x + 24);
        guest.shot_speed = speed;
        enemy_shot(x, y, 1);
    }
}

/* orig $0080E8: the step of a member whose timer, when it has run out at its picture 3 (the screen not on its odd frame), goes on to the picture 4 */
static void unit_step_advance_at_3(struct UnitMember *m)
{
    if (guest.unit_shot_rate == 0)
        return;
    m->timer--;
    if (m->timer >= 0)
        return;
    if (m->anim != 3)
        return;
    if (guest.frame_phase != 0)
        return;
    m->timer = (int16_t)((16 - guest.unit_shot_rate) << 2);
    m->anim++;
}

/* orig $008158: the step of a member that, when its timer has run out at its picture 3 or 7 (when frame_toggle is set), goes on to the next */
static void unit_step_advance_at_3_or_7(struct UnitMember *m)
{
    if (guest.unit_shot_rate == 0)
        return;
    m->timer--;
    if (m->timer >= 0)
        return;
    if ((m->anim & 7) != 3)
        return;
    if (guest.frame_toggle == 0)
        return;
    m->timer = (int16_t)((16 - guest.unit_shot_rate) << 1);
    m->anim++;
}

/* the step routines of the templates for the members of a unit */
static void unit_step_routine(uint32_t routine, struct UnitMember *m)
{
    switch (routine) {
    case 0x7EE2:
        unit_step_shoot_player(m);
        break;
    case 0x80A0:   /* a bare return */
        break;
    case 0x80E8:
        unit_step_advance_at_3(m);
        break;
    case 0x8158:
        unit_step_advance_at_3_or_7(m);
        break;
    case 0xFFFFFFFF:   /* the templates that have none */
        break;
    default:
        unit_routine_missing(routine);
        break;
    }
}

/* orig $0080A2: the member is hit by what the player has (check_hit_and_walls, with word_3a6 set to say it is a unit's): its hit points are what is left; when they
 * are negative its picture count is -1 and its last step, the byte before where it is on its path, is kept to die going on with */
static void unit_hit(struct UnitMember *m, int16_t x, int16_t y)
{
    guest.word_3a6 = 0xFFFF;
    int16_t hp = check_hit_and_walls((int16_t)(x + guest.scroll_x), (int16_t)(y + guest.scroll_y), m->hp);

    guest.word_3a6 = 0;
    m->hp = hp;
    if (hp >= 0)
        return;
    m->anim = -1;
    m->vector = (int16_t)(int8_t)m->path[-1];
}

/* orig $0173C4 and the others of the table at $6EA8: what the member is drawn with (the draw routine of the template) */
static void unit_draw(uint8_t kind, int16_t x, int16_t y, int16_t picture)
{
    switch ((int8_t)kind) {
    case 0:
        glyph_draw(x, y, (uint16_t)picture);
        break;
    case 1:
    case 2:
        sprite_draw(x, y, (uint16_t)picture);
        break;
    default:
        host_missing_routine(0x6EA8 + 4 * (uint32_t)(int32_t)(int8_t)kind);
        break;
    }
}

/* the step of a member along its path: a byte a frame, the step in x in the high nibble and in y in the low (both signed) */
static void unit_walk(struct UnitMember *m, int16_t *x, int16_t *y, int8_t step)
{
    int16_t dy = (int16_t)((int8_t)(((uint8_t)step & 0x0F) << 4) >> 4);
    int16_t dx = (int16_t)((int8_t)((uint8_t)step & 0xF0) >> 4);

    (void)m;
    *y = (int16_t)(*y + dy);
    *x = (int16_t)(*x + dx);
}

/* the body of $E486 and its copies: unit `k` (0 to 4).  A unit that has been made (state 2) puts its members at the start of the path; one going (state 3) moves
 * them, one a frame, along it */
static void unit_update(int k, uint16_t *words)
{
    struct Unit *unit = &guest.units[k];

    if ((unit->state & 2) == 0)
        return;
    struct UnitMember *member = (struct UnitMember *)guest.ptr_0cfa[k];
    int16_t index = 0;
    int16_t gone = 0;

    if ((unit->state & 1) == 0) {   /* $E49C: the members are put at the start of their path */
        int16_t delay = 0;

        do {
            words[0] = 0;
            unit->state++;
            const struct Entry8 *entry = &guest.entry_list[(int8_t)unit->path];
            const uint16_t *path = (const uint16_t *)(uintptr_t)entry->address;

            guest.unit_path_end[k] = (const uint8_t *)path + entry->size;
            if (path[0] == 0x2345) {
                member->x = (int16_t)path[1];
                member->y = (int16_t)path[2];
                member->path = (const uint8_t *)(path + 3);
            } else {
                member->x = (int16_t)(path[0] + guest.scroll_x);
                member->y = (int16_t)(path[1] + guest.scroll_y);
                member->path = (const uint8_t *)path + 4;
            }
            member->unused_4 = 0;
            member->delay = delay;
            member->anim = 0;
            member->timer = (int16_t)((int16_t)(int8_t)(16 - unit->rate) * 2);
            member->hp = unit->life;
            delay = (int16_t)(delay + unit->spacing);
            member++;
            index++;
        } while ((uint8_t)index != unit->count);
        guest.unit_count[k] = (int8_t)unit->count;
        unit->state = 3;
        return;
    }

    /* $E546 */
    words[1] = 0;
    words[2] = 0;
    guest.unit_shot_rate = unit->rate;
    guest.shot_aim = unit->aim;
    member = (struct UnitMember *)guest.ptr_0cfa[k];
    index = 0;
    for (;;) {
        if (member->hp == 0) {
            gone++;
        } else if (member->hp < 0) {   /* $E700: it dies going on as it went */
            words[2]++;
            member->anim++;
            if (member->anim >= 17) {
                member->hp = 0;
            } else {
                int16_t x = member->x, y = member->y;

                unit_walk(member, &x, &y, (int8_t)member->vector);
                member->x = x;
                member->y = y;
                sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)(member->anim + 0x5B));
            }
        } else if (member->delay != 0) {
            member->delay--;
        } else {
            int16_t x = member->x, y = member->y;
            int off = 0;

            if (guest.timer_306 == 0) {
                const uint8_t *path = member->path;
                int8_t step = (int8_t)*path++;

                if (path >= guest.unit_path_end[k]) {   /* the end of the path: it goes when it is off the screen, else it keeps its last step */
                    int16_t sx = (int16_t)(member->x - guest.scroll_x);
                    int16_t sy = (int16_t)(member->y - guest.scroll_y);

                    if (sx <= -36 || sx >= 324 || sy <= -40 || sy >= 196) {
                        member->hp = 0;
                        unit->drop = 0xFF;
                        gone++;
                        off = 1;
                    } else {
                        path--;
                    }
                }
                if (!off) {
                    guest.creature_x = member->x;
                    member->path = path;
                    unit_walk(member, &x, &y, step);
                }
            }
            if (!off) {
                member->x = x;
                member->y = y;
                x = (int16_t)(x - guest.scroll_x);
                y = (int16_t)(y - guest.scroll_y);
                if (guest.word_214 >= 0) {
                    int16_t *list = hit_list_at(guest.value_c6c);

                    list[0] = x;
                    list[1] = y;
                    list[2] = member->hp;
                    guest.flag_20e++;
                    guest.value_c6c += 6;
                }
                int16_t picture = unit_move_routine(unit->move, member, x, y);

                unit_step_routine(unit->step, member);
                guest.hit_box_extra_width = unit->box_width;
                guest.hit_box_extra_height = unit->box_height;
                unit_hit(member, x, y);
                if (member->hp < 0) {   /* $E6D6: just killed */
                    words[1]++;
                    guest.last_death_x = x;
                    guest.last_death_y = y;
                    add_kill_score(unit->life);
                }
                unit_draw(unit->draw, x, y, picture);
            }
        }
        index++;
        member++;
        if (index >= guest.unit_count[k])
            break;
    }
    /* $E674: when all of them are gone or dying and none has dropped what the unit drops yet, it is dropped where the last was */
    int16_t left = (int16_t)(guest.unit_count[k] - words[1] - words[2]);

    if (left == gone && words[0] == 0) {
        int16_t x = (int16_t)(guest.last_death_x + guest.scroll_x + 8);
        int16_t y = (int16_t)(guest.last_death_y + guest.scroll_y + 8);

        if (unit->drop != 0xFF)
            drop_thing(x, y, unit->drop);
        words[0] = 1;
    }
    if (gone >= guest.unit_count[k])
        unit->state = 0;
}

/* orig $00E486, $00E76C, $00EA52, $00ED38 and $00F01E: the five units */
void unit0_update(void)
{
    unit_update(0, guest.unit0_words);
}

void unit1_update(void)
{
    unit_update(1, guest.unit1_words);
}

void unit2_update(void)
{
    unit_update(2, guest.unit2_words);
}

void unit3_update(void)
{
    unit_update(3, guest.unit3_words);
}

void unit4_update(void)
{
    unit_update(4, guest.unit4_words);
}

/* orig $006ED0 and $0070C8 (the table at $7074 is a switch in creature_runs; $7074 to $70C8 are its cases; inlined: $6EC0, $6EEC,
 * $6F74, $7070, $70DC, $70F8, $7EAE and $9DEA with $5AAC): every frame the ten creature records at $199AC are looked at.  A record
 * with status bit 1 clear is skipped; bit 0 clear starts its group (creatures_start), set runs it (creatures_run).  While
 * timer_306 is not 0 frame_toggle is 0 for the duration (the original saves and restores it) */
void update_creatures(void)
{
    if (guest.code_6ed0 == 0x4E75)   /* (a guardian's start made the routine's first instruction an rts) */
        return;
    uint16_t saved = guest.frame_toggle;
    struct CreatureType *type = (struct CreatureType *)guest.table_199ac;

    if (guest.timer_306 != 0)
        guest.frame_toggle = 0;
    guest.creature_records_left = 9;
    for (;;) {
        if (type->status & 2) {
            if (type->status & 1)
                creatures_run(type);
            else
                creatures_start(type);
        }
        type++;
        guest.creature_records_left--;
        if (guest.creature_records_left < 0)
            break;
    }
    guest.frame_toggle = saved;
}

/* orig $012A80's caller helper ($012BDA): the text at `text` (up to a 0) is copied to `out`; `count` gets the characters added */
static uint8_t *copy_text(uint8_t *out, const uint8_t *text, int16_t *count)
{
    while (*text != 0) {
        (*count)++;
        *out++ = *text++;
    }
    return out;
}

/* five decimal digits of `value` (a signed word), most significant first, with the leading zeros; the divisions by 10 leave their
 * remainders in word_234, word_232, word_2fe and word_2fc as the original does (the low byte of each is the digit) */
static uint8_t *put_digits5(uint8_t *out, int16_t value)
{
    int16_t rest = value;
    int16_t last;

    guest.word_234 = (uint16_t)(rest % 10);
    rest = rest / 10;
    guest.word_232 = (uint16_t)(rest % 10);
    rest = rest / 10;
    guest.word_2fe = (uint16_t)(rest % 10);
    rest = rest / 10;
    guest.word_2fc = (uint16_t)(rest % 10);
    rest = rest / 10;
    last = (int16_t)(rest % 10);
    *out++ = (uint8_t)(last + 48);
    *out++ = (uint8_t)(guest.word_2fc + 48);
    *out++ = (uint8_t)(guest.word_2fe + 48);
    *out++ = (uint8_t)(guest.word_232 + 48);
    *out++ = (uint8_t)(guest.word_234 + 48);
    return out;
}

/* orig $012924 (inlined: $012BDA, the d2 of the call is the item `item`): the message of the item (an index of table_14cf8): its
 * first line of text, the second, and "Value" and its value when it has one.  An item (unless offsets_4cba says 2, or it is 83)
 * whose bit in item_message_shown is set already has no message; otherwise the bit is set, and cleared again when message_request (priority 25)
 * says no.  The original leaves its stack unbalanced when the text's offset is negative; that is not done */
void item_message(int16_t item)
{
    if (offsets_4cba[item] != 2 && item != 83) {
        uint8_t mask = (uint8_t)(1 << (item & 7));
        uint8_t was = guest.item_message_shown[item >> 3] & mask;

        guest.item_message_shown[item >> 3] |= mask;
        if (was)
            return;
    }

    uint8_t *start = next_message_slot();
    uint8_t *out = start;
    int16_t count = 0;

    if (item < 0)
        return;

    const struct Rec10 *record = &guest.table_14cf8[item];
    const uint8_t *text = guest.message_base + (int16_t)record->w[4];

    *out++ = 17;
    *out++ = 32;
    if (text < guest.message_base)
        return;
    out = copy_text(out, text, &count);
    *out++ = 32;
    *out++ = 17;
    *out++ = 32;
    if (*text != 0) {
        do {
        } while (*text++ != 0);
    }
    count = 0;
    out = copy_text(out, text, &count);
    if (count != 0) {
        *out++ = 32;
        *out++ = 17;
        *out++ = 32;
    }
    if (record->w[3] != 0) {
        out = copy_text(out, (const uint8_t *)(text_power + 6), &count);
        *out++ = 32;
        out = put_digits5(out, (int16_t)record->w[3]);
        *out++ = 32;
        *out++ = 32;
        *out++ = 17;
    }
    *out++ = 0;
    guest.message_next = start;
    if (message_request(25) == 0)
        guest.item_message_shown[item >> 3] &= (uint8_t)~(1 << (item & 7));
}

/* orig $012A9C: the message of an object (a record of object_slots, a1): its names (name1, name2 when it has one), "Power" with
 * its first_value (up to three digits, no leading zeros) and "Value" with w74 (five digits).  The priority is 30 */
void object_message(const struct Object *object)
{
    uint8_t *start = next_message_slot();
    uint8_t *out = start;
    int16_t count = 0;

    *out++ = 17;
    *out++ = 32;
    out = copy_text(out, object->name1, &count);
    *out++ = 32;
    *out++ = 17;
    *out++ = 32;
    count = 0;
    out = copy_text(out, object->name2, &count);
    if (count != 0) {
        *out++ = 32;
        *out++ = 17;
        *out++ = 32;
    }
    out = copy_text(out, (const uint8_t *)text_power, &count);
    *out++ = 32;

    int16_t rest = object->first_value;
    int16_t hundreds;

    guest.word_2fe = (uint16_t)(rest % 10);
    rest = rest / 10;
    guest.word_2fc = (uint16_t)(rest % 10);
    rest = rest / 10;
    hundreds = (int16_t)(rest % 10);
    if (hundreds != 0) {
        *out++ = (uint8_t)(hundreds + 48);
        *out++ = (uint8_t)(guest.word_2fc + 48);
    } else if ((uint8_t)guest.word_2fc != 0) {
        *out++ = (uint8_t)(guest.word_2fc + 48);
    }
    *out++ = (uint8_t)(guest.word_2fe + 48);
    *out++ = 32;
    *out++ = 17;
    *out++ = 32;
    out = copy_text(out, (const uint8_t *)(text_power + 6), &count);
    *out++ = 32;
    out = put_digits5(out, object->w74);
    *out++ = 32;
    *out++ = 17;
    *out++ = 0;
    guest.message_next = start;
    message_request(30);
}

/* orig $012CFC: the level is over (level_ended set) when word_22e is the count in the first word of ptr_0bc6; otherwise, when
 * param_index is 3, word_0266 is 0 and the player is at x 1104 or more and y 768 to 784, word_22e is cleared and the level is over.
 * (The original loses the caller's a6 here: it leaves ptr_0bc6 in it and the saved a6 in a0) */
static void level_over_check(void)
{
    if (guest.ptr_0bc6->spots != guest.word_22e) {
        if (guest.param_index != 3 || guest.word_0266 != 0 || guest.player_x < 0x450 || guest.player_y > 0x310 || guest.player_y < 0x300)
            return;
        guest.word_22e = 0;
    }
    guest.level_ended = 1;
}

/* orig $0187B8 (state 6 of the entries run by run_cell_entry): the player position is copied to previous_player_pos; when it is not within 64
 * (in x and in y) of any of the six spots of visited_spots, text_index and word_22e count up, the spots move on one (the oldest is
 * lost), the player position is the first and level_over_check runs */
void remember_visited_spot(void)
{
    int16_t x = guest.player_x, y = guest.player_y;

    guest.previous_player_pos[0] = x;
    guest.previous_player_pos[1] = y;
    for (int i = 0; i < 12; i += 2) {
        int16_t sx = guest.visited_spots[i], sy = guest.visited_spots[i + 1];

        if (!(sx - 0x40 > x || sy - 0x40 > y || sx + 0x40 < x || sy + 0x40 < y))
            return;
    }
    guest.text_index++;
    guest.word_22e++;
    for (int i = 11; i >= 2; i--)
        guest.visited_spots[i] = guest.visited_spots[i - 2];
    guest.visited_spots[0] = x;
    guest.visited_spots[1] = y;
    level_over_check();
}

/* orig $012796 (inlined: the table at $18714; a0 is the map cell `cell`): a cell of the map whose byte is above 2 names an entry of
 * the table at ptr_0cbe (4 bytes: a state, a number).  A negative or 0 state does nothing; otherwise the creature types that
 * have byte 13 set to 1 are switched off (status 0) and the routine the state names in table_18714 is called with the entry:
 * 1 is take_unit_record, 2 is take_record_to_199ac, 3 is finish_record_1895a, 6 is remember_visited_spot (the entries
 * 4, $8898, is take_walker_group_record; 7 to 10, $17E0A to $17E16, start a thing on its path: start_record_path in game_h.c) */
void cell_entry_runs(const uint8_t *cell)
{
    run_cell_entry((int16_t)((uint8_t)*cell - 3));
}

/* orig $0127A0: the entry `index` (4 bytes each) of the table at ptr_0cbe is run as cell_entry_runs says */
void run_cell_entry(int16_t index)
{
    struct EntryRef *entry = &guest.ptr_0cbe[index];

    if ((int16_t)entry->state <= 0)
        return;

    struct CreatureType *type = (struct CreatureType *)guest.table_199ac;

    for (int i = 0; i < 10; i++, type++)
        if (type->bounds == 1)
            type->status = 0;

    if (entry->state == 11) {   /* the eleventh routine of the table, $AFD6 (the table of the program has ten entries here: the guardian's start) */
        boss_start(entry);
        return;
    }

    uint32_t routine = table_18714[(int16_t)(entry->state - 1)];

    if (routine == 0x18900)
        take_unit_record(entry);
    else if (routine == 0x18890)
        take_record_to_199ac(entry);
    else if (routine == 0x1895A)
        finish_record_1895a(entry);
    else if (routine == 0x88DC)
        take_monster_record(entry);
    else if (routine == 0x8898)
        take_walker_group_record(entry);
    else if (routine == 0x187B8)
        remember_visited_spot();
    else if (routine >= 0x17E0A && routine <= 0x17E16)
        start_record_path(entry, (int16_t)((routine - 0x17E0A) >> 2));
}

/* orig $010976: the message of the hint found at the player.  The list of 40 words at ptr_0cf2 is searched for the x of the player's
 * value_1296 (less 6 bytes, plus scroll_x); its twin 80 bytes further on, a word that is an offset from message_base, is kept in ptr_0364
 * and gives the text; none found, the text is the one level_texts names for text_index.  The message "\x11 Hint \x11" and the text is
 * made (priority 100) and flag_236 is set.  The original returns 0 in d3 */
int16_t hint_message(void)
{
    const int16_t *list = (const int16_t *)guest.ptr_0cf2;
    int16_t wanted = (int16_t)(*(const int16_t *)(guest.value_1296 - 6) + guest.scroll_x);
    const int16_t *twin = list + 40;
    const uint8_t *text;
    int found = 0;

    for (int i = 0; i < 40; i++, twin++) {
        if (list[i] == wanted) {
            found = 1;
            break;
        }
    }
    if (found) {
        guest.ptr_0364 = (uint8_t *)twin;
        text = guest.message_base + *(const int16_t *)guest.ptr_0364;
    } else {
        text = (const uint8_t *)guest.level_texts[guest.text_index];
    }

    uint8_t *start = next_message_slot();
    uint8_t *out = start;

    for (int i = 0; i < 8; i++)
        *out++ = text_10a34[i];
    *out++ = 32;
    while ((*out++ = *text++) != 0)
        ;
    out[-1] = 32;
    *out++ = 17;
    *out++ = 0;
    guest.message_next = start;
    message_request(100);
    guest.flag_236 = 1;
    return 0;
}

/* orig $015328 (inlined: the item is d2, the object's record a1): which of the three slots (slot_ref_6b6, 748, 7da) a new object
 * goes in: the first for one whose first word is 0, else the second when it is free, else the third when it is free, else the one
 * whose object has the smaller last_value; the item 2 goes in the third at once (and word_fb2c becomes 18).  Returns the number of the slot */
static int pick_slot(int16_t item, const struct Object *object)
{
    if (item == 2) {
        guest.word_fb2c = 18;
        return 2;
    }
    if (object->w0 == 0)
        return 0;
    if (guest.slot_ref_748 < 0)
        return 1;
    if (guest.slot_ref_7da < 0)
        return 2;

    const struct Object *a = &((const struct Object *)guest.object_slots)[guest.slot_ref_748];
    const struct Object *b = &((const struct Object *)guest.object_slots)[guest.slot_ref_7da];

    return a->last_value < b->last_value ? 1 : 2;
}

/* orig $0152F6: the points a slot's object counts: its first_value for each of up to three steps from first_value to last_value
 * (the third step adds what is left); added to slot_value_sum */
static void count_slot(int16_t reference)
{
    int16_t shifted = (int16_t)(reference << 4);

    if (shifted < 0)
        return;

    const struct Object *object = &((const struct Object *)guest.object_slots)[reference];
    int16_t first = object->first_value;
    int16_t left = (int16_t)(object->last_value - first);

    guest.slot_value_sum = (uint16_t)(guest.slot_value_sum + first);
    if (left == 0)
        return;
    guest.slot_value_sum = (uint16_t)(guest.slot_value_sum + first);
    left--;
    if (left == 0)
        return;
    guest.slot_value_sum = (uint16_t)(guest.slot_value_sum + first);
    left--;
    guest.slot_value_sum = (uint16_t)(guest.slot_value_sum + left);
}

/* orig $0152D8: slot_value_sum is the sum of the three slots' counts */
static void count_slots(void)
{
    guest.slot_value_sum = 0;
    count_slot(guest.slot_ref_6b6);
    count_slot(guest.slot_ref_748);
    count_slot(guest.slot_ref_7da);
}

/* orig $015282 (inlined: $015328, $0152F6, $0152D8, $0152BE): the object `item` is picked up.  One already in a slot has its
 * last_value raised by one; any other goes into the slot pick_slot names (its first word is the item), the slot's list of 72
 * words is cleared, reset_object_slot sets its record up, and the message of the object is made.  slot_value_sum is worked out again and
 * (for a new object) falcon_choose_weapon runs */
void pick_up_object(int16_t item)
{
    struct Object *object = &((struct Object *)guest.object_slots)[item];

    if (item == guest.slot_ref_6b6 || item == guest.slot_ref_748 || item == guest.slot_ref_7da) {
        object->last_value++;
        object_message(object);
        count_slots();
        return;
    }

    int slot = pick_slot(item, object);
    int16_t *slot_ref = slot == 0 ? &guest.slot_ref_6b6 : slot == 1 ? &guest.slot_ref_748 : &guest.slot_ref_7da;

    *slot_ref = item;
    reset_object_slot(item, (int16_t)slot);
    object_message(object);
    clear_72_words((uint16_t *)(slot_ref + 1));
    count_slots();
    falcon_choose_weapon();
}


/* ======== from game_f.c ======== */
/* Gods, the game: the player of one frame, the state machine of $10368 and everything the states of its table (at $10280) go on with.  Each
 * function says where the original has it (`orig $ADDR`).
 *
 * The state (player_state) names a routine of the table; it gets the animation frame (word_19a, d7 in the original) and ends in $1263A, which
 * keeps the frame and goes on at $1263E (scroll and draw).  A routine here is a C function with the frame as its parameter; `player_end(d7)`
 * is the original's jump to $1263A.  The routines of the states 10, 11, 12, 22 and 23 (falling), 8, 9, 20 and 21 (jumping) and 13 and 14
 * (ladders) are each other's mirror images but are written out as they are in the original, with the differences it has. */

#define OBJECT(n) (&((struct Object *)guest.object_slots)[(n)])
#define shot_lists ((struct ShotList *)guest.shot_lists_raw)
#define SLOT_IMAGE(n) ((uint8_t *)guest.image_table[115 + (n)].data)

static void player_after(int16_t frame);
static void player_walk_right(int16_t d7);
static void player_walk_left(int16_t d7);
static void player_check_floor(int16_t d7);

/* orig $01263A */
static void player_end(int16_t d7)
{
    guest.word_19a = d7;
    player_after(d7);
}

/* orig $0125B8: the scroll is eased to the place (scroll_ease_x, word_380) while word_037c is not 0: sideways 4 a frame, up and down by half of the
 * way (at least 1); word_037c counts up to 4 once both have come there */
static void scroll_ease_to_target(void)
{
    int16_t arrived = 0;

    if (guest.scroll_ease_x != guest.scroll_x) {
        if (guest.scroll_ease_x > guest.scroll_x)
            guest.scroll_x += 4;
        else
            guest.scroll_x -= 4;
        if (guest.scroll_ease_x == guest.scroll_x) {
            guest.scroll_x = guest.scroll_ease_x;
            if ((int16_t)guest.word_037c <= 2)
                arrived++;
        }
    } else {
        arrived++;
    }

    int16_t d3 = (int16_t)(guest.word_380 - guest.scroll_y);

    if (d3 != 0) {
        int16_t up = d3 > 0;

        d3 >>= 1;
        if (d3 == 0)
            guest.scroll_y += up ? 1 : -1;
        guest.scroll_y += d3;
        if (guest.word_380 != guest.scroll_y)
            return;
        if ((int16_t)guest.word_037c > 2)
            return;
    }
    if (arrived == 0)
        return;
    if ((int16_t)guest.word_037c >= 4)
        return;
    guest.word_037c++;
}

/* orig $012710: the cells of the second map around the player are looked at: the three in the column 16 pixels to the right of his place (rows
 * of his tile and the two below) and, when he is 16 or more pixels into his tile, the three in the column to the left of that; a cell
 * whose type is over 2 has an entry that runs (cell_entry_runs) */
static void player_cell_entries(void)
{
    int16_t x = (int16_t)((guest.player_x + 16) & -32);
    int16_t y = (int16_t)(guest.player_y & -16);
    const uint8_t *cell = guest.ptr_0b86 + 8192;

    cell += x >> 5;
    cell += (int16_t)(y << 3);
    for (int row = 0; row < 3; row++, cell += 128) {
        if (cell[0] > 2)
            cell_entry_runs(cell);
    }
    if ((guest.player_x & 31) < 16)
        return;
    cell -= 384 + 1;
    for (int row = 0; row < 3; row++, cell += 128) {
        if (cell[0] > 2)
            cell_entry_runs(cell);
    }
}

/* orig $01263E: the tiles around the player are looked at ($12710), the scroll follows the player (sideways when he is within
 * scroll_margin of the screen's edge, up and down toward the middle; or eased to a place, $125B8) and the player's sprite is drawn: the first
 * sprite of his state plus the frame (52 is shown as 25, 54 as 35 and 55 as 38) */
static void player_after(int16_t d7)
{
    player_cell_entries();

    int16_t x = guest.player_x - guest.scroll_x;
    int16_t y = guest.player_y - guest.scroll_y;

    if (guest.word_037c != 0) {
        scroll_ease_to_target();
    } else {
        if (x <= (int16_t)frame_map_125ac[5]) {
            guest.scroll_x -= 4;
            if (guest.scroll_x < 0)
                guest.scroll_x = 0;
        } else if (x >= 320 - (int16_t)frame_map_125ac[5] - 32) {
            guest.scroll_x += 4;
            if (guest.scroll_x >= 3776)
                guest.scroll_x = 3776;
        }
        if (y > 112) {
            int16_t step = (int16_t)((y - 112) >> 1);

            if (step == 0)
                guest.scroll_y++;
            guest.scroll_y += step;
            if (guest.scroll_y >= 832)
                guest.scroll_y = 832;
        } else if (y < 32) {
            int16_t step = (int16_t)((32 - y) >> 1);

            if (step == 0)
                guest.scroll_y--;
            guest.scroll_y -= step;
            if (guest.scroll_y < 0)
                guest.scroll_y = 0;
        }
    }

    int16_t sprite = (int16_t)(state_table[guest.player_state * 2] + d7);

    if (sprite == 52)
        sprite = 25;
    else if (sprite == 54)
        sprite = 35;
    else if (sprite == 55)
        sprite = 38;
    draw_player_sprite(sprite);
}

/* orig $01221C: the first of the 200 records at $19B14 that is in use (third word not negative, fourth not 0) and whose place is near the
 * player's (a box from 4 to the left and 20 to the right, 12 above and 16 below the record, against the player's place 16 to the right and
 * 40 down), or 0.  The original returns it in the condition codes (N set: found) and the record in a0 */
static struct Slot8 *record_near_player(void)
{
    int16_t x = guest.player_x + 16;
    int16_t y = guest.player_y + 40;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[2] < 0 || record->w[3] == 0)
            continue;
        if (x < record->w[0] - 4 || x > record->w[0] + 20 || y < record->w[1] - 12 || y > record->w[1] + 16)
            continue;
        return record;
    }
    return 0;
}

/* orig $015546, the handler of the object's left-facing fire kind (the long at +16 of the object, table $15220) 0 = $015968: a shot to the left
 * from (x, y - 12): as many as the object's last_value less its first_value, plus one, at most three, with the directions 4, 3 and 5, and the
 * object's anim_mode is set to -1.  `slot` is where the first shot goes (`id`, `id` + 1 and `id` + 2 in the first word) */
static void spawn_shots_left_triple(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y)
{
    play_sound(2, 4);
    y -= 12;

    int16_t count = object->last_value - object->first_value + 1;

    slot[0].active = id;
    slot[0].x = x;
    slot[0].y = y;
    slot[0].direction = 4;
    object->anim_mode = -1;
    if (--count == 0)
        return;
    id++;
    slot[1].active = id;
    slot[1].x = x;
    slot[1].y = y;
    slot[1].direction = 3;
    if (--count == 0)
        return;
    id++;
    slot[2].active = id;
    slot[2].x = x;
    slot[2].y = y;
    slot[2].direction = 5;
}

/* orig $0155EC: the fireball's left-facing kind (2): the fireball's shots as the right ($01557C) but at (x + 16, y - 6) with the directions 4, 3 and 5 and anim_mode -1 */
static void spawn_shots_fireball_left(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y)
{
    static const int16_t directions[3] = { 4, 3, 5 };

    play_sound(2, 4);
    x += 16;
    y -= 6;

    int16_t column = (int16_t)(id - guest.word_fb2c - 1);
    int16_t count = (int16_t)(object->last_value - object->first_value + 1);

    object->anim_mode = -1;
    for (int i = 0; i < 3; i++) {
        slot[i].active = (int16_t)(id + i);
        slot[i].x = x;
        slot[i].y = y;
        slot[i].direction = directions[i];
        guest.shot_offsets_6a4[column + i] = -6;
        if (--count == 0)
            return;
    }
}

/* orig $01545C (with $015546): the same as spawn_player_shots to the left: each list with a free place for a shot gets one, at the player (a
 * little below him) or, when falcon_choice is the list's number, at word_8882 and word_8884 (16 below); the routine is the one the object's long
 * at +16 names in the table at $15220: 0 is $15968, 4 and 5 are bare returns; 1 to 3 and 6 to 15 did not run */
static void spawn_player_shots_left(void)
{
    struct Shot *slots[3] = { guest.shot_slot_b92, guest.shot_slot_b96, guest.shot_slot_b9a };
    int16_t ids[3] = { guest.word_2ea, guest.word_2ec, guest.word_2ee };

    for (int i = 0; i < 3; i++) {
        if (slots[i] == 0)
            continue;

        int16_t x = guest.player_x;
        int16_t y = guest.player_y + 24;

        if (guest.falcon_choice == i) {
            x = guest.word_8882;
            y = guest.word_8884 + 16;
        }

        struct Object *object = OBJECT(shot_lists[i].object);
        uint32_t kind = ((uint32_t)object->pad_16[0] << 24) | ((uint32_t)object->pad_16[1] << 16) | ((uint32_t)object->pad_16[2] << 8) |
                        object->pad_16[3];

        play_sound(2, 4);
        switch (kind) {
        case 0:
            spawn_shots_left_triple(slots[i], ids[i], object, x, y);
            break;
        case 2:
            spawn_shots_fireball_left(slots[i], ids[i], object, x, y);
            break;
        case 6:
            spawn_shots_spear(slots[i], ids[i], object, x, y, 0);
            break;
        case 8:
            spawn_shot_ball(slots[i], ids[i], x, y, 0);   /* $015870: the ball (the time bomb) to the left */
            break;
        case 10:
            spawn_shots_hunter(slots[i], ids[i], object, x, y, 1);
            break;
        case 14:
            spawn_shots_axe(slots[i], ids[i], object, x, y, 1);
            break;
        default:
            break;   /* 4 and 5 return; the others ($155EC, $1557C ...) did not run */
        }
    }
}

/* orig $00565E: the pictures of the four slots (made plain) and then the chosen one's (cut) */
static void panel_redraw(void)
{
    for (int i = 0; i < 4; i++)
        panel_slot_plain(SLOT_IMAGE(i), guest.slots_30c[i]);
    panel_slot_cut(SLOT_IMAGE((int16_t)guest.item_shown), guest.slots_30c[(int16_t)guest.item_shown]);
}

/* orig $005444: the slots' pictures are made again; a shutter that is shut at once (shutter_row negative) is opened (sound 17), otherwise one
 * that is not at 12 and not opening is made to open */
static void panel_shutter_refresh(void)
{
    panel_redraw();
    if (guest.shutter_row < 0) {
        guest.shutter_row = 0;
        guest.shutter_direction = 1;
        play_sound(17, 40);
        guest.word_2bc = 0;
        return;
    }
    if (guest.shutter_row == 12)
        return;
    if (guest.shutter_direction >= 0)
        return;
    guest.shutter_direction = 1;
    guest.word_2bc = 0;
}

/* orig $003D48: 1 when the record at (x, y) (a thing of 16 by 16) and the player (his box from 12 to the right and 0 down, 8 more across and
 * 48 down) touch; the original returns it in the condition codes (N set: no) */
static int record_touches_player(const struct Slot8 *record)
{
    int16_t x = record->w[0];
    int16_t y = record->w[1];
    int16_t d4 = guest.player_x + 12;
    int16_t d5 = guest.player_y;

    if (d4 > x + 16 || d5 > y + 16)
        return 0;
    d4 += 8;
    d5 += 48;
    if (d4 < x || d5 < y)
        return 0;
    return 1;
}

/* orig $003C94 (with $003D48 and $003DE0 inlined or written above): the player uses a thing that is touched (a lever: a record of the kind
 * whose type, in the fifth word of its entry in table_14cf8, is 3, and which has an entry of the 64 of 6 bytes at lever_flags for its place).
 * Once for each press of the button (word_35e).  For the kinds 288, 290, 292 and 232 the entry is to be toggled by game_d (ptr_0ba2, word_2ba
 * is 0), otherwise the third word of the record and of the entry are flipped in their bit 0; sound 10 */
static void use_lever_3c94(void)
{
    if (guest.word_35e != 0)
        return;
    if (guest.flags_3b0 & 4)
        return;
    if (guest.word_2ba >= 0)
        return;
    guest.word_35e = 1;
    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[3] == 0 || record->w[2] < 0)
            continue;

        const struct Rec10 *kind = &guest.table_14cf8[(uint16_t)record->w[2]];

        if (kind->w[2] != 3)
            continue;

        uint32_t place = ((uint32_t)(uint16_t)record->w[0] << 16) | (uint16_t)record->w[1];
        struct LeverFlag *entry = 0;

        for (int e = 0; e < 64; e++) {
            struct LeverFlag *p = &guest.lever_flags[e];

            if ((((uint32_t)p->x << 16) | p->y) == place) {
                entry = p;
                break;
            }
        }
        if (entry == 0)
            continue;
        if (!record_touches_player(record))
            continue;
        if (kind->w[0] == 288 || kind->w[0] == 290 || kind->w[0] == 292 || kind->w[0] == 232) {
            guest.ptr_0ba2 = &entry->x;
            guest.word_2ba = 0;
        } else {
            record->w[2] ^= 1;
            entry->state ^= 1;
        }
        play_sound(10, 6);
        return;
    }
}

/* ---- small pieces that several states end in ---- */

/* orig $011F94 */
static void ladder_sounds(void)
{
    play_sound(8, 2);
    play_sound(53, 2);
}

/* orig $011F3C and $011F68: a jump begins (state 9 to the right with the frame 12, state 8 to the left with 13): sound 40 */
static void jump_begin(int16_t state, int16_t frame)
{
    guest.player_state = state;
    guest.jump_start_x = guest.player_x;
    guest.jump_step = 0;
    play_sound(40, 2);
    player_end(frame);
}

/* the player takes hold of a ladder (the x snapped to the column, state 13 or 14, the frame `frame`), e.g. $12054 */
static void ladder_mount(int16_t state, int16_t frame)
{
    guest.player_x &= -32;
    guest.word_370 = 0;
    guest.player_state = state;
    guest.word_376 = 0;
    guest.word_360 = 0;
    guest.word_362 = 0;
    player_end(frame);
}

/* the player stands in front of something to take or open (state 18 to the left, 19 to the right): $1208C, $1234C */
static void pickup_enter(int16_t state)
{
    guest.player_state = state;
    guest.word_368 = 0;
    guest.word_1fa = 0;
    guest.word_35c = 0;
    player_end(0);
}

/* the player begins to fall (state 10 off a ladder, 11 to the left, 12 to the right) */
static void fall_begin(int16_t state)
{
    guest.player_state = state;
    guest.fall_rows_36a = 0;
    guest.air_steps = 0;
    guest.fall_distance = 0;
    player_end(0);
}

/* orig $011006 and $010D02: the player lands (state 16 to the right, 17 to the left): a sound and, after a fall of more than 20, less energy */
static void land(int16_t state)
{
    guest.player_state = state;
    guest.player_y &= -16;
    guest.landing_frames = 0;
    play_sound(16, 3);

    int16_t excess = guest.fall_distance - 20;

    if (excess > 0 && guest.flag_aa8 == 0)
        guest.player_energy -= excess >> 1;
    player_end(0);
}

/* orig $0114A8 (and the same at $11496, $1193A, $116CA): the end of a jump on the ground, no harm taken */
static void jump_land(int16_t state)
{
    guest.player_state = state;
    guest.player_y &= -16;
    guest.landing_frames = 0;
    play_sound(16, 3);
    player_end(0);
}

/* ---- falling ---- */

/* the first part of the falling states ($10EB6, $10BB2, $10D44, $11048, $111A0): down 4, then down by fall_rows_36a (it grows by 1 every other
 * frame to 10), fall_distance counts up; then the sideways steps the stick asks for (3 at most, and not into a tile of type 1): left makes the
 * state 11, right 12.  The tile the player is in before the steps is returned */
static uint8_t *air_move(void)
{
    guest.player_y += 4;
    guest.fall_distance += guest.fall_rows_36a >> 2;
    if (guest.frame_phase == 0) {
        guest.fall_rows_36a++;
        if (guest.fall_rows_36a > 10)
            guest.fall_rows_36a = 10;
    }
    guest.player_y += guest.fall_rows_36a;
    guest.fall_distance++;
    if (guest.fall_distance == 8)
        play_sound(23, 1);

    uint8_t *tile = player_tile_ptr();

    if ((guest.game_stick & 4) && guest.air_steps < 3) {
        guest.player_state = 11;
        if ((guest.player_x & 31) != 0)
            goto left_move;
        if (tile[-1] == 1 || tile[127] == 1 || tile[255] == 1)
            goto right;
        if ((guest.player_y & 15) != 0 && tile[383] == 1)
            goto right;
left_move:
        guest.player_x -= 4;
        guest.air_steps++;
    }
right:
    if ((guest.game_stick & 8) && guest.air_steps < 3) {
        int16_t d0 = guest.player_x & 28;

        guest.player_state = 12;
        if (d0 == 28 || d0 < 8) {
            if (tile[1] == 1 || tile[129] == 1 || tile[257] == 1)
                return tile;
            if ((guest.player_y & 15) != 0 && tile[385] == 1)
                return tile;
        }
        guest.player_x += 4;
        guest.air_steps++;
    }
    return tile;
}

/* orig $010EB6 (state 12, falling to the right) */
static void player_fall_right(int16_t d7)
{
    air_move();

    uint8_t *tile = player_tile_ptr();

    d7 = 0;
    if (tile[384] != 1 && ((guest.player_x & 28) < 8 || tile[385] != 1)) {
        if (!(guest.game_buttons & 1)) {
            player_end(d7);
            return;
        }
        if (claim_free_entries() != 0) {
            player_end(d7);
            return;
        }
        guest.player_state = 22;
        guest.word_36e = 1;
        player_end(1);
        return;
    }
    land(16);
}

/* orig $010BB2 (state 11, falling to the left) */
static void player_fall_left(int16_t d7)
{
    air_move();

    uint8_t *tile = player_tile_ptr();

    d7 = 0;
    if (tile[384] != 1 && ((guest.player_x & 28) < 8 || tile[385] != 1)) {
        if (!(guest.game_buttons & 1)) {
            player_end(d7);
            return;
        }
        if (claim_free_entries() != 0) {
            player_end(d7);
            return;
        }
        guest.player_state = 23;
        guest.word_36e = 1;
        player_end(1);
        return;
    }
    land(17);
}

/* orig $010D44 (state 10, falling off a ladder): the same, but the landing is a state 16 or 17 as word_370 says (not negative: 16) and the
 * frame is kept */
static void player_fall_off_ladder(int16_t d7)
{
    air_move();

    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1 && ((guest.player_x & 31) < 8 || tile[385] != 1)) {
        player_end(d7);
        return;
    }
    guest.player_state = 17;
    if (guest.word_370 >= 0)
        guest.player_state = 16;
    guest.player_y &= -16;

    int16_t excess = guest.fall_distance - 20;

    if (excess > 0 && guest.flag_aa8 == 0)
        guest.player_energy -= excess >> 1;
    play_sound(16, 3);
    player_end(0);
}

/* orig $011048 (state 23, falling to the left and firing): the frame counts up (down once when word_36e is set) */
static void player_fall_fire_left(int16_t d7)
{
    if (guest.word_36e != 0) {
        d7--;
        guest.word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = air_move();

    if (tile[384] == 1 || ((guest.player_x & 31) >= 8 && tile[385] == 1)) {
        land(17);
        return;
    }
    if (d7 == 3) {
        guest.player_state = 11;
        player_end(0);
        return;
    }
    if (d7 == 1)
        spawn_player_shots_left();
    player_end(d7);
}

/* orig $0111A0 (state 22, falling to the right and firing) */
static void player_fall_fire_right(int16_t d7)
{
    if (guest.word_36e != 0) {
        d7--;
        guest.word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = air_move();

    if (tile[384] == 1 || ((guest.player_x & 31) >= 8 && tile[385] == 1)) {
        land(16);
        return;
    }
    if (d7 == 3) {
        guest.player_state = 12;
        player_end(0);
        return;
    }
    if (d7 == 1)
        spawn_player_shots();
    player_end(d7);
}

/* orig $011CD2 (state 15; state 4 goes on here): the player stands on a tile of type 1 three rows down -> ends; otherwise he falls (state 12) */
static void player_check_floor(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] == 1) {
        player_end(d7);
        return;
    }
    if ((guest.player_x & 28) >= 8 && tile[385] == 1) {
        player_end(d7);
        return;
    }
    fall_begin(12);
}

/* ---- jumps ---- */

/* the rise of a jump ($114C2, $116F6 ...): up by the step's number of rows (twice while countdown_220 is not 0) */
static void jump_rise(void)
{
    int16_t rows = (int16_t)jump_deltas[guest.jump_step >> 1];

    guest.player_y -= rows;
    if (guest.countdown_220 != 0)
        guest.player_y -= rows;
}

/* the player's head hits a tile of type 1: he is put one tile down and falls (state 11 or 12) */
static int jump_bump(int16_t state)
{
    if (tile_one_at_player() == 0)
        return 0;
    guest.player_y &= -16;
    guest.player_y += 16;
    fall_begin(state);
    return 1;
}

/* orig $0115BC (state 9, a jump to the right) */
static void player_jump_right(int16_t d7)
{
    if (d7 == 12) {
        if (guest.jump_step >= 4)
            d7 = 5;
    } else if (d7 != 5) {
        if (d7 == 0) {
            if (guest.jump_step >= 4)
                d7 += 4;
        } else {
            d7 = 5;
        }
    }

    uint8_t *tile = player_tile_ptr();

    if ((guest.player_x & 31) >= 8 || (tile[1] != 1 && tile[129] != 1 && tile[257] != 1)) {
        if (guest.jump_step >= 4)
            guest.player_x += 4;
    }
    tile = player_tile_ptr();
    if (guest.game_stick & 1) {
        if (guest.player_x != guest.jump_start_x && (guest.player_x & 31) == 0 && tile[0] == 2) {
            guest.player_y &= -8;
            ladder_mount(14, 2);
            return;
        }
    }
    if (guest.game_stick & 2) {
        if (guest.player_x != guest.jump_start_x && (guest.player_x & 31) == 0 && (tile[0] == 2 || tile[128] == 2)) {
            guest.player_y &= -8;
            ladder_mount(13, 2);
            return;
        }
    }
    if (guest.jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    jump_rise();
    if (jump_bump(12))
        return;
    if (guest.jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    if (guest.jump_step >= 4 && (guest.game_buttons & 1) && claim_free_entries() == 0 && guest.jump_step < 44) {
        guest.player_state = 20;
        guest.word_36e = 1;
        guest.jump_step += 2;
        player_end(1);
        return;
    }
    guest.jump_step += 2;
    if (guest.jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(12);
}

/* orig $011390 (state 8, a jump to the left) */
static void player_jump_left(int16_t d7)
{
    if (d7 == 13) {
        if (guest.jump_step >= 4)
            d7 = 6;
    } else if (d7 != 6) {
        if (d7 == 0) {
            if (guest.jump_step >= 4)
                d7 += 5;
        } else {
            d7 = 6;
        }
    }

    uint8_t *tile = player_tile_ptr();

    if ((guest.player_x & 31) != 0 || (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1)) {
        if (guest.jump_step >= 4)
            guest.player_x -= 4;
    }
    if (guest.game_stick & 1) {
        if (guest.player_x != guest.jump_start_x && (guest.player_x & 31) == 0 && tile[0] == 2) {
            guest.player_y &= -8;
            ladder_mount(14, 2);
            return;
        }
    }
    if (guest.game_stick & 2) {
        if (guest.player_x != guest.jump_start_x && (guest.player_x & 31) == 0 && (tile[0] == 2 || tile[128] == 2)) {
            guest.player_y &= -8;
            ladder_mount(13, 2);
            return;
        }
    }
    if (guest.jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    jump_rise();
    if (jump_bump(11))
        return;
    if (guest.jump_step >= 18 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    if (guest.jump_step >= 4 && (guest.game_buttons & 1) && claim_free_entries() == 0 && guest.jump_step < 44) {
        guest.player_state = 21;
        guest.word_36e = 1;
        guest.jump_step += 2;
        player_end(1);
        return;
    }
    guest.jump_step += 2;
    if (guest.jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(11);
}

/* orig $0117AC (state 21, a jump to the left and firing) */
static void player_jump_fire_left(int16_t d7)
{
    if (guest.word_36e != 0) {
        d7--;
        guest.word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = player_tile_ptr();

    if ((guest.player_x & 31) != 0 || (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1))
        guest.player_x -= 4;
    if (guest.jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    jump_rise();
    if (jump_bump(11))
        return;
    if (guest.jump_step >= 18 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    if (d7 >= 3) {
        d7 = 6;
        guest.player_state = 8;
        guest.jump_step += 2;
        if (guest.jump_step < 46) {
            player_end(d7);
            return;
        }
        fall_begin(11);
        return;
    }
    if (d7 == 1)
        spawn_player_shots_left();
    guest.jump_step += 2;
    if (guest.jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(11);
}

/* orig $0118E6 (state 20, a jump to the right and firing) */
static void player_jump_fire_right(int16_t d7)
{
    if (guest.word_36e != 0) {
        d7--;
        guest.word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = player_tile_ptr();

    if ((guest.player_x & 31) >= 8 || (tile[1] != 1 && tile[129] != 1 && tile[257] != 1))
        guest.player_x += 4;
    if (guest.jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    jump_rise();
    if (jump_bump(12))
        return;
    if (guest.jump_step >= 18 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    if (d7 >= 3) {
        d7 = 5;
        guest.player_state = 9;
        guest.jump_step += 2;
        if (guest.jump_step < 46) {
            player_end(d7);
            return;
        }
        fall_begin(12);
        return;
    }
    if (d7 == 1)
        spawn_player_shots();
    guest.jump_step += 2;
    if (guest.jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(12);
}

/* ---- ladders ---- */

/* the frame of a step on a ladder counts 1 to 3 and then turns (word_376 is the direction of it, with a sound at the turn); the start of
 * $011ED6 and $011C50 */
static int16_t ladder_frame(int16_t d7)
{
    for (;;) {
        guest.word_360 = 0;
        guest.word_362 = 0;
        if (guest.word_376 == 0) {
            d7++;
            if (d7 <= 3)
                break;
            ladder_sounds();
            guest.word_376 = ~guest.word_376;
            d7--;
        }
        d7--;
        if (d7 != 0)
            break;
        ladder_sounds();
        d7++;
        guest.word_376 = ~guest.word_376;
    }
    return d7;
}

/* orig $011ED6: a step up the ladder: the frame goes on, the player 6 rows up; ends when the tile above is a ladder (type 2), otherwise the
 * step is taken back and the frame is the one of the start */
static void player_climb_up(int16_t d7)
{
    d7 = ladder_frame(d7);
    guest.word_37a = guest.player_y;
    guest.player_y -= 6;

    uint8_t *tile = player_tile_ptr();

    if (tile[128] == 2 || tile[-128] == 2) {
        player_end(d7);
        return;
    }
    d7 = guest.word_378;
    guest.player_y = guest.word_37a;
    player_end(d7);
}

/* orig $011C50: a step down the ladder: the player 6 rows down; at a tile of type 1 three rows down he stands (state 26), without a ladder
 * under him he falls (state 10) */
static void player_climb_down(int16_t d7)
{
    d7 = ladder_frame(d7);
    guest.player_y += 6;

    uint8_t *tile = player_tile_ptr();

    if (tile[384] == 1) {
        guest.player_state = 26;
        guest.player_y &= -16;
        player_end(0);
        return;
    }
    if (tile[256] == 2 || tile[384] == 2) {
        player_end(d7);
        return;
    }
    fall_begin(10);
}

/* orig $011E52 and $011BCC: fire on a ladder with the stick held to a side (word_370: 1 right, -1 left) shoots (state 24 right, 25 left); the
 * place and the frame are kept in ladder_shot_start_y and ladder_shot_frame.  Returns 1 when the frame is ended */
static int ladder_fire(int16_t d7)
{
    if (!(guest.game_buttons & 1) || guest.word_370 == 0)
        return 0;
    if (claim_free_entries() != 0)
        return 0;
    guest.ladder_shot_start_y = guest.player_y;
    guest.ladder_shot_frame = d7;
    if (!(d7 & 1))
        guest.player_y -= 4;
    guest.player_state = guest.word_370 > 0 ? 24 : 25;
    player_end(0);
    return 1;
}

/* the player on a ladder steps off it to the side: to state 1 (right) or 0 (left), $11DBE, $11B38, $11E40, $11BBA */
static void ladder_leave(int16_t state, int16_t dx)
{
    guest.player_state = state;
    guest.player_x += dx;
    guest.player_y -= 4;
    player_end(6);
}

/* orig $011D12 (state 14, on a ladder: up) */
static void player_ladder_up(int16_t d7)
{
    if (guest.frame_toggle == 0) {
        player_end(d7);
        return;
    }
    if (d7 >= 20) {
        player_climb_up(0);
        return;
    }
    guest.word_378 = d7;
    if (guest.game_stick == 1) {
        player_climb_up(d7);
        return;
    }
    if (!(guest.game_stick & 12)) {
        guest.word_360 = 0;
        guest.word_362 = 0;
    }
    if (guest.game_stick & 8) {
        if (guest.game_buttons & 1) {
            guest.word_370 = 1;
            goto fire;
        }
        if (guest.game_stick & 1) {
            jump_begin(9, 12);
            return;
        }
        if (guest.game_stick & 2) {
            guest.word_360 = 0;
            guest.word_362++;
            if (guest.word_362 > 2) {
                guest.word_362 = 0;

                uint8_t *tile = player_tile_ptr();

                if (tile[1] != 1 && tile[129] != 1 && tile[257] != 1 && ((guest.player_y & 15) == 0 || tile[385] != 1)) {
                    ladder_leave(1, 8);
                    return;
                }
            }
        }
    }
    if (guest.game_stick & 4) {
        if (guest.game_buttons & 1) {
            guest.word_370 = -1;
            goto fire;
        }
        if (guest.game_stick & 1) {
            jump_begin(8, 13);
            return;
        }
        if (guest.game_stick & 2) {
            guest.word_362 = 0;
            guest.word_360++;
            if (guest.word_360 > 2) {
                uint8_t *tile = player_tile_ptr();

                if (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1 && ((guest.player_y & 15) == 0 || tile[383] != 1)) {
                    ladder_leave(0, -8);
                    return;
                }
            }
        }
    }
fire:
    if (ladder_fire(d7))
        return;
    if (guest.game_stick & 2) {
        guest.word_376 = ~guest.word_376;
        guest.player_state = 13;
        guest.word_360 = 0;
        guest.word_362 = 0;
    }
    player_end(d7);
}

/* orig $011A94 (state 13, on a ladder: down) */
static void player_ladder_down(int16_t d7)
{
    if (guest.frame_toggle == 0) {
        player_end(d7);
        return;
    }
    if (d7 >= 20) {
        player_climb_down(0);
        return;
    }
    if (guest.game_stick == 2) {
        player_climb_down(d7);
        return;
    }
    if (!(guest.game_stick & 12)) {
        guest.word_360 = 0;
        guest.word_362 = 0;
    }
    if (guest.game_stick & 8) {
        if (guest.game_buttons & 1) {
            guest.word_370 = 1;
            goto fire;
        }
        if (guest.game_stick & 1) {
            jump_begin(9, 12);
            return;
        }
        if (guest.game_stick & 2) {
            guest.word_360 = 0;
            guest.word_362++;
            if (guest.word_362 > 2) {
                uint8_t *tile = player_tile_ptr();

                if (tile[1] != 1 && tile[129] != 1 && tile[257] != 1 && ((guest.player_y & 15) == 0 || tile[385] != 1)) {
                    ladder_leave(1, 8);
                    return;
                }
            }
        }
    }
    if (guest.game_stick & 4) {
        if (guest.game_buttons & 1) {
            guest.word_370 = -1;
            goto fire;
        }
        if (guest.game_stick & 1) {
            jump_begin(8, 13);
            return;
        }
        if (guest.game_stick & 2) {
            guest.word_362 = 0;
            guest.word_360++;
            if (guest.word_360 > 2) {
                uint8_t *tile = player_tile_ptr();

                if (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1 && ((guest.player_y & 15) == 0 || tile[383] != 1)) {
                    ladder_leave(0, -8);
                    return;
                }
            }
        }
    }
fire:
    if (ladder_fire(d7))
        return;
    if (guest.game_stick & 1) {
        guest.word_376 = ~guest.word_376;
        guest.player_state = 14;
        guest.word_360 = 0;
        guest.word_362 = 0;
    }
    player_end(d7);
}

/* orig $011A24 (state 24, a shot to the right from a ladder) */
static void player_ladder_shoot_right(int16_t d7)
{
    guest.word_370 = 1;
    d7++;
    if (d7 == 1)
        spawn_player_shots();
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    guest.player_state = 14;
    d7 = 3;
    if (!(guest.ladder_shot_frame & 1))
        d7 = guest.ladder_shot_frame;
    guest.player_y = guest.ladder_shot_start_y;
    player_end(d7);
}

/* orig $011A5C (state 25, a shot to the left from a ladder) */
static void player_ladder_shoot_left(int16_t d7)
{
    guest.word_370 = -1;
    d7++;
    if (d7 == 1)
        spawn_player_shots_left();
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    guest.player_state = 14;
    d7 = 1;
    if (!(guest.ladder_shot_frame & 1))
        d7 = guest.ladder_shot_frame;
    guest.player_y = guest.ladder_shot_start_y;
    player_end(d7);
}

/* orig $01159A (state 16, landing facing right) and $011578 (state 17, facing left): a frame of rest, then the fall distance runs down by 4 a
 * frame; at the end the walk (state 1 or 0) */
static void landing(int16_t d7, int16_t walk_state)
{
    if (guest.landing_frames == 0) {
        guest.landing_frames++;
        player_end(d7);
        return;
    }
    guest.fall_distance -= 4;
    if (guest.fall_distance >= 0) {
        player_end(d7);
        return;
    }
    guest.player_state = walk_state;
    player_end(2);
}

/* ---- things to take ---- */

/* orig $010B80: the thing in the chosen slot (item_shown) is dropped in front of the player (the slot's item + 11 is the kind) */
static void drop_chosen_item(void)
{
    panel_shutter_refresh();

    int16_t kind = (int16_t)(guest.slots_30c[guest.item_shown] + 11);

    if (kind < 0)
        return;
    guest.slots_30c[guest.item_shown] = -1;
    drop_thing(guest.player_x + 8, guest.player_y + 32, kind);
}

/* orig $010AB8 (and $010AB2 with word_368 set first), $010B2C */
static void pickup_10ab8(int16_t d7)
{
    guest.word_1fa = 0;
    panel_shutter_refresh();
    if (guest.word_368 != 0) {
        if (!(guest.game_buttons & 1))
            guest.word_368 = 0;
        player_end(d7);
        return;
    }
    if (!(guest.game_buttons & 1)) {
        player_end(d7);
        return;
    }
    /* orig $010B2C: the next slot is chosen */
    play_sound(33, 40);
    panel_shutter_refresh();
    guest.item_chosen = (guest.item_shown + 1) & 3;
    guest.word_368 = 1;
    guest.word_35c = 1;

    int16_t item = guest.slots_30c[guest.item_chosen];

    if (item >= 0)
        item_message(item);
    player_end(d7);
}

/* orig $010A48: the thing `record` (kind `kind`) is near: without the button it is put back, with it the kind is put in the chosen slot,
 * whose picture is made, and its message is shown */
static void pickup_take_record(struct Slot8 *record, int16_t kind, int16_t d7)
{
    if (!(guest.game_buttons & 1)) {
        guest.word_35c = 0;
        record->w[2] = kind;
        record->w[3] = 1;
        player_end(d7);
        return;
    }
    guest.word_368 = 1;
    guest.slots_30c[guest.item_shown] = kind;
    panel_slot_plain(SLOT_IMAGE((int16_t)guest.item_shown), kind);
    panel_shutter_refresh();
    item_message(guest.slots_30c[guest.item_chosen]);
    player_end(d7);
}

/* orig $0105DC: looks for a thing near the player (the first record near him, with a kind under 192 that is a thing to take: offsets_4cba says 2) */
static void pickup_find_near(int16_t d7)
{
    guest.word_35c = 1;

    int16_t x = guest.player_x + 16;
    int16_t y = guest.player_y + 40;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &guest.records_19b14[i];

        if (record->w[2] < 0 || record->w[3] == 0)
            continue;
        if (x < record->w[0] - 4 || x > record->w[0] + 20 || y < record->w[1] - 12 || y > record->w[1] + 16)
            continue;
        if (record->w[2] >= 192) {
            player_end(d7);
            return;
        }
        if (((const uint8_t *)offsets_4cba)[record->w[2]] != 2)
            continue;

        int16_t kind = record->w[2];

        record->w[2] = -1;
        record->w[3] = 0;
        pickup_take_record(record, kind, d7);
        return;
    }
    guest.word_1fa = 1;
    player_end(d7);
}

/* orig $01056A (state 19, in front of a thing the player can take or open, facing right) and $0104F8 (state 18, facing left: it leaves to state
 * 1 instead of 0) */
static void state_pickup(int16_t d7, int16_t leave_state)
{
    if (!(guest.game_stick & 2)) {
        if (guest.slots_30c[guest.item_shown] >= 0)
            drop_chosen_item();
        d7--;
        if (d7 >= 0) {
            player_end(d7);
            return;
        }
        guest.player_state = leave_state;
        player_end(2);
        return;
    }
    d7 = 1;
    if (guest.word_35c != 0) {
        if (guest.word_1fa == 0) {
            pickup_10ab8(d7);
            return;
        }
        if (guest.game_buttons & 1) {
            guest.word_368 = 1;
            pickup_10ab8(d7);
            return;
        }
        if (guest.slots_30c[guest.item_shown] >= 0)
            drop_chosen_item();
        if (guest.word_35c != 0) {
            player_end(d7);
            return;
        }
    }
    pickup_find_near(d7);
}

/* ---- walking ---- */

/* orig $0124DE: the frame is kept and mapped through frame_map_125ac; the machine goes on at $1263E (states 5 and 6) */
static void state_5_map(int16_t d7)
{
    guest.word_19a = d7;
    player_after((int16_t)frame_map_125ac[d7]);
}

/* orig $011FCA (state 0, walking to the left) */
static void player_walk_left(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1 && ((guest.player_x & 30) < 8 || tile[385] != 1)) {
        fall_begin(11);
        return;
    }
    if (guest.game_stick & 8) {
        guest.player_state = 2;
        player_end(0);
        return;
    }
    if (guest.game_stick & 2) {
        if (!record_near_player()) {
            tile = player_tile_ptr();
            if ((guest.player_x & 31) >= 12 && tile[385] == 2) {
                if (!(guest.game_stick & 12)) {
                    guest.player_x += 32;
                    ladder_mount(13, 26);
                    return;
                }
            } else if ((guest.player_x & 31) <= 20 && tile[384] == 2) {
                ladder_mount(13, 26);
                return;
            }
        }
        pickup_enter(19);
        return;
    }
    if (guest.game_stick & 1) {
        if (guest.game_stick & 4) {
            jump_begin(8, 0);
            return;
        }

        int16_t d0 = guest.player_x & 31;

        if (d0 <= 20) {
            if (tile[0] == 2) {
                ladder_mount(14, 26);
                return;
            }
            if (d0 >= 12 && tile[1] == 2) {
                guest.player_x += 32;
                ladder_mount(14, 26);
                return;
            }
        } else if (tile[1] == 2) {
            guest.player_x += 32;
            ladder_mount(14, 26);
            return;
        }
    }
    if ((guest.game_buttons & 1) && claim_free_entries() == 0) {
        guest.player_state = 6;
        state_5_map(0);
        return;
    }
    if (guest.word_19e != 0) {
        d7 = (int16_t)((d7 + 1) & 7);
        guest.word_19e = 0;
        guest.player_x -= 4;
        player_end(d7);
        return;
    }
    if (guest.game_stick & 4) {
        if ((guest.player_x & 31) == 0 && (tile[-1] == 1 || tile[127] == 1 || tile[255] == 1)) {
            player_end(d7);
            return;
        }
        guest.player_x -= 4;
        if (d7 == 2 || d7 == 6)
            play_sound(1, 2);
        guest.word_19e = 1;
        guest.word_19e = 0;
        if (d7 > 7) {
            d7 = 7;
            guest.player_x += 4;
        }
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (guest.game_stick & 1) {
        guest.word_35e = 0;
        guest.player_state = 26;
        player_end(-1);
        return;
    }
    player_end(d7 == 0 ? d7 : 57);
}

/* orig $012290 (state 1, walking to the right) */
static void player_walk_right(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1 && ((guest.player_x & 31) < 8 || tile[385] != 1)) {
        fall_begin(12);
        return;
    }
    if (guest.game_stick & 4) {
        guest.player_state = 3;
        player_end(2);
        return;
    }
    if (guest.game_stick & 2) {
        if (!record_near_player()) {
            tile = player_tile_ptr();

            int16_t d0 = guest.player_x & 31;

            if (d0 >= 12 && tile[385] == 2) {
                guest.player_x += 32;
                ladder_mount(13, 28);
                return;
            }
            if (d0 <= 20) {
                if (!(guest.game_stick & 12) && tile[384] == 2) {
                    ladder_mount(13, 28);
                    return;
                }
            }
        }
        pickup_enter(18);
        return;
    }
    if (guest.game_stick & 1) {
        if (guest.game_stick & 8) {
            jump_begin(9, 0);
            return;
        }

        int16_t d0 = guest.player_x & 31;

        if (d0 <= 20) {
            if (tile[0] == 2) {
                ladder_mount(14, 28);
                return;
            }
            if (d0 >= 12 && tile[1] == 2) {
                guest.player_x += 32;
                ladder_mount(14, 28);
                return;
            }
        } else if (tile[1] == 2) {
            guest.player_x += 32;
            ladder_mount(14, 28);
            return;
        }
    }
    if ((guest.game_buttons & 1) && claim_free_entries() == 0) {
        guest.player_state = 5;
        state_5_map(0);
        return;
    }
    if (guest.word_19e != 0) {
        guest.word_19e = 0;
        guest.player_x += 4;
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (guest.game_stick & 8) {
        if ((guest.player_x & 31) < 8 && (tile[1] == 1 || tile[129] == 1 || tile[257] == 1)) {
            player_end(d7);
            return;
        }
        guest.player_x += 4;
        if (d7 == 2 || d7 == 6)
            play_sound(1, 2);
        guest.word_19e = 1;
        guest.word_19e = 0;
        if (d7 > 7) {
            d7 = 7;
            guest.player_x -= 4;
        }
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (guest.game_stick & 1) {
        guest.word_35e = 0;
        guest.player_state = 26;
        player_end(1);
        return;
    }
    player_end(d7 == 0 ? d7 : 45);
}

/* orig $0124BA (state 5, shooting to the right) and $012508 (state 6, to the left): the frame goes on; at 3 the shots are spawned; from 5 on, while
 * fire is held (and a shot can be made) it begins again, otherwise the player goes back to walking (state 1 or 0) */
static void player_shoot_right(int16_t d7)
{
    d7++;
    if (d7 == 3)
        spawn_player_shots();
    if (d7 >= 5) {
        if ((guest.game_buttons & 1) && claim_free_entries() == 0) {
            state_5_map(0);
            return;
        }
        guest.player_state = 1;
        player_walk_right(2);
        return;
    }
    state_5_map(d7);
}

static void player_shoot_left(int16_t d7)
{
    d7++;
    if (d7 == 3)
        spawn_player_shots_left();
    if (d7 >= 5) {
        if ((guest.game_buttons & 1) && claim_free_entries() == 0) {
            state_5_map(0);
            return;
        }
        guest.player_state = 0;
        player_walk_left(2);
        return;
    }
    state_5_map(d7);
}

/* orig $012538 (state 2, turning to the right) */
static void player_turn_right(int16_t d7)
{
    if (guest.game_stick & 4) {
        guest.player_state = 3;
        player_end(d7);
        return;
    }
    d7++;
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    guest.player_state = 1;
    player_end(6);
}

/* orig $012560 (state 3, turning to the left) */
static void player_turn_left(int16_t d7)
{
    if (guest.game_stick & 8) {
        guest.player_state = 2;
        player_end(d7);
        return;
    }
    d7--;
    if (d7 >= 0) {
        player_end(d7);
        return;
    }
    guest.player_state = 0;
    player_end(6);
}

/* orig $012582 (state 4) */
static void state_12582(int16_t d7)
{
    if (guest.game_stick & 8) {
        guest.player_state = 2;
        player_end(2);
        return;
    }
    if (guest.game_stick & 4) {
        guest.player_state = 3;
        player_end(0);
        return;
    }
    player_check_floor(d7);
}

/* orig $01039E (state 26, at the foot of a ladder): up looks for pickups and, at a ladder tile, mounts it (state 14) */
static void player_ladder_foot(int16_t d7)
{
    d7 = 0;
    if (guest.game_stick & 8) {
        guest.player_state = 28;
        player_end(d7);
        return;
    }
    if (guest.game_stick & 4) {
        guest.player_state = 27;
        player_end(d7);
        return;
    }
    if (guest.game_buttons & 1) {
        use_lever_3c94();
        player_end(d7);
        return;
    }
    guest.word_35e = 0;
    if (!(guest.game_stick & 1)) {
        player_check_floor(d7);
        return;
    }
    check_pickups();
    if (guest.flag_1f0 != 0) {
        player_end(d7);
        return;
    }

    uint8_t *tile = player_tile_ptr();

    if ((guest.player_x & 31) >= 16) {
        if (tile[1] != 2 || tile[129] != 2) {
            player_end(d7);
            return;
        }
        guest.player_x += 32;
    } else if (tile[0] != 2 && tile[128] != 2) {
        player_end(d7);
        return;
    }
    ladder_mount(14, 26);
}

/* the routine of the state (the table at $10280) with the animation frame d7 */
static void player_state_run(int16_t state, int16_t d7)
{
    switch (state) {
    case 0:  player_walk_left(d7); break;
    case 1:  player_walk_right(d7); break;
    case 2:  player_turn_right(d7); break;
    case 3:  player_turn_left(d7); break;
    case 4:  state_12582(d7); break;
    case 5:  player_shoot_right(d7); break;
    case 6:  player_shoot_left(d7); break;
    case 8:  player_jump_left(d7); break;
    case 9:  player_jump_right(d7); break;
    case 10: player_fall_off_ladder(d7); break;
    case 11: player_fall_left(d7); break;
    case 12: player_fall_right(d7); break;
    case 13: player_ladder_down(d7); break;
    case 14: player_ladder_up(d7); break;
    case 15: player_check_floor(d7); break;
    case 16: landing(d7, 1); break;
    case 17: landing(d7, 0); break;
    case 18: state_pickup(d7, 1); break;
    case 19: state_pickup(d7, 0); break;
    case 20: player_jump_fire_right(d7); break;
    case 21: player_jump_fire_left(d7); break;
    case 22: player_fall_fire_right(d7); break;
    case 23: player_fall_fire_left(d7); break;
    case 24: player_ladder_shoot_right(d7); break;
    case 25: player_ladder_shoot_left(d7); break;
    case 26: player_ladder_foot(d7); break;
    case 27:
        guest.player_state = 0;
        player_end(2);
        break;
    case 28:
        guest.player_state = 1;
        player_end(2);
        break;
    default:
        break;
    }
}

/* orig $010368: the player for one frame: the routine of the state named by player_state runs (see the table at $10280): 0 and 1 walk, 2 to
 * 4 turn, 5 and 6 shoot, 8 and 9 jump, 10 to 12 fall, 13 and 14 climb, 16 and 17 land, 18 and 19 handle things, 20 to 23 jump and fall
 * firing, 24 and 25 shoot on a ladder, 26 mounts a ladder, 27 and 28 leave it.  The state 7 and those over 28 point into data and are not
 * states */
void player_control(void)
{
    int16_t d7 = guest.word_19a;
    int16_t state = guest.player_state;

    if (state != 18 && state != 19)
        game_step_5496();
    if (guest.code_3436 == 20081 || guest.flag_290 >= 0) {
        player_end(d7);
        return;
    }
    player_state_run(state, d7);
}


/* ======== from game_g.c ======== */
/* Gods, the game: the shot routines of the object lists at $168AE and $16A66 (what game_c.c's update_shots and run_object_list do not
 * cover).  Each function says where the original has it (`orig $ADDR`). */

#define SHOT_COUNT 9

/* orig $016A66: the cells of the map at line_text + 100 (48 columns of 8 pixels) under the point (x, y) get the low byte of the shot's first
 * word: three side by side in each of three rows (the rows start one row lower when bit 2 of y is set), when the first of them is in the map
 * (from line_text + 48 to + 1504) */
void mark_shot_cells_3x3(int16_t x, int16_t y, const struct Shot *shot)
{
    const uint8_t *map = guest.line_text;
    int16_t row = (int16_t)((y & -8) * 2);
    uint8_t *cell = (uint8_t *)(map + 100) + (x >> 3) + row;
    uint8_t value = (uint8_t)shot->active;

    cell += (int16_t)(row * 2);
    if (y & 4)
        cell += 48;
    if (cell < map + 48 || cell >= map + 48 + 1505)
        return;
    cell[0] = value;
    cell[1] = value;
    cell[2] = value;
    cell[48] = value;
    cell[49] = value;
    cell[50] = value;
    cell[96] = value;
    cell[97] = value;
    cell[98] = value;
}

/* orig $0168AE: the nine shots of a list whose object has the handler 1: each is moved by the delta of its direction and, in the column its
 * number (less word_fb2c, less one) says, down by that column's counter in shot_offsets_6a4 (the counter then counts up); a shot that is on the
 * screen is hit-tested (shot_hit_tile) and drawn as a glyph while it lives; one outside is cleared.  The object's frame goes on by its
 * anim_mode, modulo 4. */
void update_shots_sliding(struct ShotList *list, struct Object *object)
{
    struct Shot *shot = list->shot;

    for (int n = 0; n < SHOT_COUNT; n++, shot++) {
        if (shot->active == 0)
            continue;
        int16_t x = shot->x;
        int16_t y = shot->y;
        int16_t direction = shot->direction;

        guest.shot_prev_x = x;
        guest.shot_prev_y = y;
        x += shot_delta(direction * 2);
        y += shot_delta(direction * 2 + 1);
        int16_t column = (int16_t)(shot->active - guest.word_fb2c - 1);

        y += guest.shot_offsets_6a4[column];
        guest.shot_offsets_6a4[column]++;
        shot->x = x;
        shot->y = y;
        x -= guest.scroll_x;
        y -= guest.scroll_y;
        if (x <= -32 || y <= -32 || x >= 320 || y >= 192) {
            shot->active = 0;
            continue;
        }
        int16_t frame = object->frame + object->base_frame;

        shot_hit_tile(object, shot, x, y);
        if (shot->active != 0)
            glyph_draw(x, y, (uint16_t)frame);
    }
    object->frame = (int16_t)((object->frame + object->anim_mode) & 3);
}

/* orig $018560 (with the speeds d2 and d3): the first free record of records_3a0a6 is started at the screen place (x, y) with the speeds and a count of 58 */
static void start_debris_moving(int16_t x, int16_t y, int16_t speed_x, int16_t speed_y)
{
    for (int i = 0; i < 20; i++) {
        int16_t *record = &guest.records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x + guest.scroll_x;
            record[2] = y + guest.scroll_y;
            record[3] = speed_x;
            record[4] = speed_y;
            record[5] = 58;
            guest.records_3a0a6_in_use++;
            return;
        }
    }
}

/* orig $018560 (with d2 = d3 = 0): the first free record of records_3a0a6 (first word negative) is started at the screen place (x, y) with no
 * speed and a count of 58 */
static void start_debris_at(int16_t x, int16_t y)
{
    for (int i = 0; i < 20; i++) {
        int16_t *record = &guest.records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x + guest.scroll_x;
            record[2] = y + guest.scroll_y;
            record[3] = 0;
            record[4] = 0;
            record[5] = 58;
            guest.records_3a0a6_in_use++;
            return;
        }
    }
}

/* orig $006200: whether the bolt at (w[0], w[1]) is in a wall: the tile of the second map under (x, y + 2) is type 1, or the bolt is more than
 * 24 pixels into its tile and the next tile is type 1 */
static int bolt_in_wall(const struct Slot8 *bolt)
{
    int16_t x = bolt->w[0];
    int16_t y = (int16_t)(bolt->w[1] + 2);
    const uint8_t *tile = guest.ptr_0b86 + 8192;

    tile += (int16_t)((x & -32) >> 5);
    tile += (int16_t)((y & -16) << 3);
    if (tile[0] == 1)
        return 1;
    if ((x & 31) <= 24)
        return 0;
    return tile[1] == 1;
}

/* orig $006276: the bolt at the screen place (x, y) hits the player when its point (x + 10, y + 4 in the level, 4 more if the player is ducking
 * in state 18 or 19) lies in the player's box (32 wide, 48 high) and flag_aa8 is 0: the player loses 2 energy, the bolt is freed and a
 * debris starts at (x, y) */
static void bolt_hits_player(struct Slot8 *bolt, int16_t x, int16_t y)
{
    int16_t px = (int16_t)(x + 8 + guest.scroll_x + 2);
    int16_t py = (int16_t)(y + 4 + guest.scroll_y);
    int16_t top = guest.player_y;

    if (guest.word_19c >= 18 && guest.word_19c <= 19)
        top += 24;
    if (py < top || px < guest.player_x)
        return;
    if (px > (int16_t)(guest.player_x + 32) || py > (int16_t)(guest.player_y + 48))
        return;
    if (guest.flag_aa8 != 0)
        return;
    guest.player_energy -= 2;
    bolt->w[3] = -1;
    start_debris_at(x, y);
}

/* orig $006136: the twenty records of 8 bytes that ptr_0cce points at (x, y, dx, state; state negative: free) are bolts flying sideways.
 * One with dx 0 is an explosion: its state counts up, freed at 3 and 6, drawn as the glyph 69 + state.  One with dx other than 0 moves by
 * dx; when it has left the screen (to the right 320 or more, to the left 16 or more) it is freed; when it is in a wall it becomes an
 * explosion (state 4 moving right, 1 moving left, dx 0) drawn as that; else it may hit the player, and is drawn as the glyph 72 (right) or
 * 69 (left) */
void update_bolts(void)
{
    struct Slot8 *bolt = (struct Slot8 *)guest.ptr_0cce;

    for (int i = 0; i < 20; i++, bolt++) {
        if (bolt->w[3] < 0)
            continue;
        int16_t x = bolt->w[0];
        int16_t y = bolt->w[1];
        int16_t dx = bolt->w[2];

        x += dx;
        if (dx == 0) {
            bolt->w[3]++;
            if (bolt->w[3] == 3 || bolt->w[3] == 6) {
                bolt->w[3] = -1;
                continue;
            }
            glyph_draw(x - guest.scroll_x, y - guest.scroll_y, 69 + bolt->w[3]);
            continue;
        }
        bolt->w[0] = x;
        x -= guest.scroll_x;
        y -= guest.scroll_y;
        if (dx > 0) {
            if (x >= 320) {
                bolt->w[3] = -1;
                continue;
            }
        } else if (x <= -16) {
            bolt->w[3] = -1;
            continue;
        }
        if (bolt_in_wall(bolt)) {
            bolt->w[3] = dx < 0 ? 1 : 4;
            bolt->w[2] = 0;
            glyph_draw(x, y, 69 + bolt->w[3]);
            continue;
        }
        bolt_hits_player(bolt, x, y);
        glyph_draw(x, y, dx > 0 ? 72 : 69);
    }
}


/* ======== from game_h.c ======== */
/* Gods, the game: what Begin Quest does from the fire button of the menu to the first frame of the game loop: the original's
 * code at $651E, $2452 to $2C08 and the routines it calls that are not in the other game modules.  Each function says where the
 * original has it (`orig $ADDR`).  Where the original goes on at an address that did not run for level 1 (the other levels, a
 * file that cannot be read: load_file does not return then) the comment says `goes on at $ADDR`. */

/* the file names of the level, from the template at $2752 (level_name_template): the offsets of the ones that are used */
#define NAME_SPRITES 10
#define NAME_OBJECTS 22
#define NAME_WEAPONS 34
#define NAME_BITS 74
#define NAME_PATTERN 86
#define NAME_ALFILS 170
#define NAME_XTRA 182

/* ---- small clear and fill routines ---- */

/* orig $006096: 160 bytes at ptr_0cce are set to $FF */
void fill_cce_ones(void)
{
    uint32_t *p = (uint32_t *)guest.ptr_0cce;

    for (int i = 0; i < 40; i++)
        *p++ = 0xFFFFFFFF;
}

/* orig $0088CC: the 180 bytes of records_12a0 are cleared */
void clear_walk_groups(void)
{
    uint8_t *p = (uint8_t *)guest.records_12a0;

    for (int i = 0; i < 180; i++)
        *p++ = 0;
}

/* orig $00890C: the 4280 bytes of monster_records are cleared */
void clear_monsters(void)
{
    uint8_t *p = (uint8_t *)guest.monster_records;

    for (int i = 0; i < 4280; i++)
        *p++ = 0;
}

/* orig $00D650: the word at +4 of the ten records of records_113c is cleared */
void clear_records_113c(void)
{
    for (int i = 0; i < 10; i++)
        guest.records_113c[i].w[2] = 0;
}

/* orig $00D87E: word_10ae is cleared and so is the byte at +9 of the ten records (12 bytes) at spore_records */
void clear_records_1036(void)
{
    guest.word_10ae = 0;
    for (int i = 0; i < 10; i++)
        guest.spore_records[i * 12 + 9] = 0;
}

/* orig $00D6A0 (with $00D69C, $00DE26 and $00E392, which set up its arguments): the first word of ten records of 10 bytes at `base` is
 * set to -1, and the routine at $D6B8 is switched off (its first word becomes an rts) */
void reset_slots_10(int16_t *base)
{
    for (int i = 0; i < 10; i++)
        base[i * 5] = -1;
    guest.code_d6b8 = 0x4E75;
}

/* orig $00D69C */
void reset_slots_fd0(void)
{
    reset_slots_10(guest.pod_records);
}

/* orig $00E392: the routine at $E3A2 is switched off, then as $D69C for the records at $11B4 */
void reset_slots_e392(void)
{
    guest.code_e3a2 = 0x4E75;
    reset_slots_10(guest.falling_things);
}

/* orig $00DE26: the routines at $DE54 and $E142 are switched off; in level 2 the 128 words of table_ed0 are -1; then as $D69C for the
 * records at $11B4 */
void reset_slots_de26(void)
{
    guest.code_de54 = 0x4E75;
    guest.code_e142 = 0x4E75;
    if (guest.param_index == 2)
        for (int i = 0; i < 128; i++)
            guest.table_ed0[i] = -1;
    reset_slots_10(guest.falling_things);
}

/* orig $00DAEA: the 140 bytes at bytes_10b0 are set to $FF, the routine at $DB04 is switched off */
void fill_bytes_10b0(void)
{
    for (int i = 0; i < 140; i++)
        guest.bytes_10b0[i] = 0xFF;
    guest.code_db04 = 0x4E75;
}

/* orig $00F9A6: the 200 bytes of near_flags are set to $FF */
void fill_flags_39fde(void)
{
    uint32_t *p = (uint32_t *)guest.near_flags;

    for (int i = 0; i < 50; i++)
        *p++ = 0xFFFFFFFF;
}

/* orig $00FA46: pickup_glyph is the word of table_fa5c that the level (from 1) names */
void set_pickup_glyph(void)
{
    guest.pickup_glyph = table_fa5c[(int16_t)(guest.param_index - 1)];
}

/* orig $003F2A: the first long of the ten effects (10 bytes each) is -1, effect_alive is cleared */
void clear_effects(void)
{
    for (int i = 0; i < 10; i++) {
        guest.effects[i * 5] = -1;
        guest.effects[i * 5 + 1] = -1;
    }
    guest.effect_alive = 0;
}

/* orig $007464: ten records of 8 bytes at records_737e: a long -1 and a long 0; word_73ce is cleared */
void clear_records_737e(void)
{
    for (int i = 0; i < 10; i++) {
        guest.records_737e[i * 2] = 0xFFFFFFFF;
        guest.records_737e[i * 2 + 1] = 0;
    }
    guest.word_73ce = 0;
}

/* orig $017C68: the forty records of 6 bytes at table_1a30c are set to -1 */
void fill_table_1a30c(void)
{
    for (int i = 0; i < 120; i++)
        guest.table_1a30c[i] = 0xFFFF;
}

/* orig $017C7C: the hundred longs of marker_table that are not -1 are looked at (a marker's cell gets 109 in the map's other half and 1
 * in the cell, and the marker is -1); they all are -1 at the start of a level */
void clear_markers(void)
{
    for (int i = 0; i < 100; i++) {
        if (guest.marker_table[i] == 0xFFFFFFFF)
            continue;
        uint8_t *cell = (uint8_t *)(uintptr_t)guest.marker_table[i];

        cell[-8192] = 109;
        cell[0] = 1;
        guest.marker_table[i] = 0xFFFFFFFF;
    }
}

/* orig $018400: twenty records of 22 bytes at bolt_records get -1 in their first long; word_186be is cleared */
void clear_bolts(void)
{
    for (int i = 0; i < 20; i++)
        guest.bolt_records[i].routine = 0xFFFFFFFF;
    guest.word_186be = 0;
}

/* orig $00725A: flag_721c is cleared and the thirty words of records_721e are -1 */
void clear_records_721e(void)
{
    guest.flag_721c = 0;
    for (int i = 0; i < 30; i++)
        guest.records_721e[i] = -1;
}

/* orig $0077BE: the word at +4 of the 42 records (10 bytes) at records_7544 is -1; the routines at $77E6 and $7906 are switched off */
void reset_records_7544(void)
{
    for (int i = 0; i < 42; i++)
        guest.records_7544[i * 5 + 2] = -1;
    guest.code_77e6 = 0x4E75;
    guest.code_7906 = 0x4E75;
}

/* orig $00FA64: the first word of the five records (24 bytes) of `units` is cleared, so is the word at +6 of the ten
 * creature types (36 bytes) of table_199ac; then clear_records_1036 */
void clear_creature_state(void)
{
    for (int i = 0; i < 5; i++)
        guest.units[i].state = 0;
    for (int i = 0; i < 10; i++)
        *(uint16_t *)(guest.table_199ac + i * 36 + 6) = 0;
    clear_records_1036();
}

/* orig $00FA9C: the first word of each of the three lists of shots is -1 and the rest of it (72 words) is cleared; word_214 is -1 */
void clear_shot_lists(void)
{
    uint16_t *p;

    guest.slot_ref_6b6 = -1;
    p = &guest.shot_lists_raw[1];
    clear_72_words(p);
    guest.slot_ref_748 = -1;
    clear_72_words((uint16_t *)((uint8_t *)&guest.slot_ref_748 + 2));
    guest.slot_ref_7da = -1;
    clear_72_words((uint16_t *)((uint8_t *)&guest.slot_ref_7da + 2));
    guest.word_214 = -1;
}

/* orig $0134E6: the sounds 19, 23, 52, 60, 58 and 66 are stopped, then the program waits for the vertical blank until the sound
 * system has nothing to play (the long at $207A0 is 0) */
void stop_game_sounds(void)
{
    stop_sound(19);
    stop_sound(23);
    stop_sound(52);
    stop_sound(60);
    stop_sound(58);
    stop_sound(66);
    do {
        wait_vblank();
    } while (guest.sound_busy_207a0 != 0);
}

/* orig $016C14 and $016C1E: the 24 bytes at $16BCC (the same twelve words as the ones that are at $16BB4 at the start: the original
 * copies them again) are copied to $16BB4 (the data the game reads there) */
void copy_block_16bb4(void)
{
    static const uint16_t words[12] = { 0x000C, 0xFFFD, 0x000C, 0x0000, 0x000C, 0x0003, 0xFFF4, 0xFFFD, 0xFFF4, 0x0000, 0xFFF4, 0x0003 };

    for (int i = 0; i < 12; i++)
        ((uint16_t *)guest.block_16bb4)[i] = words[i];
}

/* orig $0017DBA: the 25 records of 24 bytes at ptr_0c9e get -1 in their first two words and in the byte at +4; the cells
 * marked by the records are given back */
void clear_cell_records(void)
{
    struct PathRecord *record = guest.ptr_0c9e;

    for (int i = 0; i < 25; i++, record++) {
        record->x = -1;
        record->y = -1;
        record->kind = -1;
    }
    restore_saved_bytes((struct SavedByte *)guest.entry_table, 49);
}

/* orig $00F398: the 506 bytes at bytes_dest become words at words_source (the opposite of narrow_words_to_bytes), and the last four
 * words are then 11, 0, 6 and 0 */
void widen_bytes_to_words(void)
{
    uint16_t *to = (uint16_t *)guest.words_source;
    const uint8_t *from = guest.bytes_dest;

    for (int i = 0; i < 506; i++)
        *to++ = *from++;
    to -= 4;
    to[0] = 11;
    to[1] = 0;
    to[2] = 6;
    to[3] = 0;
}

/* orig $00F3C0 and $00F382 are in menu.c.  orig $00F9B8: records_19b14 (200 records of 8 bytes) are put in order by a bubble sort: a record
 * whose kind (the word at +4) is not an entry of table_14cf8, or one of kind 3 or 5, goes behind the others */
void sort_records_by_kind(void)
{
    uint8_t *base = (uint8_t *)guest.records_19b14;

    for (int outer = 0; outer < 199; outer++) {
        int16_t d0 = 0;

        for (int inner = 0; inner < 199; inner++, d0 += 8) {
            int16_t d1 = *(int16_t *)(base + d0 + 4);
            int swap = 0;

            if (d1 < 0 || d1 >= 192) {
                swap = 1;
            } else {
                int16_t d2 = *(int16_t *)(base + d0 + 12);

                if (d2 < 0 || d2 >= 192)
                    continue;
                const uint8_t *a2 = (const uint8_t *)guest.table_14cf8 + d1 * 10;
                const uint8_t *a3 = (const uint8_t *)guest.table_14cf8 + d2 * 10;
                int16_t k1 = *(const int16_t *)(a2 + 4);
                int16_t k2 = *(const int16_t *)(a3 + 4);

                if (k1 == 3 || k1 == 5)
                    continue;
                swap = (k2 == 3 || k2 == 5);
                if (!swap)
                    continue;
            }
            if (swap) {
                uint32_t t = *(uint32_t *)(base + d0);

                *(uint32_t *)(base + d0) = *(uint32_t *)(base + d0 + 8);
                *(uint32_t *)(base + d0 + 8) = t;
                t = *(uint32_t *)(base + d0 + 4);
                *(uint32_t *)(base + d0 + 4) = *(uint32_t *)(base + d0 + 12);
                *(uint32_t *)(base + d0 + 12) = t;
            }
        }
    }
}


/* orig $01D06C: the program's random number: the seed is a word (random_seed); the new seed is made from it with the rotations and
 * additions of the original (the carry bit runs through the rotations as the extend flag of the 68000 does) and is the result */
uint16_t random_next(void)
{
    uint16_t seed = guest.random_seed;
    uint8_t d7 = (uint8_t)seed;
    uint8_t d0 = (uint8_t)(seed >> 8);
    uint8_t d6 = d0;
    unsigned x = (seed >> 7) & 1;   /* the extend flag after the shift by 8: the last bit shifted out */

#define ROXR(v, n)                                                \
    do {                                                          \
        for (int k = 0; k < (n); k++) {                           \
            unsigned out = (v) & 1;                               \
            (v) = (uint8_t)(((v) >> 1) | (x << 7));               \
            x = out;                                              \
        }                                                         \
    } while (0)
    ROXR(d0, 1);
    d6 ^= d0;
    ROXR(d0, 2);
    d6 ^= d0;
    ROXR(d0, 1);
    d7 ^= d0;
    ROXR(d0, 2);
    d0 &= 1;
    x = d7 >> 7;
    d7 = (uint8_t)(d7 << 1);
    {
        unsigned out = d6 >> 7;

        d6 = (uint8_t)((d6 << 1) | x);
        x = out;
    }
    d0 = (uint8_t)(d0 + d7);
    d7 = d0;
    seed = (uint16_t)((d6 << 8) | d7);
    guest.random_seed = seed;
    return seed;
#undef ROXR
}

/* the seed of the random numbers is made of two bytes the original reads from $FF8207 and $FF8209 (video_address_high, video_address_low): the video address
 * registers of the Atari ST the game came from, an address the Amiga does not have (the emulated machine gives $64 and $FF; the SDL port defines them so) */

/* orig $0062E6: the table of 256 random words at random_words is made (the seed comes from two bytes of the address space that the
 * program reads: they were the video address of the Atari ST the game came from); one word at an even offset up to 254 and
 * one up to 510 are cleared, random_index is cleared */
void random_table_make(void)
{
    uint16_t *table = (uint16_t *)guest.random_words;
    uint16_t or_all;

    do {
        guest.random_seed = (uint16_t)((video_address_high << 8) | video_address_low);
        or_all = 0;
        for (int i = 0; i < 256; i++) {
            uint16_t r = random_next();

            or_all |= r;
            table[i] = r;
        }
    } while (or_all == 0);
    guest.random_index = 0;
    {
        uint16_t offset = (uint16_t)(random_next() & 254);

        *(uint16_t *)((uint8_t *)table + offset) = 0;
        offset = (uint16_t)(random_next() & 510);
        *(uint16_t *)((uint8_t *)table + offset) = 0;
    }
}

/* orig $00D5EE: when the password is used (word_0266) the map of the level is searched for the cells 35 and 36 (level 1) that the
 * entries of table_ed0 are made of (goes on at $00D5FE, not run); otherwise the first two longs of table_ed0 are -1 */
void scan_password_cells(void)
{
    uint32_t *entry = (uint32_t *)guest.table_ed0;

    if (guest.word_0266 != 0)
        return;   /* goes on at $00D5FE */
    entry[0] = 0xFFFFFFFF;
    entry[1] = 0xFFFFFFFF;
}

/* orig $00D59E: clear_records_113c; in level 1 the entries of table_ed0 (four words each, 32 of them) that are not -1 in their
 * first long put their value into the map of the level; word_1ea is cleared.  (At the start of the level the first entry is -1:
 * goes on at $00D5B6 otherwise, not run) */
void records_113c_reset(void)
{
    clear_records_113c();
    if (guest.param_index == 1) {
        const int32_t *entry = (const int32_t *)guest.table_ed0;

        if (entry[0] >= 0) {
            /* goes on at $00D5B6 (not run) */
            return;
        }
    }
    guest.index_1ea = 0;
}

/* orig $00F3C0 is assign_addresses (menu.c). orig $00F302: the entry list of the level (god01.pat, 5400 bytes) is loaded and given
 * its addresses, and then the 506 words are made bytes: `names` is the file names of the level.  With the password ($266) the
 * file is not read; with a failed read of the level's map ($286 clear) the original loads the files again: not run */
void load_pattern_data(const uint8_t *names)
{
    if (guest.word_0266 == 0) {
        load_unpacked((const char *)names + NAME_PATTERN, guest.entry_list);
        file_longs(guest.entry_list, 100);
        assign_addresses();
        /* the paths of the units: the first words of each are values (where it starts, or $2345 and then where), the rest are the steps, a byte a frame */
        for (const struct Entry8 *entry = guest.entry_list; (int32_t)entry->address >= 0; entry++) {
            uint16_t *path = (uint16_t *)(uintptr_t)entry->address;

            if (entry->size < 6)
                continue;
            if (file_word(path) == 0x2345)
                file_words(path, 3);
            else
                file_words(path, 2);
        }
    }
    if (guest.level_files_read == 0) {
        /* goes on at $00F330 (not run) */
        return;
    }
    narrow_words_to_bytes();
}

/* orig $0148D0: nothing is done while level_files_read is set (goes on at $0148D6, not run, otherwise) */
void check_flag_286(void)
{
    if (guest.level_files_read == 0)
        return;   /* goes on at $0148D6 */
}

/* orig $014852: 40 words at `table` (0x50 bytes), and the 40 texts of 40 bytes that follow them, are put in the pool of texts at text_pool
 * (ptr_0cf6): a word that is not negative gets the text's offset from message_base into the 40 words that are copied to `to` + 80; the pool's
 * end is kept in text_pool_end */
void read_text_table_word_offsets(const uint16_t *table, uint16_t *to)
{
    uint8_t *pool = guest.text_pool;
    const uint8_t *text = (const uint8_t *)table + 80;
    uint16_t offset = (uint16_t)((uint32_t)(uintptr_t)pool - (uint32_t)(uintptr_t)guest.message_base);

    for (int i = 0; i < 40; i++) {
        int16_t value = (int16_t)file_word(table++);

        *to = (uint16_t)value;
        if (value >= 0) {
            const uint8_t *s = text;

            *(uint16_t *)((uint8_t *)to + 80) = offset;
            if (*s != 0) {
                do {
                    offset++;
                    *pool++ = *s++;
                } while (pool[-1] != 0);
            }
        }
        text += 40;
        to++;
    }
    guest.text_pool_end = pool;
}

/* orig $014814: 40 longs at `table` are offsets of texts (from the start of the table; none when 0 or negative); the texts are put
 * into the pool at text_pool_end and message_offsets gets, for each, the word that is its offset from message_base (-1 for none); the pool's end is
 * kept in text_pool_end.  Returns where the data goes on: 1840 bytes after the start of the table */
const uint8_t *read_text_table_long_offsets(const uint8_t *table)
{
    uint16_t *to = (uint16_t *)guest.message_offsets;
    uint8_t *pool = guest.text_pool_end;
    uint16_t offset = (uint16_t)((uint32_t)(uintptr_t)pool - (uint32_t)(uintptr_t)guest.message_base);
    const int32_t *longs = (const int32_t *)table;

    for (int i = 0; i < 40; i++) {
        int32_t value = (int32_t)file_long(&longs[i]);

        *to++ = offset;
        if (value <= 0) {
            to[-1] = 0xFFFF;
            continue;
        }
        const uint8_t *s = table + value;

        if (*s == 0) {
            to[-1] = 0xFFFF;
            continue;
        }
        do {
            offset++;
            *pool++ = *s++;
        } while (pool[-1] != 0);
    }
    guest.text_pool_end = pool;
    return table + 1840;
}

/* orig $014794: the file of the objects (`names` + 22: objects.001, 6336 bytes) is loaded; 132 records of 10 bytes (a long and a
 * long, a word that is the name's offset from message_base) are made in table_14cf8 from it, the names (two strings each) go to ptr_0e5c;
 * a record whose name is shorter than 3 characters has -1 for the offset.  The end of the names is the start of the text pool */
void load_objects_file(const uint8_t *names)
{
    uint8_t *from;
    uint16_t offset = 0;
    uint8_t *to_start;
    uint8_t *out;

    guest.message_base = guest.ptr_0e5c;
    load_unpacked((const char *)names + NAME_OBJECTS, guest.picture_buffer);
    from = guest.picture_buffer;
    out = guest.message_base;
    to_start = (uint8_t *)guest.table_14cf8;
    for (int i = 0; i < 132; i++) {
        uint8_t *record = to_start + i * 10;
        const uint8_t *name1 = from + 8;

        for (int k = 0; k < 8; k += 2)
            *(uint16_t *)(record + k) = file_word(from + k);
        *(uint16_t *)(record + 8) = offset;
        {
            uint16_t length = 0;
            uint8_t *start = out;
            const uint8_t *s = name1;

            do {
                length++;
                *out++ = *s++;
            } while (out[-1] != 0);
            s = from + 8 + 16;
            do {
                length++;
                *out++ = *s++;
            } while (out[-1] != 0);
            if ((int16_t)length <= 2) {
                out -= 2;
                *(uint16_t *)(record + 8) = 0xFFFF;
            }
            offset = (uint16_t)(offset + (uint16_t)(out - start));
        }
        from += 8 + 40;
    }
    out++;
    guest.text_pool_end = out;
    guest.text_pool = out;
}

/* orig $014888: the file of the weapons (`names` + 34: weapons.001, 880 bytes) is loaded onto object_slots; the third word of each of
 * the 11 slots (the word at +8) is kept and put back after restore_slot_words has put the saved words back */
void load_weapons_file(const uint8_t *names)
{
    int16_t kept[11];
    struct Object *objects = (struct Object *)guest.object_slots;

    for (int i = 0; i < 11; i++)
        kept[i] = objects[i].last_value;
    load_unpacked((const char *)names + NAME_WEAPONS, guest.object_slots);
    for (int i = 0; i < 11; i++) {
        uint8_t *o = (uint8_t *)&objects[i];

        file_words(o, 8);
        file_longs(o + 20, 2);
        file_words(o + 28, 2);
        file_words(o + 74, 1);
    }
    restore_slot_words();
    for (int i = 10; i >= 0; i--)
        objects[i].last_value = kept[i];
}

/* orig $01477E: the weapons file and the objects file (neither when the password is used) */
void load_level_objects(const uint8_t *names)
{
    if (guest.word_0266 != 0)
        return;
    load_weapons_file(names);
    load_objects_file(names);
}

/* orig $01955E: twenty entries of three words (x, y, kind) at level_pickups are copied to table_19456; each with a not negative x puts
 * a cell in the map (ptr_0b86): the byte of param_b ($251) for kind 0 and of param_b ($253) for another, and, 8192 bytes on, 0 or 1
 * (the kind toggled) */
void place_pickups(void)
{
    const uint16_t *from = guest.level_pickups;
    uint16_t *to = guest.table_19456;

    guest.word_356 = 1;
    for (int i = 0; i < 20; i++) {
        int16_t x = (int16_t)from[0];
        int16_t y = (int16_t)from[1];
        uint16_t kind = from[2];

        from += 3;
        to[0] = (uint16_t)x;
        to[1] = (uint16_t)y;
        to[2] = kind;
        to += 3;
        if (x < 0)
            continue;
        uint8_t *cell = guest.ptr_0b86 + (int16_t)(x >> 5) + (int16_t)(y << 3);

        if (kind == 0)
            *cell = (uint8_t)guest.param_b;
        else
            *cell = (uint8_t)(guest.param_b >> 16);
        cell += 8192;
        kind ^= 1;
        *cell = (uint8_t)kind;
    }
}

/* orig $00F3DA: the level's map file (named by map_file_name) is loaded to the background's place: two words (copper_colour_count, color_line_spacing), copper_colour_count
 * words for the copper colours (copper_colours), 8192 words of map (ptr_0b86), the record table, the texts (read_text_table_long_offsets) and 2400
 * bytes (load_buffer_2400); the copper list is made (copper_list_make) */
void load_level_map(void)
{
    const uint16_t *p;
    uint16_t *to;

    load_unpacked((const char *)guest.map_file_name, guest.background);
    p = (const uint16_t *)guest.background;
    guest.copper_colour_count = file_word(p++);
    guest.color_line_spacing = file_word(p++);
    to = (uint16_t *)guest.copper_colours;
    {
        uint16_t n = (uint16_t)(guest.copper_colour_count - 1);

        do {
            *to++ = file_word(p++);
        } while (n-- != 0);
    }
    to = (uint16_t *)guest.ptr_0b86;
    for (int i = 0; i < 8192; i++)
        *to++ = *p++;
    copy_record_table((const struct Rec6 *)p);
    p += 600;
    p = (const uint16_t *)read_text_table_long_offsets((const uint8_t *)p);
    load_buffer_2400((const uint8_t *)p);
    copper_list_make();
}

/* orig $00530A: the copper list of the level's colours at copper_colors is made from copper_colour_count colours at copper_colours (color_line_spacing is the
 * spacing of the lines): each colour is a move to COLOR01 (the colour doubled: the original's colours are ST 3-bit ones) and a wait for
 * the line, with, between the colours, a move of the colour half way between them; the list ends with the three moves the program's
 * copper lists have (copper_move_a and b and $8A0000).  The colours are saved (copper_colors_save) */
void copper_list_make(void)
{
    uint16_t *list = guest.copper_colors;
    const uint16_t *source = (const uint16_t *)guest.copper_colours;
    uint16_t spacing = guest.color_line_spacing;
    int16_t count = (int16_t)(guest.copper_colour_count - 1);
    uint16_t line;

    wait_vblank();
    guest.copper_move_a = 0x00840000;
    guest.copper_move_b = 0x00865246;
    if (count >= 0) {
        int16_t half = (int16_t)(spacing + 1) >> 1;

        guest.color_line_spacing = (uint16_t)half;
        line = (uint16_t)(half + 60);
        for (;; count--) {
            uint16_t colour = (uint16_t)(source[0] + source[0]);

            *list++ = 386;
            *list++ = colour;
            if (count != 0) {
                uint16_t next = (uint16_t)(source[1] + source[1]);
                uint16_t blue = colour & 15, green = colour & 240, red = colour & 3840;
                uint16_t d3 = next & 15, d4 = next & 240, d5 = next & 3840;
                uint16_t wait;

                d3 = (uint16_t)((int16_t)(d3 - blue) >> 1);
                blue = (uint16_t)(blue + d3);
                d4 = (uint16_t)((int16_t)(d4 - green) >> 1);
                green = (uint16_t)(green + d4);
                d5 = (uint16_t)((int16_t)(d5 - red) >> 1);
                red = (uint16_t)(red + d5);
                wait = (uint16_t)(line << 8) | 1;
                line = (uint16_t)(line + guest.color_line_spacing);
                *list++ = wait;
                *list++ = 0xFF00;
                *list++ = 386;
                *list++ = (uint16_t)(blue | green | red);
            }
            *list++ = (uint16_t)((uint16_t)(line << 8) | 1);
            line = (uint16_t)(line + guest.color_line_spacing);
            *list++ = 0xFF00;
            source++;
            if (count == 0)
                break;
        }
    }
    *list++ = 0x0084;
    *list++ = 0x0000;
    *list++ = 0x0086;
    *list++ = 0x5246;
    *list++ = 0x008A;
    *list++ = 0x0000;
    copper_colors_save(0);
    guest.color_line_spacing = spacing;
}

/* orig $017A7E: the picture file `name` is read to picture_buffer (32034 bytes) and copied to 34 bytes before the screen buffer */
void load_picture_to_screen(const uint8_t *name)
{
    const uint16_t *from;
    uint16_t *to = (uint16_t *)(guest.screen_buffer - 34);

    load_unpacked((const char *)name, guest.picture_buffer);
    from = (const uint16_t *)guest.picture_buffer;
    for (int i = 0; i < 16017; i++)
        *to++ = *from++;
}

/* orig $017B4A: `count` + 1 blocks of 32 by 16 pixels (four planes: the words at +0 and +8 bytes of each of the 16 rows) are cut out of the
 * picture on the screen, from (x, y) on, to `to`, which goes on in next_tile_block; x goes on by 32 and wraps at 320 to the next row of blocks (16
 * lines) */
static void cut_blocks(int16_t x, int16_t y, int16_t count, uint16_t *to)
{
    for (; count >= 0; count--) {
        const uint8_t *base = guest.screen_buffer + (int32_t)y * 160 + (int16_t)(x >> 1);

        for (int plane = 0; plane < 4; plane++) {
            const uint8_t *row = base + plane * 2;

            for (int r = 0; r < 16; r++, row += 160) {
                *to++ = *(const uint16_t *)row;
                *to++ = *(const uint16_t *)(row + 8);
            }
        }
        guest.next_tile_block = (uint8_t *)to;
        x = (int16_t)(x + 32);
        if (x >= 320) {
            x = 0;
            y = (int16_t)(y + 16);
        }
    }
}

/* orig $017AAA: the background's tile picture `name` (bits1a.pi1) is made the tiles background_tile_graphics points to: the screen of the picture is cut
 * (cut_blocks) into blocks, 120 at the most from the first file and, for the levels that have more, the rest from the file of
 * the second picture (xtra1a.pi1) */
void load_background_tiles(const uint8_t *name)
{
    uint8_t *top = guest.memory_base + 34048;
    int16_t blocks;
    int16_t count;

    guest.background = top - 33792;
    blocks = (int16_t)table_17a6e[(int16_t)((guest.param_index - 1) * 2) + guest.word_0266];
    guest.background = top - ((uint32_t)(uint16_t)blocks << 8);
    guest.background_tile_graphics = guest.background - 256;
    guest.word_276 = 0;
    load_picture_to_screen(name);
    count = (int16_t)(blocks - 1);
    if (count >= 119)
        count = 119;
    guest.tile_block_count = (uint16_t)count;
    cut_blocks(0, 0, count, (uint16_t *)guest.background);
    if (blocks <= 120)
        return;
    blocks = (int16_t)(blocks - 120);
    load_picture_to_screen(guest.level_names + NAME_XTRA);
    count = (int16_t)(blocks - 1);
    guest.tile_block_count = (uint16_t)count;
    cut_blocks(0, 0, count, (uint16_t *)guest.next_tile_block);
}

/* orig $0037D2: the scroll position is made from the player's position (x, y): 144 and 72 back from it, but not under 0 and, at
 * the far end, not over 3776 and 832 */
void scroll_to_player(int16_t x, int16_t y)
{
    x = (int16_t)(x - 144);
    y = (int16_t)(y - 72);
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= 3776)
        x = 3776;
    if (y >= 832)
        y = 832;
    guest.scroll_x = x;
    guest.scroll_y = y;
}

/* orig $019170: the first three words of ten records of records_19334 are -1.  The map (ptr_0b86) is looked at: a cell that is the
 * byte param_a >> 16 or the one param_a & 255 makes a block of 2 by 3 cells: the cell and the ones under it get the byte of
 * param_a & 255 and 10 and 20 more, the ones to the right of them the byte of param_b & 255 (word param_c); the collision map (8192 bytes
 * on) gets a 1 for the three cells at the left */
void mark_cells_19170(void)
{
    uint8_t wanted_a = (uint8_t)(guest.param_a >> 16);
    uint8_t wanted_b = (uint8_t)guest.param_a;
    uint8_t other = (uint8_t)guest.param_c;
    uint8_t *cell = guest.ptr_0b86;

    for (int i = 0; i < 10; i++) {
        guest.records_19334[i].w[0] = -1;
        guest.records_19334[i].w[1] = -1;
        guest.records_19334[i].w[2] = -1;
    }
    for (int n = 0; n < 8192; n++, cell++) {
        if (*cell != wanted_a && *cell != wanted_b)
            continue;
        uint8_t value = wanted_b;

        cell[8192] = 1;
        cell[8192 + 128] = 1;
        cell[8192 + 256] = 1;
        cell[0] = value;
        cell[128] = (uint8_t)(value + 10);
        cell[256] = (uint8_t)(value + 20);
        cell[1] = other;
        cell[129] = other;
        cell[257] = other;
    }
}

/* orig $00FB9A: 256 bytes (128 words) at sine_table are copied 1024 bytes on (the words from 512), and the 32 pairs of words at sine_table (at +0 and +256 of 32
 * bytes each) become 32 pairs of bytes at bytes_fbf2: the first divided by 1366, less 5, plus 16; the second shifted right
 * by 10, less 5, plus 24 */
void make_pairs_fb9a(void)
{
    uint8_t *out = guest.bytes_fbf2;

    for (int i = 0; i < 128; i++)
        guest.sine_table[512 + i] = guest.sine_table[i];
    for (int n = 0; n < 32; n++) {
        int32_t first = (int16_t)guest.sine_table[n * 16];
        int32_t second = (int16_t)guest.sine_table[128 + n * 16];
        /* the signed division by 1366 of the original (divs.w) as a multiplication, which the 68000 can do in a word (exact for 0 to 32768) */
        uint16_t magnitude = (uint16_t)(first < 0 ? -first : first);
        int16_t quotient = (int16_t)(((uint32_t)magnitude * (uint16_t)49129u) >> 26);

        if (first < 0)
            quotient = (int16_t)-quotient;

        quotient = (int16_t)(quotient - 5);
        second >>= 10;
        quotient = (int16_t)(quotient + 16);
        *out++ = (uint8_t)quotient;
        *out++ = (uint8_t)(int16_t)((int16_t)second - 5 + 24);
    }
}

/* orig $003DE0: the lever_flags entry (six bytes: the long (x << 16) | y and a word) for the point (x, y): its place or 0 when there is none.
 * The original leaves d1 negative when none is found; here the caller looks at the result */
struct LeverFlag *find_flag_entry(int16_t x, int16_t y)
{
    for (int i = 0; i < 64; i++)
        if (guest.lever_flags[i].x == (uint16_t)x && guest.lever_flags[i].y == (uint16_t)y)
            return &guest.lever_flags[i];
    return 0;
}

/* orig $003D86: a record of the record table (200 of 6 bytes) of kind 3 (a kind of table_14cf8) takes the flag word of its
 * entry in lever_flags (found by its place), or -1 when it has none; word_2ba becomes -1 */
void fix_records_3d86(void)
{
    struct Rec6 *record = guest.record_table;

    for (int i = 0; i < 200; i++, record++) {
        int16_t kind = (int16_t)record->w[2];

        if (kind < 0 || kind >= 192)
            continue;
        if ((int16_t)guest.table_14cf8[kind].w[2] != 3)
            continue;
        struct LeverFlag *entry = find_flag_entry((int16_t)record->w[0], (int16_t)record->w[1]);

        if (entry == 0)
            record->w[2] = 0xFFFF;
        else
            entry->state = record->w[2];
    }
    guest.word_2ba = -1;
}

/* orig $00F91E (with $00F994 inlined): the same records of kind 3 are looked at: one whose place has no lever_flags entry is turned off (kind -1), and so is the
 * second of two that have the same place (the first 64 places are kept in a buffer on the stack) */
void find_duplicate_records(void)
{
    uint32_t seen[64];
    struct Rec6 *record = guest.record_table;

    for (int i = 0; i < 64; i++)
        seen[i] = 0xFFFFFFFF;
    for (int i = 0; i < 200; i++, record++) {
        int16_t x = (int16_t)record->w[0];
        int16_t y = (int16_t)record->w[1];
        int16_t kind = (int16_t)record->w[2];

        if (kind >= 192)
            continue;
        /* the original multiplies the kind as an unsigned word and uses the low word as a signed offset: a negative kind looks at a
         * word far before the table */
        uint16_t offset = (uint16_t)((uint16_t)kind * 10);

        if (*(int16_t *)((uint8_t *)guest.table_14cf8 + (int16_t)offset + 4) != 3)
            continue;
        if (find_flag_entry(x, y) == 0) {
            record->w[2] = 0xFFFF;
            continue;
        }
        uint32_t key = ((uint32_t)(uint16_t)x << 16) | (uint16_t)y;
        int found = 0;

        for (int k = 0; k < 64; k++) {
            if (seen[k] == key) {
                found = 1;
                break;
            }
        }
        if (found) {
            record->w[2] = 0xFFFF;
            continue;
        }
        for (int k = 0; k < 64; k++) {
            if ((int32_t)seen[k] < 0) {
                seen[k] = key;
                break;
            }
        }
    }
}

/* orig $00565E: the pictures of the four item slots (images 115 to 118) are made plain with the item each holds, and the picture of
 * the chosen slot (item_shown) is the cut one */
void item_panels_make(void)
{
    for (int i = 0; i < 4; i++)
        panel_slot_plain((uint8_t *)guest.image_table[115 + i].data, guest.item_slots[i]);
    panel_slot_cut((uint8_t *)guest.image_table[115 + (int16_t)guest.item_shown].data, guest.item_slots[(int16_t)guest.item_shown]);
}

/* orig $01EFCC (with $01F5DA inlined: the test of bit 0 of flags_0ddd): image number `image` of the image table is drawn at (x, y) with the clip rectangles clip_rect_list points to (flags_0ddd bit 3).
 * What did not run (bit 0 and bit 5 of flags_0ddd, a negative height, the draw without a clip list) is not written */
void image_draw_clipped(int16_t x, int16_t y, uint16_t image)
{
    const struct Image *entry;
    int16_t width;
    int16_t height;
    const uint8_t *source;
    uint16_t bytes;
    const int16_t *clip;

    if (guest.flags_0ddd & 1)
        return;   /* goes on at $01F5E2 */
    guest.image_x_shift = 0;
    guest.image_clip_flags = 0;
    if (guest.flags_0ddd & 2)
        guest.image_x_shift = (uint16_t)(x & 15);
    entry = &guest.image_table[image];
    width = entry->neg_width;
    height = (int16_t)entry->height;
    source = (const uint8_t *)entry->data;
    bytes = (uint16_t)((width < 0 ? -width : width) + 1) >> 3;
    guest.image_plane_bytes = (uint32_t)bytes * (uint16_t)(height + 1);
    guest.image_mirrored = 0;
    if (width < 0) {
        width = (int16_t)-width;
        guest.image_mirrored = (uint16_t)~guest.image_mirrored;
    }
    if (height < 0)
        return;   /* goes on at $01F02C */
    guest.word_dd6 = 0;
    guest.word_dd4 = 0;
    if (guest.flags_0ddd & 0x20)
        return;   /* goes on at $01F05E */
    if (!(guest.flags_0ddd & 8))
        return;   /* goes on at $01F06C: no clip list */
    for (clip = (const int16_t *)guest.clip_rect_list; clip[0] >= 0; clip += 4) {
        uint16_t kept = guest.word_dd4;

        blit_clipped(x, y, width, height, source, clip);
        guest.word_dd4 = kept;
        guest.word_dd6 = 0;
    }
}

/* orig $005E50: the screen is filled with ones, image 127 (the digits) is drawn at (0, 0) and its sixteen... ten digits, 8 rows of four bytes
 * (one for each plane), are cut out of it into digit_font */
void hud_digits_load(void)
{
    uint32_t *fill = (uint32_t *)guest.screen_buffer;
    const uint8_t *from;
    uint8_t *out = guest.digit_font;

    for (int i = 0; i < 8000; i++)
        *fill++ = 0xFFFFFFFF;
    guest.draw_routine_index = 1;
    image_draw_clipped(0, 0, 127);
    blitter_wait();
    from = guest.screen_buffer;
    for (int pair = 0; pair < 5; pair++) {
        for (int half = 0; half < 2; half++) {
            for (int row = 0; row < 8; row++, from += 44) {
                out[0] = plane_byte(from);
                out[1] = plane_byte(from + 8448);
                out[2] = plane_byte(from + 16896);
                out[3] = plane_byte(from + 25344);
                out += 4;
            }
            from -= 351;
        }
    }
}

void records_from_table(void);   /* orig $00F8DE (below) */

/* orig $00F526: the state of the game is set for the start of a level: the work area is cleared, the player is put at the start
 * of the level, the variables and tables are reset (many of the small routines of this module), the routines that are off in the
 * level are switched off or on (their first words are rts or nop), and the item slots and the records are made */
void level_state_init(void)
{
    uint32_t spot;

    clear_work_area();
    guest.flags_3b1 &= (uint8_t)~4;
    play_sound(0, 0);
    guest.sound_enabled = 1;
    guest.flag_2b0 = 0;
    guest.word_a68 = 0;
    guest.word_19c = 4;
    guest.word_370 = 0;
    guest.player_x = guest.start_x;
    guest.player_y = guest.start_y;
    if (guest.falcon_energy >= 0) {
        guest.word_8882 = (uint16_t)guest.player_x;
        guest.word_8884 = (uint16_t)(guest.player_y - 40);
    }
    spot = (((uint32_t)(uint16_t)guest.player_x << 16) | (uint16_t)guest.player_y) & 0xFF80FF80;
    for (int i = 0; i < 12; i += 2) {
        guest.visited_spots[i] = (int16_t)(spot >> 16);
        guest.visited_spots[i + 1] = (int16_t)spot;
    }
    guest.previous_player_pos[0] = guest.start_x;
    guest.previous_player_pos[1] = guest.start_y;
    scroll_to_player(guest.start_x, guest.start_y);
    guest.word_19a = 0;
    guest.frame_toggle = 0;
    guest.frame_phase = 0;
    guest.word_1f8 = 0;
    guest.word_1ba = 0;
    guest.flag_1f0 = 0;
    guest.flag_b24 = 0;
    guest.message_state = 0;
    guest.tick_hold_296 = 0;
    guest.flag_aa8 = 0;
    guest.flag_210 = 0;
    guest.word_a54 = 0;
    guest.countdown_220 = 0;
    guest.text_index = 0;
    guest.word_22e = 0;
    guest.level_ended = 0;
    guest.fall_distance = 0;
    guest.word_3a1fa = 0;
    guest.flag_236 = 0;
    guest.word_238 = 0;
    guest.flag_258 = 0;
    guest.shop_door_index = 0;
    guest.word_280 = 0;
    guest.word_3a6 = 0;
    guest.timer_306 = 0;
    guest.word_025c = 1;
    guest.flags_3b0 &= (uint8_t)~1;
    guest.flags_3b0 &= (uint8_t)~4;
    guest.flags_3b1 &= (uint8_t)~1;
    guest.word_3436 = 0x4E75;
    guest.flag_290 = -1;
    guest.word_17e9c = 0x4E75;
    guest.word_4a2 = 0;
    guest.code_c374 = 0x4E75;
    guest.ptr_0bc6 = (const struct LevelEnd *)table_143b8[(int16_t)(guest.param_index - 1)];
    guest.records_scroll_y = guest.scroll_y;
    mark_cells_19170();
    place_pickups();
    make_pairs_fb9a();
    guest.background_tile_graphics = guest.background - 256;
    if (guest.word_037c != 0) {
        do_nothing();
        load_background_tiles(guest.level_names + NAME_BITS);
        guest.word_037c = 0;
        do_nothing();
    }
    if (guest.word_0248 != 0) {
#ifdef PIRATED
        guest.copylock_seed = 0x4B79C278;   /* the long that the copylock leaves when it fails (the immediate at $F6C0) */
#else
        guest.copylock_seed = 0;            /* what it leaves when it passes */
#endif
        if (guest.param_index == 4)
            guest.armour_level = 1;
        guest.item_shown = 3;
        guest.item_chosen = 3;
        guest.slots_30c[0] = -1;
        guest.slots_30c[1] = -1;
        guest.slots_30c[2] = -1;
        guest.slots_30c[3] = -1;
        item_panels_make();
        for (int i = 0; i < 4; i++)
            guest.score_words[i] = 0;
        guest.high_score_baa = (int32_t)table_5b9e[(int16_t)(guest.param_index - 1)];
        guest.high_score_bae = (int32_t)table_5bb2[(int16_t)(guest.param_index - 1)];
        guest.word_27c = (uint16_t)((guest.param_index - 1) * 4);
        guest.word_27e = (uint16_t)((guest.param_index - 1) * 4);
        guest.score_redraw = 1;
        guest.player_energy = 24;
        clear_shot_lists();
        copy_block_16bb4();
        falcon_end();
        guest.item_count = 2;
        guest.score_total_1fc = 0;
        guest.code_ba34 = 0x4E75;
        guest.flags_3b0 &= (uint8_t)~0x40;
    }
    guest.word_272 = (int16_t)table_1527a[(int16_t)(guest.param_index - 1)];
    guest.word_268 = 0;
    guest.energy_shown = 0;
    guest.word_2a6 = -1;
    guest.word_a9a = -1;
    clear_creature_state();
    clear_records_721e();
    clear_bolts();
    clear_effects();
    clear_cell_records();
    clear_markers();
    fill_table_1a30c();
    records_113c_reset();
    clear_records_1036();
    reset_slots_fd0();
    reset_slots_de26();
    reset_slots_e392();
    fill_bytes_10b0();
    clear_walk_groups();
    clear_monsters();
    clear_records_737e();
    reset_records_7544();
    guest.code_8a54 = 0x4E71;
    guest.code_9ae6 = 0x4E71;
    guest.code_6ed0 = 0x4E71;
    stop_game_sounds();
    guest.ptr_0b76 = table_1077a[(int16_t)(guest.param_index - 1)];
    set_pickup_glyph();
    for (int i = 0; i < 20; i++)
        guest.records_3a0a6[i * 6] = -1;
    guest.records_3a0a6_in_use = 0;
    for (int i = 0; i < 240; i++)
        guest.particle_records[i] = -1;
    guest.flag_a4c = 0;
    guest.particle_glyph_step = 0;
    guest.particle_burst_counter = 0;
    guest.word_1f2 = 4;
    guest.last_drop_frame = -1;
    guest.records_316[0] = -1;
    guest.records_316[3] = -1;
    guest.records_316[6] = -1;
    guest.records_316[9] = -1;
    guest.shutter_row = -1;
    guest.fall_kind = -1;
    guest.shutter_direction = 0;
    panel_blank((uint8_t *)guest.image_table[115].data);
    panel_blank((uint8_t *)guest.image_table[116].data);
    panel_blank((uint8_t *)guest.image_table[117].data);
    guest.tick_count = 0;
    for (int i = 0; i < 16; i++)
        guest.item_message_shown[i] = 0;
    if (guest.param_index == 1) {
        guest.table_7a6e[0] = 530;
        guest.table_7a6e[1] = 531;
        guest.table_7a6e[2] = 532;
        guest.table_7a6e[3] = 531;
    } else {
        guest.table_7a6e[0] = 523;
        guest.table_7a6e[1] = 524;
        guest.table_7a6e[2] = 525;
        guest.table_7a6e[3] = 524;
    }
    records_from_table();
}

/* orig $00F8DE: the duplicates of the record table are turned off ($F91E), its 200 records of 6 bytes are copied to records_19b14 (8 bytes; the third word negative: the record is off), the
 * records of kind 3 take their flags ($3D86) and the records that are near the screen are marked ($3EB4) */
void records_from_table(void)
{
    find_duplicate_records();
    for (int i = 0; i < 200; i++) {
        struct Slot8 *to = &guest.records_19b14[i];
        const struct Rec6 *from = &guest.record_table[i];

        if ((int16_t)from->w[2] < 0) {
            to->w[0] = 0xFFFF;
            to->w[1] = 0xFFFF;
            to->w[2] = 0xFFFF;
        } else {
            to->w[0] = from->w[0];
            to->w[1] = from->w[1];
            to->w[2] = from->w[2];
        }
        to->w[3] = 0;
    }
    fix_records_3d86();
    activate_records_now();
}

/* orig $00FC32: one frame of the game as the start-up draws it before the first fade-in: the loop's routines in the loop's order, without
 * its checks (the demo, the energy, the second table and the player's position that the loop looks at) */
void draw_start_frame(void)
{
    wait_vblank();
    message_tick();
    host_read_skip();   /* (the original's read of this frame is counted, not waited for) */
    game_read_input();
    guest.frame_toggle = (uint16_t)~guest.frame_toggle;
    guest.frame_phase++;
    if ((int16_t)guest.frame_phase >= 2)
        guest.frame_phase = 0;
    if (!(guest.game_buttons & 1))
        guest.fire_latch = 0;
    guest.energy_old = guest.player_energy;
    fill_buffer();
    draw_background();
    draw_pickups();
    do_nothing_b();
    update_crumbles();
    redraw_record_images();
    activate_records();
    update_records();
    chest_pieces_move();
    run_object_lists();
    fn_15cc4_c();
    falcon_seek_target();
    guest.value_c6c = 3030;
    guest.flag_20e = -1;
    update_creatures();
    move_shards();
    face_bolts_move();
    face_bolt_pieces_move();
    draw_effects();
    update_falling_thing();
    draw_marks_316();
    guest.scroll_x_a = guest.scroll_x;
    guest.scroll_y_a = guest.scroll_y;
    player_control();
    guest.scroll_x_b = guest.scroll_x;
    guest.scroll_y_b = guest.scroll_y;
    guest.scroll_x = guest.scroll_x_a;
    guest.scroll_y = guest.scroll_y_a;
    fn_16aee_c();
    bats_run();   /* $FD02 */
    dropped_sparkle_update();
    update_archers();   /* $FD0C (this routine, the frame of a change of the screen, does not call the routine at $E142) */
    update_walker_groups();
    shop_door();
    mark_player_cells();
    update_monsters();
    draw_sprites_721e();
    update_falling_slots();
    unit0_update();
    unit1_update();
    unit2_update();
    unit3_update();
    unit4_update();
    pods_update();   /* $FD3C */
    spores_update();
    move_records_3a0a6();
    falcon_draw();
    update_bolts();
    bolts_update();
    update_score_display();
    draw_score_panel();
    update_item_panel();
    draw_shutter();
    update_energy_display();
    draw_energy_bar();
    draw_item_row();
    guest.scroll_x = guest.scroll_x_b;
    guest.scroll_y = guest.scroll_y_b;
}

/* ---- the records that move along a path ($17E0A to $18400): a thing of ptr_0c9e (24 bytes) goes from a point to the next by a line of
 * steps, so many a frame (the speed is the byte at +5 of its description in ptr_0c9a, 28 bytes, plus one).  The original keeps the
 * line's registers in the record from +6 on and leaves the loop (a coroutine) by storing the address it goes on at; the four
 * loops (the line is steeper or flatter, goes left or right) are tagged by those addresses */
#define PATH_FLAT_RIGHT 0x182E4
#define PATH_STEEP_RIGHT 0x18316
#define PATH_FLAT_LEFT 0x1835E
#define PATH_STEEP_LEFT 0x18390

/* the steps of a line, from the loop `loop` with the registers of the state: at most word_185bc steps are made, then the state is kept.
 * Returns the number of steps left, less one (negative when the line is done) */
static void path_run(struct PathState *s, uint32_t loop)
{
    int16_t x = s->x, y = s->y, y_step = s->y_step;
    int16_t length = s->length, shorter = s->shorter, error = s->error, count = s->count;
    int flat = (loop == PATH_FLAT_RIGHT || loop == PATH_FLAT_LEFT);
    int right = (loop == PATH_FLAT_RIGHT || loop == PATH_STEEP_RIGHT);

    for (;;) {
        if (flat) {
            error = (int16_t)(error - shorter);
            if (error < 0) {
                error = (int16_t)(error + length);
                y = (int16_t)(y + y_step);
            }
            x = (int16_t)(right ? x + 1 : x - 1);
        } else {
            y = (int16_t)(y + y_step);
            error = (int16_t)(error - length);
            if (error < 0) {
                error = (int16_t)(error + shorter);
                x = (int16_t)(right ? x + 1 : x - 1);
            }
        }
        guest.word_185bc = (int16_t)(guest.word_185bc - 1);
        if (guest.word_185bc == 0)
            break;
        if (count-- == 0)
            break;
    }
    /* the loop was left by the counter of steps (word_185bc) or by the end of the line (count was 0: dbf leaves it at -1) */
    count = (int16_t)(count - 1);
    s->loop = loop;
    s->x = x;
    s->y = y;
    s->y_step = y_step;
    s->length = length;
    s->shorter = shorter;
    s->error = error;
    s->count = count;
}

/* orig $0182C4 (and the loops at $18310, $18342 and $1838A): a line from (x0, y0) to (x1, y1) is made the state `s` and walked word_185bc steps */
static void path_start(struct PathState *s, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx;
    int16_t dy;
    int16_t y_step = 1;
    int right = x0 <= x1;
    uint32_t loop;

    s->x = x0;
    s->y = y0;
    dx = right ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    dy = (int16_t)(y1 - y0);
    if (dy < 0) {
        dy = (int16_t)-dy;
        y_step = -1;
    }
    s->y_step = y_step;
    if ((uint16_t)dx < (uint16_t)dy) {
        s->count = dy;
        s->error = (int16_t)((uint16_t)dy >> 1);
        loop = right ? PATH_STEEP_RIGHT : PATH_STEEP_LEFT;
    } else {
        s->count = dx;
        s->error = (int16_t)((uint16_t)dx >> 1);
        loop = right ? PATH_FLAT_RIGHT : PATH_FLAT_LEFT;
    }
    s->length = dx;
    s->shorter = dy;
    if (loop == PATH_STEEP_RIGHT || loop == PATH_STEEP_LEFT) {
        /* the steep loops count the steps of the longer side, which is dy: length and shorter are as the original keeps them */
        s->length = dx;
        s->shorter = dy;
    }
    path_run(s, loop);
}

/* orig $0182B0: the line goes on from the state kept in the record */
static void path_resume(struct PathState *s)
{
    path_run(s, s->loop);
}

/* the waypoint k (1 to 4) of a description (28 bytes): a point (words at +6 + 4 (k - 1)) or none (x 0) */
static int path_waypoint(const struct PathDescription *description, struct PathRecord *record, int k, int16_t *x, int16_t *y)
{
    const struct PathPoint *point = &description->waypoint[k - 1];

    if (point->x == 0)
        return 0;
    *x = point->x;
    *y = point->y;
    record->waypoint = (int8_t)k;
    record->kind = 2;
    return 1;
}

/* orig $01813C (the table at $18284, whose routines are $018154, $018164, $018192, $0181C0, $0181D8, $0181F0, $018208, $018220, $018238, $018250 and $018268 inlined): which waypoint the line of the record goes to next, by the
 * kind in the description's byte at +22 + `variant`: the next one (kinds 2 to 5), the one before (7 to 10), the next of the row (0) or the
 * one before (1 and 6) */
static void path_target(const struct PathDescription *description, struct PathRecord *record, int8_t kind, int16_t *x, int16_t *y)
{
    int k;

    if (kind < 0) {
        record->kind = (int8_t)0xFF;
        guest.word_4a2--;
        return;   /* the target is where the line starts */
    }
    if (kind == 0) {
        record->kind = 0;
        k = record->waypoint + 1;
        if (k > 4)
            k = 1;
        path_target(description, record, (int8_t)(k + 1), x, y);
        record->kind = 0;
        return;
    }
    if (kind == 1 || kind == 6) {
        record->kind = 1;
        k = record->waypoint - 1;
        if (k < 1)
            k = 4;
        path_target(description, record, (int8_t)(6 + k), x, y);
        record->kind = 1;
        return;
    }
    if (kind >= 2 && kind <= 5) {
        k = kind - 1;
        while (!path_waypoint(description, record, k, x, y))
            k = (k == 4) ? 1 : k + 1;
    } else {
        k = kind - 6;
        while (!path_waypoint(description, record, k, x, y))
            k = (k == 1) ? 4 : k - 1;
    }
}

/* orig $017E0A, $017E0E, $017E12, $017E16 (the four entries, `variant` is 0 to 3) and $017E18: the thing of the entry (the number of its description in
 * the second word) is put on its path: when the description has a waypoint the entry is taken (first word -1), the one that moves is
 * counted (word_4a2) and redraw_record_images is switched on; the line from where it is (or where its description puts it) to the
 * waypoint is walked and the record's place is where that left it */
void start_record_path(struct EntryRef *entry, int16_t variant)
{
    int16_t number = (int16_t)entry->number;
    const struct PathDescription *description = &guest.ptr_0c9a[number];
    struct PathRecord *record = &guest.ptr_0c9e[number];
    int16_t x, y, tx = 0, ty = 0;

    if ((description->waypoint[0].x | description->waypoint[1].x | description->waypoint[2].x | description->waypoint[3].x) == 0)
        return;
    entry->state = 0xFFFF;
    guest.word_4a2++;
    guest.word_17e9c = 0x4E71;
    if (record->kind < 0) {
        x = description->x;
        y = description->y;
    } else {
        x = record->x;
        y = record->y;
    }
    guest.word_185bc = (int16_t)(description->speed + 1);
    record->waypoint = 0;
    tx = x;
    ty = y;
    path_target(description, record, description->kind[variant], &tx, &ty);
    path_start(&record->state, x, y, tx, ty);
    record->x = record->state.x;
    record->y = record->state.y;
}

/* orig $018072 (from $01808E on, with $018092 to $0180C6): the record goes on along its line: the place it reaches is kept in the
 * record; when the line is done the record is at its waypoint, and with a kind above 1 (byte +4) it is done with the path (byte +5
 * -1).  The kinds 0 and 1 go on to the next waypoint ($18106) */
void path_continue(struct PathRecord *record, const struct PathDescription *description)
{
    path_resume(&record->state);
    record->x = record->state.x;
    record->y = record->state.y;
    if (record->state.count >= 0)
        return;

    int16_t line_x = record->state.x;   /* (d4 and d5 of the original: where the line ended) */
    int16_t line_y = record->state.y;
    const struct PathPoint *point = &description->waypoint[record->waypoint - 1];

    record->x = point->x;
    record->y = point->y;
    if (record->kind <= 1) {   /* $18106: the line goes on to the next waypoint (kind 0: the next of the row, 1: the one before) */
        int16_t tx = line_x, ty = line_y;

        path_target(description, record, record->kind, &tx, &ty);
        guest.word_185bc = (int16_t)(description->speed + 1);
        path_start(&record->state, line_x, line_y, tx, ty);
        return;
    }
    record->waypoint = (int8_t)0xFF;
}

/* orig $012710: the cells of the map (ptr_0b86 + 8192) at the player are looked at, three rows of one column from the player's x + 16 (to a
 * multiple of 32) and, when his x is 16 or more into a column of 32, of the column before it too: a cell above 2 is an entry
 * (cell_entry_runs) */
void player_tile_scan(void)
{
    int16_t x = (int16_t)((guest.player_x + 16) & -32);
    int16_t y = (int16_t)(guest.player_y & -16);
    const uint8_t *base = guest.ptr_0b86 + 8192 + (x >> 5) + (int16_t)(y << 3);
    const uint8_t *cell = base;

    for (int row = 0; row < 3; row++, cell += 128)
        if (*cell > 2)
            cell_entry_runs(cell);
    if ((guest.player_x & 31) < 16)
        return;
    cell = base - 1;
    for (int row = 0; row < 3; row++, cell += 128)
        if (*cell > 2)
            cell_entry_runs(cell);
}

/* orig $006556: the screen is faded out, the background drawn and `text` shown in the middle of it (row 70), and faded in: used for the
 * "LOADING LEVEL n" screen */
static void show_text_screen(const char *text)
{
    fade_out(0, 0xFFFF);
    do_nothing();
    background_draw();
    guest.text_y = 70;
    text_draw_centered(text);
    guest.copper_setup_done = 1;
    display_reset();
    screens_flip();
    if (level_next_wait)
        host_disk_wait();   /* (the lockstep: the next level's loading screen is faded in when the original has done it) */
    host_fade_lag(2);   /* (the colours as they are, the copper list's a frame late) */
    fade_in((const uint16_t *)(guest.background + 2), 0);
    host_fade_lag(0);
}

/* orig $012D44: the text "LOADING LEVEL n" (the level's number is put in the 15th character) is shown (show_text_screen) */
static void loading_level_screen(void)
{
    static const char text[] = "LOADING LEVEL ";

    for (int i = 0; i < 14; i++)
        guest.level_text[i] = (uint8_t)text[i];
    guest.level_text[14] = (uint8_t)(guest.param_index + 93);   /* the original adds 93 to the number: level 1 is '^' */
    guest.level_text[15] = 0;
    show_text_screen((const char *)guest.level_text);
}

/* the level file's monster types (struct MonsterType, 22 bytes, fifty of them from monster_types for the flying groups and fifty on for the walkers): the words (flags, drop and start cell) are made the machine's own */
static void monster_types_from_disk(void)
{
    struct MonsterType *type = (struct MonsterType *)guest.monster_types;

    for (int i = 0; i < 100; i++, type++) {   /* (the second fifty are the groups of walkers') */
        file_words(&type->flags, 1);
        file_words(&type->item, 2);
    }
}

/* the tables of the level that the file calfils brings (7000 bytes from data_1aa66 on, loaded from the disk): the entries of the cells (ptr_0cbe, 253), the groups of creatures (creature_entries, 100)
 * and the things on paths (path_descriptions, 25) have words, which are made the machine's own */
static void level_tables_from_disk(void)
{
    monster_types_from_disk();
    for (int i = 0; i < 253; i++)
        file_words(&guest.ptr_0cbe[i], 2);
    for (int i = 0; i < 100; i++) {
        struct CreatureEntry *entry = &guest.creature_entries[i];

        file_words(&entry->spawn_x, 2);
        file_words(&entry->delay_step, 1);
        file_words(&entry->template_index, 1);
    }
    for (int i = 0; i < 25; i++) {
        struct PathDescription *description = &guest.path_descriptions[i];

        file_words(&description->x, 2);
        file_words(description->waypoint, 8);
    }
    file_words(guest.lever_flags, 3 * 64);
    file_words(guest.level_pickups, 60);   /* the pick-ups of the level */
    file_words(guest.table_1c1d6, 90);   /* the teleport gems */
    for (int i = 0; i < 100; i++)   /* the scripts of the units (8 bytes each): the word that is the delay between the members */
        file_words(&((struct UnitScript *)guest.unit_scripts)[i].spacing, 1);
}

/* the quickload (the SDL port, guest.c): the memory of the original is in the machine's byte order and the words of the tables the program makes native when it loads a level are
 * swapped here, once: the tables level_tables_from_disk handles (but the ones that are variables with a type, which guest.c's fixups swap), the two tables of the hints' texts,
 * and the creatures of each group (their records are what struct CreatureType points to) */
void level_tables_quickload(void)
{
    monster_types_from_disk();
    for (int i = 0; i < 10; i++) {   /* the monsters of the groups that run (struct MonsterPart, ten to a group; the blocks of the others may overlap other tables): the words are x to speed
                                      * and the two targets and the object's place */
        if (guest.monster_records[i].state == 0)
            continue;
        for (int k = 0; k < 10; k++) {
            struct MonsterPart *part = &((struct MonsterPart *)guest.ptr_0cee)[10 * i + k];

            file_words(part, 12);
            file_words(&part->target_x, 4);
        }
    }
    for (int i = 0; i < 10; i++) {   /* the walkers of the groups that run (struct WalkMember, ten to a group, at asset_directory + 240 a group): twelve words each */
        if (guest.records_12a0[i].state == 0)
            continue;
        for (int k = 0; k < 10; k++)
            file_words(&((struct WalkMember *)(guest.asset_directory + 240 * i))[k], 12);
    }
    for (int i = 0; i < 253; i++)
        file_words(&guest.ptr_0cbe[i], 2);
    for (int i = 0; i < 100; i++) {
        struct CreatureEntry *entry = &guest.creature_entries[i];

        file_words(&entry->spawn_x, 2);
        file_words(&entry->delay_step, 1);
        file_words(&entry->template_index, 1);
    }
    for (int i = 0; i < 25; i++) {
        struct PathDescription *description = &guest.path_descriptions[i];

        file_words(&description->x, 2);
        file_words(description->waypoint, 8);
    }
    for (int i = 0; i < 25; i++) {   /* the records that go along the paths (struct PathRecord, 24 bytes: two words, two bytes, the line's state: a long and seven words) */
        struct PathRecord *record = &guest.ptr_0c9e[i];

        file_words(&record->x, 2);
        file_longs(&record->state.loop, 1);
        file_words(&record->state.x, 7);
    }
    file_words(guest.lever_flags, 3 * 64);
    for (int i = 0; i < 100; i++)
        file_words(&((struct UnitScript *)guest.unit_scripts)[i].spacing, 1);
    file_words(guest.message_offsets, 40);
    file_words(guest.ptr_0cf2, 80);
    for (int i = 0; i < 100; i++)   /* the 2400 bytes load_buffer_2400 fills */
        file_words(&guest.script_records[i], 12);
    file_words(guest.copper_colours, guest.copper_colour_count);   /* the colours of the level's copper list */
    file_words(guest.random_words, 256);   /* the table of random words */
    for (int k = 0; k < 5; k++) {   /* the ten members of each unit (struct UnitMember, 20 bytes: four words, the path's address, four words; also the ones not in use: the
                                     * words a unit does not set when it starts them stay as they were) */
        struct UnitMember *member = (struct UnitMember *)guest.ptr_0cfa[k];

        if (member == 0)
            continue;
        for (int i = 0; i < 10; i++, member++) {
            file_words(&member->x, 4);
            file_pointers(&member->path, 1);
            file_words(&member->hp, 4);
        }
    }

    struct CreatureType *type = (struct CreatureType *)guest.table_199ac;

    for (int i = 0; i < 10; i++, type++) {   /* (table_199ac is declared as bytes: the fields of each record are swapped here) */
        file_pointers(&type->creatures, 1);
        file_words(&type->count, 4);
        file_words(&type->delay_step, 1);
        file_words(&type->draw_kind, 1);
        file_longs(&type->move_routine, 1);
        file_words(&type->box_width, 2);
        file_longs(&type->step_routine, 1);
        if (type->creatures && type->number)
            file_words(type->creatures, 7 * type->number);
    }
    for (int i = 0; i < 20; i++)   /* the ten shards (struct Shard: four words) that the stone faces spit: the table is typed as longs, which the loader made native as longs: the words of each are the other way round */
        guest.records_737e[i] = (guest.records_737e[i] >> 16) | (guest.records_737e[i] << 16);
    file_words(guest.hit_list, 75);   /* the list of the creatures hit (x, y, strength: 25 of them): what an earlier frame left there is read as the value of a killed monster ($9DDA) */
    file_words(guest.bytes_10b0, 70);   /* the bats */
    file_words(guest.ball_values, 3);   /* the bouncing ball's speeds and jump */
    file_words(guest.ball_pieces, 50);   /* its burst's pieces */
    file_words(guest.ball_pillars, 21);   /* and its pillars of fire */
    for (int i = 0; i < 10; i++) {   /* the spores: the four words and the count of each */
        file_words(&spores()[i].x, 4);
        file_words(&spores()[i].count, 1);
    }
    file_words(guest.axe_steps, 27);   /* the magic axe's nine shots' steps ($66E) */
    boss_quickload();
}

/* orig $00323C: when the small font asked for it (menu_state_25a) it is loaded again: the asset set of the level's sprites */
static void reload_sprites(void)
{
    if (guest.menu_state_25a == 0)
        return;
    guest.asset_flag = -1;
    guest.flags_0ddd |= 0x10;
    assets_load((const char *)guest.level_names + NAME_SPRITES, guest.palette_words, guest.asset_end_0ca2, (struct AssetEntry *)guest.asset_table_0ca6);
    guest.palette_words[0] = 0;
    guest.menu_state_25a = 0;
}

/* orig $001C65E: the two longs of table_1c678 that the level names (8 bytes each) go to level_creature_templates and ptr_0b82 */
static void set_level_pointers(void)
{
    const ByteP *from = &table_1c678[(int16_t)((guest.param_index - 1) * 2)];

    guest.level_creature_templates = (uint8_t *)from[0];
    guest.ptr_0b82 = (uint8_t *)from[1];
}

/* orig $00651E and $002452 to $002C08 (the rts at $016E3E that it calls is not written): Begin Quest.  The level is set up from the files of the disk: the level's names are made from
 * the template, the objects, weapons, text and map files are loaded, the sprites of the level, the pictures; the state is
 * made (level_state_init), two frames are drawn (draw_start_frame) behind the loading screen, which is then faded in: the game loop goes
 * on from here.  (The original puts the stack back to its saved value at $2464; that is not needed here) */
/* orig $00247C to $002AA8 (the end of its part that the loading of a world also runs): the level's names are made from the template (the `b` files of a world that follows another, word_0266 not 0), the objects,
 * weapons, text and map files are read, the sprites of the level and the pictures.  With word_0384 not 0 (the loading of a world in the middle of a play, $013532) it ends where the original goes on at $013568 */
static void load_level_files(void)
{
    uint8_t *names = guest.level_names;
    const uint8_t *template = level_name_template;
    uint8_t letter = 'a';
    uint8_t c;

    guest.background = guest.memory_base;
    select_params();
    if (guest.word_0266 != 0)
        letter = 'b';
    do {
        c = *template++;
        if (c == '?')
            c = (uint8_t)('0' + guest.param_index);
        if (c == '*')
            c = letter;
        *names++ = c;
    } while ((int8_t)c >= 0);
    *names++ = *template++;
    names = guest.level_names;
    save_slot_words();
    load_level_objects(names);
    guest.level_files_read = -1;
    load_unpacked((const char *)names + NAME_ALFILS, guest.background);
    for (int i = 0; i < 7000; i++)
        guest.data_1aa66[i] = guest.background[i];
    level_tables_from_disk();
    read_text_table_word_offsets((const uint16_t *)(guest.background + 7000), (uint16_t *)(guest.data_1aa66 + 7000));
    place_pickups();
    {
        uint32_t start = level_starts[(int16_t)(guest.param_index - 1)];

        guest.start_x = (int16_t)(start >> 16);
        guest.start_y = (int16_t)start;
    }
    guest.map_file_name = names;
    load_level_map();
    load_background_tiles(names + NAME_BITS);
    random_table_make();
    scan_password_cells();
    check_flag_286();
    build_table_14cf8();
    if (guest.word_0266 == 0) {
        guest.entry_list[0].address = 0xFFFFFFFF;
        guest.entry_list[0].size = 0xFFFFFFFF;
    }
    load_pattern_data(names);
    guest.flags_0ddd |= 0x10;
    if (guest.word_0266 == 0) {
        const uint32_t *source = &shift_tables[(int16_t)((guest.param_index - 1) * 10)];

        guest.line_text = guest.ptr_0b5a;
        guest.ptr_03ac = guest.ptr_0b5e;
        guest.asset_directory = guest.ptr_0b52;
        if (guest.param_index == 4) {
            guest.line_text = guest.picture_buffer + (0x373BC - 0x31FDE);   /* (the text buffers lie in the picture buffer) */
            guest.ptr_03ac = guest.picture_buffer + (0x379FC - 0x31FDE);
            guest.asset_directory = (uint8_t *)guest.entry_list + 2820;
        }
        guest.asset_mem_end = guest.asset_end_28ec;
        guest.asset_table_end = (struct AssetEntry *)guest.asset_table_28f0;
        guest.asset_flag = 0;
        guest.shift_bits = source[0];
        guest.shift_count = 31;
        guest.shift_source = &source[1];
        assets_load((const char *)objects_name, guest.palette_words, guest.asset_mem_end, guest.asset_table_end);
        guest.asset_flag = -1;
        guest.asset_end_0ca2 = guest.asset_mem_end;
        guest.asset_table_0ca6 = (uint8_t *)guest.asset_table_end;
        if (guest.word_0384 == 0) {
            guest.ptr_26de = names + NAME_SPRITES;
            assets_load((const char *)names + NAME_SPRITES, guest.palette_words, guest.asset_mem_end, guest.asset_table_end);
            guest.menu_state_25a = 0;
        }
    }
    set_level_pointers();
    guest.draw_routine_index = 1;
    guest.screen_row_bytes = 176;   /* the original's value from the start (the bytes of a row of the screen, and the table at $0D9A... of the draw routines) */
    guest.flags_0ddd = 0;
    guest.flags_0ddd |= 2;
    guest.flags_0ddd |= 8;
    guest.clip_rect_list = (const int16_t *)clip_rectangles;
    if (guest.word_0384 != 0)
        return;   /* goes on at $013568 */
}

void begin_quest(void)
{
    if (host_quickload()) {   /* (not in the original) */
        game_loop();
        return;
    }
    guest.param_index = host_start_level();
    guest.word_0248 = 1;   /* (the start of a play: the score, the lives and the weapons are made; the next level of a play does not) */
    guest.menu_active = 0;
    guest.word_0266 = 0;
    level_begin();
}

/* orig $00247C to $002C08 and what comes before it ($002452: the part of Begin Quest that sets the level up from the files, with param_index set; the original's own start of a level in the middle of a play comes
 * to it from $013646 with the stack put back and a few tables cleared: the port's are the ones of the start) */
static void level_begin(void)
{
    reset_marker_table();
    clear_entry_table();
    clear_cell_records();
    do_nothing();
    background_draw();
    loading_level_screen();
    load_level_files();
    if (guest.word_0384 != 0)
        return;   /* goes on at $013568 */

    /* $002AAA */
    if (level_next_wait)
        host_disk_wait();   /* (the original's reads of the next level's files take what its drive takes, the loading screen on show) */
    copper_colors_restore();
    hud_digits_load();
    clear_entry_table();
    guest.word_276 = 0;
    guest.message_shown = 0;
    guest.tick_hold_26c = 0;
    clear_work_area();
    guest.demo_mode = 0;
    reload_sprites();
    guest.word_226 = 0;
    guest.word_228 = 0;
    guest.message_shown = 0;
    guest.message_arg = 0;
    level_state_init();
    widen_bytes_to_words();
    guest.boss_frame = boss_rts;   /* (the original's address $AFD2: not called with flag_23a off) */
    guest.flag_23a = 0;
    guest.code_c58e = 0x4E75;
    guest.code_cb0e = 0x4E75;
    guest.code_c406 = 0x4E75;
    guest.code_bc1a = 0x4E75;
    sort_records_by_kind();
    fill_flags_39fde();
    activate_records_now();
    guest.frame_wait = 2;
    guest.word_1de = 0;
    fill_cce_ones();
    update_energy_display();
    if (guest.menu_active != 0) {
        if (guest.param_index != 1) {   /* a level entered by its password: $00334E */
            password_start_level();
            return;
        }
        guest.menu_active = 0;
    }
    file_read((const char *)sound1_name, guest.picture_buffer);
    if (level_next_wait)
        host_disk_wait();   /* (the original's start of the level fades out when it has done its work) */
    level_next_fade = level_next_wait;
    level_next_wait = 0;
    fade_out(0, 0xFFFF);
    guest.copper_setup_done = 0;
    draw_start_frame();
    draw_start_frame();
    stop_game_sounds();
    play_sound(9, 127);
    screens_flip();
    guest.palette_words[0] = 0;
    if (level_next_fade)
        host_fade_bind();   /* (the lockstep: the fade-in begins when the original calls its own) */
    level_next_fade = 0;
    fade_in(guest.palette_words, 1);
    guest.draw_routine_index = 1;
    guest.word_ddc = 0;
    guest.flags_0ddd |= 8;
    guest.flags_0ddd |= 2;
    guest.tick_hold_26c = 0;
    guest.word_0248 = 0;
    drives_idle();
    if (guest.flag_aa8 != 0)
        play_sound(60, 2);
    else
        stop_sound(60);
    if (guest.param_index == 3)
        play_sound(66, 1);
    game_loop();
}

/* orig $0135C6: what the records of the world that is left held is cleared (the same routines as the start of a level calls, in the order of this handler) */
static void world_records_clear(void)
{
    clear_creature_state();
    clear_records_721e();
    clear_bolts();
    clear_effects();
    clear_cell_records();
    clear_markers();
    fill_table_1a30c();
    records_113c_reset();
    clear_records_1036();
    reset_slots_fd0();
    reset_slots_de26();
    clear_walk_groups();
    clear_monsters();
    clear_records_737e();
}

/* orig $0032D8 to $003314 (the part of the loading of a world that reads its sprites): the sounds are stopped, the sprites of the shop (the set "shop": the grid, the font and the door, the images from 448 on) are read after the world's and the
 * palette's first colour is black */
static void world_assets(void)
{
    stop_game_sounds();
    guest.word_292 = 0x1C0;
    guest.asset_flag = -1;
    guest.flags_0ddd |= 0x10;
    assets_load("shop", (uint16_t *)guest.ptr_0e44, guest.asset_end_0ca2, (struct AssetEntry *)guest.asset_table_0ca6);   /* (the shop set's colours go to the place ptr_0e44 names: the level's stay) */
    guest.palette_words[0] = 0;
    guest.menu_state_25a = 1;
}

/* orig $003316 to $00333E: the shop's door of the world is put in shop_door_x and the next words (x, y and the way it walks); shop_door_index, the count of worlds, goes up (the loading takes it down again) */
static void world_door(void)
{
    const uint16_t *door = &shop_doors_351a[(int16_t)((guest.param_index - 1) * 2 + guest.shop_door_index) * 3];

    guest.shop_door_x = door[0];
    guest.shop_door_y = door[1];
    guest.word_28c = door[2];
    guest.shop_door_index++;
    guest.word_28e = 0;
}

/* orig $0032D8 to $00333E (what the loading of a world runs of the start of a level) */
static void world_set_up(void)
{
    world_assets();
    world_door();
}

/* orig $013532 to $0135C4: the next world of the level is loaded from the disk while the play goes on (the end of the world's record sets it): the tick count is kept, the files are read the way a level's
 * are (the `b` ones: word_0266 is 1), the records of the world are made, the shop's door is set (shop_door_index is as it was after) and the sound file is read again.  The loading takes the frames of the disk */
static void level_load_world(void)
{
    uint32_t saved_ticks = guest.tick_count;

    guest.word_0384 = 1;
    guest.word_0266 = 1;
    play_sound(0, 0);
    wait_vblank();
    guest.tick_hold_38a = 1;
    load_level_files();
    guest.tick_hold_38a = 0;   /* $013568 */
    guest.tick_hold_26c = 0;
    build_table_14cf8();
    records_from_table();
    fix_records_3d86();
    sort_records_by_kind();
    activate_records_now();
    world_set_up();
    guest.shop_door_index--;
    guest.score_redraw = 0xFFFF;
    guest.word_0384 = 0;
    file_read((const char *)sound1_name, guest.picture_buffer);
    drives_idle();
    host_world_load_wait();   /* (the original's drive takes about 600 frames for the files of the world and its sprites) */
    guest.tick_count = saved_ticks;
}

/* ======== the shop (orig $013690 to $0141C0, and the door that leads to it, $003438) ======== */

/* orig $003438 to $003514: the shop's door.  It walks along (6 pixels in a frame in the way word_28c says, turning at a wall or where there is no floor), is drawn from the image word_292 + 4 + 4 * way + step
 * (the step, word_28e, goes on every other frame) and, when it touches the player (a box of 32 by 48 hurts him; his energy is given back), the routine is switched off and the shop is entered.  It runs
 * while the first word of the routine at $3436 is not an rts (the end of a world: $3342).  Returns 1 when the player has gone in */
static int shop_door(void)
{
    if (guest.code_3436 == 0x4E75)
        return 0;

    int16_t x = (int16_t)guest.shop_door_x;
    int16_t y = (int16_t)guest.shop_door_y;
    int16_t way = (int16_t)guest.word_28c;

    if (guest.frame_toggle != 0) {
        x = (int16_t)(x + 6 * way);
        int16_t edge = x & 0x1F;

        if (edge <= 4 || edge >= 0x1C) {
            const uint8_t *cell = guest.ptr_0b86 + 0x2000 + ((int16_t)(y & 0xFFF0) << 3) + ((int16_t)(x & 0xFFE0) >> 5) + way;

            if (cell[0] == 1 || cell[0x80] == 1 || cell[0x100] == 1 || cell[0x180] != 1)
                guest.word_28c = (uint16_t)-(int16_t)guest.word_28c;
        }
    }
    guest.shop_door_x = (uint16_t)x;
    guest.shop_door_y = (uint16_t)y;
    sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)((int16_t)guest.word_28c * 4 + 4 + guest.word_28e + guest.word_292));
    if (guest.frame_toggle == 0)
        guest.word_28e = (guest.word_28e + 1) & 7;
    guest.hit_box_extra_width = 0x20;
    guest.hit_box_extra_height = 0x30;
    if (damage_player_at((int16_t)guest.shop_door_x, (int16_t)guest.shop_door_y, 5) < 0) {
        if (guest.flag_aa8 == 0)
            guest.player_energy += 5;
        guest.code_3436 = 0x4E75;
        return 1;
    }
    return 0;
}

/* orig $0033D4: while the world's end counts down (flag_290) and when the door has come: the player is made to stand (state 0 when the door walks to the left, 1 when to the right) or to go on in the state he is in
 * with the stick the way the door is (bit 2, left, when the door's word is not negative; bit 3, right, when it is) */
void world_end_auto_walk(void)
{
    const uint16_t *door = &shop_doors_351a[(int16_t)((guest.param_index - 1) * 2 + guest.shop_door_index) * 3];

    if ((int16_t)door[2] >= 0) {
        if (guest.player_state != 0)
            guest.game_stick = 4;
        else {
            player_end(guest.word_19a);
            return;
        }
    } else {
        if (guest.player_state != 1)
            guest.game_stick = 8;
        else {
            player_end(guest.word_19a);
            return;
        }
    }
    player_state_run(guest.player_state, guest.word_19a);
}

/* orig $00328A: the screen is copied and "LOADING" and "PLEASE  WAIT" are written on it */
static void draw_loading_screen(void)
{
    copy_other_screen();
    text_draw_at("LOADING", 0x68, 0x28);
    text_draw_at("PLEASE  WAIT", 0x40, 0x64);
    host_iteration_cpu(2);   /* (the lockstep: the flip waits for the original's: it draws the two lines in a frame or two and flips then) */
    screens_flip();
}

/* orig $0032B4 to $003342: the countdown of the world's end (flag_290, 65 steps, one for each frame of the loop): at 25 the creatures' state is cleared; when it goes below 0 the shop's sprites are read if
 * they are not there (with the loading screen), the shop's door is set up and the routine at $3436 is made to run (it is a NOP) */
static void world_end_countdown(void)
{
    if (guest.flag_290 == 0x19)
        clear_creature_state();
    guest.flag_290--;
    if ((int16_t)guest.flag_290 >= 0)
        return;
    if (guest.menu_state_25a != 1) {
        draw_loading_screen();
        world_assets();
    }
    world_door();
    guest.code_3436 = 0x4E71;
}

/* orig $00B09E: the fifteen records of ten bytes at $518 are free (the word at +6 is -1) */
static void boss_free_records(void)
{
    struct BossState *boss = boss_state();

    for (int i = 0; i < 15; i++)
        boss->shots[i].frame = -1;
}

/* orig $00B082 and $00B09E: the 500 bytes at the start of ptr_0e44 are cleared, the records of the boss are free */
static void boss_clear(void)
{
    uint8_t *p = (uint8_t *)guest.ptr_0e44;

    for (int i = 0; i < 500; i++)
        p[i] = 0;
    boss_free_records();
}

/* the level 1 guardian's path (orig $C63E, 70 rows of the picture and the step in x; the byte $FF ends it): the guardian walks left in a loop of steps, jumps and breathes fire (picture 16) */
static const int8_t boss1_path[71][2] = {
    {0, 0}, {1, -4}, {2, -4}, {3, -4}, {4, -4}, {5, -4}, {6, -4}, {7, -4}, {8, -4}, {9, -4}, {0, -4}, {1, -4}, {2, -4}, {10, 0}, {11, 0}, {12, 0}, {13, 0}, {14, 0}, {15, 0},
    {16, 0}, {17, 0}, {4, -4}, {5, -4}, {6, -4}, {7, -4}, {8, -4}, {9, -4}, {0, -4}, {1, -4}, {2, -4}, {10, 0}, {11, 0}, {12, 0}, {13, 0}, {14, 0}, {15, 0}, {16, 0}, {17, 0},
    {10, 0}, {11, 0}, {12, 0}, {13, 0}, {14, 0}, {15, 0}, {16, 0}, {17, 0}, {2, 4}, {1, 4}, {0, 4}, {9, 4}, {8, 4}, {7, 4}, {6, 4}, {5, 4}, {4, 4}, {3, 4}, {2, 4}, {10, 0},
    {11, 0}, {12, 0}, {13, 0}, {14, 0}, {15, 0}, {16, 0}, {17, 0}, {2, 4}, {1, 4}, {0, 4}, {0, 0}, {0, 0}, {-1, 0}};

#define BOSS1_PATH 0xC63Eu

/* orig $00C3D8 (the shot `shot` has just bounced, d0 and d1 are its place): the first free shot becomes the one that goes on from the bounce (the place, the fall speed of the shot and a sideways speed of -6).
 * With no free one the original goes on past the fifteenth record (a2 is left after them): the writes are done there too */
static struct BossShot *boss_bounce_shot_new(const struct BossShot *shot, int16_t x, int16_t y)
{
    struct BossShot *record = boss_state()->shots;
    int i;

    for (i = 0; i < 15; i++, record++)
        if (record->frame < 0)
            break;
    if (i == 15)
        return record;
    record->x = x;
    record->y = y;
    record->speed_y = shot->speed_y;
    record->speed_x = -6;
    record->frame = 0;
    return record;
}

/* orig $00C38A: a shot starts at the guardian (the sound 38, priority 75): the first free record gets its x, 22 below its y, the fall speed 8, the picture 0 and the sideways speed -6 */
static void boss1_shot_start(void)
{
    struct BossState *boss = boss_state();

    play_sound(0x26, 0x4B);
    for (int i = 0; i < 15; i++) {
        struct BossShot *shot = &boss->shots[i];

        if (shot->frame >= 0)
            continue;
        shot->x = boss->x;
        shot->y = (int16_t)(boss->y + 0x16);
        shot->speed_y = 8;
        shot->frame = 0;
        shot->speed_x = -6;
        return;
    }
}

/* orig $00C51E: ten sparks (of the fifty records of ptr_0e44: x, y and the speeds, a record is free when its x is 0) are started at the guardian's mouth: x 3 to 10 and y 23 to 38 from the guardian, the
 * sideways speed 4 to -11 and the other -8 to 7 */
static void boss1_mouth_sparks_start(void)
{
    struct BossState *boss = boss_state();
    int16_t *record = (int16_t *)guest.ptr_0e44;
    int16_t started = 0;

    for (int i = 0; i < 50; i++, record += 5) {
        if (record[0] != 0)
            continue;
        record[0] = (int16_t)((random_word() & 7) + boss->x + 3);
        record[1] = (int16_t)((random_word() & 15) + boss->y + 0x17);

        uint16_t r = random_word();

        record[2] = (int16_t)(-(int16_t)((r & 15) - 4));
        record[3] = (int16_t)(((r >> 4) & 15) - 8);
        started++;
        if (started >= 10)
            return;
    }
}

/* orig $00C5DE: a 2 by 2 pixel spark is put in the screen buffer at the screen place (x, y): the colour 7 (planes 0 to 2 set, plane 3 cleared) at the even pixel column of x + x_offset */
static void boss_spark_pixel(int16_t x, int16_t y)
{
    int16_t px = (int16_t)(x + guest.x_offset);
    int shift = px & 0xE;
    uint8_t *row = guest.screen_buffer + (uint16_t)(y * 44) + (((int16_t)(px & 0xFFF0)) >> 3);
    uint16_t set = (uint16_t)(0xC000u >> shift);
    uint16_t keep = (uint16_t)((0x3FFFu >> shift) | (0x3FFFu << (16 - shift)));

    for (int line = 0; line < 2; line++, row += 44) {
        for (int plane = 0; plane < 4; plane++) {
            uint16_t *word = (uint16_t *)(row + plane * 0x2100);

            *word = (uint16_t)((*word & keep) | (plane < 3 ? set : 0));
        }
    }
}

/* orig $00C58E (called after the guardian's routine; a bare return unless a guardian's start set it to a nop): the sparks move by their speeds (the second one goes down one every other frame) and
 * the ones that are on the screen are drawn; one off it is free */
static void boss_sparks_move(void)
{
    if (guest.code_c58e != 0x4E71)
        return;

    int16_t *record = (int16_t *)guest.ptr_0e44;

    for (int i = 0; i < 50; i++, record += 5) {
        if (record[0] == 0)
            continue;
        record[0] = (int16_t)(record[0] + record[2]);
        record[1] = (int16_t)(record[1] + record[3]);
        if (guest.frame_phase == 0)
            record[3]++;

        int16_t x = (int16_t)(record[0] - guest.scroll_x);
        int16_t y = (int16_t)(record[1] - guest.scroll_y);

        if (x < 0 || y < 0 || x >= 0x13F || y >= 0xBF) {
            record[0] = 0;
            continue;
        }
        boss_spark_pixel(x, y);
    }
}

/* orig $00C406 (a bare return unless a guardian's start set it to a nop; then $C408): the shots go by their speeds.  A shot that is in a wall (a cell of the second map, 1) is over with a spark that
 * goes on with its speeds; one that falls on a floor bounces (sound 35, fall speed -11) and makes a second shot with a sideways speed of its own less one or nothing.  The others turn through their four pictures
 * (the glyphs 347 to 350), are over when they leave the screen, hit the player (aimed_bolt_hits_player) and are drawn */
static void boss_shots_move(void)
{
    if (guest.code_c406 != 0x4E71)
        return;

    struct BossState *boss = boss_state();

    for (int i = 0; i < 15; i++) {
        struct BossShot *shot = &boss->shots[i];

        if (shot->frame < 0)
            continue;

        int16_t x = (int16_t)(shot->x + shot->speed_x);
        int16_t y = (int16_t)(shot->y + shot->speed_y);

        shot->speed_y++;
        if (shot->speed_y >= 0) {
            const uint8_t *cell = guest.ptr_0b86 + 0x2000 + (x >> 5) + (int16_t)(uint16_t)((y & 0xFFF0) << 3);

            if (cell[0] == 1) {
                start_debris_moving((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), shot->speed_x, shot->speed_y);
                shot->frame = -1;
                continue;
            }
            if (cell[0x80] == 1) {
                play_sound(0x23, 3);
                shot->speed_y = -11;

                struct BossShot *next = boss_bounce_shot_new(shot, x, y);

                next->speed_x = shot->speed_x;
                next->speed_x--;
                if (random_word() & 1)
                    next->speed_x++;
            }
        }
        shot->x = x;
        shot->y = y;
        if (guest.frame_toggle == 0)
            shot->frame = (int16_t)((shot->frame + 1) & 3);

        int16_t picture = (int16_t)(shot->frame + 0x15B);

        x = (int16_t)(x - guest.scroll_x);
        if (x <= -16 || x > 0x168) {
            shot->frame = -1;
            continue;
        }
        y = (int16_t)(y - guest.scroll_y);
        aimed_bolt_hits_player((uint32_t *)&shot->frame, (int16_t)(x + 4), (int16_t)(y + 4));
        glyph_draw(x, y, (uint16_t)picture);
    }
}

/* orig $00C374 (a bare return unless the guardian's death set it to a nop): 1000 points are added to the score each frame, ten times the level's number of frames */
static void boss_death_score(void)
{
    if (guest.code_c374 != 0x4E71)
        return;
    score_add_packed(0x10);

    uint16_t *count = (uint16_t *)boss_at(0x668);

    (*count)--;
    if (*count != 0)
        return;
    guest.code_c374 = 0x4E75;
}

/* orig $00C300: the guardian is dead: seven bursts of sparks (spawn_particle_burst) at random places on it, the fight is over (flag_23a off), the pieces of a chest burst from it (the sprites from
 * the twentieth word of words_3fde on) and the score is paid out */
static void boss_die(void)
{
    struct BossState *boss = boss_state();

    guest.flag_a4c = 0;
    for (int n = 0; n < 7; n++) {
        int16_t y = (int16_t)((random_word() & 0x3F) + 0x10);
        int16_t x = (int16_t)((random_word() & 0x3F) + boss->x);

        spawn_particle_burst(x, (int16_t)(y + boss->y));
    }
    guest.word_a54 = -4;
    guest.flag_23a = 0;
    chest_burst_at((int16_t)(boss->x + 0x18), (int16_t)(boss->y + 0x28), words_3fde + 20);
    *(uint16_t *)boss_at(0x668) = (uint16_t)(guest.param_index * 10);
    guest.code_c374 = 0x4E71;
}

/* orig $00C216 (the level 1 guardian, per frame: boss_frame): while it is alive it goes on along its path every other frame (the picture and a step in x; at the end of the path the start again);
 * in the picture 16 it breathes fire (sparks while it moves, a shot in the other frames), sounds go with some pictures; its two halves (the pictures 448 + 2n and +1, 32 apart) are drawn, the player's
 * shots are looked at against a box of 48 by 96 (check_hit_and_walls) and the energy is what is left; when it is below 0 the guardian dies (boss_die) */
static void boss1_frame(void)
{
    struct BossState *boss = boss_state();

    if ((int16_t)boss->energy_high < 0)
        return;

    int16_t picture = (int16_t)boss->picture;

    if (guest.frame_toggle == 0) {
        uint32_t path = ((uint32_t)boss->path_high << 16) | boss->path_low;
        const int8_t *row = boss1_path[(path - BOSS1_PATH) / 2];

        picture = row[0];
        boss->x = (uint16_t)(boss->x + row[1]);
        row += 2;
        if (row[0] < 0)
            path = BOSS1_PATH;
        else
            path += 2;
        boss->path_high = (uint16_t)(path >> 16);
        boss->path_low = (uint16_t)path;
        if (picture == 16)
            boss1_mouth_sparks_start();
    } else if (picture == 16) {
        boss1_shot_start();
    }
    if (picture == 3 || picture == 10 || picture == 9)
        play_sound(0x3D, 0x1E);
    if (picture == 14)
        play_sound(0x3E, 0x29);
    boss->picture = (uint16_t)picture;

    int16_t x = (int16_t)(boss->x - guest.scroll_x);
    int16_t y = (int16_t)(boss->y - guest.scroll_y);
    uint16_t image = (uint16_t)(picture * 2 + 0x1C0);

    sprite_draw(x, y, image);
    sprite_draw((int16_t)(x + 0x20), y, (uint16_t)(image + 1));
    guest.hit_box_extra_width = 0x30;
    guest.hit_box_extra_height = 0x60;

    int16_t energy = check_hit_and_walls((int16_t)(boss->x + 0x10), boss->y, (int16_t)boss->energy_low);

    boss->energy_low = (uint16_t)energy;
    boss->energy_high = energy < 0 ? 0xFFFF : 0;
    if (energy < 0)
        boss_die();
}

/* orig $00B060 (callback_514: the boss's state is made again when a level is started with the flag on): the place of the level 1 boss and its path */
static void boss1_514(void)
{
    struct BossState *boss = boss_state();

    boss->x = 0xF20;
    boss->y = 0x230;
    boss->picture = 0;
    boss->path_high = 0;
    boss->path_low = BOSS1_PATH;   /* the path: the original's table at $C63E */
    boss_clear();
}

/* ======== the level 2 guardian (orig $00B0B4 to $00D008): a body with a jaw that moves about, a neck of ten pieces (a curve of bezier.dat, four weights of a hundred and ninety-nine... 200 words each,
 * halved when read) that is drawn from the head's three joints, and a head at its end that sometimes shoots at the player ======== */

/* the variables of the level 2 guardian in the original's places ($4E8 to $632) */
struct Boss2Box {
    int16_t x;   /* the screen place the piece was drawn at (the hit boxes of $C6F2) */
    int16_t y;
    int16_t w;
    int16_t h;
};

struct Boss2 {
    int16_t x;                        /* $4E8 */
    int16_t y;                        /* $4EA */
    int16_t jaw;                      /* $4EC */
    int16_t neck;                     /* $4EE: the strength of the neck and head (-2: it is gone) */
    int16_t unused_4f0;
    int16_t body;                     /* $4F2: the strength of the body */
    int16_t offset_x;                 /* $4F4: where the body piece goes on its moves from the place */
    int16_t offset_y;                 /* $4F6 */
    int16_t step;                     /* $4F8: the move (a row of three bytes of boss2_moves) it is in, as a byte offset */
    int16_t angle_a;                  /* $4FA: the angles of the neck's three joints (offsets in sine_table, $100 to $258) */
    int16_t angle_b;                  /* $4FC */
    int16_t angle_c;                  /* $4FE */
    int16_t speed_b;                  /* $500 */
    int16_t speed_c;                  /* $502 */
    int16_t speed_a;                  /* $504 */
    int16_t shots_made;               /* $506: shots started in this move (-1: not shooting) */
    uint16_t picture;                 /* $508 */
    uint16_t energy_high;             /* $50A */
    uint16_t energy_low;              /* $50C */
    uint16_t unused_50e[5];           /* $50E */
    struct BossShot shots[8];         /* $518: x, y, count (the +4 word: speed_y), frame (-1: free) */
    uint16_t pad_568[9];
    int16_t cycle;                    /* $57A: 0 to 2, the shots' picture offset */
    uint16_t pad_57c[25];
    int16_t tail_x;                   /* $5AE */
    int16_t tail_y;                   /* $5B0 */
    int16_t target_x;                 /* $5B2 */
    int16_t target_y;                 /* $5B4 */
    int16_t unused_5b6[2];
    struct Boss2Box boxes[7];         /* $5BA: the jaw 1.. see boss2_draw */
    uint16_t pad_5f2[12];
    int16_t points[10][2];            /* $60A: the places of the neck's pieces (screen) */
};

_Static_assert(offsetof(struct Boss2, shots) == 0x518 - 0x4E8, "shots");
_Static_assert(offsetof(struct Boss2, cycle) == 0x57A - 0x4E8, "cycle");
_Static_assert(offsetof(struct Boss2, tail_x) == 0x5AE - 0x4E8, "tail");
_Static_assert(offsetof(struct Boss2, boxes) == 0x5BA - 0x4E8, "boxes");
_Static_assert(offsetof(struct Boss2, points) == 0x60A - 0x4E8, "points");

static struct Boss2 *boss2_state(void)
{
    return (struct Boss2 *)guest.boss_area;
}

/* orig $00CDE4: the moves of the body (the step in x and y and what it is: 0 nothing, 1 and 2 other pictures, 2 it shoots; a negative one ends the table) */
static const int8_t boss2_moves[25][3] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, -1, 0}, {1, -1, 0}, {2, -1, 0}, {3, -1, 0}, {-3, 2, 1}, {-3, 4, 1}, {-3, 4, 1}, {-2, 4, 2}, {0, 0, 2},
    {0, 0, 2}, {0, 0, 2}, {1, -3, 1}, {1, -3, 0}, {1, -2, 0}, {1, -2, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, -1}};
/* orig $00CE30: the jaw's picture (from 448) and place (the y) in its four frames */
static const int16_t boss2_jaw[4][2] = {{6, 0}, {4, 14}, {5, 27}, {4, 14}};
/* orig $00CC96: the head's picture (from 482) places by what it does */
static const int16_t boss2_head[8][2] = {{-8, 0}, {-11, -4}, {-7, -7}, {-4, -10}, {-1, -7}, {2, -4}, {-1, -2}, {-4, 2}};
/* orig $00CCB6: what the head does for the angle of the neck's first joint (twenty for the angle going up, twenty for going down): 1 shoots */
static const int8_t boss2_head_by_angle[40] = {6, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 7, 6, 0, 0, 0, 0};

/* bezier.dat: four weights of 200 words each, halved (orig $CE64) when it is read; the neck's points are weighed from them */
static uint16_t boss2_bezier[800];
/* orig $00CFBA to $00CFCC: the neck's anchor, three joints and the last point */
static int16_t boss2_anchor_x, boss2_anchor_y, boss2_joint_b[2], boss2_joint_c[2], boss2_joint_a[2], boss2_point[2];

static void boss2_bezier_load(void)
{
    static uint16_t raw[1024];   /* (the file layer reads whole sectors, and makes the words of a .dat file native) */

    file_read("bezier.dat", raw);
    for (int i = 0; i < 800; i++)
        boss2_bezier[i] = (uint16_t)(raw[i] >> 1);
}

/* orig $00B09E and the rest of $00B0EE: the eight shots are free, the body is at its start place, the neck's joints are at their angles */
static void boss2_514(void)
{
    struct Boss2 *boss = boss2_state();

    guest.word_268 = 1;
    boss->x = 0x280;
    boss->y = 0x370;
    boss->offset_x = -28;
    boss->offset_y = -30;
    boss->step = 0;
    boss->shots_made = -1;
    boss->angle_a = 0x100;
    boss->angle_c = 0x150;
    boss->angle_b = 0x130;
    boss->speed_a = 0x18;
    boss->speed_b = 0x18;
    boss->speed_c = 0x18;
    boss->target_x = 0x280;
    boss->target_y = 0x370;
    boss->unused_5b6[0] = 0;
    boss->unused_5b6[1] = 0;
    boss_free_records();
    boss->cycle = 0;
}

/* orig $00C7BA: the body goes to the point it aims at (a quarter of the way, at most 12 in x); when it is there, or while it shoots, a new point is taken: 112 right and 60 above the player or
 * (while the player is in front of a thing of the kind 109 in the list of this frame) of that thing, further right the further the player is right of 128, and a spread of 16 either way */
static void boss2_body_seek_target(void)
{
    struct Boss2 *boss = boss2_state();
    int pick = boss->shots_made >= 0;

    if (!pick) {
        if (boss->x == boss->target_x && boss->y == boss->target_y) {
            pick = 1;
        } else {
            int16_t dx = (int16_t)((boss->x - boss->target_x) >> 2);
            int16_t dy = (int16_t)((boss->y - boss->target_y) >> 2);

            if (dx > 12)
                dx = 12;
            boss->x = (int16_t)(boss->x - dx);
            boss->y = (int16_t)(boss->y - dy);
            return;
        }
    }
    for (;;) {
        if (guest.word_037c <= 2)
            return;

        int16_t tx = guest.player_x;
        int16_t ty = guest.player_y;

        if (guest.list_count_1294 != 0) {
            int16_t n = guest.list_count_1294 > 0x14 ? 0x14 : (int16_t)guest.list_count_1294;
            const int16_t *thing = (const int16_t *)guest.list_121c;
            int found = 0;

            for (int i = 0; i < n; i++, thing += 3) {
                if (thing[2] == 0x6D) {
                    tx = (int16_t)(thing[0] + guest.scroll_x);
                    ty = (int16_t)(thing[1] + guest.scroll_y - 0x30);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                tx = guest.player_x;
                ty = guest.player_y;
            }
        }

        int16_t away = (int16_t)(tx - 0x80);

        tx = (int16_t)(tx + 0x70);
        ty = (int16_t)(ty - 0x3C);
        if (away >= 0) {
            away = (int16_t)(away >> 2);
            ty = (int16_t)(ty + away);
            away = (int16_t)(away >> 1);
            tx = (int16_t)(tx - away);
        }
        tx = (int16_t)(tx + (int16_t)((random_word() & 0x1F) - 0x10));
        ty = (int16_t)(ty + (int16_t)((random_word() & 0x1F) - 0x10));
        boss->target_x = tx;
        boss->target_y = ty;
        if (boss->shots_made >= 0)
            return;
        /* $C852 */
        if (boss->x == tx && boss->y == ty)
            continue;
        int16_t dx = (int16_t)((boss->x - tx) >> 2);
        int16_t dy = (int16_t)((boss->y - ty) >> 2);

        if (dx > 12)
            dx = 12;
        boss->x = (int16_t)(boss->x - dx);
        boss->y = (int16_t)(boss->y - dy);
        return;
    }
}

/* orig $00CA8E: a shot is started at the body (the sound 65 with the first of a row): the first free record gets the place (6 right and 16 below the body's piece), the count 7 and the frame 0 */
static void boss2_shot_start(void)
{
    struct Boss2 *boss = boss2_state();

    if (boss->shots_made == 7)
        return;
    if (boss->shots_made == 0)
        play_sound(0x41, 0x3C);
    for (int i = 0; i < 8; i++) {
        struct BossShot *shot = &boss->shots[i];

        if (shot->frame >= 0)
            continue;
        shot->x = (int16_t)(boss->x + boss->offset_x + 6);
        shot->y = (int16_t)(boss->y + boss->offset_y + 0x10);
        shot->speed_y = 7;
        shot->frame = 0;
        boss->shots_made++;
        return;
    }
}

/* orig $00CCDE: the neck's three joints are put at their angles from the anchor (the sine and cosine of sine_table less 9, 11 and 10 bits), and the angles go on by their speeds (back at $100 and $258) */
static void boss2_joints_move(void)
{
    struct Boss2 *boss = boss2_state();
    int16_t *angles[3] = {&boss->angle_a, &boss->angle_b, &boss->angle_c};
    int16_t *speeds[3] = {&boss->speed_a, &boss->speed_b, &boss->speed_c};
    int16_t *joints[3] = {boss2_joint_a, boss2_joint_b, boss2_joint_c};
    static const int bits[3] = {9, 11, 10};

    for (int i = 0; i < 3; i++) {
        int16_t sine = (int16_t)guest.sine_table[(uint16_t)*angles[i] >> 1];
        int16_t cosine = (int16_t)-(int16_t)guest.sine_table[((uint16_t)*angles[i] + 0x100) >> 1];

        sine = (int16_t)(sine >> bits[i]);
        cosine = (int16_t)(cosine >> bits[i]);
        if (i == 0) {
            sine = (int16_t)(sine >> 1);
            cosine = (int16_t)(cosine >> 1);
            sine = (int16_t)(sine + (sine >> 1));
            cosine = (int16_t)(cosine + (cosine >> 1));
        }
        joints[i][0] = (int16_t)(sine + boss2_anchor_x);
        joints[i][1] = (int16_t)(cosine + boss2_anchor_y);
    }
    for (int i = 0; i < 3; i++) {
        int16_t next = (int16_t)(*angles[i] + *speeds[i]);

        if (next <= 0x100)
            *speeds[i] = (int16_t)-*speeds[i];
        if (next >= 0x258)
            *speeds[i] = (int16_t)-*speeds[i];
        *angles[i] = next;
    }
}

/* orig $00CF0A: the point of the neck at the weight index `index` (weights of the four tables of bezier.dat times the anchor and the three joints, 15 bits down) */
static void boss2_neck_point(int16_t index)
{
    const int16_t *anchor[4] = {&boss2_anchor_x, boss2_joint_b, boss2_joint_c, boss2_joint_a};
    int16_t x = 0, y = 0;

    for (int i = 0; i < 4; i++) {
        int16_t weight = (int16_t)boss2_bezier[i * 200 + index];
        int16_t px = i == 0 ? boss2_anchor_x : anchor[i][0];
        int16_t py = i == 0 ? boss2_anchor_y : anchor[i][1];

        x = (int16_t)(x + (int16_t)(((int32_t)weight * px) >> 15));
        y = (int16_t)(y + (int16_t)(((int32_t)weight * py) >> 15));
    }
    boss2_point[0] = x;
    boss2_point[1] = y;
}

/* orig $00CE78: the neck is drawn as ten pieces (the glyph 451, every 20 words of the weights) and their places kept (for the hits); when it is dying (the strength below 0) the pieces fly off as debris
 * with the speed they moved at, and the neck is gone (-2) */
static void boss2_neck_draw(void)
{
    struct Boss2 *boss = boss2_state();

    if (boss->neck < -1)
        return;
    int k = 0;

    for (int16_t index = 0; index < 0xC8; index += 0x14, k++) {
        boss->tail_x = boss2_point[0];
        boss->tail_y = boss2_point[1];
        boss2_neck_point(index);

        int16_t x = (int16_t)(boss2_point[0] - guest.scroll_x);
        int16_t y = (int16_t)(boss2_point[1] - guest.scroll_y);

        if (boss->neck < 0) {
            start_debris_moving((int16_t)(x - 2), (int16_t)(y - 2), (int16_t)(x - boss->points[k][0]), (int16_t)(y - boss->points[k][1]));
            x = (int16_t)(x - 2);   /* (the original's d0 and d1 are 2 less from there on: the piece is drawn so) */
            y = (int16_t)(y - 2);
        } else {
            boss->points[k][0] = x;
            boss->points[k][1] = y;
        }
        glyph_draw(x, y, 0x1C3);
    }
    if (boss->neck < 0)
        boss->neck = -2;
}

/* orig $00CBCA: the head at the end of the neck: its picture by the first joint's angle (boss2_head_by_angle); the picture 1 shoots at the player from the end of the neck (aimed_shot, the player's place
 * moved a little at random for it) in the frames of frame_toggle */
static void boss2_head_run(void)
{
    struct Boss2 *boss = boss2_state();

    if (boss->neck <= -2)
        return;

    int16_t angle = (int16_t)(boss->angle_a - 0x100);

    if (angle < 0)
        angle = 0;

    const int8_t *table = boss2_head_by_angle + (boss->speed_a < 0 ? 20 : 0);
    int16_t kind = table[(uint16_t)angle / 0x18];

    if (kind == 1 && guest.frame_toggle != 0) {
        int16_t old_x = guest.player_x;
        int16_t old_y = guest.player_y;

        guest.player_y = (int16_t)(guest.player_y - (int16_t)((random_word() & 7) - 3));
        guest.player_x = (int16_t)(guest.player_x + (int16_t)((random_word() & 0xF) - 8));
        aimed_shot((int16_t)(boss2_point[0] + (int16_t)(((uint16_t)table[0] << 8) | (uint8_t)table[1])), (int16_t)(boss2_point[1] + (int16_t)(((uint16_t)table[2] << 8) | (uint8_t)table[3])), 6, 0);
        guest.player_y = old_y;
        guest.player_x = old_x;
    }

    int16_t x = (int16_t)(boss2_point[0] + boss2_head[kind][0] - guest.scroll_x);
    int16_t y = (int16_t)(boss2_point[1] + boss2_head[kind][1] - guest.scroll_y);

    boss->boxes[6].x = x;
    boss->boxes[6].y = y;
    boss->boxes[6].w = 0x10;
    boss->boxes[6].h = 0x10;
    glyph_draw(x, y, (uint16_t)(0x1E2 + kind));
}

/* orig $00C886: the body, its jaw, the neck and the head are drawn; every other frame the body goes on in its moves (boss2_moves); the pieces' places are kept as the boxes the hits are made against */
static void boss2_draw_all(void)
{
    struct Boss2 *boss = boss2_state();
    int16_t x = (int16_t)(boss->x - guest.scroll_x + boss->offset_x);
    int16_t y = (int16_t)(boss->y - guest.scroll_y + boss->offset_y);
    int16_t step = boss->step;

    if (guest.frame_phase == 0) {
        step = (int16_t)(step + 3);
        if (boss2_moves[step / 3][2] < 0)
            step = 0;
        boss->step = step;
        x = (int16_t)(x + boss2_moves[step / 3][0]);
        boss->offset_x = (int16_t)(boss->offset_x + boss2_moves[step / 3][0]);
        y = (int16_t)(y + boss2_moves[step / 3][1]);
        boss->offset_y = (int16_t)(boss->offset_y + boss2_moves[step / 3][1]);
    }

    int16_t kind = boss2_moves[step / 3][2];

    if (kind == 2)
        boss2_shot_start();
    else
        boss->shots_made = -1;
    kind = (int16_t)(kind + 0x1C7);

    boss->boxes[0] = (struct Boss2Box){x, y, 0x20, 0x20};

    int16_t bx = (int16_t)(boss->x - guest.scroll_x);
    int16_t by = (int16_t)(boss->y - guest.scroll_y);
    int16_t x4 = (int16_t)(bx + 0x1E + boss->offset_x);
    int16_t y4 = (int16_t)(by + 5 + boss->offset_y);
    int16_t x2 = (int16_t)(bx + 0x10);
    int16_t y2 = (int16_t)(by - 0xB);

    boss->boxes[1] = (struct Boss2Box){x2, y2, 0x10, 0x10};
    glyph_draw(x2, y2, 0x1C2);

    int16_t half_x = (int16_t)((x2 - x4) >> 1);
    int16_t half_y = (int16_t)((y2 - y4) >> 1);
    int16_t x3 = (int16_t)(x2 - half_x + 1);
    int16_t y3 = (int16_t)(y2 - half_y);

    boss->boxes[2] = (struct Boss2Box){x3, y3, 0x10, 0x10};
    glyph_draw(x3, y3, 0x1C2);
    boss->boxes[3] = (struct Boss2Box){x4, y4, 0x10, 0x10};
    glyph_draw(x4, y4, 0x1C2);
    sprite_draw(x, y, (uint16_t)kind);

    x = (int16_t)(bx + 0x24);
    y = (int16_t)(by - 0x1A);
    if (guest.frame_phase == 0) {
        boss->jaw = (int16_t)((boss->jaw + 1) & 3);
        if (boss->jaw == 0)
            play_sound(0x27, 0x0B);
    }
    y = (int16_t)(y + boss2_jaw[boss->jaw][1]);
    boss->boxes[4] = (struct Boss2Box){x, y, 0x10, 0x10};
    sprite_draw(x, y, (uint16_t)(0x1C0 + boss2_jaw[boss->jaw][0]));

    boss2_anchor_x = (int16_t)(boss->x + 0x3C);
    boss2_anchor_y = (int16_t)(boss->y + 0x1F);
    boss2_joints_move();
    boss2_neck_draw();
    boss2_head_run();

    x = bx;
    y = by;
    boss->boxes[5] = (struct Boss2Box){x, y, 0x40, 0x40};
    sprite_draw(x, y, 0x1C0);
    sprite_draw((int16_t)(x + 0x20), y, 0x1C1);
}

/* orig $00C6F2: the pieces are hit by the player's shots: the six boxes of the body, the head and the pieces of the neck (8 by 8) each against the strength it has (check_hit_and_walls); then the neck's
 * strength is shared (half of what it has more than the body's goes to the body), the energy is the two together and when it is below 0 the guardian dies (the thing 31 is dropped at (352, 896)) */
static void boss2_hit_by_shots(void)
{
    struct Boss2 *boss = boss2_state();

    for (int i = 0; i < 6; i++) {
        guest.hit_box_extra_width = boss->boxes[i].w;
        guest.hit_box_extra_height = boss->boxes[i].h;
        boss->body = check_hit_and_walls((int16_t)(boss->boxes[i].x + guest.scroll_x), (int16_t)(boss->boxes[i].y + guest.scroll_y), boss->body);
    }
    if (boss->neck >= 0) {
        guest.hit_box_extra_width = boss->boxes[6].w;
        guest.hit_box_extra_height = boss->boxes[6].h;
        boss->neck = check_hit_and_walls((int16_t)(boss->boxes[6].x + guest.scroll_x), (int16_t)(boss->boxes[6].y + guest.scroll_y), boss->neck);
        for (int i = 0; i < 10; i++) {
            guest.hit_box_extra_width = 8;
            guest.hit_box_extra_height = 8;
            boss->neck = check_hit_and_walls((int16_t)(boss->points[i][0] + guest.scroll_x), (int16_t)(boss->points[i][1] + guest.scroll_y), boss->neck);
        }
    }

    int16_t neck = boss->neck;
    int16_t total = (int16_t)((neck < 0 ? 0 : neck) + boss->body);

    boss->energy_low = (uint16_t)total;
    boss->energy_high = total < 0 ? 0xFFFF : 0;
    if (total < 0) {
        drop_thing(0x160, 0x380, 0x1F);
        boss_die();
        return;
    }
    if (neck >= 0 && neck >= boss->body) {
        neck = (int16_t)(neck >> 1);
        boss->neck = (int16_t)(boss->neck - neck);
        boss->body = (int16_t)(boss->body + neck);
    }
}

/* orig $00C6CC (the level 2 guardian, per frame: boss_frame): the scroll is eased to (160, 832), the body goes after its point, is drawn and hit */
static void boss2_frame(void)
{
    guest.scroll_ease_x = 0xA0;
    guest.word_380 = 0x340;
    if (guest.word_037c == 0)
        guest.word_037c = 1;
    boss2_body_seek_target();
    boss2_draw_all();
    boss2_hit_by_shots();
}

/* orig $00CB10 (called after the sparks, a bare return unless the level 2 guardian's start made it a nop): the shots fall by 7 to the left and 5 down; one below the screen's row 176 bursts
 * into debris; the others turn through their pictures (the glyph 458 + 3 * count + the cycle), hit the player and are drawn; every other frame the count goes down to 0 */
static void boss2_shots_move(void)
{
    if (guest.code_cb0e != 0x4E71)
        return;

    struct Boss2 *boss = boss2_state();
    int16_t cycle = boss->cycle;

    for (int i = 0; i < 8; i++) {
        struct BossShot *shot = &boss->shots[i];

        if (shot->frame >= 0) {
            shot->x = (int16_t)(shot->x - 7);
            shot->y = (int16_t)(shot->y + 5);

            int16_t x = (int16_t)(shot->x - guest.scroll_x);
            int16_t y = (int16_t)(shot->y - guest.scroll_y);

            if (y >= 0xB0) {
                int16_t speed_x = (int16_t)(-6 - (random_word() & 3));
                int16_t speed_y = (int16_t)(random_word() & 3);

                start_debris_moving(x, y, speed_x, speed_y);
                shot->frame = -1;
            } else {
                glyph_draw(x, y, (uint16_t)(shot->speed_y * 3 + cycle + 0x1CA));
                aimed_bolt_hits_player((uint32_t *)&shot->speed_y, (int16_t)(x + 4), (int16_t)(y + 4));
                if (guest.frame_phase == 0) {
                    shot->speed_y--;
                    if (shot->speed_y < 0)
                        shot->speed_y = 0;
                }
            }
        }
        cycle++;
        if (cycle >= 3)
            cycle = 0;
    }
    boss->cycle++;
    if (boss->cycle >= 3)
        boss->cycle = 0;
}

/* ======== the level 3 guardian (orig $00B158, $00B17E and $00BBF8 to $00BF3A, the path at $BF3C): a big creature that walks along a path of three bytes (a picture, a step in x, a step in y; the picture 29 and 39 shoot a
 * ball to the left and to the right), the two halves of the picture (448 + 2n and +1, 32 apart) are drawn, 15 balls fly and bounce... ======== */

#define BOSS3_PATH 0xBF3Cu

/* the guardian's path (orig $BF3C: picture, step in x, step in y; a row of -3 knocks the player back, one of -2 goes 31 rows back unless the guardian is right of the player; a first byte of -1 ends the table) */
static const int8_t boss3_path[][3] = {
    {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0}, {6, -4, 0}, {6, 0, 0},
    {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0}, {14, -4, 0}, {14, 0, 0},
    {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0}, {6, -4, 0}, {6, 0, 0},
    {25, 0, 0}, {25, 0, 0}, {27, 0, 0}, {27, 0, 0}, {29, 0, 0}, {29, 0, 0}, {29, 0, 0}, {25, 0, 0},
    {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0}, {14, -4, 0}, {14, 0, 0},
    {12, 4, 0}, {12, 0, 0}, {10, 4, 0}, {10, 0, 0}, {8, 4, 0}, {8, 0, 0}, {6, 4, 0}, {6, 0, 0},
    {4, 4, 0}, {4, 0, 0}, {2, 4, 0}, {2, 0, 0}, {0, 4, 0}, {0, 0, 0}, {14, 4, 0}, {14, 0, 0},
    {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0}, {6, -4, 0}, {6, 0, 0},
    {25, 0, 0}, {25, 0, 0}, {27, 0, 0}, {27, 0, 0}, {29, 0, 0}, {29, 0, 0}, {29, 0, 0}, {25, 0, 0},
    {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0}, {14, -4, 0}, {14, 0, 0},
    {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0}, {6, -4, 0}, {6, 0, 0},
    {25, 0, 0}, {27, 0, 0}, {31, -6, -8}, {31, -6, -8}, {31, -6, -8}, {31, -6, -8}, {31, -6, -8}, {31, -6, -7},
    {31, -6, -6}, {31, -6, -5}, {31, -6, -4}, {31, -6, -3}, {31, -6, -2}, {31, -6, -1}, {31, -6, 0}, {31, -6, 1},
    {31, -6, 2}, {31, -6, 3}, {31, -6, 4}, {31, -6, 5}, {31, -6, 6}, {31, -6, 7}, {31, -6, 8}, {31, -6, 8},
    {31, -6, 8}, {31, -6, 8}, {31, -6, 8}, {-3, 0, 0}, {25, 0, 0}, {25, 0, 0}, {25, 0, 0}, {25, 0, 0},
    {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {37, 0, 0}, {37, 0, 0}, {39, 0, 0}, {39, 0, 0},
    {39, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {37, 0, 0},
    {37, 0, 0}, {33, 6, -8}, {33, 6, -8}, {33, 6, -8}, {33, 6, -8}, {33, 6, -8}, {33, 6, -7}, {33, 6, -6},
    {33, 6, -5}, {33, 6, -4}, {33, 6, -3}, {33, 6, -2}, {33, 6, -1}, {33, 6, 0}, {33, 6, 1}, {33, 6, 2},
    {33, 6, 3}, {33, 6, 4}, {33, 6, 5}, {33, 6, 6}, {33, 6, 7}, {33, 6, 8}, {33, 6, 8}, {33, 6, 8},
    {33, 6, 8}, {33, 6, 8}, {-3, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {35, 0, 0}, {-2, 0, 0},
    {25, 0, 0}, {25, 0, 0}, {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0},
    {14, -4, 0}, {14, 0, 0}, {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0},
    {6, -4, 0}, {6, 0, 0}, {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0},
    {14, -4, 0}, {14, 0, 0}, {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0},
    {6, -4, 0}, {6, 0, 0}, {25, 0, 0}, {25, 0, 0}, {27, 0, 0}, {27, 0, 0}, {29, 0, 0}, {29, 0, 0},
    {29, 0, 0}, {25, 0, 0}, {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0},
    {14, -4, 0}, {14, 0, 0}, {0, -4, 0}, {0, 0, 0}, {2, -4, 0}, {2, 0, 0}, {4, -4, 0}, {4, 0, 0},
    {6, -4, 0}, {6, 0, 0}, {25, 0, 0}, {25, 0, 0}, {27, 0, 0}, {27, 0, 0}, {29, 0, 0}, {29, 0, 0},
    {29, 0, 0}, {25, 0, 0}, {8, -4, 0}, {8, 0, 0}, {10, -4, 0}, {10, 0, 0}, {12, -4, 0}, {12, 0, 0},
    {14, -4, 0}, {14, 0, 0}, {-1, 0, 0}
};

/* a ball of the guardian (the records at $518: ten bytes, 15 of them): its place, the sideways speed, the picture (0 to 3, -1: free) and the fall speed (it goes up by one each frame) */
struct Boss3Shot {
    int16_t x;
    int16_t y;
    int16_t speed_x;
    int16_t frame;
    int16_t speed_y;
};

#define BOSS3_CAN_SHOOT (*(uint16_t *)boss_at(0x5B2))
#define BOSS3_FALL (*(uint16_t *)boss_at(0x5B8))

/* orig $00BD6E: a spark of blood (debris) at the place (x, y) of the screen: moved by up to 8 in each direction, with speeds -4 to 3 */
static void boss3_blood_spark(int16_t x, int16_t y)
{
    int16_t saved_x = x;

    y = (int16_t)(y + (int16_t)((random_word() & 0xF) - 8));

    int16_t speed_x = (int16_t)((random_word() & 7) - 4);
    int16_t speed_y = (int16_t)((random_word() & 7) - 4);

    x = (int16_t)(saved_x + (int16_t)((random_word() & 0xF) - 8));
    start_debris_moving(x, y, speed_x, speed_y);
}

/* orig $00BCDA: the fifteen balls: each goes by its speeds (the fall speed goes up by one), the picture goes on every other frame (frame_toggle 0), one below the screen is free, the others are drawn as the sprite 469 + the picture and
 * hit the player (aimed_bolt_hits_player 12 pixels in): a hit takes 2 more of the energy and two bursts of blood */
static void boss3_balls_move(void)
{
    struct Boss3Shot *shot = (struct Boss3Shot *)boss_state()->shots;

    for (int i = 0; i < 15; i++, shot++) {
        if (shot->frame < 0)
            continue;

        int16_t x = (int16_t)(shot->x + shot->speed_x);
        int16_t y = (int16_t)(shot->y + shot->speed_y);
        int16_t picture = shot->frame;

        shot->x = x;
        shot->y = y;
        shot->speed_y++;
        if (guest.frame_toggle == 0)
            shot->frame = (int16_t)((shot->frame + 1) & 3);

        x = (int16_t)(x - guest.scroll_x);
        y = (int16_t)(y - guest.scroll_y);
        if (y >= 0xC0) {
            shot->frame = -1;
            continue;
        }
        sprite_draw(x, y, (uint16_t)(picture + 0x1D5));
        aimed_bolt_hits_player((uint32_t *)&shot->frame, (int16_t)(x + 0xC), (int16_t)(y + 0xC));
        if (shot->frame < 0) {
            guest.player_energy = (uint16_t)(guest.player_energy - 2);
            boss3_blood_spark(x, y);
            boss3_blood_spark(x, y);
        }
    }
}

/* orig $00BE1E and $00BDA8: when the guardian has just come into the picture 29 (to the left) or 39 (to the right) a ball starts at its mouth in the first free record (3 or 29 right of it, 19 down, the sideways speed -8 or 8,
 * the picture 2, the fall speed -1 or 1 by chance) */
static void boss3_balls_start(int16_t picture)
{
    struct Boss3Shot *shot = (struct Boss3Shot *)boss_state()->shots;
    struct BossState *boss = boss_state();
    int right;

    if (BOSS3_CAN_SHOOT == 0)
        return;
    if (picture == 0x1D)
        right = 0;
    else if (picture == 0x27)
        right = 1;
    else
        return;
    for (int i = 0; i < 15; i++, shot++) {
        if (shot->frame >= 0)
            continue;
        shot->x = (int16_t)(boss->x + (right ? 0x1D : 3));
        shot->y = (int16_t)(boss->y + 0x13);
        shot->speed_x = (int16_t)(right ? 8 : -8);
        shot->frame = 2;
        shot->speed_y = (int16_t)((random_word() & 2) - 1);
        return;
    }
}

/* orig $00BF14: the two halves of the guardian are drawn (the sprites 448 + the picture and the next, 32 further) */
static void boss3_draw_halves(int16_t x, int16_t y, int16_t picture)
{
    x = (int16_t)(x - guest.scroll_x);
    y = (int16_t)(y - guest.scroll_y);
    picture = (int16_t)(picture + 0x1C0);
    sprite_draw(x, y, (uint16_t)picture);
    sprite_draw((int16_t)(x + 0x20), y, (uint16_t)(picture + 1));
}

/* orig $00BE34 (the guardian's path): it goes along the rows of the path from the place $510 names: the sound 61 starts with a picture of 0, 25 or 8 (when it is not the one it was in), a row of -3 knocks the player
 * back (the sound 63; the player's place goes 16 up, the scroll 8) and goes on to the next row, a row of -2 goes 31 rows back when the guardian is not right of the player (and on to the next row when it is);
 * a row with a picture of 0 or more moves it (x in 96 to 704) and is the one of the frame, ending the path restarts it.  `*x`, `*y` and `*picture` are the place and the picture at the end */
static void boss3_follow_path(int16_t *x_out, int16_t *y_out, int16_t *picture_out)
{
    struct BossState *boss = boss_state();
    int16_t x = (int16_t)boss->x;
    int16_t y = (int16_t)boss->y;
    int16_t picture = (int16_t)boss->picture;
    uint32_t path = ((uint32_t)boss->path_high << 16) | boss->path_low;

    BOSS3_CAN_SHOOT = 1;
    if (picture == 0x1D || picture == 0x27)
        BOSS3_CAN_SHOOT = 0;

    for (;;) {
        const int8_t *row = &boss3_path[0][0] + (path - BOSS3_PATH);   /* (a row is three bytes, as the path's addresses go by) */
        int16_t id = (uint8_t)row[0];   /* (move.b into a word that has the high byte 0) */
        int16_t dx = row[1];
        int16_t dy = row[2];

        path += 3;
        if (id != picture && (id == 0 || id == 0x19 || id == 8))
            play_sound(0x3D, 0x28);
        if ((int8_t)id >= 0) {
            x = (int16_t)(x + dx);
            if (x <= 0x60)
                x = 0x60;
            if (x > 0x2C0)
                x = 0x2C0;
            boss->x = (uint16_t)x;
            y = (int16_t)(y + dy);
            boss->y = (uint16_t)y;
            if ((uint8_t)(&boss3_path[0][0])[path - BOSS3_PATH] == 0xFF)
                path = BOSS3_PATH;
            picture = (int16_t)(int8_t)id;
            break;
        }
        if ((int8_t)id == -3) {
            guest.flag_26e = 8;
            guest.scroll_y_b = (int16_t)(guest.scroll_y_b - 8);
            guest.player_y = (int16_t)(guest.player_y - 0x10);
            play_sound(0x3F, 0x4B);
            continue;
        }
        if (x > guest.player_x)
            continue;
        path -= 0x5D;
    }
    boss->picture = (uint16_t)picture;
    boss->path_high = (uint16_t)(path >> 16);
    boss->path_low = (uint16_t)path;
    *x_out = x;
    *y_out = y;
    *picture_out = picture;
}

/* orig $00BCC8: the thing the guardian leaves (kind 31) is dropped at (544, 128) */
static void boss3_drop_item(void)
{
    drop_thing(0x220, 0x80, 0x1F);
}

/* orig $00BC86: the player's shots are looked at against a box of 40 by 88 at the guardian (12 right of its place); the energy is what is left; below 0 the guardian dies (boss_die), the death's fall starts (bc1a is switched on),
 * the thing it leaves is dropped and armour_level (the armour) goes up */
static void boss3_hit_by_shots(void)
{
    struct BossState *boss = boss_state();

    guest.hit_box_extra_width = 0x28;
    guest.hit_box_extra_height = 0x58;

    int16_t energy = check_hit_and_walls((int16_t)(boss->x + 0xC), boss->y, (int16_t)boss->energy_low);

    boss->energy_low = (uint16_t)energy;
    boss->energy_high = energy < 0 ? 0xFFFF : 0;
    if (energy >= 0)
        return;
    boss_die();
    guest.code_bc1a = 0x4E71;
    BOSS3_FALL = 0;
    boss3_drop_item();
    guest.armour_level++;
}

/* orig $00BBF8 (the level 3 guardian, per frame: boss_frame): the path, the balls, a new ball, the drawing and the hit; a dead guardian (energy below 0) is left to the death fall at $BC1C */
static void boss3_frame(void)
{
    struct BossState *boss = boss_state();
    int16_t x, y, picture;

    if ((int16_t)boss->energy_high < 0)
        return;   /* (the original goes on with whatever the registers have: after its death the frame is not run: flag_23a is off) */
    boss3_follow_path(&x, &y, &picture);
    boss3_balls_move();
    boss3_balls_start(picture);
    boss3_draw_halves(x, y, picture);
    boss3_hit_by_shots();
}

/* orig $00BC1C (what $BC1A, called each frame after the fight's callbacks, becomes when the guardian dies): the body falls to y 112 (the fall speed goes up to 16) in the picture 41, or 43 when its picture was 32 or more,
 * and is drawn as its two halves */
void boss3_death_fall(void)
{
    if (guest.code_bc1a != 0x4E71)
        return;

    struct BossState *boss = boss_state();
    int16_t picture = (int16_t)boss->picture >= 0x20 ? 0x2B : 0x29;
    int16_t y = (int16_t)(boss->y + BOSS3_FALL);

    if (y >= 0x70) {
        y = 0x70;
        BOSS3_FALL = 0;
    } else {
        BOSS3_FALL++;
        if (BOSS3_FALL >= 0x10)
            BOSS3_FALL = 0x10;
    }
    boss->y = (uint16_t)y;
    boss3_draw_halves((int16_t)boss->x, y, picture);
}

/* ======== the level 4 guardian (orig $00B198 to $00BBF8, the last of the table at $B014): eight pieces (the records at $10B0: x, y, the frames to wait, the place in the path and where it ends) follow a
 * path of the level (the last entry of entry_list: a step of two signed nibbles each) two steps a frame, the second is the head; the head is hit (energy $50E) and when it is gone the pieces burst, the
 * guardian's own energy ($50A, a long) goes down by 1000 and they start again; it breathes a ball of fire at the player; the throne rises from below the screen when the fight begins (callback_4e4) ======== */

#define B4_WORD(address) (*(int16_t *)boss_at(address))
#define BOSS4_Y B4_WORD(0x65A)            /* the throne's y (the guardian sits on it): $180 and it rises to $B0 */
#define BOSS4_DONE B4_WORD(0x65C)         /* the count of the pieces that are at the end of their path (+1) or burst (-1) */
#define BOSS4_HEAD_ENERGY B4_WORD(0x50E)  /* the head's energy: 750, then 1000 (negative: the pieces burst) */
#define BOSS4_FIRE_TIMER B4_WORD(0x4FA)   /* the frames until the fire: it breathes while it is negative */
#define BOSS4_THRONE_Y B4_WORD(0x4EC)     /* the throne's y of the last frame of its rise */
#define BOSS4_LAST_X B4_WORD(0x5AE)       /* the place of the piece that was run last */
#define BOSS4_LAST_Y B4_WORD(0x5B0)
#define BOSS4_FIRE_X B4_WORD(0x518)       /* the ball of fire: x, y, its picture (-1: none) and the frames of its life (-1: it has hit) */
#define BOSS4_FIRE_Y B4_WORD(0x51A)
#define BOSS4_FIRE_PICTURE B4_WORD(0x51C)
#define BOSS4_FIRE_LIFE B4_WORD(0x51E)
#define BOSS4_HEAD_TABLE_HIGH B4_WORD(0x664)   /* where the head's table is (the long at $664: an address of the original's $B574 on) */
#define BOSS4_HEAD_TABLE_LOW B4_WORD(0x666)
#define BOSS4_HEAD_TABLE_BASE 0xB574u

/* a piece of the guardian's body (14 bytes at $10B0, eight of them): the place, the frames to wait (they count down to -1; then it moves), the place in the path and the end of the path (longs, kept as two words each); a
 * piece that bursts has the sideways speed there instead of the wait, the speed down where the path is (the word after it the picture of the burst) and -1 for the end */
struct Boss4Piece {
    int16_t x;
    int16_t y;
    int16_t wait;
    uint16_t path_high;
    uint16_t path_low;
    uint16_t end_high;
    uint16_t end_low;
};

#define BOSS4_PIECES ((struct Boss4Piece *)guest.bytes_10b0)

/* the head's pictures: from the byte of the table below (the low three bits the way the head looks, the next bits for the mouth open) */
static const uint8_t boss4_head_table[256] = {
    0x06, 0x0E, 0x16, 0x0E, 0x06, 0x06, 0x0E, 0x16, 0x0E, 0x06, 0x06, 0x07, 0x07, 0x07, 0x00, 0x00,
    0x08, 0x10, 0x08, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02, 0x0A, 0x12, 0x0A, 0x02, 0x02, 0xA2, 0x12,
    0xA2, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x08, 0x10, 0x08, 0x00,
    0x00, 0x08, 0x10, 0x08, 0x00, 0x00, 0x07, 0x07, 0x07, 0x07, 0x06, 0x06, 0x0E, 0x16, 0x0E, 0x06,
    0x06, 0x0E, 0x16, 0x0E, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x04, 0x04, 0x0C, 0x14, 0x0C, 0x04, 0x0C, 0x14, 0x0C, 0x04, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02,
    0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x0A, 0x12, 0x0A, 0x02, 0x03, 0x03, 0x03, 0x04, 0x0C,
    0x14, 0x0C, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x0E, 0x16, 0x0E, 0x16, 0x0E, 0x06, 0x06,
    0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04, 0x04, 0x0C, 0x14, 0x0C, 0x04, 0x04,
    0x03, 0x03, 0x03, 0x02, 0x0A, 0x12, 0x0A, 0x02, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x06, 0x05, 0x05, 0x05, 0x04, 0x04, 0x03,
    0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* the places (x, y, and the other pair, which the original works out and does not draw) of the head's picture by the way it looks (orig $B4E8) */
static const int16_t boss4_head_offsets[32] = {
    11, -3, 32, 0, 3, 7, 19, 19, -13, 10, 0, 32, -26, 1, -20, 18,
    -27, -13, -32, 0, -17, -25, -18, -20, -4, -30, 0, -32, 9, -18, 20, -18,
};

/* orig $00B430: the piece goes one step along its path (the high nibble of the byte is the step in x, the low one the step in y, both signed); 0 when it has come to the end of the path */
static int boss4_piece_step(struct Boss4Piece *piece)
{
    uint32_t path = ((uint32_t)piece->path_high << 16) | piece->path_low;
    uint32_t end = ((uint32_t)piece->end_high << 16) | piece->end_low;

    if (path == end)
        return 0;

    uint8_t step = *(const uint8_t *)(uintptr_t)path;

    path++;
    piece->path_high = (uint16_t)(path >> 16);
    piece->path_low = (uint16_t)path;
    piece->y = (int16_t)(piece->y + (((int8_t)(uint8_t)(step << 4)) >> 4));
    piece->x = (int16_t)(piece->x + (((int8_t)(step & 0xF0)) >> 4));
    return 1;
}

/* orig $00B6CE and $00B6B4, $00B69A: the pieces are put at the start of the last entry of entry_list (its first two words after the marker are the place; the path begins 8 bytes on) and end where the entry ends;
 * the first waits `wait` frames and each next two more */
static void boss4_pieces_place(int16_t wait)
{
    int count = 0;

    while ((int32_t)guest.entry_list[count].address >= 0)
        count++;

    const struct Entry8 *entry = &guest.entry_list[count - 1];
    const uint16_t *place = (const uint16_t *)(uintptr_t)(entry->address + 2);
    uint32_t path = entry->address + 2 + 8;
    uint32_t end = entry->address + entry->size;
    struct Boss4Piece *piece = BOSS4_PIECES;

    for (int i = 0; i < 8; i++, piece++) {
        piece->x = (int16_t)place[0];
        piece->y = (int16_t)place[1];
        piece->wait = wait;
        piece->path_high = (uint16_t)(path >> 16);
        piece->path_low = (uint16_t)path;
        piece->end_high = (uint16_t)(end >> 16);
        piece->end_low = (uint16_t)end;
        wait = (int16_t)(wait + 2);
    }
}

/* orig $00B674: unless the guardian is dead the pieces start again (they wait 70 frames and two more each), and the head's table from its start */
static void boss4_pieces_restart(void)
{
    if ((int16_t)boss_state()->energy_high < 0)
        return;
    B4_WORD(0x660) = 6;
    B4_WORD(0x65E) = 6;
    BOSS4_HEAD_TABLE_HIGH = 0;
    BOSS4_HEAD_TABLE_LOW = (int16_t)BOSS4_HEAD_TABLE_BASE;
    boss4_pieces_place(0x46);
    B4_WORD(0x504) = 3;
}

/* orig $00B528: the next byte of the head's table: d6 (its low three bits) and d5 (all of it) of the original.  A negative word at $662 would make the original work the byte out and write it (not run) */
static uint8_t boss4_head_next_byte(void)
{
    if (B4_WORD(0x662) < 0)
        host_missing_routine(0xB540);

    uint32_t at = ((uint32_t)(uint16_t)BOSS4_HEAD_TABLE_HIGH << 16) | (uint16_t)BOSS4_HEAD_TABLE_LOW;
    uint8_t byte = boss4_head_table[at - BOSS4_HEAD_TABLE_BASE];

    at++;
    BOSS4_HEAD_TABLE_HIGH = (int16_t)(at >> 16);
    BOSS4_HEAD_TABLE_LOW = (int16_t)at;
    return byte;
}

/* orig $00B466: the head is drawn (the sprite 479 + the picture, from the place of the piece before it ($5AE) and the offsets of the way it looks).  The sprite's last row is 89 while it is left of 129 on the screen
 * (the original changes the constant in the sprite routine for the call) */
static void boss4_head_draw(void)
{
    uint8_t byte = boss4_head_next_byte();
    int16_t look = byte & 7;
    int16_t picture = look;

    if (byte & 0xF8) {
        picture += 0x11;
        if (byte & 0x10)
            picture++;
    }

    const int16_t *offset = &boss4_head_offsets[look * 4];
    int16_t x = (int16_t)(BOSS4_LAST_X + offset[0] - guest.scroll_x);
    int16_t y = (int16_t)(BOSS4_LAST_Y + offset[1] - guest.scroll_y);
    uint16_t saved = sprite_last_row;

    if (x < 0x81)
        sprite_last_row = 0x59;
    sprite_draw(x, y, (uint16_t)(picture + 0x1DF));
    sprite_last_row = saved;
}

/* orig $00B3A2, $00B370: a piece that bursts (it begins with a speed of -8 to 7 each way, by chance) goes by its speeds and is drawn as the sprites 91 to 107 in turn (centred on the piece); when it has been
 * through them it is counted as gone */
static void boss4_piece_burst_move(struct Boss4Piece *piece)
{
    int16_t *picture = (int16_t *)&piece->path_low;

    if (*picture < 0) {
        BOSS4_DONE--;
        return;
    }
    piece->x = (int16_t)(piece->x + piece->wait);
    piece->y = (int16_t)(piece->y + (int16_t)piece->path_high);
    sprite_draw((int16_t)(piece->x - 8 - guest.scroll_x), (int16_t)(piece->y - 8 - guest.scroll_y), (uint16_t)(*picture + 0x5B));
    (*picture)++;
    if (*picture >= 0x11)
        *picture = -1;
}

static void boss4_piece_burst_start(struct Boss4Piece *piece)
{
    piece->wait = (int16_t)((random_word() & 0xF) - 8);
    piece->path_high = (uint16_t)((random_word() & 0xF) - 8);
    piece->path_low = 0;
    piece->end_high = 0xFFFF;
    piece->end_low = 0xFFFF;
    boss4_piece_burst_move(piece);
}

/* orig $00B352 (all the pieces have burst): 1000 off the guardian's energy; below 0 it is dead (orig $B32E: it is at (2976, the throne's y), it dies like the others, the fight is over and the wait for the
 * sequence of its end begins: tick_count from 0), otherwise the head is whole again with 1000 and the pieces start again */
static void boss4_all_burst(void)
{
    struct BossState *boss = boss_state();
    int32_t energy = (int32_t)(((uint32_t)boss->energy_high << 16) | boss->energy_low) - 0x3E8;

    boss->energy_high = (uint16_t)((uint32_t)energy >> 16);
    boss->energy_low = (uint16_t)energy;
    if (energy < 0) {
        boss->x = 0xBA0;
        boss->y = (uint16_t)BOSS4_Y;
        boss_die();
        guest.flag_23a = 0;
        guest.code_ba34 = 0x4E71;
        guest.tick_count = 0;
        return;
    }
    guest.flags_3b0 &= (uint8_t)~0x10;
    BOSS4_HEAD_ENERGY = 0x3E8;
    boss4_pieces_restart();
}

/* orig $00B23A: the eight pieces: each waits, then moves two steps (the head is drawn as well), is hit (a box of 24 by 24: the head's energy goes down, below 0 all burst), is drawn as the glyph 495 (not when
 * it is left of 130 and below 89 on the screen), and bursts when the head has no energy; pieces that are at the end of their path are counted: with all eight the pieces start again.  The throne's two sprites last */
static void boss4_pieces_run(void)
{
    struct Boss4Piece *piece = BOSS4_PIECES;

    BOSS4_DONE = 0;
    for (int remaining = 7; remaining >= 0; remaining--, piece++) {
        if (piece->end_high == 0xFFFF && piece->end_low == 0xFFFF) {
            boss4_piece_burst_move(piece);
            continue;
        }
        if (piece->wait >= 0) {   /* $B404 */
            if (guest.flag_23a == 2)
                BOSS4_DONE--;
            else
                piece->wait--;
            continue;
        }
        if ((guest.flags_3b0 & 0x10) != 0 || BOSS4_HEAD_ENERGY == 0) {
            boss4_piece_burst_start(piece);
            continue;
        }
        if (!boss4_piece_step(piece) || !boss4_piece_step(piece)) {   /* $B41C */
            BOSS4_DONE++;
            continue;
        }
        if (remaining == 6)
            boss4_head_draw();
        guest.hit_box_extra_width = 0x18;
        guest.hit_box_extra_height = 0x18;

        int16_t energy = BOSS4_HEAD_ENERGY;

        if (energy < 0) {
            boss4_piece_burst_start(piece);
            continue;
        }
        energy = check_hit_and_walls(piece->x, piece->y, energy);
        BOSS4_HEAD_ENERGY = energy;
        if (energy < 0)
            guest.flags_3b0 |= 0x10;
        BOSS4_LAST_X = piece->x;
        BOSS4_LAST_Y = piece->y;

        int16_t x = (int16_t)(piece->x - guest.scroll_x);
        int16_t y = (int16_t)(piece->y - guest.scroll_y);

        if (x > 0x81 || y < 0x59)
            glyph_draw(x, y, 0x1EF);
    }
    if (BOSS4_DONE == -8) {
        boss4_all_burst();
        return;
    }
    if (BOSS4_DONE == 8) {
        boss4_pieces_restart();
        return;
    }
    if (guest.flag_23a == 2 || BOSS4_THRONE_Y > 0xB0)
        return;
    sprite_draw((int16_t)(0xBA0 - guest.scroll_x), (int16_t)(BOSS4_Y + 3 - guest.scroll_y), 0x1D1);
    sprite_draw((int16_t)(0xBC0 - guest.scroll_x), (int16_t)(BOSS4_Y + 8 - guest.scroll_y), 0x1D2);
}

/* orig $00B85A: two bolts at the player's place of the screen (the player's place is set to (320, 97) of it for them, the bolts begin at (69, 111) and (98, 110), speed 6) */
static void boss4_bolts_start(void)
{
    int16_t saved_x = guest.player_x;
    int16_t saved_y = guest.player_y;
    int16_t kind = 0;   /* (d6 of the original: only its bit 0 counts; what the register has) */

    guest.player_y = (int16_t)(0x61 + guest.scroll_y);
    guest.player_x = (int16_t)(0x140 + guest.scroll_x);
    aimed_shot((int16_t)(0x45 + guest.scroll_x), (int16_t)(0x6F + guest.scroll_y), 6, kind);
    aimed_shot((int16_t)(0x62 + guest.scroll_x), (int16_t)(0x6E + guest.scroll_y), 6, kind);
    guest.player_y = saved_y;
    guest.player_x = saved_x;
}

/* orig $00B75C: the ball of fire goes 6 to the right and 1 down (to 73 below the throne) a frame through the pictures 472 to 476 round and round; with the picture counted to 7 it is gone (5 and 6 are its end);
 * when it is left of 3004 a splash is drawn at the throne's end; it is hit like a shot (a box of 24 by 24, with the fight off for the call): when it is hit or reaches 272 on the screen it ends with
 * the two bolts */
static void boss4_fireball_move(void)
{
    int16_t picture = BOSS4_FIRE_PICTURE;

    if (picture < 0)
        return;

    int16_t x = BOSS4_FIRE_X;
    int16_t y = BOSS4_FIRE_Y;

    BOSS4_FIRE_X = (int16_t)(BOSS4_FIRE_X + 6);
    BOSS4_FIRE_PICTURE++;
    if (BOSS4_FIRE_PICTURE == 5)
        BOSS4_FIRE_PICTURE = 0;
    if (BOSS4_FIRE_PICTURE == 7) {
        BOSS4_FIRE_PICTURE = -1;
        return;
    }

    int16_t limit = (int16_t)(BOSS4_Y + 0x49);

    BOSS4_FIRE_Y++;
    if (limit < BOSS4_FIRE_Y)
        BOSS4_FIRE_Y = limit;
    sprite_draw((int16_t)(x - guest.scroll_x), (int16_t)(y - guest.scroll_y), (uint16_t)(picture + 0x1D8));
    if (x <= 0xBBC) {
        int16_t splash_x = (int16_t)(0xBB0 - guest.scroll_x);
        int16_t splash_y = (int16_t)(BOSS4_Y + 0x38 - guest.scroll_y);

        sprite_draw(splash_x, splash_y, 0x1CE);
        glyph_draw((int16_t)(splash_x + 0x20), splash_y, 0x1CF);
        sprite_draw((int16_t)(0xBB0 - guest.scroll_x), (int16_t)(BOSS4_Y + 0x4B - guest.scroll_y), 0x1D0);
    }

    int16_t life = BOSS4_FIRE_LIFE;

    if (life < 0)
        return;
    guest.hit_box_extra_width = 0x18;
    guest.hit_box_extra_height = 0x18;

    uint16_t saved_flag = guest.flag_23a;

    guest.flag_23a = 0;
    life = check_hit_and_walls(x, y, life);
    guest.flag_23a = saved_flag;
    if ((int16_t)(x - guest.scroll_x) < 0x110) {
        BOSS4_FIRE_LIFE = life;
        if (life >= 0)
            return;
    }
    BOSS4_FIRE_PICTURE = 5;
    BOSS4_FIRE_LIFE = -1;
    boss4_bolts_start();
}

/* orig $00B8F8: the mouth of the wall at the left (x 64 and 96, 58 below the throne) open (the sprites 457 and 458) or half (459 and 460) by `open` (0 to 1: 457, 2: 459) */
static void boss4_mouth_draw(int16_t open)
{
    int16_t picture = (int16_t)((open & 2) + 0x1C9);
    int16_t y = (int16_t)(BOSS4_Y + 0x3A - guest.scroll_y);

    sprite_draw(0x40, y, (uint16_t)picture);
    sprite_draw(0x60, y, (uint16_t)(picture + 1));
}

/* orig $00B8B8: while the fire timer is negative the mouth opens and closes (from the frames it has been: 1: the sound 64, 1 and 2 half open, 3 open and the ball of fire starts when none flies, 4 half, 5 the
 * timer is 40 again) */
static void boss4_mouth_fire(void)
{
    int16_t phase = (int16_t)((4 - BOSS4_FIRE_TIMER) >> 2);

    if (phase == 1)
        play_sound(0x40, 0x14);
    if (phase == 3) {
        boss4_mouth_draw(2);
        if (BOSS4_FIRE_PICTURE >= 0)
            return;
        BOSS4_FIRE_X = 0xBAA;
        BOSS4_FIRE_Y = (int16_t)(BOSS4_Y + 0x3A);
        BOSS4_FIRE_PICTURE = 0;
        BOSS4_FIRE_LIFE = 0x28;
        return;
    }
    if (phase < 3) {
        boss4_mouth_draw(phase);
        return;
    }
    if (phase == 4) {
        boss4_mouth_draw(1);
        return;
    }
    BOSS4_FIRE_TIMER = 0x28;
}

/* orig $00B6EA (the level 4 guardian, per frame: boss_frame): the sound 52 once, then (while it is alive) the fire timer, the fire, the pieces and the hit on the guardian's body (a box of 64 by 96 at (2960, the throne)) */
static void boss4_frame(void)
{
    if (!(guest.flags_3b1 & 2)) {
        guest.flags_3b1 |= 2;
        play_sound(0x34, 5);
    }
    struct BossState *boss = boss_state();

    if ((int16_t)boss->energy_high < 0) {
        boss4_pieces_run();
        return;
    }
    BOSS4_FIRE_TIMER--;
    if (BOSS4_FIRE_TIMER < 0)
        boss4_mouth_fire();
    boss4_fireball_move();
    boss4_pieces_run();
    guest.hit_box_extra_width = 0x40;
    guest.hit_box_extra_height = 0x60;
    check_hit_and_walls(0xB90, BOSS4_Y, (int16_t)boss->energy_low);
}

/* orig $00B1EC (callback_514): the fire is none, the timer 120, the head's energy 750, the head's table from its start and the pieces wait 170 frames (and two more each) */
static void boss4_514(void)
{
    BOSS4_FIRE_PICTURE = -1;
    BOSS4_FIRE_TIMER = 0x78;
    BOSS4_HEAD_ENERGY = 0x2EE;
    BOSS4_HEAD_TABLE_HIGH = 0;
    BOSS4_HEAD_TABLE_LOW = (int16_t)BOSS4_HEAD_TABLE_BASE;
    boss4_pieces_place(0xAA);
}

/* orig $00B9B8 and $00BAAE: the 96 by 112 pixels of the screen at row 80, byte 4 (four planes) are copied to the buffer behind the sprites (asset_mem_end, planes $FC0 apart), or back (`restore`) */
static void boss4_screen_copy(int restore)
{
    uint8_t *screen = guest.screen_buffer + 0xDC4;
    uint8_t *saved = guest.asset_mem_end;

    for (int plane = 0; plane < 4; plane++)
        for (int row = 0; row < 112; row++) {
            uint8_t *on_screen = screen + plane * 0x2100 + row * 44;
            uint8_t *kept = saved + plane * 0xFC0 + row * 12;

            for (int i = 0; i < 12; i++) {
                if (restore)
                    on_screen[i] = kept[i];
                else
                    kept[i] = on_screen[i];
            }
        }
}

/* orig $00BB68: the throne's pieces (nine, the sprites 448 to 456: a glyph of one word, a sprite of two words or the clipped draw of the image by the list at $BBC2: the place from the throne (2960, its y) and the way it is drawn) */
static void boss4_throne_pieces(void)
{
    static const int16_t list[9][3] = {
        {0, 0, 0}, {16, 0, 1}, {48, 0, 1}, {16, 10, 2}, {64, 10, 0}, {64, 39, 1}, {64, 59, 0}, {16, 78, 0}, {32, 78, 1},
    };
    int16_t x = (int16_t)(0xB90 - guest.scroll_x);
    int16_t y = (int16_t)(BOSS4_Y - guest.scroll_y);

    guest.flags_0ddd = 8;
    guest.draw_routine_index = 0;
    for (int i = 0; i < 9; i++) {
        int16_t piece_x = (int16_t)(x + list[i][0]);
        int16_t piece_y = (int16_t)(y + list[i][1]);
        uint16_t picture = (uint16_t)(0x1C0 + i);

        if (list[i][2] == 0)
            glyph_draw(piece_x, piece_y, picture);
        else if (list[i][2] == 1)
            sprite_draw(piece_x, piece_y, picture);
        else
            image_draw_clipped(piece_x, piece_y, picture);
    }
}

/* orig $00BB2A: the two sprites (461) at the throne's side */
static void boss4_throne_sprites(void)
{
    sprite_draw((int16_t)(0xB86 - guest.scroll_x), (int16_t)(BOSS4_Y + 0x67 - guest.scroll_y), 0x1CD);
    sprite_draw((int16_t)(0xB8D - guest.scroll_x), (int16_t)(BOSS4_Y + 0x4B - guest.scroll_y), 0x1CD);
}

/* orig $00B958 (callback_4e4 while the throne rises): the screen is eased to (2912, 96); the screen behind the throne is kept, the throne drawn and it rises 2 a frame; at 176 the sound 52 stops, the screen is
 * kept as the background ($1785C) and the callback is over */
static void boss4_throne_rise(void)
{
    BOSS4_THRONE_Y = BOSS4_Y;
    guest.scroll_ease_x = 0xB60;
    guest.word_380 = 0x60;
    if (guest.word_037c == 0)
        guest.word_037c = 1;
    boss4_screen_copy(0);
    boss4_throne_sprites();
    boss4_throne_pieces();
    BOSS4_Y = (int16_t)(BOSS4_Y - 2);
    if (BOSS4_Y >= 0xB0)
        return;
    stop_sound(0x34);
    BOSS4_Y = 0xB0;
    guest.word_037c = 3;
    guest.background = guest.memory_base + 256;   /* $1785C: the screen is kept to be copied the next times */
    copy_screen(guest.screen_buffer, guest.background);
    guest.word_037c = 4;
    guest.code_b958 = 0x4E75;
    guest.callback_4e4 = boss_rts;
}

/* orig $00B222: the message of the end (priority 30000) */
static void boss4_end_message(void)
{
    static uint8_t text[] = "                                         you have destroyed the last great guardian.  reluctantly, the gods bestow upon you the greatest gift of all - immortality.......    collect your gift";

    guest.message_next = text;
    guest.message_arg = 0;
    message_request(0x7530);
}

/* orig $00BA36 (called each frame, when the guardian is dead; $BA34 is a bare return before): from 9 seconds on (tick_count 450) the message of the end is asked for once, and when it has gone ten gifts (the
 * thing 53) are dropped in a row at (2954, 112) and flags_3b0 bit 5 is set; when the player has them all (item_count is 10 more) bit 6 is set.  From 8 seconds (400) the level is over (level_ended).  The screen behind
 * the throne is put back each frame */
void boss4_death_wait(void)
{
    if (guest.code_ba34 != 0x4E71)
        return;
    if ((int32_t)guest.tick_count < 0x1C2) {
        if ((int32_t)guest.tick_count >= 0x190)
            guest.level_ended = 1;
        boss4_screen_copy(1);
        return;
    }
    if (!(guest.flags_3b1 & 1)) {
        boss4_end_message();
        guest.flags_3b1 |= 1;
    }
    if (!(guest.flags_3b0 & 0x20)) {
        if (guest.message_shown != 0) {
            boss4_screen_copy(1);
            return;
        }
        guest.flags_3b0 |= 0x20;
        BOSS4_Y = (int16_t)(guest.item_count + 10);
        for (int i = 0, x = 0xB8A; i < 10; i++, x += 0x17)
            drop_record((int16_t)x, 0x70, 0x35);
        boss4_screen_copy(1);
        return;
    }
    if (guest.item_count == BOSS4_Y)
        guest.flags_3b0 |= 0x40;
    boss4_screen_copy(1);
}

/* orig $00B198 (the start): the throne is at 384, the routines are the guardian's, its energy 6000 (the long at $50A) and the head's 750; the throne rises (callback_4e4) and the flags of its end are off */
static void boss4_start(void)
{
    struct BossState *boss = boss_state();

    BOSS4_Y = 0x180;
    guest.boss_frame = boss4_frame;
    guest.callback_514 = boss4_514;
    guest.flag_258 = 1;
    boss->energy_high = 0;
    boss->energy_low = 0x1770;
    BOSS4_HEAD_ENERGY = 0x2EE;
    guest.callback_4e4 = boss4_throne_rise;
    guest.code_b958 = 0x4E71;
    guest.code_ba34 = 0x4E75;
    guest.flags_3b0 &= (uint8_t)~0x20;
    guest.flags_3b1 &= (uint8_t)~1;
    guest.flags_3b1 &= (uint8_t)~2;
    boss4_514();
}

/* the quickload of the fight (or of the end after it): the words of the guardian's state are the file's (big endian), the routines are the port's */
static void boss4_quickload(void)
{
    file_words(boss_at(0x4E8), 3);
    file_words(boss_at(0x4FA), 1);
    file_words(boss_at(0x504), 1);
    file_words(boss_at(0x50A), 3);
    file_words(boss_at(0x518), 4);
    file_words(boss_at(0x5AE), 2);
    file_words(boss_at(0x65A), 8);
    guest.boss_frame = boss4_frame;
    guest.callback_4e4 = guest.code_b958 == 0x4E71 ? boss4_throne_rise : boss_rts;
    guest.callback_514 = boss4_514;
}

/* orig $002F12 (the player has all ten gifts of the guardian of the level 4: bit 6 of flags_3b0): the picture "payoff.pi1" (the palette 32 bytes after a word, then the 192 rows of the picture) fades in, two messages are
 * shown (the second until a button is pressed), and the game begins again at the level 1 with more armour (armour_level from 2 to 4 more) and the items and score made again; the original goes on at $2452 */
static void game_won_screen(void)
{
    static uint8_t text_one[] = " The way to mount olympus lies open...  You may now claim your place among the gods...  Enjoy your life, it will last forever. ";
    static uint8_t text_two[] = " So what will you do with this gift?     journey again to the city if you dare... this time your task will be harder. ";

    guest.flags_3b0 &= (uint8_t)~0x40;
    fade_out(1, 0xFFFF);
    load_unpacked("payoff.pi1", guest.picture_buffer);

    const uint16_t *file = (const uint16_t *)(guest.picture_buffer + 2);
    uint16_t *palette = (uint16_t *)guest.ptr_0cee;

    for (int i = 0; i < 16; i++)   /* (the unpacked file's words are the port's own) */
        palette[i] = file[i];

    const uint16_t *picture = (const uint16_t *)(guest.picture_buffer + 0x22);
    uint8_t *to = guest.screen_buffer;

    for (int row = 0; row < 192; row++) {
        for (int word = 0; word < 20; word++) {
            *(uint16_t *)to = *picture++;
            to += 2;
            *(uint16_t *)(to + 0x20FE) = *picture++;
            *(uint16_t *)(to + 0x41FE) = *picture++;
            *(uint16_t *)(to + 0x62FE) = *picture++;
        }
        for (int plane = 0; plane < 4; plane++) {
            uint16_t *tail = (uint16_t *)(to + (plane == 0 ? 0 : 4 + plane * 0x2100 - 2));

            tail[0] = 0;
            tail[1] = 0;
        }
        to += 4;
    }
    display_reset();

    uint8_t *shown = guest.screen_buffer;
    uint8_t *other = guest.screen_other;

    guest.screen_buffer = other;   /* (the sound file is read with the screens as they are swapped) */
    guest.screen_other = shown;
    file_read((const char *)sound1_name, guest.picture_buffer);
    host_disk_wait();
    guest.screen_buffer = shown;
    guest.screen_other = other;
    *(uint16_t *)(guest.screen_buffer + 0x4200) = 0xFFFF;
    *(uint16_t *)(guest.screen_buffer + 0x6300) = 0xFFFF;
    screens_flip();
    guest.copper_setup_done = 1;
    fade_in(palette, 1);

    guest.message_next = text_one;
    guest.message_arg = 0;
    message_request(0x7D00);
    do {
        wait_vblank();
        wait_vblank();
        wait_vblank();
        message_tick();
    } while (guest.message_shown != 0);
    guest.message_next = text_two;
    message_request(0x7CFF);
    do {
        wait_vblank();
        message_tick();
        wait_vblank();
        wait_vblank();
    } while (!(guest.button_bits[0] & 1));
    fade_out(0, 0xFFFF);

    guest.armour_level++;
    if (guest.armour_level < 2)
        guest.armour_level = 2;
    if (guest.armour_level >= 4)
        guest.armour_level = 4;
    guest.param_index = 1;
    guest.word_0266 = 0;
    guest.score_redraw = 0xFFFF;
    clear_shot_lists();
    copy_block_16bb4();
    guest.word_0248++;
    falcon_end();
    guest.word_0248 = 0;
    guest.score_total_1fc = 0;
    guest.item_shown = 3;
    guest.item_chosen = 3;
    for (int i = 0; i < 4; i++)
        guest.slots_30c[i] = -1;
    item_panels_make();
    guest.code_ba34 = 0x4E75;
    level_next_wait = 1;   /* (the lockstep: the level's loading waits for the original's reads, as after the end of a level) */
    level_begin();
}

/* orig $00334E (the level of a password is begun: menu_active is 1 and the level is above 1): the score is 80000 (the table at $33C8), the world's sprites and the door are set (as the loading of a world does) and the shop
 * opens first, to spend it; when it is left the play begins ($33B6: menu_active is cleared and the game goes on at $2BC8) */
static void password_start_level(void)
{
    guest.shop_door_index = 0;
    guest.score_total_1fc = 80000;
    world_set_up();
    guest.shop_door_index = 0;
    guest.shop_score_shown = guest.score_total_1fc;
    guest.cursor_pos = 0;
    guest.word_0384 = 0;
    guest.shop_repeat = 0;
    guest.shop_held[0] = 0;
    guest.shop_selected = -1;
    guest.message_shown = 0;
    guest.draw_routine_index = 0;
    guest.flags_0ddd = 0;
    level_next_wait = 0;
    shop_from_password = 1;
    shop_run();
    guest.menu_active = 0;
    game_loop();
}

/* orig $0064C2 (what follows the password screen): the line that was typed (the numbers of the letters, A 1) is compared with the saved codes (the three bytes at the top of each long: the first two entries of the table at
 * $66BC name the same code, the level 2's); a code that is not there (its first byte 0) is passed; with a match the level is the entry's (the first is 2) and it begins as a password's level (menu_active 1, word_0248 1: the
 * score and the weapons are made); without one the screen fades out and the menu starts again.  The level the game has now is $6536: not written (the files are there already) */
void password_entered(void)
{
    static const int slot_of_entry[4] = { 0, 0, 1, 2 };
    const uint8_t *typed = guest.line_text;
    int16_t level = 0;

    for (int i = 0; i < 4; i++) {
        uint32_t code = guest.password_slots[slot_of_entry[i]];
        uint8_t bytes[4] = { (uint8_t)(code >> 24), (uint8_t)(code >> 16), (uint8_t)(code >> 8), (uint8_t)code };

        if (bytes[0] == 0)
            continue;

        int k = 0;

        while (bytes[k] != 0 && typed[k] == bytes[k])
            k++;
        if (bytes[k] != 0 || typed[k] != 0)
            continue;
        level = (int16_t)(i + 1);
        break;
    }
    if (level == 0) {
        fade_out(1, 0xFFFF);
        menu_main();
        return;
    }
    if (level == 1)
        level = 2;
    if (level == (int16_t)guest.param_index)
        host_missing_routine(0x6536);   /* (the level that is already there: the files are not read again) */
    guest.param_index = (uint16_t)level;
    guest.menu_active = 1;
    guest.word_0248 = 1;
    guest.word_0266 = 0;
    level_next_wait = 1;   /* (the lockstep: the level's loading waits for the original's reads, as after the end of a level) */
    level_begin();
}

/* orig $00B17E (callback_514): the place of the level 3 guardian and its path */
static void boss3_514(void)
{
    struct BossState *boss = boss_state();

    boss->x = 0x2C0;
    boss->y = 0x70;
    boss->picture = 0;
    boss->path_high = 0;
    boss->path_low = BOSS3_PATH;
}

/* orig $00AFD6: the entry of the map (state 11 of table_18714) that begins the fight with the level's guardian: the entry is taken, the flag for the fight is on, the routines of the walkers and of the
 * things of the level are switched off, the records of the boss are free and the level's own start is made (the table at $B014: $B024 for the level 1) */
static void boss_start(struct EntryRef *entry)
{
    entry->state = 0xFFFF;
    guest.flags_3b1 |= 4;   /* bset.b #$a, $3b1 */
    guest.flag_23a = 1;
    guest.code_8a54 = 0x4E75;
    guest.code_9ae6 = 0x4E75;
    guest.code_6ed0 = 0x4E75;
    boss_free_records();
    if (guest.param_index == 2) {   /* $B0B4 */
        struct Boss2 *boss = boss2_state();

        guest.boss_frame = boss2_frame;
        guest.callback_514 = boss2_514;
        guest.flag_258 = 1;
        boss->neck = 0x7D0;
        boss->body = 0x960;
        boss->energy_high = 0;
        boss->energy_low = 0x1130;
        guest.code_cb0e = 0x4E71;
        guest.callback_4e4 = boss_rts;
        boss2_514();
        return;
    }
    if (guest.param_index == 3) {   /* $B158 */
        struct BossState *boss = boss_state();

        guest.boss_frame = boss3_frame;
        guest.flag_258 = 1;
        guest.callback_514 = boss3_514;
        boss->energy_high = 0;
        boss->energy_low = 0x8CA;
        guest.callback_4e4 = boss_rts;
        boss3_514();
        return;
    }
    if (guest.param_index == 4) {   /* $B198 */
        boss4_start();
        return;
    }
    if (guest.param_index != 1) {
        host_missing_routine(0xB014 + 4 * (uint32_t)(guest.param_index - 1));   /* (no other level has a guardian) */
        guest.boss_frame = boss_rts;
        guest.callback_4e4 = boss_rts;
        guest.callback_514 = boss_rts;
        guest.flag_258 = 1;
        return;
    }
    /* $B024 */
    guest.boss_frame = boss1_frame;
    guest.flag_258 = 1;
    guest.callback_514 = boss1_514;
    guest.code_c58e = 0x4E71;
    guest.code_c406 = 0x4E71;
    boss_state()->energy_high = 0;   /* its energy: the original has the copylock's value here when the lock passes (1500); the bclr.b #7, $50A takes the top byte's bit 7 off */
    boss_state()->energy_low = 0x5DC;
    guest.callback_4e4 = boss_rts;
    boss1_514();
}

/* orig $00AFD4: a bare return */
static void boss_rts(void)
{
}

/* the quickload of a fight with the level 1 guardian (flag_23a on, its sprites there): the words of its state, its shots, its sparks (the 500 bytes of ptr_0e44) and the count of its death's score are the file's
 * (big endian); its routines are the port's (the original's callbacks are addresses of its own code) */
static void boss_quickload(void)
{
    if (guest.flag_23a != 0 && guest.menu_state_25a == 2 && guest.param_index == 2) {
        struct Boss2 *boss = boss2_state();

        file_words(boss, (0x632 - 0x4E8) / 2);
        file_words(boss_at(0x668), 1);
        guest.boss_frame = boss2_frame;
        guest.callback_4e4 = boss_rts;
        guest.callback_514 = boss2_514;
        boss2_bezier_load();
        return;
    }
    if (guest.flag_23a != 0 && guest.menu_state_25a == 2 && guest.param_index == 3) {
        struct BossState *boss = boss_state();

        file_words(boss, 2);
        file_words(&boss->picture, 6);
        file_words(boss->shots, 75);
        file_words(boss_at(0x5B2), 1);
        file_words(boss_at(0x5B8), 1);
        guest.boss_frame = boss3_frame;
        guest.callback_4e4 = boss_rts;
        guest.callback_514 = boss3_514;
        return;
    }
    if (guest.param_index == 4 && (guest.flag_23a != 0 || guest.code_ba34 == 0x4E71)) {
        boss4_quickload();
        return;
    }
    if (guest.flag_23a != 0 && guest.param_index != 1) {   /* the other levels' guardians are not ported: their callbacks are the original's addresses */
        guest.boss_frame = boss_rts;
        guest.callback_4e4 = boss_rts;
        guest.callback_514 = boss_rts;
        return;
    }
    if (guest.flag_23a == 0 || guest.menu_state_25a != 2 || guest.param_index != 1)
        return;

    struct BossState *boss = boss_state();

    file_words(boss, 2);
    file_words(&boss->picture, 6);
    file_words(boss->shots, 75);
    file_words(guest.ptr_0e44, 250);
    file_words(boss_at(0x668), 1);
    guest.boss_frame = boss1_frame;
    guest.callback_4e4 = boss_rts;
    guest.callback_514 = boss1_514;
}

/* orig $0030DA (the frame ends there when flag_258 is on, which the guardian's start sets): the guardian's sprites (the set `guard1` of the level's names) and the sound files are read behind the
 * loading screen, and the game loop begins again ($2BD6) */
static void boss_load_30da(void)
{
    guest.flag_258 = 0;
    if (guest.menu_state_25a == 2)
        return;   /* goes on at $2C20: they are there */
    draw_loading_screen();
    if (guest.param_index == 3) {   /* $30EC: the level 3 changes two pointers of the text */
        guest.line_text = guest.picture_buffer + (0x373BC - 0x31FDE);
        guest.ptr_03ac = guest.picture_buffer + (0x379FC - 0x31FDE);
    }
    play_sound(0, 0);
    uint8_t *saved_directory = guest.asset_directory;

    if (guest.param_index == 3)   /* $311A: the table of sprite records goes where the list of entries is while the guardian's sprites are read ($315E puts it back) */
        guest.asset_directory = (uint8_t *)guest.entry_list;
    guest.asset_flag = -1;
    guest.flags_0ddd |= 0x10;
    assets_load((const char *)guest.level_names + 0xA2, guest.palette_words, guest.asset_end_0ca2, (struct AssetEntry *)guest.asset_table_0ca6);
    guest.palette_words[0] = 0;
    if (guest.param_index == 3)
        guest.asset_directory = saved_directory;
    if (guest.param_index == 2)
        boss2_bezier_load();   /* $316A: the level 2 reads bezier.dat */
    drives_idle();   /* $16E3E */
    guest.menu_state_25a = 2;
    file_read((const char *)sound1_name, guest.picture_buffer);   /* (the original reads sound1.bin, sound2.bin and, in the odd levels, sound3.bin) */
    host_disk_wait();   /* (the original's reads take what its drive takes, the loading screen on show) */
    drives_idle();
    if (guest.flag_aa8 != 0)
        play_sound(60, 2);
    else
        stop_sound(60);
    if (guest.param_index == 3)
        play_sound(66, 1);
}

/* the first byte of what shop.pi1 and the rest of the shop's screen are drawn on is the screen buffer: the rows are 44 bytes, the planes 8448 bytes apart */
#define SHOP_ROW 44

/* the strip of text: where shop_text points; before the shop has made it (a message while a world is loaded) the original's pointer is 0 and it works on the memory from address 0 */
static uint8_t *shop_strip(void)
{
    return guest.shop_text != 0 ? guest.shop_text : (uint8_t *)&guest;
}

/* orig $013D3C: the strip of text (9 rows of 64 bytes) is cleared */
static void shop_clear_text(void)
{
    uint32_t *p = (uint32_t *)shop_strip();

    for (int i = 0; i < 9 * 16; i++)
        *p++ = 0;
}

/* orig $013D50: the strip is copied to the screen that is not shown (from row 175, byte 14, the planes 8448 bytes apart) and the two end pieces (images 41 and 42 of the shop's set) are drawn at its ends */
static uint16_t strip_shown[9 * 32];   /* the strip as it was when it was last shown (the original's tick copies it to the screen that is not shown: a clear of the text, when a message is asked for, does not reach the screen until the next tick) */
static int strip_shown_valid;

static void shop_strip_draw(const uint16_t *from)
{
    uint8_t *to = guest.screen_other + 0x1E22;

    for (int row = 0; row < 9; row++) {
        for (int column = 0; column < 8; column++) {
            *(uint16_t *)to = *from++;
            *(uint16_t *)(to + 0x2100) = *from++;
            *(uint16_t *)(to + 0x4200) = *from++;
            *(uint16_t *)(to + 0x6300) = *from++;
            to += 2;
        }
        to += 0x1C;
    }

    uint8_t *saved = guest.screen_buffer;

    guest.screen_buffer = guest.screen_other;
    glyph_draw(0x70, 0xAE, (uint16_t)(guest.word_292 + 0x29));
    glyph_draw(0xE0, 0xAE, (uint16_t)(guest.word_292 + 0x2A));
    guest.screen_buffer = saved;
}

static void shop_text_show(void)
{
    const uint16_t *from = (const uint16_t *)shop_strip();

    for (int i = 0; i < 9 * 32; i++)
        strip_shown[i] = from[i];
    strip_shown_valid = 1;
    shop_strip_draw(strip_shown);
}

/* orig $013DB4: the strip is moved one pixel to the left (each plane of each row is a chain of 8 words) */
static void shop_text_shift(void)
{
    uint16_t *row = (uint16_t *)shop_strip();

    for (int r = 0; r < 9; r++) {
        for (int plane = 0; plane < 4; plane++) {
            uint16_t carry = 0;

            for (int w = 7; w >= 0; w--) {
                uint16_t *word = &row[w * 4 + plane];
                uint16_t out = (uint16_t)(*word >> 15);

                *word = (uint16_t)((*word << 1) | carry);
                carry = out;
            }
        }
        row += 32;
    }
}

/* orig $013E50: one pixel column of the message's text is put at the right end of the strip (the characters are 10 pixels wide: the column of the character that message_wide counts to; the end of the text
 * sets message_wide to -1, and the strip then goes on running by message_x until 140 pixels have gone: the message is over) */
static void shop_text_column(void)
{
    int16_t column = (int16_t)guest.message_wide;

    if (column < 0) {
        guest.message_x++;
        if ((int16_t)guest.message_x >= 0x8C) {
            guest.message_x = 0;
            guest.message_wide = 0;
            guest.message_arg = 0;
            guest.message_shown = 0;
            shop_clear_text();
        }
        return;
    }

    const uint8_t *character = (guest.message_text != 0 ? guest.message_text : (const uint8_t *)&guest) + column / 10;   /* (no text: the original's pointer is 0 and it reads the memory from address 0) */
    int16_t rest = (int16_t)(column % 10);

    if (*character == 0) {
        guest.message_wide = 0xFFFF;
        guest.message_arg = 0;
        guest.message_x = 0;
        return;
    }

    uint16_t mask = (uint16_t)(0x8000 >> rest);
    int16_t shift = (int16_t)(15 - rest);
    const struct Image *image = &guest.image_table[(int8_t)*character + (int16_t)guest.word_292 + 0x2B];
    int16_t rows = (int16_t)image->height;
    const uint16_t *planes = image->data;
    int16_t stride = (int16_t)((rows + 1));   /* the words of one plane */
    uint16_t *to = (uint16_t *)shop_strip() + 0x1C;

    for (int row = 0; row <= rows; row++) {
        to[0] |= (uint16_t)((planes[row] & mask) >> shift);
        to[1] |= (uint16_t)((planes[row + stride] & mask) >> shift);
        to[2] |= (uint16_t)((planes[row + 2 * stride] & mask) >> shift);
        to[3] |= (uint16_t)((planes[row + 3 * stride] & mask) >> shift);
        to += 32;
    }
    guest.message_wide++;
}

/* orig $013E3A (the frame tick calls it for a message while the shop is on): two columns of text go in, the strip is shown */
void shop_message_frame(void)
{
    if (guest.shop_text == 0)
        return;   /* (the strip is not made yet: the vertical blank came while a world's loading began, before its first file read cleared tick_hold_38a; the original's did not come then) */
    shop_text_shift();
    shop_text_column();
    shop_text_shift();
    shop_text_column();
    shop_text_show();
}

/* orig $013DF2 (message_request while the shop is on): a message of a lower priority than the one on show is not taken; the same text again is not either; any other starts over in the strip.
 * Returns 0 when nothing was done */
uint16_t shop_message_request(int16_t priority)
{
    if (priority < (int16_t)guest.message_arg)
        return 0;
    if (guest.message_shown) {
        const uint8_t *next = guest.message_next;
        const uint8_t *shown = guest.message_text;

        for (;;) {
            uint8_t ch = *next++;

            if (ch == 0)
                return 0;
            if (ch != *shown++)
                break;
        }
        shop_clear_text();
    }
    guest.message_arg = (uint16_t)priority;
    guest.message_text = guest.message_next;
    guest.message_wide = 0;
    guest.message_shown = 1;
    shop_clear_text();
    return 1;
}

/* orig $013C60: the grid of cells (6 by 3, from (96, 53), 32 pixels apart; image word_292 + 16, the cursor's cell +1) with the items (glyph 170 + item, or 302 + item - 192 for an object) in them; the
 * last cell is the way out (image 18 and 19, drawn at (256, 117)); when there are more than 17 items the grid shows the last 17 */
static void shop_draw_grid(void)
{
    const int16_t *list = (const int16_t *)guest.shop_list;
    int16_t index = 0;
    int16_t y = 0x35;

    guest.shop_scroll = 0;
    if ((int16_t)guest.shop_count > 17) {
        guest.shop_scroll = (uint16_t)(guest.shop_count - 17);
        list += guest.shop_scroll;
    }

    int row = 0;
    int ended = 0;

    for (; row < 3 && !ended; row++, y += 0x20) {
        int16_t x = 0x60;

        for (int column = 0; column < 6; column++, x += 0x20, index++) {
            uint16_t image = (uint16_t)(0x10 + guest.word_292);

            if (index == (int16_t)guest.cursor_pos)
                image++;
            sprite_draw(x, y, image);
            if (*list < 0) {   /* the list ends: the rest of the cells are empty ones */
                column++;
                x += 0x20;
                index++;
                for (; column < 6; column++, x += 0x20, index++) {
                    image = (uint16_t)(0x10 + guest.word_292);
                    if (index == (int16_t)guest.cursor_pos)
                        image++;
                    sprite_draw(x, y, image);
                }
                row++;
                y += 0x20;
                for (; row < 3; row++, y += 0x20) {
                    x = 0x60;
                    for (column = 0; column < 6; column++, x += 0x20, index++) {
                        image = (uint16_t)(0x10 + guest.word_292);
                        if (index == (int16_t)guest.cursor_pos)
                            image++;
                        sprite_draw(x, y, image);
                    }
                }
                ended = 1;
                break;
            }
            int16_t item = *list++;

            glyph_draw((int16_t)(x + 8), (int16_t)(y + 7), (uint16_t)(item >= 0xC0 ? item - 0xC0 + 0x12E : item + 0xAA));
        }
        if (ended)
            break;
    }
    if (!ended)
        return;
    {
        uint16_t image = (uint16_t)(0x12 + guest.word_292);

        if ((int16_t)guest.cursor_pos == 0x11)
            image++;
        sprite_draw(0x100, (int16_t)(y - 0x20), image);
    }
}

/* orig $013FD6: the digit (8 rows of the four planes' bytes of digit_font) is put in the screen at `to`, the rows 44 bytes and the planes 8448 bytes apart */
static void shop_draw_digit(uint8_t *to, int16_t digit)
{
    const uint8_t *from = (const uint8_t *)guest.digit_font + (int16_t)(digit << 5);   /* (adda.w: the offset is a signed word) */

    for (int row = 0; row < 8; row++) {
        set_plane_byte(to, *from++);
        set_plane_byte(to + 0x2100, *from++);
        set_plane_byte(to + 0x4200, *from++);
        set_plane_byte(to + 0x6300, *from++);
        to += SHOP_ROW;
    }
}

/* orig $013F18: the score as shown goes to the score by 750 a frame (the sound 33 plays while it does and no sound is busy); its seven digits are drawn at the left of the foot of the screen (as the divisions
 * of the 68000 do them: a quotient that does not fit a word is not made, and the digit is then a part of the number), and the energy bar is run and drawn 16 rows higher than in the game */
static void shop_score(void)
{
    int32_t shown = (int32_t)guest.shop_score_shown;
    int32_t score = (int32_t)guest.score_total_1fc;

    if (shown != score) {
        if (guest.sound_busy_207a0 == 0)
            play_sound(0x21, 1);
        if (shown > score) {
            shown -= 750;
            if (shown < score)
                shown = score;
        } else {
            shown += 750;
            if (shown > score)
                shown = score;
        }
        guest.shop_score_shown = (uint32_t)shown;
    }

    uint32_t number = guest.shop_score_shown;
    uint8_t *to = guest.screen_buffer + 0x1E48;

    for (int i = 0; i < 7; i++) {
        uint32_t digit_word = number;   /* d2 after the division */
        int16_t digit;

        if ((number >> 16) < 10) {   /* divu.w #10 */
            uint32_t quotient = 0;
            uint32_t remainder = 0;

            for (int bit = 31; bit >= 0; bit--) {   /* (shifts and subtractions: the 68000 has no 32-bit division in the library) */
                remainder = (remainder << 1) | ((number >> bit) & 1);
                if (remainder >= 10) {
                    remainder -= 10;
                    quotient |= 1u << bit;
                }
            }

            number = (uint32_t)(int32_t)(int16_t)quotient;   /* ext.l */
            digit = (int16_t)remainder;
        } else {
            number = (uint32_t)(int32_t)(int16_t)number;
            digit = (int16_t)(digit_word >> 16);
        }
        shop_draw_digit(to, digit);
        to -= 1;
    }
    guest.frame_phase = 0;
    guest.screen_buffer -= 0x2C0;
    update_energy_display();
    draw_energy_bar();
    guest.screen_buffer += 0x2C0;
}

/* orig $0141C2: what an item costs (the value of the thing in table_14cf8, or of the object in its record) */
static uint32_t shop_item_value(uint16_t id)
{
    if ((int16_t)id < 0xC0)
        return guest.table_14cf8[id].w[3];
    return (uint16_t)((struct Object *)guest.object_slots)[id - 0xC0].w74;
}

/* orig $0140A4 and what it calls ($01413C, $01415C, $0141F8): the list is filtered: an item is left out when its kind of pick-up has been bought, when (no weapon in any slot) it is item 2 or 3, when
 * (falcon_energy is -1) it is item 40 or 41, or when it costs more than the score; the ones left are put in ascending order of price ($014132) and counted; when there is none the cursor is on the way out */
static void shop_stock_filter(void)
{
    uint16_t *list = guest.shop_list;
    uint16_t *out = guest.shop_offers;

    for (int i = 0; i < 30; i++)
        guest.shop_offers[i] = 0xFFFF;
    for (;;) {
        uint16_t id = *list++;

        if ((int16_t)id < 0) {
            *out++ = id;
            break;
        }
        if ((int16_t)id < 0xC0) {
            int16_t kind = 0;

            for (int i = 0; i < (int16_t)id; i++)
                if (offsets_4cba[i] == 0)
                    kind++;
            if (guest.shop_bought & (1u << kind))
                continue;
            if ((guest.slot_ref_6b6 & guest.slot_ref_748 & guest.slot_ref_7da) == -1 && (id == 2 || id == 3))
                continue;
            if (guest.falcon_energy == -1 && (id == 0x28 || id == 0x29))
                continue;
        }
        if ((int32_t)shop_item_value(id) > (int32_t)guest.score_total_1fc)
            continue;
        *out++ = id;
    }

    list = guest.shop_list;
    out = guest.shop_offers;
    guest.shop_count = 0;
    for (int i = 0; i < 30; i++) {
        *list = *out++;
        if ((int16_t)*list++ >= 0)
            guest.shop_count++;
    }
    *list = 0xFFFF;
    if (guest.shop_count == 0) {
        guest.cursor_pos = 0x11;
        return;
    }
    if (guest.shop_count > 1) {   /* ascending by price (a bubble sort of the words, as the original does it through the stack) */
        int16_t count = (int16_t)guest.shop_count;
        uint16_t stack[30];

        for (int i = 0; i < count; i++)
            stack[count - 1 - i] = guest.shop_list[i];
        for (int pass = 0; pass < count; pass++)
            for (int j = 0; j < count - 1; j++)
                if ((int16_t)shop_item_value(stack[j]) > (int16_t)shop_item_value(stack[j + 1])) {
                    uint16_t t = stack[j];

                    stack[j] = stack[j + 1];
                    stack[j + 1] = t;
                }
        for (int i = 0; i < count; i++)
            guest.shop_list[i] = stack[i];
    }
}

/* a long of shop_data_14206 (the words of the table are as the 68000's memory has them: the high one first) */
static uint32_t shop_data_long(uint32_t address)
{
    uint32_t index = (address - 0x14206) / 2;

    return ((uint32_t)shop_data_14206[index] << 16) | shop_data_14206[index + 1];
}

/* orig $01405E: the list of the shop's stock for the level and world (menu_active: for the level), up to 30 words, filtered */
static void shop_stock_build(void)
{
    uint32_t address;

    if (guest.menu_active != 0)
        address = shop_data_long(0x14348 + 4 * (uint32_t)(int16_t)(guest.param_index - 1));
    else
        address = shop_data_long(0x14206 + 4 * (uint32_t)(int16_t)((guest.param_index - 1) * 2 + guest.shop_door_index - 1));

    const uint16_t *stock = &shop_data_14206[(address - 0x14206) / 2];

    for (int i = 0; i < 30; i++)
        guest.shop_list[i] = stock[i];
    shop_stock_filter();
}

/* orig $013C16: the question that a weapon replaces another: "Warning.. this weapon replaces the" and the name of the one in the slot (a0 the slot's word) */
static void shop_replace_message(const int16_t *slot)
{
    static uint8_t text[96];
    const struct Object *object = &((const struct Object *)guest.object_slots)[*slot];
    uint8_t *to = text;

    for (int i = 0; i < 37; i++)
        *to++ = (uint8_t)text_replace[i];
    for (const uint8_t *name = object->name1; *name != 0; name++)
        *to++ = *name;
    *to++ = 0x20;
    *to++ = 0x11;
    *to = 0;
    guest.message_next = text;
    guest.message_arg = 0;
    message_request(0x19);
}

/* the weapon's slot (the word of slot_ref_6b6, 748 or 7da) that the object `item` would go in ($015328) */
static int16_t *shop_object_slot(int16_t item)
{
    int slot = pick_slot(item, &((const struct Object *)guest.object_slots)[item]);

    return slot == 0 ? &guest.slot_ref_6b6 : slot == 1 ? &guest.slot_ref_748 : &guest.slot_ref_7da;
}

/* orig $013A22 to $013C12: the fire button is pressed on a cell.  The first press chooses the item (its message is shown); the second buys it: a pick-up of a kind with a handler (shop_kinds_13b32) has the
 * handler run, another goes into a free slot (or the price is given back and "Inventory Full" shown); an object that would replace another asks first (the next press then buys it), and the value of what it
 * replaces (the price less half) is given back.  Returns 1 when the shop is to be left (the way out) */
static int shop_press_work(void)
{
    while (guest.button_bits[0] & 1)
        wait_vblank();   /* (the original waits at once; it sees the button let go in the vertical blank that polls it: the host's thread must not read the bits in the middle of the poll) */
    host_read_wait();   /* (the lockstep: the original's wait for the release is over, $13A2C) */
    int16_t cell = (int16_t)guest.cursor_pos;

    if (cell == 0x11)
        return 1;

    int16_t id = (int16_t)guest.shop_list[cell + (int16_t)guest.shop_scroll];

    if (id < 0)
        return 0;

    int16_t chosen = (int16_t)guest.shop_selected;

    if (chosen < 0)
        chosen = (int16_t)-chosen;
    if (id != chosen) {   /* choosing */
        guest.shop_selected = (uint16_t)id;
        guest.message_arg = 0;
        host_cpu_frames(1);   /* (the choice of an item while a message is on show: the original takes a frame more before it clears and restarts, the old text scrolled out one step more in its frames; the question of a replacement does not) */
        if (id < 0xC0) {
            guest.item_message_shown[id >> 3] &= (uint8_t)~(1 << (id & 7));
                    item_message(id);
        } else
            object_message(&((const struct Object *)guest.object_slots)[id - 0xC0]);
        return 0;
    }

    if (id < 0xC0) {   /* a pick-up */
        play_sound(0x21, 3);
        uint32_t price = guest.table_14cf8[id].w[3];

        guest.score_total_1fc -= price;
        int16_t kind = 0;

        for (int i = 0; i < id; i++)
            if (offsets_4cba[i] == 0)
                kind++;
        guest.shop_bought |= 1u << kind;
        if (shop_kinds_13b32[kind] != 0) {
            if (pickup_handler(kind, 0) < 0)
                host_missing_routine(0x1067A);
        } else {
            int placed = 0;

            for (int slot = 0; slot < 4; slot++)
                if (guest.item_slots[slot] < 0 && slot != (int16_t)guest.item_shown) {
                    guest.item_slots[slot] = id;
                    placed = 1;
                    break;
                }
            if (!placed) {   /* the price is given back */
                guest.score_total_1fc += price;
                guest.message_next = (const uint8_t *)text_inventory_full;
                guest.message_arg = 0;
                if (guest.message_shown != 0)
                    host_cpu_frames(1);   /* (as the choice of an item: the old text scrolls out one step more) */
                message_request(0x19);
            }
        }
        shop_stock_build();
        host_cpu_frames(1);   /* (the filter and the sort of the list take the original about a frame) */
        return 0;
    }

    int16_t item = (int16_t)(id - 0xC0);
    int16_t *slot = shop_object_slot(item);

    if ((int16_t)guest.shop_selected >= 0) {
        if (*slot >= 0 && item != *slot) {   /* a weapon is replaced: asked first */
            shop_replace_message(slot);
            guest.shop_selected = (uint16_t)-(int16_t)guest.shop_selected;
            return 0;
        }
    }
    play_sound(0x21, 3);
    slot = shop_object_slot(item);
    if (*slot >= 0 && item != *slot) {   /* what the weapon is worth is given back, half of it */
        const struct Object *old = &((const struct Object *)guest.object_slots)[*slot];
        uint32_t value = (uint32_t)(uint16_t)(old->last_value - old->first_value) * (uint16_t)old->w74;

        value += (uint32_t)(int32_t)(int16_t)old->w74;
        guest.score_total_1fc = (uint32_t)((int32_t)guest.score_total_1fc + ((int32_t)value >> 1));
    }
    guest.score_total_1fc -= (uint16_t)((const struct Object *)guest.object_slots)[item].w74;

    uint16_t saved = guest.message_arg;

    guest.message_arg = 0x32;
    pick_up_object(item);
    guest.message_arg = saved;
    shop_stock_build();
    return 2;   /* (a weapon bought: no frame goes by here: the original is back at the top of its loop in the frame it was let go in) */
}

/* (and about two frames more, the draw of the grid, before its flip) */
static int shop_press(void)
{
    int leave = shop_press_work();

    if (leave == 0)
        wait_vblank();
    return leave == 1;
}

/* the original waits in the loop until more than two frames have passed since the last flip, then flips ($01D2FC) */
static uint8_t shop_button_old, shop_button_older, shop_stick_old, shop_stick_older;   /* the button as the blank before the flip's saw it: the original's loop reads it before the blank's poll of the frame it begins in */

static void shop_wait_flip(void)
{
    uint16_t seen = guest.vbl_count;

    shop_button_older = shop_button_old;
    shop_button_old = guest.button_bits[0] & 1;
    shop_stick_older = shop_stick_old;
    shop_stick_old = guest.joystick;
    while ((int16_t)(2 - guest.vbl_saturating) >= 0) {
        host_spin();
        if (guest.vbl_count != seen) {   /* (a blank has passed: the bits are not in the middle of its poll) */
            seen = guest.vbl_count;
            shop_button_older = shop_button_old;
            shop_button_old = guest.button_bits[0] & 1;
            shop_stick_older = shop_stick_old;
            shop_stick_old = guest.joystick;
        }
    }
    host_spin_end();
    screens_flip();
}

/* the screen that a flip brings to the front had its strip drawn when it was last shown: the original's interrupt draws it before the frame is shown, so the strip is drawn on it as the flip is done */
void shop_flip_strip(void)
{
    if (guest.tick_hold_38a && guest.shop_text != 0 && strip_shown_valid)
        shop_strip_draw(strip_shown);
}

/* orig $013690 to $0139DE: the shop.  The screen fades out, shop.pi1 is read to the hidden screen (the rows of 40 bytes of its planes go to the rows of 44), the screen is faded in again with the level's colours; the
 * stick moves the cursor over the grid (the first move at once, then after 8 frames every 2), fire presses; the score counts to the score, the text of the messages runs along the strip at the foot.  The way out
 * (cell 17) fades the screen out, draws a frame of the game, and the game goes on from the start of a frame ($2BD6) */
static void shop_run(void)
{
    uint32_t saved_ticks = shop_from_password ? 0 : guest.tick_count;

    guest.shop_score_shown = guest.score_total_1fc;
    guest.cursor_pos = 0;
    guest.shop_repeat = 0;
    guest.shop_held[0] = guest.shop_held[1] = 0;
    guest.shop_selected = 0xFFFF;
    shop_button_old = shop_button_older = shop_stick_old = shop_stick_older = 0;
    guest.message_shown = 0;
    guest.draw_routine_index = 0;
    *(volatile uint8_t *)&guest.flags_0ddd = 0;   /* clr.b $ddd.w */
    if (!shop_from_password) {
        host_fade_lag(4);   /* (the lockstep: the steps are shown as the original shows them: a frame after the thread has made them when its first step began late in a frame, as they are when it began early) */
        fade_out(1, 2);
        host_fade_lag(0);
    }

    uint16_t saved_25c = shop_from_password ? 1 : guest.word_025c;
    shop_from_password = 0;
    int16_t saved_x_offset = guest.x_offset;

    guest.flags_3b0 &= (uint8_t)~4;
    guest.flags_3b1 &= (uint8_t)~4;
    load_unpacked(shop_pi1_name, guest.picture_buffer - 0x22);
    host_disk_wait();   /* (the original's read of the picture takes what its drive takes before it fades out again) */

    const uint16_t *picture = (const uint16_t *)guest.picture_buffer;
    uint8_t *to = guest.screen_buffer;

    for (int row = 0; row < 192; row++) {
        for (int word = 0; word < 20; word++) {
            *(uint16_t *)to = *picture++;
            to += 2;
            *(uint16_t *)(to + 0x20FE) = *picture++;
            *(uint16_t *)(to + 0x41FE) = *picture++;
            *(uint16_t *)(to + 0x62FE) = *picture++;
        }
        for (int plane = 0; plane < 4; plane++) {   /* (clr.l (a1)+ and clr.l $20FE(a1) ... with a1 four bytes on: the other planes' longs start two bytes after the end of their rows) */
            uint16_t *tail = (uint16_t *)(to + (plane == 0 ? 0 : 4 + plane * 0x2100 - 2));

            tail[0] = 0;
            tail[1] = 0;
        }
        to += 4;
    }

    fade_keep_screens = 1;   /* (the original's fade-out does not clear the screens here: the word at $1CFC0 is an rts for the time) */
    host_fade_lag(1);
    fade_out(1, 0);
    host_fade_lag(0);
    fade_keep_screens = 0;
    display_reset();

    uint8_t *shown = guest.screen_buffer;
    uint8_t *other = guest.screen_other;

    guest.screen_buffer = other;   /* (the sound file is read with the screens as they are swapped) */
    guest.screen_other = shown;
    file_read(sound1_name, guest.picture_buffer);
    host_disk_wait();
    guest.screen_buffer = shown;
    guest.screen_other = other;
    drives_idle();
    guest.shop_text = (uint8_t *)guest.line_text;
    shop_clear_text();
    for (int i = 0; i < 9 * 32; i++)
        strip_shown[i] = 0;
    strip_shown_valid = 1;
    guest.tick_hold_38a = 1;
    guest.shop_bought = 0;
    shop_stock_build();
    shop_draw_grid();
    shop_score();

    uint16_t saved_1ec = guest.copper_colour_count;
    uint16_t saved_1ee = guest.color_line_spacing;
    uint8_t *saved_cda = guest.copper_colours;

    guest.copper_colour_count = 0x19;
    guest.color_line_spacing = 4;
    uint16_t saved_scroll_y = guest.scroll_y;

    guest.scroll_y = 0;
    screens_flip();
    guest.copper_colours = (uint8_t *)shop_data_14206 + (0x14378 - 0x14206);
    copper_list_make();
    clear_work_area();

    uint16_t saved_words[8];

    for (int i = 0; i < 8; i++)
        saved_words[i] = guest.message_words[i];
    copper_colors_restore();
    guest.message_words[0] = guest.message_words[2] = guest.palette_words[6];
    guest.message_words[1] = guest.message_words[3] = guest.palette_words[6];
    guest.message_words[4] = guest.message_words[6] = guest.palette_words[4];
    guest.message_words[5] = guest.message_words[7] = guest.palette_words[4];
    guest.word_025c = 1;
    guest.copper_setup_done = 0;
    host_fade_bind();   /* (the lockstep: the fade-in begins when the original calls its own) */
    host_fade_lag(0);
    fade_in(guest.palette_words, 1);
    host_fade_lag(0);
    host_cpu_frames(1);   /* (the original goes on 31 frames after the call of the fade) */
    copy_other_screen();
    host_cpu_frames(2);   /* (the copy of a screen takes the original about two frames) */

    for (;;) {
        host_iteration_mark();
        guest.frame_toggle = (uint16_t)~guest.frame_toggle;
        message_tick();
        guest.shop_cursor_old = guest.cursor_pos;
        host_read_wait();   /* (the lockstep: the original's read of the stick at the top of its loop is done: the byte is the one it had) */
        shop_stick_older = guest.joystick;

        int moved = 1;

        if (shop_stick_older != 0) {
            if (guest.shop_held[1] == 0) {
                guest.shop_repeat = 1;
                guest.shop_held[1] = shop_stick_older;
            } else if (guest.shop_held[1] == shop_stick_older) {
                guest.shop_repeat++;
                if ((int16_t)guest.shop_repeat < 8)
                    moved = 0;
                else
                    guest.shop_repeat = 6;
            } else {
                guest.shop_repeat = 0;
                moved = 0;
            }
        } else {
            guest.shop_held[0] = guest.shop_held[1] = 0;
            guest.shop_repeat = 0;
        }
        if (moved) {
            int16_t cell = (int16_t)guest.cursor_pos;

            if (shop_stick_older & 8) {
                cell++;
                if (cell > 17)
                    cell--;
            }
            if (shop_stick_older & 4) {
                cell--;
                if (cell < 0)
                    cell++;
            }
            if (shop_stick_older & 1) {
                cell -= 6;
                if (cell < 0)
                    cell += 6;
            }
            if (shop_stick_older & 2) {
                cell += 6;
                if (cell > 17)
                    cell -= 6;
            }
            guest.cursor_pos = (uint16_t)cell;
        }
        if (guest.cursor_pos != guest.shop_cursor_old)
            play_sound(0x16, 2);
        host_read_wait();   /* (the lockstep: the original's read of the button, $1390A, is done) */
        shop_button_older = (guest.joystick >> 7) & 1;
        if (shop_button_older) {
            shop_button_old = shop_button_older = 0;   /* (the old samples are of the press that is handled now) */
            if (shop_press())
                break;
            continue;   /* (the original goes back to the top of its loop after a press, without a flip: the frame's toggle, the message tick and the reads are done again before the grid is drawn) */
        }
        shop_draw_grid();
        host_iteration_cpu(2);   /* (the grid takes the original about two frames to draw: the score goes on after) */
        shop_score();
        shop_wait_flip();
    }

    for (int i = 0; i < 8; i++)
        guest.message_words[i] = saved_words[i];
    guest.scroll_y = saved_scroll_y;
    guest.copper_colours = saved_cda;
    guest.color_line_spacing = saved_1ee;
    guest.copper_colour_count = saved_1ec;
    guest.message_shown = 0;
    wait_vblank();
    reload_sprites();
    wait_vblank();
    file_read(sound1_name, guest.picture_buffer);
    host_disk_wait();   /* (the original's reads of the sprites and the sound take what its drive takes before it fades out) */
    host_fade_lag(1);
    fade_out(1, 0);
    host_fade_lag(0);
    guest.tick_hold_38a = 0;
    draw_start_frame();
    screens_flip();
    copper_list_make();
    clear_work_area();
    guest.x_offset = saved_x_offset;
    guest.word_025c = saved_25c;
    copper_colors_restore();
    host_fade_bind();   /* (the lockstep: the fade-in begins when the original calls its own) */
    host_fade_lag(2);
    fade_in(guest.palette_words, 1);
    host_fade_lag(0);
    guest.tick_count = saved_ticks;
    guest.word_0248 = 0;
    guest.tick_hold_26c = 0;
    guest.message_shown = 0;
    guest.message_arg = 0;
    clear_work_area();
    wait_vblank();
    drives_idle();
    if (guest.flag_aa8 != 0)
        play_sound(60, 2);
    else
        stop_sound(60);
    if (guest.param_index == 3)
        play_sound(66, 1);
}
