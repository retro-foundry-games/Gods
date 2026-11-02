/* Gods, the game: what runs while a level is played.  The loop that calls it all is at $2C20; the routines are written in the
 * order they were found, named by what they do (a provisional name is `fn_` and the address). */
#include "menu.h"
#include "sound.h"

/* orig $00363A: this frame's input: the joystick byte and the button bits are kept for the game; when the input is being recorded
 * (demo_mode positive) one byte of them is put in the record (the buttons in bits 6 and 7, the stick below) */
void game_read_input(void)
{
    game_stick = joystick;
    game_buttons = button_bits[0];
    if (demo_mode == 0)
        return;
    if (demo_mode < 0)
        return;   /* playing a demo back: the original goes on at $3668, which did not run */
    *game_record_ptr++ = (uint8_t)((game_buttons << 6) | game_stick);
}

/* orig $004952: a thing at the screen position (x, y) is counted when it is near the screen: x from -16 to 320 and y, less the
 * scroll, above -32 and below 192 */
void count_if_visible(int16_t x, int16_t y)
{
    y -= scroll_y;
    if (x > 320 || x < -16 || y <= -32 || y >= 192)
        return;
    onscreen_count++;
}

/* orig $00D314: the first of the ten records of 12 bytes at $113C whose third word is not 0, or 0 (the original goes on at $D32A) */
struct Slot12 *find_record_113c(void)
{
    for (int i = 0; i < 10; i++)
        if (records_113c[i].w[2] != 0)
            return &records_113c[i];
    return 0;
}

/* orig $01924C: the first of the ten records of 8 bytes at $19334 whose third word is not negative, or 0 (the original goes on at $19262) */
struct Slot8 *find_record_19334(void)
{
    for (int i = 0; i < 10; i++)
        if (records_19334[i].w[2] >= 0)
            return &records_19334[i];
    return 0;
}

/* orig $018EF6: the first free (third word negative) of the two hundred records of 8 bytes at $19B14, or 0 when all are in use */
struct Slot8 *find_free_record_19b14(void)
{
    for (int i = 0; i < 200; i++)
        if (records_19b14[i].w[2] < 0)
            return &records_19b14[i];
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
 * times the argument (as digits, kept in bcd_346, which sets flag_2d0 as score_add does) is at least / at most the score; 15 and
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
        int16_t found = arg == item_slots[0] || arg == item_slots[1] || arg == item_slots[2] || arg == item_slots[3];

        clear = kind == 1 ? !found : found;
        break;
    }
    case 3:
    case 4: {
        int16_t found = arg == slot_ref_6b6 || arg == slot_ref_748 || arg == slot_ref_7da;

        clear = kind == 3 ? !found : found;
        break;
    }
    case 5:
    case 6: {
        int16_t state = (int16_t)ptr_0cbe[arg - 1].state;

        clear = kind == 5 ? state >= 0 : state < 0;
        break;
    }
    case 7:
        clear = arg >= (int16_t)player_energy;
        break;
    case 8:
        clear = arg <= (int16_t)player_energy;
        break;
    case 9:
    case 10: {
        uint32_t ticks = tick_count;
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
        int set = flags_1c056[(uint16_t)arg].state & 1;

        clear = kind == 11 ? !set : set;
        break;
    }
    case 13:
    case 14: {
        int16_t m5 = (int16_t)(arg * 5);   /* the number is m5 * 1000 */
        int cmp = 0;

        flag_2d0 = 1;
        bcd_346[0] = 0;
        bcd_346[1] = (uint16_t)((m5 % 10) << 4);
        bcd_346[2] = (uint16_t)(((m5 / 10) % 10) | (((m5 / 100) % 10) << 4));
        bcd_346[3] = (uint16_t)(((m5 / 1000) % 10) | (((m5 / 10000) % 10) << 4));
        for (int i = 3; i >= 0 && cmp == 0; i--)
            cmp = (int16_t)bcd_346[i] < (int16_t)score_words[i] ? -1 : (int16_t)bcd_346[i] > (int16_t)score_words[i] ? 1 : 0;
        clear = kind == 13 ? cmp >= 0 : cmp <= 0;
        break;
    }
    case 15:
        clear = arg >= item_count;
        break;
    case 16:
        clear = arg <= item_count;
        break;
    }
    if (clear)
        *result = 0;
}

/* orig $012A80: the first message line, or the one after it when it is the one on show (message_text or message_current) */
uint8_t *next_message_slot(void)
{
    uint8_t *slot = message_slots;

    if (slot == message_text || slot == message_current)
        slot += 88;
    return slot;
}

/* orig $001D2FC: wait until more than `frames` frames have passed since the last flip (vbl_saturating counts them), then flip */
static void wait_frames_then_flip(uint16_t frames)
{
    while ((int16_t)(frames - vbl_saturating) >= 0)
        ;
    screens_flip();
}

/* not in the original: the loop of the original at $2C20 is sent to game_loop (a jump over its first six bytes), which runs game_frame in
 * its place and goes back to $2C20.  This is called before the original's Begin Quest, Continue and the code after the password
 * screen start the game */
void game_loop_install(void)
{
    *(volatile uint16_t *)GUEST(0x2c20) = 0x4ef9;
    *(volatile uint32_t *)GUEST(0x2c22) = (uint32_t)(uintptr_t)game_loop;
}

/* orig $0030C6: the screen is pushed to the right, 4 pixels a frame (the toggle word shifted twice, or 4 when it is 0), and flag_26e (the
 * frames left) counts down.  The recording never reaches it (flag_26e is 0 there) */
static void scroll_push(void)
{
    int16_t step = (int16_t)(frame_toggle << 2);

    if (step == 0)
        step = 4;
    scroll_x += step;
    flag_26e--;
}

/* orig $002C20: one frame of the game.  The routines it calls are the original's (fn_ and the address) until they are written
 * in C.  Where the original goes on at an address that did not run in the recorded play ($3A36, $3A6C, $3A54, $39F6, $32B4,
 * $30DA, $35D2, $2F12) the frame ends here: those are the ends of a level, a death, the pause menu and the like */
void game_frame(void)
{
    message_tick();
    game_read_input();
    frame_toggle = ~frame_toggle;
    frame_phase++;
    if ((int16_t)frame_phase >= 2)
        frame_phase = 0;
    timer_306--;
    if ((int16_t)timer_306 < 0)
        timer_306 = 0;
    if ((int16_t)demo_mode < 0 && (button_bits[0] & 1)) {
        demo_mode = 0;
        return;   /* goes on at $3A36 */
    }
    if (!(game_buttons & 1))
        fire_latch = 0;
    energy_old = player_energy;
    if ((int16_t)energy_shown <= 0)
        return;   /* goes on at $3A6C */
    fill_buffer();
    if (flag_26e)
        scroll_push();
    draw_background();
    draw_records_19334();
    do_nothing_b();
    do_nothing_b();
    do_nothing_b();
    if (flag_23a)
        callback_4e4();
    redraw_record_images();
    if (param_index == 2 || flag_23a != 1) {
        activate_records();
        update_records();
        update_falling_thing();
        game_step_4070();
    }
    run_object_lists();
    fn_15cc4_c();
    step_836c();
    value_c6c = 3030;
    flag_20e = -1;
    update_creatures();
    if (flag_23a == 0) {
        game_step_73d2();
        game_nothing();
        game_nothing();
        draw_effects();
        draw_marks_316();
    }
    scroll_x_a = scroll_x;
    scroll_y_a = scroll_y;
    if (flag_290 >= 0)
        fn_33d4();
    else
        player_control();
    count_zones();
    scroll_x_b = scroll_x;
    scroll_y_b = scroll_y;
    scroll_x = scroll_x_a;
    scroll_y = scroll_y_a;
    step_16aee();
    if (flag_23a == 0) {
        do_nothing_b();
        step_190f2();
        do_nothing_b();
        do_nothing_b();
        update_records_12a0();
        game_nothing();
        mark_player_cells();
        update_records_1354();
        draw_sprites_721e();
        do_nothing_b();
        unit0_update();
        unit1_update();
        unit2_update();
        unit3_update();
        unit4_update();
        next_table_entry();
        find_record_113c();
        do_nothing_b();
        step_d898();
    } else {
        callback_4e0();
        do_nothing_b();
        do_nothing_b();
        do_nothing_b();
    }
    do_nothing_b();
    move_records_3a0a6();
    draw_sprite_8472();
    update_bolts();
    step_1843e();
    do_nothing_b();
    update_score_display();
    draw_score_panel();
    update_item_panel();
    draw_shutter();
    update_energy_display();
    draw_energy_bar();
    draw_item_row();
    if (flag_268) {
        int16_t d = (int16_t)energy_old;

        if (d >= (int16_t)player_energy) {
            d -= (int16_t)player_energy;
            player_energy += d >> 1;
        }
    }
    scroll_x = scroll_x_b;
    scroll_y = scroll_y_b;

    int16_t request = (int16_t)take_pending_request();

    if (flag_23a == 0 && request == 27)
        return;   /* goes on at $3A54 */
    if (request == 32) {   /* the space bar: the game waits for a request or a button, and the clock goes on from where it was */
        uint32_t saved_ticks = tick_count;

        tick_hold_26c = 1;
        while ((uint16_t)take_pending_request() != 0)
            ;
        for (;;) {
            request = (int16_t)take_pending_request();
            if (request != 0 || button_bits[0] != 0)
                break;
        }
        tick_hold_26c = 0;
        tick_count = saved_ticks;
    }
    if (flag_2b0 == 18)
        request = 98;
    if (request == 70)
        return;   /* goes on at $39F6 */
    if (flag_230)
        fn_12e08();
    else
        wait_frames_then_flip(frame_wait);
    if ((int16_t)flag_290 >= 0)
        return;   /* goes on at $32B4 */
    if (flag_258)
        return;   /* goes on at $30DA */
    if (flag_1f0)
        return;   /* goes on at $35D2 */
    if (flags_3b0 & 0x40)
        return;   /* goes on at $2F12 */
}

/* orig $006002: the 1600 bytes line_text points at are filled with a pattern of 56 bytes (fourteen longs): the words of fill_words
 * made longs, or the longs of fill_longs when flag_290 is not negative (then flag_210 is cleared as well) */
void fill_buffer(void)
{
    uint32_t pattern[14];

    for (int i = 0; i < 14; i++)
        pattern[i] = (uint32_t)(int32_t)(int16_t)fill_words[i];
    if ((int16_t)flag_290 >= 0) {
        for (int i = 0; i < 14; i++)
            pattern[i] = fill_longs[i];
        flag_210 = 0;
    }

    uint32_t *end = (uint32_t *)((uint8_t *)line_text + 1600);

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
 * the screen with 64 to the left, 48 above, 384 to the right and 240 below is marked (bit 0 of its last byte; flags_39fde says
 * for the new ones), the others lose their flags word */
void activate_records(void)
{
    int16_t d0;

    if ((scroll_x & 31) != 0) {
        flag_b24 = 0;
    } else if (flag_b24 == 0) {
        activate_records_now();
        return;
    }
    d0 = scroll_y - word_2aa;
    if (d0 < 0)
        d0 = -d0;
    if (!(d0 & 0x20))
        return;
    activate_records_now();
}

/* orig $003EB4: the part of activate_records that marks the records (also called at the start of a level) */
void activate_records_now(void)
{
    flag_b24 = 1;
    word_2aa = scroll_y;

    int16_t x0 = (scroll_x & -32) - 64;
    int16_t y0 = (scroll_y & -16) - 48;
    int16_t x1 = (scroll_x & -32) + 384;
    int16_t y1 = (scroll_y & -16) + 240;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];

        if (record->w[2] < 0 || record->w[0] < x0 || record->w[0] > x1 || record->w[1] < y0 || record->w[1] > y1) {
            record->w[3] = 0;
            continue;
        }
        if (!(record->w[3] & 1)) {
            record->w[3] |= 1;
            flags_39fde[i] = 0xff;
        }
    }
}

/* orig $009912: which of the seven zones the player's box is in (a box 32 by 48 from player_x, player_y less the scroll against
 * the limits of zone_limits) and how many zones of each group of three that is: zone_counts */
void count_zones(void)
{
    uint8_t *counts = zone_counts;
    int16_t x = player_x - scroll_x;
    int16_t y = player_y - scroll_y;
    uint16_t mask = 0;

    *(uint32_t *)(zone_counts) = 0;   /* $4B6 and $4BA: two longs */
    *(uint32_t *)(zone_counts + 4) = 0;
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
 * again.  flag_2ac says whether any is alive */
void draw_effects(void)
{
    if (flag_2ac == 0)
        return;
    flag_2ac = 0;
    for (int i = 0; i < 10; i++) {
        int16_t *effect = &effects[i * 5];

        if (effect[0] < 0)
            continue;
        flag_2ac = 1;
        glyph_draw(effect[0] - scroll_x, effect[1] - scroll_y, effect[3] + 80);
        if (--effect[3] < 0)
            effect[0] = -1;
    }
}

/* orig $005E02: the energy as shown goes a step a frame toward the player's: down (the sound is played once as it begins) or up
 * (to 24 at most) */
void update_energy_display(void)
{
    int16_t shown = (int16_t)energy_shown;

    if (shown > (int16_t)player_energy) {
        energy_shown--;
        if (flag_2e0 == 0) {
            play_sound(41, 14);
            flag_2e0++;
        }
    } else if (shown < (int16_t)player_energy) {
        flag_2e0 = 0;
        shown++;
        if (shown > 24)
            shown = 24;
        energy_shown = shown;
    } else {
        flag_2e0 = 0;
    }
}

/* orig $003806: a row of glyphs (212) at the top of the screen, 16 pixels apart, one for each of the things the player has, at most 10 */
void draw_item_row(void)
{
    int16_t n = item_count;

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
    uint16_t *p = (uint16_t *)((uint8_t *)work_area + 320);

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
    int16_t column = message_wide;

    if (column < 0) {
        if (++message_x >= 328) {
            message_x = 0;
            message_wide = 0;
            message_arg = 0;
            message_shown = 0;
        }
        return;
    }

    int16_t glyph = (int8_t)message_text[(column & -8) >> 3];

    if (glyph == 0) {
        message_x = 0;
        message_wide = -1;
        message_arg = 0;
        flag_236 = 0;
        return;
    }

    int shift = 7 - (column & 7);
    uint16_t mask = (uint16_t)(0x80 >> (column & 7));
    const uint8_t *bits = font_8x8 + (glyph << 3);
    uint16_t *to = (uint16_t *)((uint8_t *)work_area + 40 + 38);

    for (int row = 0; row < 7; row++) {
        *to |= (uint16_t)((bits[row] & mask) >> shift);
        to = (uint16_t *)((uint8_t *)to + 40);
    }
    message_wide++;
}

/* orig $009AE6: the ten records of 18 bytes at $12A0, each with a block of 240 bytes (at asset_directory + 240 a record) of its own:
 * a record that is not free (first word not 0) goes on at $9B24 or $9A20, which did not run */
void update_records_12a0(void)
{
    flag_244 = -1;
    for (int i = 0; i < 10; i++) {
        temp_bb6 = asset_directory + 240 * i;
        if (records_12a0[i * 9] != 0)
            return;
    }
}

/* orig $008A54: the same for the ten records of 428 bytes at $1354, with blocks of 400 bytes at ptr_0cee; value_1296 is set to $121C
 * first; a record that is not free goes on at $8A98 or $891C, which did not run */
void update_records_1354(void)
{
    flag_244 = 0;
    value_1296 = GUEST(0x121c);
    for (int i = 0; i < 10; i++) {
        temp_bb6 = ptr_0cee + 400 * i;
        if (records_1354[i * 214] != 0)
            return;
    }
}

/* orig $00D47E: with param_index 1 the entry of table_ed0 index_1ea names (four words) is looked at: the one whose third word is
 * not 0 or that is not in the window of 9 by 11 cells (scroll in cells of 32 by 16) at the scroll is passed by: index_1ea goes on
 * 8, back to 0 at 256 or when the first long of the table is negative.  An entry in the window (third word 0) goes on at $D4E2,
 * which did not run */
void next_table_entry(void)
{
    if (param_index != 1) {
        index_1ea = 0;
        return;
    }
    int16_t *entry = (int16_t *)((uint8_t *)table_ed0 + (int16_t)index_1ea);

    if (entry[2] == 0) {
        int16_t cell_x = scroll_x >> 5;
        int16_t cell_y = scroll_y >> 4;

        if (entry[0] > cell_x && entry[1] > cell_y && entry[0] < cell_x + 9 && entry[1] < cell_y + 11)
            return;   /* goes on at $D4E2 */
    }
    int16_t next = (int16_t)index_1ea + 8;

    if (next >= 256 || ((int32_t)((table_ed0[0] << 16) | (uint16_t)table_ed0[1])) < 0) {
        index_1ea = 0;
        return;
    }
    index_1ea = next;
}

/* orig $003C56: the player's place in the buffer line_text points at, 101 bytes in: four bytes of $FD around where the player is
 * on the screen (a cell of 8 pixels, 12 bytes of the buffer a row of cells) */
void mark_player_cells(void)
{
    int16_t x = (player_x - scroll_x) >> 3;
    int16_t y = (int16_t)(player_y - scroll_y) & -8;
    uint8_t *p = (uint8_t *)line_text + 100 + 1;

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

    if (flag_721c == 0)
        return;
    for (int i = 0; i < 10; i++) {
        int16_t *record = &records_721e[i * 3];

        if (record[2] < 0)
            continue;
        alive++;
        sprite_draw(record[0] - scroll_x, record[1] - scroll_y, record[2] + 100);
        if (--record[2] < 0) {
            record[2] = -1;
            alive--;
        }
    }
    if (alive == 0)
        flag_721c = 0;
}

/* orig $003DFC: when flag_2a8 says so, the twenty records of six words at $3A0A6 that are in use (first word not negative) move
 * (the third and fourth words are added to the second and third: x and y) and are drawn as glyphs of the sixth word where they are on
 * the screen; the sixth word goes up and at 69 the record is free again (and flag_2a8 down) */
void move_records_3a0a6(void)
{
    if (flag_2a8 == 0)
        return;
    for (int i = 0; i < 20; i++) {
        int16_t *record = &records_3a0a6[i * 6];

        if (record[0] < 0)
            continue;
        record[1] += record[3];
        record[2] += record[4];
        glyph_draw(record[1] - scroll_x, record[2] - scroll_y, record[5]);
        record[5]++;
        if (record[5] >= 69) {
            record[0] = -1;
            flag_2a8--;
        }
    }
}

/* orig $003F46: an effect is started where (x, y) is on the screen (x and y are the screen's, the scroll is added): in the first free
 * record of the ten at `effects` (the first if none is alive), with the kind (`d2`), a count of 6 and -12; none if all are in use */
void spawn_effect(int16_t x, int16_t y, int16_t kind)
{
    int16_t *effect = effects;

    if (flag_2ac != 0) {
        int i;

        for (i = 0; i < 10; i++, effect += 5)
            if (effect[0] < 0)
                break;
        if (i == 10)
            return;
    }
    effect[0] = x + scroll_x;
    effect[1] = y + scroll_y;
    effect[2] = kind;
    effect[3] = 6;
    effect[4] = -12;
    flag_2ac = 1;
}

/* orig $010470: the player (a box 32 wide and 16 high at the player) against the twenty records of 8 bytes at $19334: the first that
 * is in use (third word above 0) and inside it sets flag_1f0, copies its third and fourth words and plays sound 5 with priority 25 */
void check_pickups(void)
{
    int16_t x0 = player_x - 16, y0 = player_y - 8, x1 = player_x + 16, y1 = player_y + 8;

    for (int i = 0; i < 20; i++) {
        struct Slot8 *record = &records_19334[i];

        if (record->w[2] <= 0)
            continue;
        if (record->w[0] < x0 || record->w[0] > x1 || record->w[1] < y0 || record->w[1] > y1)
            continue;
        flag_1f0 = 1;
        word_200 = record->w[2];
        word_202 = record->w[3];
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
    const uint8_t *from = digit_font + (digit << 5);

    for (int row = 0; row < 8; row++) {
        set_plane_byte(to, *from++);
        set_plane_byte(to + 150, *from++);
        set_plane_byte(to + 300, *from++);
        set_plane_byte(to + 450, *from++);
        to += 10;
    }
}

/* orig $005842: when flag_2d0 says so the eight digits of the score are drawn in the score image (image 121 of the image table, 48
 * bytes in, from the right: the low digit of the first word first), then the routine at $5AF6 compares it with the high scores */
void update_score_display(void)
{
    if (flag_2d0 == 0)
        return;
    flag_2d0 = 0;

    uint8_t *to = *(uint8_t **)((uint8_t *)image_table + 968 + 4) + 48;

    for (int w = 0; w < 4; w++) {
        draw_score_digit(to, score_words[w] & 15);
        to--;
        draw_score_digit(to, score_words[w] >> 4);
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

/* orig $004070: nothing is done while word_2ae is 0; the original goes on at $4076 otherwise, which did not run */
void game_step_4070(void)
{
    if (word_2ae == 0)
        return;
    /* goes on at $4076 */
}

/* the shutter's rows: for each of the 12 steps (word_2c0) a word with a bit for each of the 24 rows of its columns (12 from the top, then 12
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
 * columns of 32 pixels and 24 rows each, 384 bytes a column) is drawn while word_2c0 is not negative.  word_2c0 is the step (0 to 11,
 * 12 is open); word_2be, added to it, says which way it goes.  At a step under 12 the rows named by the step's word in
 * shutter_masks (shutter_rows) are packed into ptr_0e44 and blitted, a column at a time, the part moving down a row of 44
 * bytes a step; at 12 the picture is drawn whole.  A step to 11 plays sound 22; at 12 and with word_2be 0 the step goes back by 1
 * (or word_2be turns to -1 when word_2bc was set) */
void draw_shutter(void)
{
    if (word_2c0 < 0)
        return;

    const uint8_t *image = (const uint8_t *)image_table[115].data;
    uint8_t *buffer = ptr_0e44;
    uint8_t *screen = screen_buffer + 7492 + (((x_offset & 31) & -16) >> 3);
    int16_t shift = (x_offset & 31) & 15;

    if (word_2c0 == 12) {
        screen -= 484;
        for (int column = 0; column < 4; column++) {
            blit_image_wide(screen, image, 96, shift, 24);
            screen += 4;
            image += 384;
        }
        if (word_2be != 0) {
            word_2c0--;
            return;
        }
        if (word_2bc != 0) {
            word_2be = -1;
            word_2bc = 0;
        }
        return;
    }
    screen -= (uint16_t)word_2c0 * 44;
    for (int column = 0; column < 4; column++) {
        int rows = shutter_rows(image, buffer, shutter_masks[word_2c0]);

        if (rows != 0)
            blit_image_wide(screen, buffer, 96, shift, rows);
        image += 384;
        screen += 4;
    }
    word_2c0 += word_2be;
    if (word_2be < 0)
        return;
    if (word_2c0 != 11)
        return;
    play_sound(22, 2);
}

/* orig $005496: word_2bc is set to 1 unless word_2c0 is 12 (then sound 57, priority 40, is played and the shutter's way word_2be is -1) */
void game_step_5496(void)
{
    if (word_2c0 != 12) {
        word_2bc = 1;
        return;
    }
    play_sound(57, 40);
    word_2be = -1;
}

/* orig $0073D2: nothing is done while word_73ce is 0; the original goes on at $73D8 otherwise, which did not run */
void game_step_73d2(void)
{
    if (word_73ce == 0)
        return;
    /* goes on at $73D8 */
}

/* orig $0057E0: a blank slot picture (image 119, 384 bytes: 24 rows, four planes of 96 bytes... as four longs a row) is copied to `to` */
void panel_blank(uint8_t *to)
{
    const uint8_t *from = (const uint8_t *)image_table[119].data;

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

    const struct Image *image = &image_table[glyph];
    int16_t height = (int16_t)image->height;
    const uint8_t *source = (const uint8_t *)image->data;
    int16_t stride = (int16_t)((height + 1) * 2);
    uint16_t *dst = (uint16_t *)(to + 16 + (((16 - height) >> 1) << 2));
    const uint16_t *p0 = (const uint16_t *)source;
    const uint16_t *p1 = (const uint16_t *)(source + stride);
    const uint16_t *p2 = (const uint16_t *)(source + 2 * stride);
    const uint16_t *p3 = (const uint16_t *)(source + 3 * stride);
    const uint32_t keep = 0xf000ffff;

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
    panel_item(to, (int16_t)table_14cf8[item].w[0]);
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
    panel_item(to, (int16_t)table_14cf8[item].w[0]);
}

/* orig $005608 (with $0056BE, $0056D8, $00570A and $0057E0 inlined): when the slot that is chosen (word_2ce) is another than the one
 * shown (word_2cc) the picture of the old one (image 115 and the slot's number) is made plain and the new one's cut, and the new
 * is the one shown */
void update_item_panel(void)
{
    int16_t chosen = (int16_t)word_2ce;

    if (chosen == (int16_t)word_2cc)
        return;
    panel_slot_plain((uint8_t *)image_table[115 + (int16_t)word_2cc].data, item_slots[(int16_t)word_2cc]);
    panel_slot_cut((uint8_t *)image_table[115 + chosen].data, item_slots[chosen]);
    word_2cc = word_2ce;
}

/* orig $007E90: where in the map of the level (ptr_0b86 + 8192, a byte for each cell of 32 by 16 pixels, 128 bytes a row of
 * cells... 8 bytes a 16 pixel step of y) the point (x, y) is */
uint8_t *map_cell(int16_t x, int16_t y)
{
    int16_t across = (int16_t)(x & -32) >> 5;
    int16_t down = (int16_t)((y & -16) << 3);

    return ptr_0b86 + 8192 + across + down;
}

/* orig $005AF6 (and inlined: $006882): the score as one long (the low byte of each of its four words, the low digits in the
 * lowest byte) is compared with the two high scores.  Under the first and not under the second (and flag_230 is 0) the original goes
 * on at $5B46; at or over the first at $5B04; neither ran */
void check_high_scores(void)
{
    uint32_t score = ((uint32_t)score_words[3] << 24) | ((uint32_t)score_words[2] << 16) | ((uint32_t)score_words[1] << 8) |
                     score_words[0];

    if ((int32_t)score >= high_score_baa)
        return;   /* goes on at $5B04 */
    if (flag_230 != 0)
        return;
    if ((int32_t)score < high_score_bae)
        return;
    /* goes on at $5B46 */
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

/* orig $004718 (and inlined: $0047C8, $0047CC, $0047D0, $0047D4): `points` is added to the score, a decimal digit at a time (the
 * low digit of the remainder of division by 10 to the low digit of the first word, the next to its high digit, then the second
 * word and so on, four words of two digits) and flag_2d0 says the score is to be drawn again.  The original begins with the carry
 * (X flag) the caller left: the pick-up at $4654 comes from `asr.w #3,d0`, so it is the last bit shifted out */
void score_add_carry(int16_t points, int carry_in)
{
    int carry = carry_in;

    for (int i = 0; i < 8; i++) {
        int digit = (points % 10) << ((i & 1) ? 4 : 0);   /* the odd digits are the high digits of a byte */

        points = points / 10;
        for (int word = i >> 1; word < 4; word++) {
            uint8_t sum = bcd_add((uint8_t)score_words[word], word == (i >> 1) ? (uint8_t)digit : 0, &carry);

            score_words[word] = (uint16_t)((score_words[word] & 0xff00) | sum);
        }
        flag_2d0 = 1;
        if (points == 0)
            return;
    }
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
    int16_t *record = records_721e;
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
    flag_721c = 1;
}

/* the end of $007F88 ($008058): the first free of records_19b14 gets x, y, kind less 11 and 1 and its byte in flags_39fde is set.
 * Kinds below 11 (the original goes on at $8088) did not run.  The original's result is in a5 and d5 and the condition codes;
 * nothing uses it */
static void drop_record(int16_t x, int16_t y, int16_t kind)
{
    struct Slot8 *record = find_free_record_19b14();

    if (record == 0)
        return;
    record->w[0] = x;
    record->w[1] = y;
    if (kind < 11)
        return;   /* goes on at $8088 */
    record->w[2] = kind - 11;
    record->w[3] = 1;
    flags_39fde[record - records_19b14] = 0xff;
}

/* orig $007F88 (with $008058 inlined: drop_record): a thing is dropped at (x, y) with kind `kind` (not 61): when none falls yet
 * it starts to fall (fall_kind); when one falls already a record is made at once */
void drop_thing(int16_t x, int16_t y, int16_t kind)
{
    if (kind == 61)
        return;   /* the original goes to $7F80, which did not run */
    if (fall_kind >= 0) {
        drop_record(x, y, kind);
        return;
    }
    fall_x = x;
    fall_y = y;
    fall_speed = -6;
    fall_kind = kind;
}

/* orig $007FAA: the thing that falls (fall_kind not negative) goes down by fall_speed and is drawn (the glyph of the kind: kinds from
 * 11 on are in table_14cf8, the others are drawn as the glyph kind + 302; the sprites 279 and 280 eight pixels higher); fall_speed
 * goes up by one and at 4 the fall is over: the thing becomes a record (drop_record) */
void update_falling_thing(void)
{
    if (fall_kind < 0)
        return;

    int16_t x = fall_x;
    int16_t y = fall_y + fall_speed;
    int16_t kind = fall_kind;
    int16_t glyph;

    if (kind < 11) {
        glyph = kind + 302;
        fall_y = y;
        glyph_draw(x - scroll_x, y - scroll_y, glyph);
    } else {
        glyph = (int16_t)table_14cf8[kind - 11].w[0];
        fall_y = y;
        if (glyph == 279 || glyph == 280)
            sprite_draw(x - scroll_x, y - scroll_y - 8, glyph);
        else
            glyph_draw(x - scroll_x, y - scroll_y, glyph);
    }
    fall_speed++;
    if (fall_speed < 4)
        return;
    x = fall_x;
    y = fall_y;
    kind = fall_kind;
    fall_kind = -1;
    drop_record(x, y, kind);
}

/* orig $005D76: the picture of the energy (image 126 of the table: 24 rows, two words wide) is drawn at the foot of the screen
 * in two parts: the first rows come from the beginning of the image, the others from further on by the picture (word_2de, 4
 * less it times 384 bytes) the bar is cycled; the number of rows of the first part falls as the energy goes up.  word_2de goes
 * on every other frame (frame_phase 0) */
void draw_energy_bar(void)
{
    if (frame_phase == 0)
        word_2de = (word_2de + 1) & 3;

    int16_t back = (int16_t)((4 - word_2de) * 384);
    const uint8_t *src = (const uint8_t *)image_table[126].data;
    uint8_t *dst = screen_buffer + 7042 + ((x_offset & -16) >> 3);
    int16_t shift = x_offset & 15;
    int16_t energy = energy_shown;

    if (energy < 0)
        energy = 0;

    int16_t rows = (int16_t)(((24 - energy) >> 1) + 6);

    blit_image_wide(dst, src, 96, shift, rows);
    blit_image_wide(dst + 44 * rows, src + 4 * rows - back, 96, shift, 23 - rows);
}

/* orig $007BA8: the walker's step along the ground (the movement routine at +22 of the type record at $199AC, `type`; the object has
 * x, y, the step 0 to 7 (8: just turned) at +4 and the direction 0 or 1 at +10).  x is the object's x, y its y.  Every other
 * frame (frame_toggle) the step goes on and the walker moves 4 pixels in its direction; at a cell border (a multiple of 32) the map
 * is looked at: ground ahead or none below turns it round.  Results: the new x (*x_io) and the picture (the return value: word_7c50,
 * the first picture, plus the step, plus the direction times 9).  The original sets word_7c50 itself.  Where it goes on at $7CB0
 * (type byte 18 set and a wall ahead walking left) did not run */
static int16_t walk_pace(const uint8_t *type, int16_t *obj, int16_t *x_io, int16_t y)
{
    int16_t x = *x_io;
    int16_t step;

    word_7c50 = 496;
    step = obj[2];
    if (step > 7) {
        obj[2]--;
        step = 8;
        goto done;
    }
    if (frame_toggle == 0)
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
        const uint8_t *player_cell = ptr_0b86 + 8192 + ((int16_t)(player_x & -32) >> 5) +
                                     (int16_t)(((player_y & -16) + 16) << 3);

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
    return (int16_t)(step + obj[5] + (obj[5] << 3) + word_7c50);
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

/* orig $00836C: ends at once when flag_8890 is negative; otherwise the original goes on at $8374, which did not run */
void step_836c(void)
{
    if (flag_8890 < 0)
        return;
}

/* orig $008452: the glyph word_8472 (if it is not negative) is drawn at (word_846e, word_8470) */
void draw_sprite_8472(void)
{
    if (word_8472 < 0)
        return;
    sprite_draw(word_846e, word_8470, word_8472);
}

/* orig $008742: when flag_8890 is negative nothing is done; otherwise the original goes on at $874C (which looks at the three
 * records of slot_ref_* and sets word_888e), which did not run */
void update_choice_888e(void)
{
    if (flag_8890 < 0)
        return;
}

/* orig $00D898: nothing is done when word_10ae is 0; otherwise the original goes on at $D89E, which did not run */
void step_d898(void)
{
    if (word_10ae == 0)
        return;
}

/* orig $00E486: the first of five records of 24 bytes at $18741 is only looked at while bit 1 of its first byte is set (the
 * original then goes on at $E490, which did not run) */
void unit0_update(void)
{
    if (!(unit_flags_18741[0] & 2))
        return;
}

/* orig $00E76C: the same for the second record */
void unit1_update(void)
{
    if (!(unit_flags_18741[24] & 2))
        return;
}

/* orig $00EA52: the same for the third record */
void unit2_update(void)
{
    if (!(unit_flags_18741[48] & 2))
        return;
}

/* orig $00ED38: the same for the fourth record */
void unit3_update(void)
{
    if (!(unit_flags_18741[72] & 2))
        return;
}

/* orig $00F01E: the same for the fifth record */
void unit4_update(void)
{
    if (!(unit_flags_18741[96] & 2))
        return;
}

/* orig $016AEE: nothing is done unless flag_aa8 is 1; the original then goes on at $16AF6, which did not run */
void step_16aee(void)
{
    if (flag_aa8 != 1)
        return;
}

/* orig $01843E: nothing is done when word_186be is 0; the original goes on at $18446, which did not run */
void step_1843e(void)
{
    if (word_186be == 0)
        return;
}

/* orig $0190F2: nothing is done when word_354 is negative; the original goes on at $190F8, which did not run */
void step_190f2(void)
{
    if (word_354 < 0)
        return;
}

/* orig $00D270: a hit at (x, y) of the given strength: when the player is alive and the point is inside the box around the player
 * (widened by half of word_39c and word_39e) the energy goes down by strength/32 + 5 (unless flag_aa8), and the result is -1
 * unless flag_23a (the strength is the result otherwise); the sound 4 is played when the routine at $3436 is only an rts */
int16_t damage_player_at(int16_t x, int16_t y, int16_t strength)
{
    if (energy_shown < 0)
        return strength;

    int16_t left = player_x + 10, right = player_x + 22;
    int16_t top = player_y + 8, bottom = player_y + 38;

    if (word_19c >= 18 && word_19c <= 19)
        top += 20;
    int16_t half = word_39c >> 1;

    x += half;
    left -= half;
    right += half;
    half = word_39e >> 1;
    y += half;
    top -= half;
    bottom += half;
    if (x < left || x > right || y < top || y > bottom)
        return strength;

    int16_t loss = ((int16_t)(strength + 1) >> 5) + 5;

    if (flag_aa8 == 0)
        player_energy -= loss;
    if (flag_23a == 0)
        strength = -1;
    if (word_3436 == 0x4E75)
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
 * 3 in word +2 becomes 0; word_214 is -1.  Ends there when flag_8890 is negative and none of the three slot_refs is 8;
 * otherwise the original goes on at $FB94, which did not run */
void reset_object_slot(int16_t index, int16_t other)
{
    uint16_t *r = (uint16_t *)&object_slots[index];

    (void)other;   /* the original also forms an address from it (in a0) that it throws away */
    r[2] = 0;
    r[4] = r[3];
    if (r[1] == 3)
        r[1] = 0;
    word_214 = -1;
    if (flag_8890 >= 0 || slot_ref_6b6 == 8 || slot_ref_748 == 8 || slot_ref_7da == 8)
        return;   /* goes on at $FB94 */
}

/* orig $018D14: the four records of records_316 that are in use (the first word not negative) are drawn as the glyph 108 plus
 * the state, at their position less the scroll, and the state counts up; at 7 the record is free again */
void draw_marks_316(void)
{
    for (int i = 0; i < 4; i++) {
        int16_t *r = &records_316[i * 3];

        if (r[0] < 0)
            continue;
        sprite_draw(r[1] - scroll_x, r[2] - scroll_y, r[0] + 108);
        r[0]++;
        if (r[0] >= 7)
            r[0] = -1;
    }
}

/* orig $017972: one tile of the background: the byte `*map` names the tile (256 bytes of ptr_0c70 each), and the blits of
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
    if (word_037c > 2) {   /* the background is kept: it is copied to the screen */
        copy_screen(background, screen_buffer);
        return;
    }
    x_offset = scroll_x & 31;
    blitter_setup_copy();

    const uint8_t *map = ptr_0b86 + ((int16_t)(scroll_x & -16) >> 5) + (int16_t)((scroll_y & -16) << 3);
    uint8_t *dst = (uint8_t *)screen_buffer;
    int16_t y = 0;
    int16_t into = scroll_y & 15;

    y -= into;
    dst -= into * 4 + into * 8 + into * 32;
    for (;;) {
        const uint8_t *row_map = map;
        uint8_t *row_dst = dst;

        for (int16_t x = 0; x < 352; x += 32) {
            map = draw_background_tile(map, ptr_0c70, dst, y, 64, 0x2100);
            dst += 4;
        }
        map = row_map + 128;
        dst = row_dst + 704;
        y += 16;
        if ((int16_t)(y + scroll_y) >= 1024)
            break;
        if (y >= 192)
            break;
    }
    if (word_037c != 2)
        return;
    background = memory_base + 256;   /* the screen drawn is kept to be copied the next times */
    copy_screen(screen_buffer, background);
    word_037c = 4;
}

/* orig $018072: the entry a2 is looked at (word_185bc gets its byte at +5, plus one); when the record a1 has its byte at +5
 * negative that is all; otherwise it goes on along its line (path_continue, $01808E) */
void step_18072(struct PathRecord *a1, const struct PathDescription *a2)
{
    word_185bc = (int16_t)a2->speed + 1;
    if (a1->waypoint < 0)
        return;
    path_continue(a1, a2);
}

/* orig $017FA0: the picture of a record (a2: byte +4 is the number of a 256-byte block of the background, +26 and +27 the size
 * in 32 pixel columns and 16 line rows, less one) is made an image at the end of the image table and drawn with sprite_draw at
 * (x, y), less the scroll, in columns and rows */
void draw_record_image(int16_t x, int16_t y, const struct PathDescription *a2)
{
    struct Image *image = (struct Image *)asset_table_end;
    int16_t columns;
    int16_t rows = a2->rows;

    image->height = 15;
    image->data = (const uint16_t *)(background + ((int32_t)a2->block << 8));
    uint16_t index = (uint16_t)((int16_t)((uint32_t)asset_table_end - (uint32_t)image_table) >> 3);

    x -= scroll_x;
    y -= scroll_y;
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
    uint8_t *cell = ptr_0b86 + 8192 + (col >> 5) + (int16_t)(row << 3);
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

/* orig $018890: the record `a0` (its word +2 numbers an entry of the table ptr_0d0e, 16 bytes each) is taken out of use (its
 * first word -1) and a free one of the ten records of 36 bytes of table_199ac is filled from the entry (a state 2, the entry's
 * words and bytes and then 14 bytes copied from the table ptr_0b82 + 2, 16 bytes for each of the entry's word +10); when none is
 * free that is all */
void take_record_to_199ac(struct EntryRef *a0)
{
    a0->state = 0xFFFF;
    const struct CreatureEntry *entry = &ptr_0d0e[a0->number];
    struct CreatureType *type = (struct CreatureType *)table_199ac;
    int i;

    for (i = 0; i < 10; i++, type++)
        if (type->status == 0)
            break;
    if (i == 10)
        return;
    const struct CreatureTemplate *template = (const struct CreatureTemplate *)ptr_0b82 + entry->template_index;

    type->status = 2;
    type->start_x = entry->start_x;
    type->start_y = entry->start_y;
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


/* orig $018900: like take_record_to_199ac, but the entry's record (its word +2 numbers 8 bytes of ptr_0cb2) is made into the first
 * free one of the five records of 24 bytes of records_18740 (a state 0): state 2, the 8 bytes (the byte +6 split into two
 * nibbles), and 14 bytes copied from the table ptr_0b7e + 2 (16 bytes for each of the signed byte +5); when none is free that is
 * all */
void take_record_to_18740(struct EntryRef *a0)
{
    a0->state = 0xFFFF;

    const uint8_t *s = ptr_0cb2 + (int16_t)(a0->number << 3);
    uint8_t *slot = records_18740;
    int i;

    for (i = 0; i < 5; i++, slot += 24)
        if (*(uint16_t *)slot == 0)
            break;
    if (i == 5)
        return;
    uint8_t *out = slot;
    *(uint16_t *)out = 2;
    out[2] = s[0];
    out[3] = s[1];
    out[4] = s[2];
    out[5] = s[3];
    out[6] = s[4];
    out[7] = s[6] >> 4;
    out[8] = s[6] & 15;
    const uint8_t *src = ptr_0b7e + 2 + (int16_t)((int8_t)s[5] << 4);

    out[9] = src[1];
    for (int k = 0; k < 12; k++)
        out[10 + k] = src[2 + k];
    out[22] = s[7];
}

/* orig $01FA8C: the sound `id` is stopped when a voice plays it: the voice whose entry in voice_ids_207a6 equals the byte +1 of the
 * sound's record in sound_table_20bce.  When none does that is all; when one does its entry is cleared and $1FE3E runs
 * (d0 = 22, d1 = the voice) */
void stop_sound(uint16_t id)
{
    uint8_t wanted = sound_table_20bce[(uint16_t)(id * 6) + 1];

    for (int i = 0; i < 4; i++)
        if (voice_ids_207a6[i] == wanted) {
            voice_ids_207a6[i] = 0;
            voice_start(22, (uint16_t)i);
            return;
        }
}

/* orig $016A08: the cells of the map at line_text + 100 (48 columns of 8 pixels, rows of 8) under the point (x, y) are marked with the
 * low byte of *flag when the first of them is in the map (from line_text + 48 to + 1500): two cells side by side, or when bit 2 of y is set
 * the two cells a row down and the two a row further.  Used where a shot did not end in a tile */
void mark_shot_cells(int16_t x, int16_t y, const int16_t *flag)
{
    const uint8_t *map = line_text;
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
    int16_t col = (int16_t)(x + scroll_x + 8) & -32;
    int16_t row = (int16_t)(y + scroll_y + 8) & -16;
    const uint8_t *cell = ptr_0b86 + 8192 + (col >> 5) + (int16_t)(row << 3);

    if (*cell != 1) {
        mark_shot_cells(x, y, flag);
        return;
    }
    play_sound(3, 4);
    if (cell[-8192] == 109 && (int16_t)param_index >= 3)
        return;   /* calls $17B98 here and goes on */
    int16_t *debris = records_3a0a6;
    int i;

    for (i = 0; i < 20; i++, debris += 6)
        if (debris[0] < 0)
            break;
    if (i < 20) {
        debris[0] = 0;
        debris[1] = x + scroll_x;
        debris[2] = y + scroll_y;
        debris[3] = 0;
        debris[4] = 0;
        debris[5] = 58;
        flag_2a8++;
    }
    *flag = 0;
}

/* orig $017F46: a record that moved is looked at for being near the screen: on it (the screen's terms: -32 to 320 across and -16 to 192
 * down) or, when it is to the left of it or above, with its image's size (a2: the columns +26 and the rows +27) reaching it: bit 7 of
 * flags_3b0 is set */
static void record_seen(const struct PathRecord *a1, const struct PathDescription *a2)
{
    int16_t sx = (int16_t)(a1->x - scroll_x);
    int16_t sy = (int16_t)(a1->y - scroll_y);

    if (sx > -32) {
        if (sx > 320)
            return;
        if (sy > -16) {
            if (sy <= 192)
                flags_3b0 |= 0x80;
            return;
        }
    }
    int16_t wide = (int16_t)((a2->rows << 5) + sx);
    int16_t high = (int16_t)(((a2->columns & 7) << 4) + sy);

    if (wide > -32 && high > -16)
        flags_3b0 |= 0x80;
}

/* orig $017E9C (with $017E9E and $017F46): the pictures of the 25 records of ptr_0c9e (24 bytes) and ptr_0c9a (28 bytes) are redrawn: the cells marked
 * last frame are given back (entry_table), and each record that is not turned off (byte +4 not negative) is looked at
 * ($18072), has its cells marked in the tile map (entry_table) and its image drawn; one that moved (its first long changed) is
 * looked at once more for being on the screen or within 32 by 16 pixels of it (bit 7 of flags_3b0, $17F46).  The sound 52 is played
 * (priority 8) at the end when bit 7 of flags_3b0 got set, otherwise stopped.  The routine patches itself into an rts when word_4a2 is
 * 0 (the first word of it is then $4E75) */
void redraw_record_images(void)
{
    if (word_17e9c == 0x4E75)
        return;
    if (word_4a2 == 0) {
        word_17e9c = 0x4E75;
        stop_sound(52);
        return;
    }
    flags_3b0 &= 0x7F;
    restore_saved_bytes((struct SavedByte *)entry_table, 49);

    struct SavedByte *list = (struct SavedByte *)entry_table;
    struct PathRecord *a1 = ptr_0c9e;
    const struct PathDescription *a2 = ptr_0c9a;
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
    if (flags_3b0 & 0x80)
        play_sound(52, 8);
    else
        stop_sound(52);
}

/* orig $015C54: up to eight particles of a burst at (x, y) take the free records (first word negative) of records_86c, each with
 * the next speeds of particle_speeds_15b5c; the sound 34 (priority 45) is played for each.  Nothing is done while the burst
 * flag_a4c is set and word_a52 is 190 or more.  The first one started sets flag_a4c to -1 (when it was not negative), word_a52 to 0 and
 * word_a54 to -10 */
static void spawn_particles_15c54(int16_t x, int16_t y)
{
    if (flag_a4c != 0 && word_a52 >= 0xBE)
        return;

    int16_t *record = records_86c;
    const uint16_t *speeds = particle_speeds_15b5c;
    int started = 0;

    for (int n = 0; n < 80; n++, record += 3) {
        if (record[0] >= 0)
            continue;
        play_sound(0x22, 0x2D);
        record[0] = x;
        record[1] = y;
        record[2] = (int16_t)*speeds++;
        if (flag_a4c >= 0) {
            word_a52 = 0;
            flag_a4c = -1;
            word_a54 = -10;
        }
        if (++started == 8)
            return;
    }
}

/* orig $008822: the end of what flag_8890 counts: when it is negative a burst of particles is started at the place
 * (word_8882 + 4, word_8884 + 4) unless word_0248 is set.  Then flag_8890, word_888e and word_8472 become -1, and word_214
 * becomes -1 unless one of the three slots holds the object 8 */
void end_flag_8890(void)
{
    if (flag_8890 < 0 && word_0248 == 0)
        spawn_particles_15c54((int16_t)(word_8882 + 4), (int16_t)(word_8884 + 4));
    flag_8890 = -1;
    word_888e = -1;
    word_8472 = -1;
    if (slot_ref_6b6 != 8 && slot_ref_748 != 8 && slot_ref_7da != 8)
        word_214 = -1;
}

/* orig $0159FC (inlined): what the map cell before `after` (a byte not 0) does when a hit reaches it.  A positive byte names a shot
 * (1 to 9 of the first list of shots, 10 to 18 of the second, then the third): the damage word_1cc is the last_value of the
 * list's object (plus one eighth of word_272 - word_274 when that is positive), the sound 3 is played when flag_23a, and the shot
 * is cleared when the object's byte +73 is set; returns 0.  The byte -1 does word_8892 and returns 1; -2 does 10000 (0 with
 * flag_23a), -3 does 0, and a byte below it does 10000 and (as the original goes on at $15AC2) makes a burst of particles at the
 * cell (or sets the damage to 50 with flag_23a); these return 0 */
static int16_t cell_effect_159fc(const uint8_t *after)
{
    int16_t type = (int8_t)after[-1];

    if (type < 0) {
        if (type == -1) {
            hit_damage_1cc = word_8892;
            return 1;
        }
        if (type == -2) {
            hit_damage_1cc = flag_23a != 0 ? 0 : 10000;
            return 0;
        }
        if (type == -3) {
            hit_damage_1cc = 0;
            return 0;
        }
        hit_damage_1cc = 10000;

        int32_t offset = (int32_t)(after - line_text) - 100;
        int16_t row = 0;
        int16_t column;

        while (offset >= 48) {   /* divs.w #48 (no division in the C runtime) */
            offset -= 48;
            row++;
        }
        column = (int16_t)offset;
        int16_t x = (int16_t)((column << 3) + scroll_x);
        int16_t y = (int16_t)((row << 3) + scroll_y);

        if (flag_23a != 0)
            hit_damage_1cc = 50;
        else
            spawn_particles_15c54(x, y);
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

    struct ShotList *list = &((struct ShotList *)shot_lists_raw)[list_number];
    const struct Object *object = &((const struct Object *)object_slots)[list->object];

    hit_damage_1cc = object->last_value;

    int16_t more = (int16_t)(word_272 - word_274);

    if (more > 0)
        hit_damage_1cc = (int16_t)(hit_damage_1cc + (more >> 3));
    if (flag_23a != 0)
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
        int16_t *record = &records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x;
            record[2] = y;
            record[3] = 0;
            record[4] = 0;
            record[5] = 58;
            flag_2a8++;
            return;
        }
    }
}

/* the part of check_hit_and_walls at $D130: the cell before `after` is not 0, so its effect (cell_effect_159fc) is looked at.  The
 * type -1 takes the damage off flag_8890 (end_flag_8890 when it goes negative).  With word_284 the damage is 1, less its
 * eighth, quarter ... by word_284 (a shift 3 - word_284), otherwise the cell's damage; when nothing is left of the strength the
 * player is hit (the sounds 4/44) and -1 comes back.  Otherwise word_3a4 is the strength left; with damage left the debris
 * flies off near the hit at (x, y) (levels coordinates) and word_3a4 comes back, without damage the strength as it was */
static int16_t hit_marked_cell(const uint8_t *after, int16_t x, int16_t y, int16_t strength)
{
    int16_t flag = cell_effect_159fc(after);
    int16_t damage = hit_damage_1cc;
    int16_t left;

    if (flag != 0) {
        flag_8890 = (int16_t)(flag_8890 - strength);
        if (flag_8890 < 0)
            end_flag_8890();
    }
    if (word_284 != 0) {
        /* the original loads the long $4B79C278 and does moveq #1 when it is not 0, which it never is */
        uint16_t one = 1;
        int16_t shift = (int16_t)(3 - word_284);
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
        if (word_3a6 == 0) {
            play_sound(4, 15);
        } else {
            play_sound(44, 15);
            play_sound(4, 14);
        }
        return -1;
    }
    word_3a4 = left;
    if (damage == 0)
        return strength;

    int16_t half_width = (int16_t)(word_3a0 >> 1);
    int16_t spread = (int16_t)(half_width & 0xFFF0);
    int16_t random;

    x = (int16_t)(x + half_width);
    spread = (int16_t)((spread != 0 ? spread : 16) - 1);
    random = (int16_t)random_word();
    x = (int16_t)(random < 0 ? x - (random & spread) : x + (random & spread));

    int16_t half_height = (int16_t)(word_3a2 >> 1);

    spread = (int16_t)(half_height & 0xFFF0);
    y = (int16_t)(y + half_height);
    spread = (int16_t)((spread != 0 ? spread : 16) - 1);
    random = (int16_t)random_word();
    y = (int16_t)(random < 0 ? y - (random & spread) : y + (random & spread));
    start_debris((int16_t)(x - 8), (int16_t)(y - 8));
    return word_3a4;
}

/* orig $00D00A and $00D114 (inside the unrolled loop) and the continuation at $D130: a hit at (x, y) of the given strength.
 * damage_player_at first (the result -1 means the player was hit: the sound 4 is played, or 44 when word_3a6 is not 0, and
 * that is all).  Otherwise word_3a0 and word_3a2 keep the size of the box (word_39c, word_39e) and the box is cut to the
 * screen's cells (8 pixels) in x and y; when nothing is left that is all.  The cells of the map (line_text + 100, 48 columns)
 * that the box covers, less a column, are looked at; the first cell that is not 0 is dealt with by hit_marked_cell and ends the
 * routine.  Returns the strength (or -1, or the strength left after a hit on a cell) */
int16_t check_hit_and_walls(int16_t x, int16_t y, int16_t strength)
{
    int16_t hit_x = x;
    int16_t hit_y = y;

    strength = damage_player_at(x, y, strength);
    if (strength < 0) {
        play_sound(word_3a6 == 0 ? 4 : 44, 15);
        return strength;
    }
    word_3a0 = word_39c;
    word_3a2 = word_39e;
    x = (int16_t)((x - scroll_x + 4) & -8);
    y = (int16_t)((y - scroll_y + 4) & -8);
    if (y > -16) {
        int16_t over = word_39e + y;

        if (over >= 208) {
            over -= 208;
            word_39e -= over;
            if (word_39e <= 0)
                return strength;
        }
    } else {
        y += 16;
        word_39e += y;
        if (word_39e <= 0)
            return strength;
        y = -16;
    }
    if (x > -16) {
        int16_t over = word_39c + x;

        if (x >= 336) {
            over -= 336;
            word_39c -= over;
            if (word_39c <= 0)
                return strength;
        }
    } else {
        x += 16;
        word_39c += x;
        if (word_39c <= 0)
            return strength;
        x = -16;
    }
    if (word_214 >= 0) {
        int16_t *list = (int16_t *)value_c6c;

        list[0] = x;
        list[1] = y;
        list[2] = strength;
        value_c6c += 6;
        flag_20e++;
    }

    const uint8_t *cell = line_text + 100 + (x >> 3);

    y &= -8;
    cell += y * 6;
    int16_t rows = (word_39e >> 3) - 1;
    int16_t columns = (word_39c >> 3) - 1;

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
        if (item_slots[i] == item) {
            item_slots[i] = -1;
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
        uint16_t state = table_14cf8[item].w[2];

        if (state != 2 && state != 0)
            continue;
        for (int i = 0; i < 4; i++)
            if (item_slots[i] == item)
                item_slots[i] = -1;
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
 * number in the record's word +18 and the state 1; its byte in flags_39fde is set.  For the numbers 64 to 68 that is all; 83 sets
 * bit 2 of flags_3b0; the position and number are kept in words_34e (word_354 cleared) and the sound 48 is played (priority 30)
 * unless offsets_4cba says 4 for the number */
static void kind_drop_item(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)a1->x, y = (int16_t)a1->y, number = (int16_t)a1->effect_argument;
    struct Slot8 *record = first_free_slot8(records_19b14, 200);

    if (record == 0)
        return;
    record->w[0] = x;
    record->w[1] = y;
    record->w[2] = number;
    record->w[3] = 1;
    flags_39fde[record - records_19b14] = 0xFF;
    if (number >= 0x40 && number <= 0x44)
        return;
    if (number == 0x53)
        flags_3b0 |= 4;
    words_34e[0] = x;
    words_34e[1] = y;
    words_34e[2] = number;
    word_354 = 0;
    if (offsets_4cba[number] != 4)
        play_sound(0x30, 0x1E);
}

/* orig $018EB6 (kind 1): as kind 0 but the number is the word +18 plus 192 and no sound is played */
static void kind_drop_192(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)a1->x, y = (int16_t)a1->y, number = (int16_t)(a1->effect_argument + 0xC0);
    struct Slot8 *record = first_free_slot8(records_19b14, 200);

    if (record == 0)
        return;
    record->w[0] = x;
    record->w[1] = y;
    record->w[2] = number;
    record->w[3] = 1;
    flags_39fde[record - records_19b14] = 0xFF;
    words_34e[0] = x;
    words_34e[1] = y;
    words_34e[2] = number;
    word_354 = 0;
}

/* orig $019280: whether one of the first ten records of records_19334 is at (x, y) already */
static int record_19334_at(int16_t x, int16_t y)
{
    for (int i = 0; i < 10; i++)
        if (records_19334[i].w[0] == x && records_19334[i].w[1] == y)
            return 1;
    return 0;
}

/* orig $018D64 (kind 4: looks in the first ten records of records_19334 for a free one) and $018DE4 (kind 3: in all twenty, and
 * then plays the sound 7, priority 5): a free record gets the position of the record a1 (word +12 less 16 and rounded down to 32,
 * word +14 plus 8 rounded down to 16), the state 3 (kind 4) or 1 (kind 3) and the number of the word +18.  When a record is at the
 * position already ($019280) nothing is done */
static void kind_place_19334(const struct ScriptRecord *a1, int twenty)
{
    struct Slot8 *record = first_free_slot8(records_19334, twenty ? 20 : 10);

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
 * further and rounded to 16) is set to six tiles counted up from tile_24d and the three cells of the second plane (+8192)
 * cleared; the sound 7 is played (priority 5) */
static void kind_set_tiles(const struct ScriptRecord *a1)
{
    int16_t x = (int16_t)(((a1->x & 0xFFE0) + 12) & 0xFFE0);
    int16_t y = (int16_t)((a1->y + 8) & 0xFFF0);
    uint8_t *cell = ptr_0b86 + (int16_t)(x >> 5) + (int16_t)(y << 3);
    uint8_t base = tile_24d;

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

/* orig $018C30 (kind 6): every record of records_19b14 within 12 of the record a1's position (words +12, +14) whose number's
 * table_14cf8 entry has the word +4 at 4 is taken out (word +4 -1, +6 0) and makes a mark in records_316 (state 0, x less 8, y
 * (8 less for the number 96)), at most two, each with the sound 4 (priority 15); the marks go to the first free entry and on
 * from there (this may pass the end of the four, as in the original) */
static void kind_burst(const struct ScriptRecord *a1)
{
    int16_t x0 = (int16_t)(a1->x - 12), y0 = (int16_t)(a1->y - 12);
    int16_t x1 = (int16_t)(a1->x + 12), y1 = (int16_t)(a1->y + 12);
    int16_t *mark = records_316;
    int16_t hits = 0;

    word_314 = 0;
    for (int i = 0; i < 4 && mark[0] >= 0; i++)
        mark += 3;
    if (mark >= records_316 + 12)
        mark = records_316 + 9;
    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];

        if (record->w[2] < 0)
            continue;
        if (table_14cf8[record->w[2]].w[2] != 4)
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
        word_314 = ++hits;
        if (hits == 2)
            return;
    }
}

/* orig $018F16 (kind 10): the slot_ref (6b6, 748, 7da) that equals the word +18 of the record a1 becomes -1; the first only */
static void kind_free_slot_ref(const struct ScriptRecord *a1)
{
    int16_t number = (int16_t)a1->effect_argument;

    if (slot_ref_6b6 == number)
        slot_ref_6b6 = -1;
    else if (slot_ref_748 == number)
        slot_ref_748 = -1;
    else if (slot_ref_7da == number)
        slot_ref_7da = -1;
}

/* orig $01895A: the record a0 (its word +2 numbers a record of 24 bytes of buffer_cb6) is looked at by call_by_kind (kind and
 * argument: its three pairs of words; the result of each into words_300); when all three results (words_300) are still -1 the
 * record a0 is finished: its first word becomes -1, a message of the record's word +20 is requested when it has one (priority
 * 50), the slots its items are in are emptied (empty_slots_of_record, empty_slots_of_flagged_record) and the routine its kind
 * (word +16 without the top bit) names in dispatch_18ba4 does its part (written in here: 0 to 6, 10 to 15; 11 to 13 and 15 do
 * nothing, 14 clears tick_count; 7, 8 and 9 ($19618, $19658, $193D4) did not run and are left out).  Nothing is done while
 * flag_230 */
void finish_record_1895a(struct EntryRef *a0)
{
    if (flag_230)
        return;

    const struct ScriptRecord *a1 = &buffer_cb6[a0->number];

    *(int32_t *)words_300 = -1;
    words_300[2] = -1;
    call_by_kind(a1->condition[0].kind, a1->condition[0].argument, &words_300[0]);
    call_by_kind(a1->condition[1].kind, a1->condition[1].argument, &words_300[1]);
    call_by_kind(a1->condition[2].kind, a1->condition[2].argument, &words_300[2]);
    if ((int16_t)(words_300[0] & words_300[1] & words_300[2]) >= 0)
        return;
    a0->state = 0xFFFF;

    const int16_t *offset = (const int16_t *)(ptr_0cba + (uint16_t)(a1->message * 2));

    if (*offset >= 0) {
        message_next = ptr_0b66 + *offset;
        if (*message_next != 0)
            message_request(50);
    }
    empty_slots_of_record(a1);
    empty_slots_of_flagged_record(a1);
    switch (a1->effect & 0x7FFF) {
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
    case 14:
        tick_count = 0;
        break;
    default:
        break;   /* 7, 8 and 9 go to $19618, $19658 and $193D4, which did not run */
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
 * the count in the first word of its object_slots record allows: the entry is kept in entry_b92 and its number in entry_id_2ea.
 * The first unit sets word_2f0 when its object has the count 1 and the others then leave it alone.  Returns 0 when some
 * unit got an entry, else 1 (also 1 and nothing done when fire_latch is not 0; fire_latch is set) */
uint16_t claim_free_entries(void)
{
    int16_t *refs[3] = { &slot_ref_6b6, &slot_ref_748, &slot_ref_7da };
    int16_t first_id[3] = { 1, 10, 19 };
    uint16_t result = 1;

    if (fire_latch != 0)
        return 1;
    fire_latch = 1;
    word_2f0 = 0;
    entry_b92[0] = 0;
    entry_b92[1] = 0;
    entry_b92[2] = 0;
    for (int u = 0; u < 3; u++) {
        if (*refs[u] < 0)
            continue;
        int16_t count = ((uint16_t *)&object_slots[*refs[u]])[0];
        int16_t number = first_id[u];

        if (u > 0 && count < 0)
            continue;
        uint16_t *entry = first_free_entry((uint16_t *)refs[u] + 1, count, &number);

        if (entry == 0)
            continue;
        if (u > 0 && count == 1 && word_2f0 != 0)
            continue;
        entry_b92[u] = entry;
        entry_id_2ea[u] = number;
        result = 0;
        if (u == 0 && count == 1)
            word_2f0 = 1;
    }
    return result;
}


/* ======== from game_c.c ======== */
/* Gods, the game: the routines at $10000-$167FF (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */

#define OBJECT(n) (&((struct Object *)object_slots)[(n)])
#define shot_lists ((struct ShotList *)shot_lists_raw)

/* orig $0112F6: where in the level's second map (8192 bytes after ptr_0b86; a tile is 32 by 16 pixels, a row of them 128 bytes) the
 * tile at the player's place is */
uint8_t *player_tile_ptr(void)
{
    int16_t x = player_x & -32;
    int16_t y = player_y & -16;
    uint8_t *tile = ptr_0b86 + 8192;

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
    if ((player_x & 31) >= 8 && tile[385] == 1)
        return 1;
    return 0;
}

/* orig $01136C: the same for the row of the player's tile itself */
int16_t tile_one_at_player(void)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[0] == 1)
        return 1;
    if ((player_x & 31) >= 8 && tile[1] == 1)
        return 1;
    return 0;
}

/* orig $015CC4: goes on at $15CCC when flag_a4c is negative (that did not run) */
void fn_15cc4_c(void)
{
    if (flag_a4c < 0)
        return;   /* goes on at $15CCC */
}

/* orig $016AEE: goes on at $16AF6 when flag_aa8 is 1 (that did not run); $16AEC is a bare return */
void fn_16aee_c(void)
{
    if (flag_aa8 == 1)
        return;   /* goes on at $16AF6 */
}

/* orig $01290C: the sprite of index `index` is drawn where the player is, relative to the scroll kept while the loop runs */
void draw_player_sprite(uint16_t index)
{
    sprite_draw(player_x - scroll_x_a, player_y - scroll_y_a, index);
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

    int16_t tx = (int16_t)((x + scroll_x + 8) & -32);
    int16_t ty = (int16_t)((y + scroll_y + 8) & -16);
    uint8_t *tile = ptr_0b86 + 8192;

    tile += tx >> 5;
    tile += (int16_t)(ty << 3);
    if (*tile != 1) {
        mark_shot_cells(x, y, &shot->active);
        return;
    }
    play_sound(3, 4);
    if (tile[-8192] == 109 && param_index >= 3)
        return;   /* goes on at $169BE: calls $17B98 (did not run) */
    for (int i = 0; i < 20; i++) {
        int16_t *record = &records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x + scroll_x;
            record[2] = y + scroll_y;
            record[3] = 0;
            record[4] = 0;
            record[5] = 58;
            flag_2a8++;
            shot->active = 0;
            return;
        }
    }
    shot->active = 0;   /* the original's $16A04 (no free record) */
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

        shot_prev_x = x;
        shot_prev_y = y;
        x += (int16_t)shot_deltas[shot->direction * 2];
        y += (int16_t)shot_deltas[shot->direction * 2 + 1];
        shot->x = x;
        shot->y = y;
        x -= scroll_x;
        y -= scroll_y;
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
    default:
        break;   /* goes on at $15DB2, $1645C or $1661E */
    }
}

/* orig $015B04: the three lists of shots are run, list_index_a98 saying which */
void run_object_lists(void)
{
    for (int i = 0; i < 3; i++) {
        list_index_a98 = i;
        run_object_list(&shot_lists[i]);
    }
}

/* orig $01590E: the handler 1 of the table at $15220 (the object's fire_kind): the object's list gets its first shots at (x + 16, y - 12):
 * as many as the object's last_value less its first_value, plus one, at most three, the first with the direction 1, the next 0 and 2, and
 * the object's anim_mode is set to 1.  `slot` is where the first shot goes (they are `id`, `id` + 1 and `id` + 2 in the first word). */
static void spawn_shots_1590e(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y)
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

/* orig $015510: an object's list `list` gets a shot at (x, y): sound 2 (priority 4) and the routine the object's fire_kind names in the
 * table at $15220: 1 is $1590E (spawn_shots_1590e), 4 and 5 are bare returns; 0, 2, 3, 6 to 15 ($15968, $155EC, $1557C, $158F0 ...) did
 * not run */
static void spawn_shots(struct Shot *slot, int16_t id, struct ShotList *list, int16_t x, int16_t y)
{
    struct Object *object = OBJECT(list->object);

    play_sound(2, 4);
    switch (object->fire_kind) {
    case 1:
        spawn_shots_1590e(slot, id, object, x, y);
        break;
    default:
        break;   /* 4 and 5 return; the others ($15968, $155EC ...) did not run */
    }
}

/* orig $0153A2: each of the three lists that has a free place for a shot (shot_slot_b92, b96 and b9a, not 0) gets one, at the player (a
 * little below him) or, when word_888e is the list's number, at the place word_8882 and word_8884 give (8 and 16 on) */
void spawn_player_shots(void)
{
    struct Shot *slots[3] = { shot_slot_b92, shot_slot_b96, shot_slot_b9a };
    int16_t ids[3] = { word_2ea, word_2ec, word_2ee };

    for (int i = 0; i < 3; i++) {
        if (slots[i] == 0)
            continue;

        int16_t x = player_x;
        int16_t y = player_y + 24;

        if (word_888e == i) {
            x = word_8882 + 8;
            y = word_8884 + 16;
        }
        spawn_shots(slots[i], ids[i], &shot_lists[i], x, y);
    }
}


/* ======== from game_d.c ======== */
/* Gods, the game: the routines at $417C and $44D2 and what they call.  Each function says where the original has it (`orig $ADDR`). */

/* orig $0057FC: the score panel (image 121 of the table, 5 words wide and 16 rows, its planes 150 bytes apart) is blitted to the screen
 * buffer, 7420 bytes in and at the column x_offset says (in steps of 16 pixels, shifted by the low four bits).  When menu_state_25a is
 * 2 and flag_23a is not 0 the original goes on at $5CD2, which did not run */
void draw_score_panel(void)
{
    if (menu_state_25a == 2 && flag_23a != 0)
        return;   /* goes on at $5CD2 */

    uint8_t *dst = screen_buffer + 7420;
    const uint8_t *src = (const uint8_t *)image_table[121].data;
    uint16_t x = (uint16_t)x_offset;

    dst += (int16_t)(x & 240) >> 3;
    blit_image_5wide(dst, src, 150, x & 15, 15);
}

/* orig $007EAE: the record (a5: x at +0, y at +2, a word at +4 and the strength at +8) is checked against the walls at the point
 * (x + scroll_x, y + scroll_y) with check_hit_and_walls; the strength word is replaced by the result, and when that is negative the
 * word at +4 becomes -1.  Returns the result (the original's N flag is its sign) */
int16_t record_check_walls(int16_t x, int16_t y, int16_t *record)
{
    int16_t result = check_hit_and_walls((int16_t)(x + scroll_x), (int16_t)(y + scroll_y), record[4]);

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
    word_19a = frame;
    player_tile_scan();

    int16_t x = player_x - scroll_x;
    int16_t y = player_y - scroll_y;

    if (word_037c != 0)
        return;   /* goes on at $1265C */
    if (x <= (int16_t)frame_map_125ac[5]) {
        scroll_x -= 4;
        if (scroll_x < 0)
            scroll_x = 0;
    } else if (x >= 320 - (int16_t)frame_map_125ac[5] - 32) {
        scroll_x += 4;
        if (scroll_x >= 3776)
            scroll_x = 3776;
    }
    if (y > 112) {
        int16_t step = (int16_t)((y - 112) >> 1);

        if (step == 0)
            scroll_y++;
        scroll_y += step;
        if (scroll_y >= 832)
            scroll_y = 832;
    } else if (y < 32) {
        int16_t step = (int16_t)((32 - y) >> 1);

        if (step == 0)
            scroll_y--;
        scroll_y -= step;
        if (scroll_y < 0)
            scroll_y = 0;
    }

    int16_t sprite = (int16_t)(state_table[player_state * 2] + frame);

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
 * adds 32 for each point to flag_8890 (when that is not negative) ($0108F8) */
static void energy_pickup(int16_t amount, int16_t sound)
{
    if (energy_shown < 0)
        return;
    if (sound != 0)
        play_sound(sound, 55);
    player_energy += amount;
    if ((int16_t)player_energy > 24) {
        int16_t over = (int16_t)player_energy - 24;

        player_energy = 24;
        if (flag_8890 >= 0)
            flag_8890 += over << 5;
    }
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
        flag_268 = 1;            /* orig $01082A */
        return 1;
    case 11:
        pickup_player_end(counter);   /* orig $01263A */
        return 0;
    case 12:
        timer_306 = 0xaa;        /* orig $0106D6 */
        return 1;
    case 13:
        countdown_220 = 1000;    /* orig $010802 */
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
    if (energy_shown < 0)
        return 0;

    int16_t left = player_x - scroll_x, top = player_y - scroll_y;
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
        word_2b2 = id;
        pick_up_object(id);
        play_sound(6, 6);
        spawn_effect(x, y, id);
        return 0;
    }
    word_2b2 = id;

    const struct Rec10 *info = &table_14cf8[id];
    int16_t kind = (int16_t)info->w[2];

    if (kind > 1)
        return 1;
    if (kind == 1) {
        if (id >= 12 && id < 18)
            return 0;   /* goes on at $45DC (the keys), which did not run */
        play_sound(6, 6);
        record->w[2] = -1;
        record->w[3] = 0;
        item_message(id);
        int16_t points = (int16_t)info->w[3];

        score_total_1fc += points;
        score_add_carry(points >> 3, (points >> 2) & 1);   /* asr.w #3 leaves the last bit out in X */
        spawn_effect(x, y, word_2b2 + 170);
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
            message_arg = 0;
        item_message(id);
    }
    spawn_effect(x, y, word_2b2 + 170);
    return 0;
}

/* The player's energy goes down by 2 unless flag_aa8 (inlined: the tests of $49C2 and $4A50) */
static void hurt_player(void)
{
    if (flag_aa8 == 0)
        player_energy -= 2;
}

/* orig $004978: the thing 300 (a bat): counted when near the screen, its glyph by counter_262; it hurts the player when its box (x + 16,
 * y + 16) is inside the box 4 left of the player and 36 right, 8 above and 56 below; drawn as a sprite 2 words wide */
static void draw_thing_300(int16_t x, int16_t y)
{
    count_if_visible(x, y);

    uint16_t glyph = frames_4b08[counter_262];
    int16_t bx = x + 16, by = y + 16;
    int16_t left = player_x - scroll_x, top = player_y;
    int16_t right = left + 36;

    left -= 4;
    if (!(bx < left || bx > right)) {
        int16_t bottom = top + 56;

        top -= 8;
        if (!(by < top || by > bottom))
            hurt_player();
    }
    sprite_draw(x, y - scroll_y, glyph);
}

/* orig $0049F8: the thing 287 or 220 (a flying thing whose image is cut to a height that counts 0 to 15 and back with counter_204;
 * the image of the glyph is changed for it): counted, it hurts the player when its box is on him, drawn in two parts (the lower with
 * glyph_plane_size 64, from 32 bytes further in the image) and then its head, by counter_262 (287) or counter_20c (220) */
static void draw_thing_flyer(int16_t x, int16_t y, int16_t glyph)
{
    struct Image *image = &image_table[glyph];
    int16_t height = (counter_204 & 16) ? 15 - (counter_204 & 15) : (int16_t)counter_204;

    count_if_visible(x, y);
    image->height = (uint16_t)height;
    y = y + 31 - height - 16;

    int16_t left = player_x - scroll_x;
    int16_t right = left + 32;
    int16_t bx = x + 8;

    if (!(bx < left || bx > right)) {
        int16_t top = player_y;

        if (!(y < top || y > top + 40))
            hurt_player();
    }
    y += 16;
    image->data = (const uint16_t *)((const uint8_t *)image->data + 32);
    y -= scroll_y;
    glyph_plane_size = 64;
    glyph_draw(x, y, glyph);
    glyph_plane_size = 0;
    image->data = (const uint16_t *)((const uint8_t *)image->data - 32);
    y -= 16;
    image->height = 16;

    uint16_t head;

    if (glyph == 287) {
        head = counter_262;
        if (head == 0)
            glyph_plane_size = 64;
        head = frames_4afc[head];
    } else {
        head = counter_20c;
        if (head == 0)
            glyph_plane_size = 64;
        head = frames_4af4[head];
    }
    glyph_draw(x, y, head);
    glyph_plane_size = 0;
}

/* orig $004BD0 (a thing of kind 3): drawn as a glyph at (x, y - scroll_y).  When word_2ba is not negative and the glyph is one of
 * 232, 288, 290 and 292 and the record ptr_0ba2 points at is at (x, y) on the screen, the glyph is the next of a row of five
 * (frames_4ba8, by param_index), word_2ba counts on, at 3 the record's third word is toggled, at 5 word_2ba is -1 again */
static void draw_kind3(int16_t x, int16_t y, int16_t glyph)
{
    y -= scroll_y;
    if (word_2ba >= 0 && (glyph == 232 || glyph == 288 || glyph == 290 || glyph == 292)) {
        uint16_t *other = ptr_0ba2;

        if ((int16_t)(other[0] - scroll_x) == x && (int16_t)(other[1] - scroll_y) == y) {
            glyph = (int16_t)frames_4ba8[(param_index - 1) * 5 + word_2ba];
            word_2ba++;
            if (word_2ba == 3)
                other[2] ^= 1;
            if (word_2ba == 5)
                word_2ba = -1;
        }
    }
    glyph_draw(x, y, glyph);
}

/* orig $0047E2 (a thing of kind 5 or more): by the glyph the thing is drawn at (x, y - scroll_y) as a glyph (the default), a sprite
 * (the glyphs 219, 265, 267, 268, 269, 270, 282 and 301 of the table at $47FE), with a second glyph 16 higher by the column (225 and
 * 251) or changing with the column (252).  The glyphs 283 and 284 (the original goes on at $4862) did not run */
static void draw_kind5(int16_t x, int16_t y, int16_t glyph)
{
    y -= scroll_y;
    switch (glyph) {
    case 0xdb: case 0x119: case 0x109: case 0x114: case 0x115: case 0x116: case 0x11a: case 0x12d:
        sprite_draw(x, y, glyph);
        break;
    case 0xfc:
        glyph_draw(x, y, frames_494a[((x >> 5) + counter_2b4) & 3]);
        break;
    case 0xe1:
        glyph_draw(x, y, glyph);
        glyph_draw(x, y - 16, frames_493a[((x >> 5) + counter_2b4) & 3]);
        break;
    case 0xfb:
        glyph_draw(x, y, glyph);
        glyph_draw(x, y - 16, frames_4942[((x >> 5) + counter_2b4) & 3]);
        break;
    case 0x11b: case 0x11c:
        break;   /* goes on at $4862 */
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
    const uint8_t *map = ptr_0b86 + 8192;
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
    ptr_0ba6 = record;
    int16_t y0 = record->w[1];
    int16_t id = record->w[2];

    byte_2b6 = *flag;
    thing_fall(record, flag);

    int16_t x = (int16_t)(record->w[0] - scroll_x);
    int16_t y = record->w[1];

    if (id >= 192) {
        /* $4C66: an object (the list is filled, and drawn as the glyph id + 110) */
        y -= scroll_y;
        list_count_1294++;
        if (list_count_1294 <= 20) {
            int16_t *list = (int16_t *)value_1296;

            *list++ = x;
            *list++ = y;
            *list++ = id;
            value_1296 = (uint32_t)(uintptr_t)list;
        }
        player_touch_record(x, y, record, counter);
        glyph_draw(x, y, id + 110);
        return;
    }

    const struct Rec10 *info = &table_14cf8[id];
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
            /* any other glyph: the original goes on at $4B14, which did not run */
        } else if (kind > 4) {
            draw_kind5(x, y, glyph);
        } else {
            draw_kind3(x, y, glyph);
        }
        return;
    }

    y -= scroll_y;
    list_count_1294++;
    if (list_count_1294 <= 20) {
        if (id == 45) {
            value_1296 = GUEST(0x121c);
            list_count_1294 = 20;
            flag_25e = 1;
        }
        int16_t *list = (int16_t *)value_1296;

        *list++ = x;
        *list++ = y;
        *list++ = id;
        value_1296 = (uint32_t)(uintptr_t)list;
    }
    switch (glyph) {
    case 253:
        glyph = (int16_t)frames_44c4[counter_24a];
        break;
    case 224:
        if (counter_260 != 0)
            glyph = counter_260 + 402;
        break;
    case 279:
        sprite_draw(x, y - 8, frames_44bc[counter_2b4]);
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
    list_count_1294 = 0;
    flag_25e = 0;
    onscreen_count = 0;
    value_1296 = GUEST(0x121c);
    counter_204 = (counter_204 + 1) & 31;
    if (frame_toggle != 0)
        counter_20c = (counter_20c + 1) & 3;
    if (frame_toggle != 0) {
        counter_2b4 = (counter_2b4 + 1) & 3;
    } else {
        counter_24a++;
        if ((int16_t)counter_24a >= 5)
            counter_24a = 0;
    }
    counter_262++;
    if ((int16_t)counter_262 >= 6)
        counter_262 = 0;
    counter_260 = (counter_260 + 1) & 7;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];

        if (record->w[3] == 0 || record->w[2] < 0)
            continue;
        record_update(record, &flags_39fde[i], (int16_t)(199 - i));
    }
    if (flag_25e != 0)
        list_count_1294 = 1;
    if (onscreen_count != 0 && word_3436 == 0x4e75 && tick_hold_38a == 0) {
        play_sound(19, 1);
        return;
    }
    stop_sound(19);
}

/* orig $01924C (the whole of the function whose first part is find_record_19334): every one of the ten records of 8 bytes at $19334
 * whose third word is not negative is drawn as the sprite word_26a at its position less the scroll */
void draw_records_19334(void)
{
    for (int i = 0; i < 10; i++) {
        const struct Slot8 *record = &records_19334[i];

        if (record->w[2] >= 0)
            sprite_draw(record->w[0] - scroll_x, record->w[1] - scroll_y, word_26a);
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
 * a quotient above 9 adds the rest in the high digit of the second word's next add.  flag_2d0 says the score is to be drawn again */
static void add_kill_score(uint8_t value)
{
    uint16_t points = (uint16_t)((value >> 1) + 1);
    uint16_t quotient = points / 10;
    uint16_t remainder = points % 10;
    uint16_t extra = quotient > 9 ? (uint16_t)(quotient - 9) : 0;
    uint8_t packed = (uint8_t)((quotient << 4) | remainder);
    int x = 0;

    for (int round = 0; round < 2; round++) {
        uint8_t b1 = abcd((uint8_t)score_words[1], packed, &x);
        uint8_t b2 = abcd((uint8_t)score_words[2], 0, &x);
        uint8_t b3 = abcd((uint8_t)score_words[3], 0, &x);

        score_words[1] = (uint16_t)((score_words[1] & 0xFF00) | b1);
        score_words[2] = (uint16_t)((score_words[2] & 0xFF00) | b2);
        score_words[3] = (uint16_t)((score_words[3] & 0xFF00) | b3);
        flag_2d0 = 1;
        if (round == 0) {
            if (extra == 0)
                return;
            packed = (uint8_t)(extra << 4);
            x = 0;
        }
    }
}

/* orig $007BA8 as the movement routine of a creature's record (the one at +22): the new x (d0) and the picture (d2).  Only the
 * walker ran.  The step of walk_pace_frame moves it 4 pixels in its direction (as the direction is before a turn) when its step
 * is at most 7 and frame_toggle is set; the x is worked out here for that reason */
static int call_move(const struct CreatureType *type, struct Creature *creature, int16_t *x, int16_t y)
{
    if (type->move_routine == 0x7BA8) {
        int16_t old_x = *x;

        if (creature->state <= 7 && frame_toggle != 0)
            *x += creature->direction == 0 ? -4 : 4;
        return walk_pace_frame((const uint8_t *)type, (int16_t *)creature, old_x, y);
    }
    /* goes on at the routine the record names ($7B44 to $7B9E, $72E8, $7A0E, $7A36, ...): none of those ran */
    return 0;
}

/* orig $0060AE (right false) and $0060F2 (right true): an enemy shot at (x, y) with the speed word_2e4 is put in the first free
 * (fourth word negative) of the twenty records of 8 bytes at ptr_0cce, when the place is near enough to the screen */
static void enemy_shot(int16_t x, int16_t y, int right)
{
    int16_t sx = (int16_t)(x - scroll_x);
    int16_t sy = (int16_t)(y - scroll_y);

    if (sy >= 192 || sy < -4)
        return;
    if (right ? (sx > 312 || sx < -8) : (sx < 8 || sx > 328))
        return;

    struct Slot8 *record = (struct Slot8 *)ptr_0cce;

    for (int i = 0; i < 20; i++, record++) {
        if (record->w[3] < 0) {
            record->w[0] = x;
            record->w[1] = y;
            record->w[2] = word_2e4;
            record->w[3] = 0;
            return;
        }
    }
}

/* orig $00717E: the step routine of the shooting creatures: its timer counts down (nothing is done while the type's speed is 0); at
 * 0 it is set again (from the speed) and a shot goes out sideways (to the left from 8 pixels before the creature, to the right from
 * 32 pixels after) with a speed that goes up with the type's speed.  A type with byte 18 set aims its shot ($7154, with the line
 * drawing of $183BC) and the original goes on there: that did not run */
static void step_717e(const struct CreatureType *type, struct Creature *creature)
{
    if (type->speed == 0)
        return;
    creature->timer--;
    if (creature->timer != 0)
        return;
    creature->timer = (int16_t)((int16_t)(int8_t)(16 - type->speed) << 2);
    if (type->flag_18 != 0)
        return;   /* goes on at $7154 */

    int16_t speed = (int16_t)(((int16_t)(int8_t)type->speed >> 2) + 4);

    if (creature->direction == 0) {
        word_2e4 = (int16_t)-speed;
        enemy_shot((int16_t)(creature->x - 8), creature->y, 0);
    } else {
        word_2e4 = speed;
        enemy_shot((int16_t)(creature->x + 32), creature->y, 1);
    }
}

/* the step routine of a creature's record (+30), called with the screen not frozen (timer_306 0): only $717E ran */
static void call_step(const struct CreatureType *type, struct Creature *creature)
{
    if (type->step_routine == 0x717E)
        step_717e(type, creature);
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
        creature->x = type->start_x;
        creature->y = type->start_y;
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
 * in word_7f74 and word_7f76 (where the group's drop goes); at the end its hp is 0 */
static void creature_dies(struct Creature *creature)
{
    word_6ebe++;
    if (creature->state < 0) {
        word_7f74 = creature->x;
        word_7f76 = creature->y;
    }
    creature->state++;
    if (creature->state >= 7) {
        creature->hp = 0;
        creature->delay = 0;
        creature->direction = 0;
        return;
    }
    sprite_draw((int16_t)(creature->x - scroll_x), (int16_t)(creature->y - scroll_y), (uint16_t)(creature->state + 108));
}

/* orig $006F74 to $007070: one creature of a started group (not gone, not dying): the delay runs down, the step routine and the
 * movement routine are called, the position is kept, the way out of the screen the group's bounds name is looked at, the box is
 * put in word_39c / word_39e and the creature is looked at by check_hit_and_walls (its hp is kept; a negative one starts the
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

    word_1da = x;
    if (timer_306 == 0)
        call_step(type, creature);

    int16_t frame = (int16_t)call_move(type, creature, &x, y);

    creature->x = x;
    creature->y = y;
    x = (int16_t)(x - scroll_x);
    y = (int16_t)(y - scroll_y);

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

    word_39c = type->box_width;
    word_39e = type->box_height;

    /* orig $007EAE */
    creature->hp = check_hit_and_walls((int16_t)(x + scroll_x), (int16_t)(y + scroll_y), creature->hp);
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

    word_6ebe = 0;
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

    if ((int16_t)(gone + word_6ebe) == type->count) {
        if (type->drop != 0xFF)
            drop_thing((int16_t)(word_7f74 + 8), (int16_t)(word_7f76 + 8), type->drop);
        type->drop = 0xFF;
    }
    if (gone >= type->count)
        type->status = 0;
}

/* orig $006ED0 and $0070C8 (the table at $7074 is a switch in creature_runs; $7074 to $70C8 are its cases; inlined: $6EC0, $6EEC,
 * $6F74, $7070, $70DC, $70F8, $7EAE and $9DEA with $5AAC): every frame the ten creature records at $199AC are looked at.  A record
 * with status bit 1 clear is skipped; bit 0 clear starts its group (creatures_start), set runs it (creatures_run).  While
 * timer_306 is not 0 frame_toggle is 0 for the duration (the original saves and restores it) */
void update_creatures(void)
{
    uint16_t saved = frame_toggle;
    struct CreatureType *type = (struct CreatureType *)table_199ac;

    if (timer_306 != 0)
        frame_toggle = 0;
    word_6ebc = 9;
    for (;;) {
        if (type->status & 2) {
            if (type->status & 1)
                creatures_run(type);
            else
                creatures_start(type);
        }
        type++;
        word_6ebc--;
        if (word_6ebc < 0)
            break;
    }
    frame_toggle = saved;
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

    word_234 = (uint16_t)(rest % 10);
    rest = rest / 10;
    word_232 = (uint16_t)(rest % 10);
    rest = rest / 10;
    word_2fe = (uint16_t)(rest % 10);
    rest = rest / 10;
    word_2fc = (uint16_t)(rest % 10);
    rest = rest / 10;
    last = (int16_t)(rest % 10);
    *out++ = (uint8_t)(last + 48);
    *out++ = (uint8_t)(word_2fc + 48);
    *out++ = (uint8_t)(word_2fe + 48);
    *out++ = (uint8_t)(word_232 + 48);
    *out++ = (uint8_t)(word_234 + 48);
    return out;
}

/* orig $012924 (inlined: $012BDA, the d2 of the call is the item `item`): the message of the item (an index of table_14cf8): its
 * first line of text, the second, and "Value" and its value when it has one.  An item (unless offsets_4cba says 2, or it is 83)
 * whose bit in bits_1970a is set already has no message; otherwise the bit is set, and cleared again when message_request (priority 25)
 * says no.  The original leaves its stack unbalanced when the text's offset is negative; that is not done */
void item_message(int16_t item)
{
    if (offsets_4cba[item] != 2 && item != 83) {
        uint8_t mask = (uint8_t)(1 << (item & 7));
        uint8_t was = bits_1970a[item >> 3] & mask;

        bits_1970a[item >> 3] |= mask;
        if (was)
            return;
    }

    uint8_t *start = next_message_slot();
    uint8_t *out = start;
    int16_t count = 0;

    if (item < 0)
        return;

    const struct Rec10 *record = &table_14cf8[item];
    const uint8_t *text = ptr_0b66 + (int16_t)record->w[4];

    *out++ = 17;
    *out++ = 32;
    if (text < ptr_0b66)
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
        out = copy_text(out, (text_power + 6), &count);
        *out++ = 32;
        out = put_digits5(out, (int16_t)record->w[3]);
        *out++ = 32;
        *out++ = 32;
        *out++ = 17;
    }
    *out++ = 0;
    message_next = start;
    if (message_request(25) == 0)
        bits_1970a[item >> 3] &= (uint8_t)~(1 << (item & 7));
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
    out = copy_text(out, text_power, &count);
    *out++ = 32;

    int16_t rest = object->first_value;
    int16_t hundreds;

    word_2fe = (uint16_t)(rest % 10);
    rest = rest / 10;
    word_2fc = (uint16_t)(rest % 10);
    rest = rest / 10;
    hundreds = (int16_t)(rest % 10);
    if (hundreds != 0) {
        *out++ = (uint8_t)(hundreds + 48);
        *out++ = (uint8_t)(word_2fc + 48);
    } else if ((uint8_t)word_2fc != 0) {
        *out++ = (uint8_t)(word_2fc + 48);
    }
    *out++ = (uint8_t)(word_2fe + 48);
    *out++ = 32;
    *out++ = 17;
    *out++ = 32;
    out = copy_text(out, (text_power + 6), &count);
    *out++ = 32;
    out = put_digits5(out, object->w74);
    *out++ = 32;
    *out++ = 17;
    *out++ = 0;
    message_next = start;
    message_request(30);
}

/* orig $012CFC: the level is over (flag_230 set) when word_22e is the count in the first word of ptr_0bc6; otherwise, when
 * param_index is 3, word_0266 is 0 and the player is at x 1104 or more and y 768 to 784, word_22e is cleared and the level is over.
 * (The original loses the caller's a6 here: it leaves ptr_0bc6 in it and the saved a6 in a0) */
static void level_over_check(void)
{
    if (ptr_0bc6[0] != word_22e) {
        if (param_index != 3 || word_0266 != 0 || player_x < 0x450 || player_y > 0x310 || player_y < 0x300)
            return;
        word_22e = 0;
    }
    flag_230 = 1;
}

/* orig $0187B8 (state 6 of the entries run by run_cell_entry): the player position is copied to words_2a2; when it is not within 64
 * (in x and in y) of any of the six spots of points_b3a, word_22c and word_22e count up, the spots move on one (the oldest is
 * lost), the player position is the first and level_over_check runs */
void remember_visited_spot(void)
{
    int16_t x = player_x, y = player_y;

    words_2a2[0] = x;
    words_2a2[1] = y;
    for (int i = 0; i < 12; i += 2) {
        int16_t sx = points_b3a[i], sy = points_b3a[i + 1];

        if (!(sx - 0x40 > x || sy - 0x40 > y || sx + 0x40 < x || sy + 0x40 < y))
            return;
    }
    word_22c++;
    word_22e++;
    for (int i = 11; i >= 2; i--)
        points_b3a[i] = points_b3a[i - 2];
    points_b3a[0] = x;
    points_b3a[1] = y;
    level_over_check();
}

/* orig $012796 (inlined: the table at $18714; a0 is the map cell `cell`): a cell of the map whose byte is above 2 names an entry of
 * the table at ptr_0cbe (4 bytes: a state, a number).  A negative or 0 state does nothing; otherwise the creature types that
 * have byte 13 set to 1 are switched off (status 0) and the routine the state names in table_18714 is called with the entry:
 * 1 is take_record_to_18740, 2 is take_record_to_199ac, 3 is finish_record_1895a, 6 is remember_visited_spot (the entries
 * 4 and 5, $8898 and $88DC, are not written; 7 to 10, $17E0A to $17E16, start a thing on its path: start_record_path in game_h.c) */
void cell_entry_runs(const uint8_t *cell)
{
    run_cell_entry((int16_t)((uint8_t)*cell - 3));
}

/* orig $0127A0: the entry `index` (4 bytes each) of the table at ptr_0cbe is run as cell_entry_runs says */
void run_cell_entry(int16_t index)
{
    struct EntryRef *entry = &ptr_0cbe[index];

    if ((int16_t)entry->state <= 0)
        return;

    struct CreatureType *type = (struct CreatureType *)table_199ac;

    for (int i = 0; i < 10; i++, type++)
        if (type->bounds == 1)
            type->status = 0;

    uint32_t routine = table_18714[(int16_t)(entry->state - 1)];

    if (routine == 0x18900)
        take_record_to_18740(entry);
    else if (routine == 0x18890)
        take_record_to_199ac(entry);
    else if (routine == 0x1895A)
        finish_record_1895a(entry);
    else if (routine == 0x187B8)
        remember_visited_spot();
    else if (routine >= 0x17E0A && routine <= 0x17E16)
        start_record_path(entry, (int16_t)((routine - 0x17E0A) >> 2));
}

/* orig $010976: the message of the hint found at the player.  The list of 40 words at ptr_0cf2 is searched for the x of the player's
 * value_1296 (less 6 bytes, plus scroll_x); its twin 80 bytes further on, a word that is an offset from ptr_0b66, is kept in ptr_0364
 * and gives the text; none found, the text is the one ptrs_10a20 names for word_22c.  The message "\x11 Hint \x11" and the text is
 * made (priority 100) and flag_236 is set.  The original returns 0 in d3 */
int16_t hint_message(void)
{
    const int16_t *list = (const int16_t *)ptr_0cf2;
    int16_t wanted = (int16_t)(*(const int16_t *)(value_1296 - 6) + scroll_x);
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
        ptr_0364 = (uint8_t *)twin;
        text = ptr_0b66 + *(const int16_t *)ptr_0364;
    } else {
        text = (const uint8_t *)ptrs_10a20[word_22c];
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
    message_next = start;
    message_request(100);
    flag_236 = 1;
    return 0;
}

/* orig $015328 (inlined: the item is d2, the object's record a1): which of the three slots (slot_ref_6b6, 748, 7da) a new object
 * goes in: the first for one whose first word is 0, else the second when it is free, else the third when it is free, else the one
 * whose object has the smaller last_value; the item 2 goes in the third at once (and word_fb2c becomes 18).  Returns the number of the slot */
static int pick_slot(int16_t item, const struct Object *object)
{
    if (item == 2) {
        word_fb2c = 18;
        return 2;
    }
    if (object->w0 == 0)
        return 0;
    if (slot_ref_748 < 0)
        return 1;
    if (slot_ref_7da < 0)
        return 2;

    const struct Object *a = &((const struct Object *)object_slots)[slot_ref_748];
    const struct Object *b = &((const struct Object *)object_slots)[slot_ref_7da];

    return a->last_value < b->last_value ? 1 : 2;
}

/* orig $0152F6: the points a slot's object counts: its first_value for each of up to three steps from first_value to last_value
 * (the third step adds what is left); added to word_274 */
static void count_slot(int16_t reference)
{
    int16_t shifted = (int16_t)(reference << 4);

    if (shifted < 0)
        return;

    const struct Object *object = &((const struct Object *)object_slots)[reference];
    int16_t first = object->first_value;
    int16_t left = (int16_t)(object->last_value - first);

    word_274 = (uint16_t)(word_274 + first);
    if (left == 0)
        return;
    word_274 = (uint16_t)(word_274 + first);
    left--;
    if (left == 0)
        return;
    word_274 = (uint16_t)(word_274 + first);
    left--;
    word_274 = (uint16_t)(word_274 + left);
}

/* orig $0152D8: word_274 is the sum of the three slots' counts */
static void count_slots(void)
{
    word_274 = 0;
    count_slot(slot_ref_6b6);
    count_slot(slot_ref_748);
    count_slot(slot_ref_7da);
}

/* orig $015282 (inlined: $015328, $0152F6, $0152D8, $0152BE): the object `item` is picked up.  One already in a slot has its
 * last_value raised by one; any other goes into the slot pick_slot names (its first word is the item), the slot's list of 72
 * words is cleared, reset_object_slot sets its record up, and the message of the object is made.  word_274 is worked out again and
 * (for a new object) update_choice_888e runs */
void pick_up_object(int16_t item)
{
    struct Object *object = &((struct Object *)object_slots)[item];

    if (item == slot_ref_6b6 || item == slot_ref_748 || item == slot_ref_7da) {
        object->last_value++;
        object_message(object);
        count_slots();
        return;
    }

    int slot = pick_slot(item, object);
    int16_t *slot_ref = slot == 0 ? &slot_ref_6b6 : slot == 1 ? &slot_ref_748 : &slot_ref_7da;

    *slot_ref = item;
    reset_object_slot(item, (int16_t)slot);
    object_message(object);
    clear_72_words((uint16_t *)(slot_ref + 1));
    count_slots();
    update_choice_888e();
}


/* ======== from game_f.c ======== */
/* Gods, the game: the player of one frame, the state machine of $10368 and everything the states of its table (at $10280) go on with.  Each
 * function says where the original has it (`orig $ADDR`).
 *
 * The state (player_state) names a routine of the table; it gets the animation frame (word_19a, d7 in the original) and ends in $1263A, which
 * keeps the frame and goes on at $1263E (scroll and draw).  A routine here is a C function with the frame as its parameter; `player_end(d7)`
 * is the original's jump to $1263A.  The routines of the states 10, 11, 12, 22 and 23 (falling), 8, 9, 20 and 21 (jumping) and 13 and 14
 * (ladders) are each other's mirror images but are written out as they are in the original, with the differences it has. */

#define OBJECT(n) (&((struct Object *)object_slots)[(n)])
#define shot_lists ((struct ShotList *)shot_lists_raw)
#define SLOT_IMAGE(n) ((uint8_t *)image_table[115 + (n)].data)

static void player_after(int16_t frame);
static void state_12290(int16_t d7);
static void state_11fca(int16_t d7);
static void state_11cd2(int16_t d7);

/* orig $01263A */
static void player_end(int16_t d7)
{
    word_19a = d7;
    player_after(d7);
}

/* orig $0125B8: the scroll is eased to the place (word_37e, word_380) while word_037c is not 0: sideways 4 a frame, up and down by half of the
 * way (at least 1); word_037c counts up to 4 once both have come there */
static void scroll_ease_125b8(void)
{
    int16_t arrived = 0;

    if (word_37e != scroll_x) {
        if (word_37e > scroll_x)
            scroll_x += 4;
        else
            scroll_x -= 4;
        if (word_37e == scroll_x) {
            scroll_x = word_37e;
            if ((int16_t)word_037c <= 2)
                arrived++;
        }
    } else {
        arrived++;
    }

    int16_t d3 = (int16_t)(word_380 - scroll_y);

    if (d3 != 0) {
        int16_t up = d3 > 0;

        d3 >>= 1;
        if (d3 == 0)
            scroll_y += up ? 1 : -1;
        scroll_y += d3;
        if (word_380 != scroll_y)
            return;
        if ((int16_t)word_037c > 2)
            return;
    }
    if (arrived == 0)
        return;
    if ((int16_t)word_037c >= 4)
        return;
    word_037c++;
}

/* orig $012710: the cells of the second map around the player are looked at: the three in the column 16 pixels to the right of his place (rows
 * of his tile and the two below) and, when he is 16 or more pixels into his tile, the three in the column to the left of that; a cell
 * whose type is over 2 has an entry that runs (cell_entry_runs) */
static void cells_at_player_12710(void)
{
    int16_t x = (int16_t)((player_x + 16) & -32);
    int16_t y = (int16_t)(player_y & -16);
    const uint8_t *cell = ptr_0b86 + 8192;

    cell += x >> 5;
    cell += (int16_t)(y << 3);
    for (int row = 0; row < 3; row++, cell += 128) {
        if (cell[0] > 2)
            cell_entry_runs(cell);
    }
    if ((player_x & 31) < 16)
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
    cells_at_player_12710();

    int16_t x = player_x - scroll_x;
    int16_t y = player_y - scroll_y;

    if (word_037c != 0) {
        scroll_ease_125b8();
    } else {
        if (x <= (int16_t)frame_map_125ac[5]) {
            scroll_x -= 4;
            if (scroll_x < 0)
                scroll_x = 0;
        } else if (x >= 320 - (int16_t)frame_map_125ac[5] - 32) {
            scroll_x += 4;
            if (scroll_x >= 3776)
                scroll_x = 3776;
        }
        if (y > 112) {
            int16_t step = (int16_t)((y - 112) >> 1);

            if (step == 0)
                scroll_y++;
            scroll_y += step;
            if (scroll_y >= 832)
                scroll_y = 832;
        } else if (y < 32) {
            int16_t step = (int16_t)((32 - y) >> 1);

            if (step == 0)
                scroll_y--;
            scroll_y -= step;
            if (scroll_y < 0)
                scroll_y = 0;
        }
    }

    int16_t sprite = (int16_t)(state_table[player_state * 2] + d7);

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
    int16_t x = player_x + 16;
    int16_t y = player_y + 40;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];

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
static void spawn_shots_left_15968(struct Shot *slot, int16_t id, struct Object *object, int16_t x, int16_t y)
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

/* orig $01545C (with $015546): the same as spawn_player_shots to the left: each list with a free place for a shot gets one, at the player (a
 * little below him) or, when word_888e is the list's number, at word_8882 and word_8884 (16 below); the routine is the one the object's long
 * at +16 names in the table at $15220: 0 is $15968, 4 and 5 are bare returns; 1 to 3 and 6 to 15 did not run */
static void spawn_player_shots_left(void)
{
    struct Shot *slots[3] = { shot_slot_b92, shot_slot_b96, shot_slot_b9a };
    int16_t ids[3] = { word_2ea, word_2ec, word_2ee };

    for (int i = 0; i < 3; i++) {
        if (slots[i] == 0)
            continue;

        int16_t x = player_x;
        int16_t y = player_y + 24;

        if (word_888e == i) {
            x = word_8882;
            y = word_8884 + 16;
        }

        struct Object *object = OBJECT(shot_lists[i].object);
        uint32_t kind = ((uint32_t)object->pad_16[0] << 24) | ((uint32_t)object->pad_16[1] << 16) | ((uint32_t)object->pad_16[2] << 8) |
                        object->pad_16[3];

        play_sound(2, 4);
        switch (kind) {
        case 0:
            spawn_shots_left_15968(slots[i], ids[i], object, x, y);
            break;
        default:
            break;   /* 4 and 5 return; the others ($155EC, $1557C ...) did not run */
        }
    }
}

/* orig $00565E: the pictures of the four slots (made plain) and then the chosen one's (cut) */
static void panel_redraw_565e(void)
{
    for (int i = 0; i < 4; i++)
        panel_slot_plain(SLOT_IMAGE(i), slots_30c[i]);
    panel_slot_cut(SLOT_IMAGE((int16_t)word_2cc), slots_30c[(int16_t)word_2cc]);
}

/* orig $005444: the slots' pictures are made again; a shutter that is shut at once (word_2c0 negative) is opened (sound 17), otherwise one
 * that is not at 12 and not opening is made to open */
static void panel_shutter_5444(void)
{
    panel_redraw_565e();
    if (word_2c0 < 0) {
        word_2c0 = 0;
        word_2be = 1;
        play_sound(17, 40);
        word_2bc = 0;
        return;
    }
    if (word_2c0 == 12)
        return;
    if (word_2be >= 0)
        return;
    word_2be = 1;
    word_2bc = 0;
}

/* orig $003D48: 1 when the record at (x, y) (a thing of 16 by 16) and the player (his box from 12 to the right and 0 down, 8 more across and
 * 48 down) touch; the original returns it in the condition codes (N set: no) */
static int record_touches_player(const struct Slot8 *record)
{
    int16_t x = record->w[0];
    int16_t y = record->w[1];
    int16_t d4 = player_x + 12;
    int16_t d5 = player_y;

    if (d4 > x + 16 || d5 > y + 16)
        return 0;
    d4 += 8;
    d5 += 48;
    if (d4 < x || d5 < y)
        return 0;
    return 1;
}

/* orig $003C94 (with $003D48 and $003DE0 inlined or written above): the player uses a thing that is touched (a lever: a record of the kind
 * whose type, in the fifth word of its entry in table_14cf8, is 3, and which has an entry of the 64 of 6 bytes at flags_1c056 for its place).
 * Once for each press of the button (word_35e).  For the kinds 288, 290, 292 and 232 the entry is to be toggled by game_d (ptr_0ba2, word_2ba
 * is 0), otherwise the third word of the record and of the entry are flipped in their bit 0; sound 10 */
static void use_lever_3c94(void)
{
    if (word_35e != 0)
        return;
    if (flags_3b0 & 4)
        return;
    if (word_2ba >= 0)
        return;
    word_35e = 1;
    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];

        if (record->w[3] == 0 || record->w[2] < 0)
            continue;

        const struct Rec10 *kind = &table_14cf8[(uint16_t)record->w[2]];

        if (kind->w[2] != 3)
            continue;

        uint32_t place = ((uint32_t)(uint16_t)record->w[0] << 16) | (uint16_t)record->w[1];
        struct LeverFlag *entry = 0;

        for (int e = 0; e < 64; e++) {
            struct LeverFlag *p = &flags_1c056[e];

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
            ptr_0ba2 = &entry->x;
            word_2ba = 0;
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
    player_state = state;
    jump_start_x = player_x;
    jump_step = 0;
    play_sound(40, 2);
    player_end(frame);
}

/* the player takes hold of a ladder (the x snapped to the column, state 13 or 14, the frame `frame`), e.g. $12054 */
static void ladder_mount(int16_t state, int16_t frame)
{
    player_x &= -32;
    word_370 = 0;
    player_state = state;
    word_376 = 0;
    word_360 = 0;
    word_362 = 0;
    player_end(frame);
}

/* the player stands in front of something to take or open (state 18 to the left, 19 to the right): $1208C, $1234C */
static void pickup_enter(int16_t state)
{
    player_state = state;
    word_368 = 0;
    word_1fa = 0;
    word_35c = 0;
    player_end(0);
}

/* the player begins to fall (state 10 off a ladder, 11 to the left, 12 to the right) */
static void fall_begin(int16_t state)
{
    player_state = state;
    fall_rows_36a = 0;
    air_steps = 0;
    fall_distance = 0;
    player_end(0);
}

/* orig $011006 and $010D02: the player lands (state 16 to the right, 17 to the left): a sound and, after a fall of more than 20, less energy */
static void land(int16_t state)
{
    player_state = state;
    player_y &= -16;
    word_1e6 = 0;
    play_sound(16, 3);

    int16_t excess = fall_distance - 20;

    if (excess > 0 && flag_aa8 == 0)
        player_energy -= excess >> 1;
    player_end(0);
}

/* orig $0114A8 (and the same at $11496, $1193A, $116CA): the end of a jump on the ground, no harm taken */
static void jump_land(int16_t state)
{
    player_state = state;
    player_y &= -16;
    word_1e6 = 0;
    play_sound(16, 3);
    player_end(0);
}

/* ---- falling ---- */

/* the first part of the falling states ($10EB6, $10BB2, $10D44, $11048, $111A0): down 4, then down by fall_rows_36a (it grows by 1 every other
 * frame to 10), fall_distance counts up; then the sideways steps the stick asks for (3 at most, and not into a tile of type 1): left makes the
 * state 11, right 12.  The tile the player is in before the steps is returned */
static uint8_t *air_move(void)
{
    player_y += 4;
    fall_distance += fall_rows_36a >> 2;
    if (frame_phase == 0) {
        fall_rows_36a++;
        if (fall_rows_36a > 10)
            fall_rows_36a = 10;
    }
    player_y += fall_rows_36a;
    fall_distance++;
    if (fall_distance == 8)
        play_sound(23, 1);

    uint8_t *tile = player_tile_ptr();

    if ((game_stick & 4) && air_steps < 3) {
        player_state = 11;
        if ((player_x & 31) != 0)
            goto left_move;
        if (tile[-1] == 1 || tile[127] == 1 || tile[255] == 1)
            goto right;
        if ((player_y & 15) != 0 && tile[383] == 1)
            goto right;
left_move:
        player_x -= 4;
        air_steps++;
    }
right:
    if ((game_stick & 8) && air_steps < 3) {
        int16_t d0 = player_x & 28;

        player_state = 12;
        if (d0 == 28 || d0 < 8) {
            if (tile[1] == 1 || tile[129] == 1 || tile[257] == 1)
                return tile;
            if ((player_y & 15) != 0 && tile[385] == 1)
                return tile;
        }
        player_x += 4;
        air_steps++;
    }
    return tile;
}

/* orig $010EB6 (state 12, falling to the right) */
static void state_10eb6(int16_t d7)
{
    air_move();

    uint8_t *tile = player_tile_ptr();

    d7 = 0;
    if (tile[384] != 1 && ((player_x & 28) < 8 || tile[385] != 1)) {
        if (!(game_buttons & 1)) {
            player_end(d7);
            return;
        }
        if (claim_free_entries() != 0) {
            player_end(d7);
            return;
        }
        player_state = 22;
        word_36e = 1;
        player_end(1);
        return;
    }
    land(16);
}

/* orig $010BB2 (state 11, falling to the left) */
static void state_10bb2(int16_t d7)
{
    air_move();

    uint8_t *tile = player_tile_ptr();

    d7 = 0;
    if (tile[384] != 1 && ((player_x & 28) < 8 || tile[385] != 1)) {
        if (!(game_buttons & 1)) {
            player_end(d7);
            return;
        }
        if (claim_free_entries() != 0) {
            player_end(d7);
            return;
        }
        player_state = 23;
        word_36e = 1;
        player_end(1);
        return;
    }
    land(17);
}

/* orig $010D44 (state 10, falling off a ladder): the same, but the landing is a state 16 or 17 as word_370 says (not negative: 16) and the
 * frame is kept */
static void state_10d44(int16_t d7)
{
    air_move();

    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1 && ((player_x & 31) < 8 || tile[385] != 1)) {
        player_end(d7);
        return;
    }
    player_state = 17;
    if (word_370 >= 0)
        player_state = 16;
    player_y &= -16;

    int16_t excess = fall_distance - 20;

    if (excess > 0 && flag_aa8 == 0)
        player_energy -= excess >> 1;
    play_sound(16, 3);
    player_end(0);
}

/* orig $011048 (state 23, falling to the left and firing): the frame counts up (down once when word_36e is set) */
static void state_11048(int16_t d7)
{
    if (word_36e != 0) {
        d7--;
        word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = air_move();

    if (tile[384] == 1 || ((player_x & 31) >= 8 && tile[385] == 1)) {
        land(17);
        return;
    }
    if (d7 == 3) {
        player_state = 11;
        player_end(0);
        return;
    }
    if (d7 == 1)
        spawn_player_shots_left();
    player_end(d7);
}

/* orig $0111A0 (state 22, falling to the right and firing) */
static void state_111a0(int16_t d7)
{
    if (word_36e != 0) {
        d7--;
        word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = air_move();

    if (tile[384] == 1 || ((player_x & 31) >= 8 && tile[385] == 1)) {
        land(16);
        return;
    }
    if (d7 == 3) {
        player_state = 12;
        player_end(0);
        return;
    }
    if (d7 == 1)
        spawn_player_shots();
    player_end(d7);
}

/* orig $011CD2 (state 15; state 4 goes on here): the player stands on a tile of type 1 three rows down -> ends; otherwise he falls (state 12) */
static void state_11cd2(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] == 1) {
        player_end(d7);
        return;
    }
    if ((player_x & 28) >= 8 && tile[385] == 1) {
        player_end(d7);
        return;
    }
    fall_begin(12);
}

/* ---- jumps ---- */

/* the rise of a jump ($114C2, $116F6 ...): up by the step's number of rows (twice while countdown_220 is not 0) */
static void jump_rise(void)
{
    int16_t rows = (int16_t)jump_deltas[jump_step >> 1];

    player_y -= rows;
    if (countdown_220 != 0)
        player_y -= rows;
}

/* the player's head hits a tile of type 1: he is put one tile down and falls (state 11 or 12) */
static int jump_bump(int16_t state)
{
    if (tile_one_at_player() == 0)
        return 0;
    player_y &= -16;
    player_y += 16;
    fall_begin(state);
    return 1;
}

/* orig $0115BC (state 9, a jump to the right) */
static void state_115bc(int16_t d7)
{
    if (d7 == 12) {
        if (jump_step >= 4)
            d7 = 5;
    } else if (d7 != 5) {
        if (d7 == 0) {
            if (jump_step >= 4)
                d7 += 4;
        } else {
            d7 = 5;
        }
    }

    uint8_t *tile = player_tile_ptr();

    if ((player_x & 31) >= 8 || (tile[1] != 1 && tile[129] != 1 && tile[257] != 1)) {
        if (jump_step >= 4)
            player_x += 4;
    }
    tile = player_tile_ptr();
    if (game_stick & 1) {
        if (player_x != jump_start_x && (player_x & 31) == 0 && tile[0] == 2) {
            player_y &= -8;
            ladder_mount(14, 2);
            return;
        }
    }
    if (game_stick & 2) {
        if (player_x != jump_start_x && (player_x & 31) == 0 && (tile[0] == 2 || tile[128] == 2)) {
            player_y &= -8;
            ladder_mount(13, 2);
            return;
        }
    }
    if (jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    jump_rise();
    if (jump_bump(12))
        return;
    if (jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    if (jump_step >= 4 && (game_buttons & 1) && claim_free_entries() == 0 && jump_step < 44) {
        player_state = 20;
        word_36e = 1;
        jump_step += 2;
        player_end(1);
        return;
    }
    jump_step += 2;
    if (jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(12);
}

/* orig $011390 (state 8, a jump to the left) */
static void state_11390(int16_t d7)
{
    if (d7 == 13) {
        if (jump_step >= 4)
            d7 = 6;
    } else if (d7 != 6) {
        if (d7 == 0) {
            if (jump_step >= 4)
                d7 += 5;
        } else {
            d7 = 6;
        }
    }

    uint8_t *tile = player_tile_ptr();

    if ((player_x & 31) != 0 || (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1)) {
        if (jump_step >= 4)
            player_x -= 4;
    }
    if (game_stick & 1) {
        if (player_x != jump_start_x && (player_x & 31) == 0 && tile[0] == 2) {
            player_y &= -8;
            ladder_mount(14, 2);
            return;
        }
    }
    if (game_stick & 2) {
        if (player_x != jump_start_x && (player_x & 31) == 0 && (tile[0] == 2 || tile[128] == 2)) {
            player_y &= -8;
            ladder_mount(13, 2);
            return;
        }
    }
    if (jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    jump_rise();
    if (jump_bump(11))
        return;
    if (jump_step >= 18 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    if (jump_step >= 4 && (game_buttons & 1) && claim_free_entries() == 0 && jump_step < 44) {
        player_state = 21;
        word_36e = 1;
        jump_step += 2;
        player_end(1);
        return;
    }
    jump_step += 2;
    if (jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(11);
}

/* orig $0117AC (state 21, a jump to the left and firing) */
static void state_117ac(int16_t d7)
{
    if (word_36e != 0) {
        d7--;
        word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = player_tile_ptr();

    if ((player_x & 31) != 0 || (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1))
        player_x -= 4;
    if (jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    jump_rise();
    if (jump_bump(11))
        return;
    if (jump_step >= 18 && tile_one_three_rows_down() != 0) {
        jump_land(17);
        return;
    }
    if (d7 >= 3) {
        d7 = 6;
        player_state = 8;
        jump_step += 2;
        if (jump_step < 46) {
            player_end(d7);
            return;
        }
        fall_begin(11);
        return;
    }
    if (d7 == 1)
        spawn_player_shots_left();
    jump_step += 2;
    if (jump_step < 46) {
        player_end(d7);
        return;
    }
    fall_begin(11);
}

/* orig $0118E6 (state 20, a jump to the right and firing) */
static void state_118e6(int16_t d7)
{
    if (word_36e != 0) {
        d7--;
        word_36e = 0;
    } else {
        d7++;
    }

    uint8_t *tile = player_tile_ptr();

    if ((player_x & 31) >= 8 || (tile[1] != 1 && tile[129] != 1 && tile[257] != 1))
        player_x += 4;
    if (jump_step >= 22 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    jump_rise();
    if (jump_bump(12))
        return;
    if (jump_step >= 18 && tile_one_three_rows_down() != 0) {
        jump_land(16);
        return;
    }
    if (d7 >= 3) {
        d7 = 5;
        player_state = 9;
        jump_step += 2;
        if (jump_step < 46) {
            player_end(d7);
            return;
        }
        fall_begin(12);
        return;
    }
    if (d7 == 1)
        spawn_player_shots();
    jump_step += 2;
    if (jump_step < 46) {
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
        word_360 = 0;
        word_362 = 0;
        if (word_376 == 0) {
            d7++;
            if (d7 <= 3)
                break;
            ladder_sounds();
            word_376 = ~word_376;
            d7--;
        }
        d7--;
        if (d7 != 0)
            break;
        ladder_sounds();
        d7++;
        word_376 = ~word_376;
    }
    return d7;
}

/* orig $011ED6: a step up the ladder: the frame goes on, the player 6 rows up; ends when the tile above is a ladder (type 2), otherwise the
 * step is taken back and the frame is the one of the start */
static void ladder_up_11ed6(int16_t d7)
{
    d7 = ladder_frame(d7);
    word_37a = player_y;
    player_y -= 6;

    uint8_t *tile = player_tile_ptr();

    if (tile[128] == 2 || tile[-128] == 2) {
        player_end(d7);
        return;
    }
    d7 = word_378;
    player_y = word_37a;
    player_end(d7);
}

/* orig $011C50: a step down the ladder: the player 6 rows down; at a tile of type 1 three rows down he stands (state 26), without a ladder
 * under him he falls (state 10) */
static void ladder_down_11c50(int16_t d7)
{
    d7 = ladder_frame(d7);
    player_y += 6;

    uint8_t *tile = player_tile_ptr();

    if (tile[384] == 1) {
        player_state = 26;
        player_y &= -16;
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
 * place and the frame are kept in word_372 and word_374.  Returns 1 when the frame is ended */
static int ladder_fire(int16_t d7)
{
    if (!(game_buttons & 1) || word_370 == 0)
        return 0;
    if (claim_free_entries() != 0)
        return 0;
    word_372 = player_y;
    word_374 = d7;
    if (!(d7 & 1))
        player_y -= 4;
    player_state = word_370 > 0 ? 24 : 25;
    player_end(0);
    return 1;
}

/* the player on a ladder steps off it to the side: to state 1 (right) or 0 (left), $11DBE, $11B38, $11E40, $11BBA */
static void ladder_leave(int16_t state, int16_t dx)
{
    player_state = state;
    player_x += dx;
    player_y -= 4;
    player_end(6);
}

/* orig $011D12 (state 14, on a ladder: up) */
static void state_11d12(int16_t d7)
{
    if (frame_toggle == 0) {
        player_end(d7);
        return;
    }
    if (d7 >= 20) {
        ladder_up_11ed6(0);
        return;
    }
    word_378 = d7;
    if (game_stick == 1) {
        ladder_up_11ed6(d7);
        return;
    }
    if (!(game_stick & 12)) {
        word_360 = 0;
        word_362 = 0;
    }
    if (game_stick & 8) {
        if (game_buttons & 1) {
            word_370 = 1;
            goto fire;
        }
        if (game_stick & 1) {
            jump_begin(9, 12);
            return;
        }
        if (game_stick & 2) {
            word_360 = 0;
            word_362++;
            if (word_362 > 2) {
                word_362 = 0;

                uint8_t *tile = player_tile_ptr();

                if (tile[1] != 1 && tile[129] != 1 && tile[257] != 1 && ((player_y & 15) == 0 || tile[385] != 1)) {
                    ladder_leave(1, 8);
                    return;
                }
            }
        }
    }
    if (game_stick & 4) {
        if (game_buttons & 1) {
            word_370 = -1;
            goto fire;
        }
        if (game_stick & 1) {
            jump_begin(8, 13);
            return;
        }
        if (game_stick & 2) {
            word_362 = 0;
            word_360++;
            if (word_360 > 2) {
                uint8_t *tile = player_tile_ptr();

                if (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1 && ((player_y & 15) == 0 || tile[383] != 1)) {
                    ladder_leave(0, -8);
                    return;
                }
            }
        }
    }
fire:
    if (ladder_fire(d7))
        return;
    if (game_stick & 2) {
        word_376 = ~word_376;
        player_state = 13;
        word_360 = 0;
        word_362 = 0;
    }
    player_end(d7);
}

/* orig $011A94 (state 13, on a ladder: down) */
static void state_11a94(int16_t d7)
{
    if (frame_toggle == 0) {
        player_end(d7);
        return;
    }
    if (d7 >= 20) {
        ladder_down_11c50(0);
        return;
    }
    if (game_stick == 2) {
        ladder_down_11c50(d7);
        return;
    }
    if (!(game_stick & 12)) {
        word_360 = 0;
        word_362 = 0;
    }
    if (game_stick & 8) {
        if (game_buttons & 1) {
            word_370 = 1;
            goto fire;
        }
        if (game_stick & 1) {
            jump_begin(9, 12);
            return;
        }
        if (game_stick & 2) {
            word_360 = 0;
            word_362++;
            if (word_362 > 2) {
                uint8_t *tile = player_tile_ptr();

                if (tile[1] != 1 && tile[129] != 1 && tile[257] != 1 && ((player_y & 15) == 0 || tile[385] != 1)) {
                    ladder_leave(1, 8);
                    return;
                }
            }
        }
    }
    if (game_stick & 4) {
        if (game_buttons & 1) {
            word_370 = -1;
            goto fire;
        }
        if (game_stick & 1) {
            jump_begin(8, 13);
            return;
        }
        if (game_stick & 2) {
            word_362 = 0;
            word_360++;
            if (word_360 > 2) {
                uint8_t *tile = player_tile_ptr();

                if (tile[-1] != 1 && tile[127] != 1 && tile[255] != 1 && ((player_y & 15) == 0 || tile[383] != 1)) {
                    ladder_leave(0, -8);
                    return;
                }
            }
        }
    }
fire:
    if (ladder_fire(d7))
        return;
    if (game_stick & 1) {
        word_376 = ~word_376;
        player_state = 14;
        word_360 = 0;
        word_362 = 0;
    }
    player_end(d7);
}

/* orig $011A24 (state 24, a shot to the right from a ladder) */
static void state_11a24(int16_t d7)
{
    word_370 = 1;
    d7++;
    if (d7 == 1)
        spawn_player_shots();
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    player_state = 14;
    d7 = 3;
    if (!(word_374 & 1))
        d7 = word_374;
    player_y = word_372;
    player_end(d7);
}

/* orig $011A5C (state 25, a shot to the left from a ladder) */
static void state_11a5c(int16_t d7)
{
    word_370 = -1;
    d7++;
    if (d7 == 1)
        spawn_player_shots_left();
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    player_state = 14;
    d7 = 1;
    if (!(word_374 & 1))
        d7 = word_374;
    player_y = word_372;
    player_end(d7);
}

/* orig $01159A (state 16, landing facing right) and $011578 (state 17, facing left): a frame of rest, then the fall distance runs down by 4 a
 * frame; at the end the walk (state 1 or 0) */
static void landing(int16_t d7, int16_t walk_state)
{
    if (word_1e6 == 0) {
        word_1e6++;
        player_end(d7);
        return;
    }
    fall_distance -= 4;
    if (fall_distance >= 0) {
        player_end(d7);
        return;
    }
    player_state = walk_state;
    player_end(2);
}

/* ---- things to take ---- */

/* orig $010B80: the thing in the chosen slot (word_2cc) is dropped in front of the player (the slot's item + 11 is the kind) */
static void drop_item_10b80(void)
{
    panel_shutter_5444();

    int16_t kind = (int16_t)(slots_30c[word_2cc] + 11);

    if (kind < 0)
        return;
    slots_30c[word_2cc] = -1;
    drop_thing(player_x + 8, player_y + 32, kind);
}

/* orig $010AB8 (and $010AB2 with word_368 set first), $010B2C */
static void pickup_10ab8(int16_t d7)
{
    word_1fa = 0;
    panel_shutter_5444();
    if (word_368 != 0) {
        if (!(game_buttons & 1))
            word_368 = 0;
        player_end(d7);
        return;
    }
    if (!(game_buttons & 1)) {
        player_end(d7);
        return;
    }
    /* orig $010B2C: the next slot is chosen */
    play_sound(33, 40);
    panel_shutter_5444();
    word_2ce = (word_2cc + 1) & 3;
    word_368 = 1;
    word_35c = 1;

    int16_t item = slots_30c[word_2ce];

    if (item >= 0)
        item_message(item);
    player_end(d7);
}

/* orig $010A48: the thing `record` (kind `kind`) is near: without the button it is put back, with it the kind is put in the chosen slot,
 * whose picture is made, and its message is shown */
static void pickup_take_10a48(struct Slot8 *record, int16_t kind, int16_t d7)
{
    if (!(game_buttons & 1)) {
        word_35c = 0;
        record->w[2] = kind;
        record->w[3] = 1;
        player_end(d7);
        return;
    }
    word_368 = 1;
    slots_30c[word_2cc] = kind;
    panel_slot_plain(SLOT_IMAGE((int16_t)word_2cc), kind);
    panel_shutter_5444();
    item_message(slots_30c[word_2ce]);
    player_end(d7);
}

/* orig $0105DC: looks for a thing near the player (the first record near him, with a kind under 192 that is a thing to take: offsets_4cba says 2) */
static void pickup_scan_105dc(int16_t d7)
{
    word_35c = 1;

    int16_t x = player_x + 16;
    int16_t y = player_y + 40;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];

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
        pickup_take_10a48(record, kind, d7);
        return;
    }
    word_1fa = 1;
    player_end(d7);
}

/* orig $01056A (state 19, in front of a thing the player can take or open, facing right) and $0104F8 (state 18, facing left: it leaves to state
 * 1 instead of 0) */
static void state_pickup(int16_t d7, int16_t leave_state)
{
    if (!(game_stick & 2)) {
        if (slots_30c[word_2cc] >= 0)
            drop_item_10b80();
        d7--;
        if (d7 >= 0) {
            player_end(d7);
            return;
        }
        player_state = leave_state;
        player_end(2);
        return;
    }
    d7 = 1;
    if (word_35c != 0) {
        if (word_1fa == 0) {
            pickup_10ab8(d7);
            return;
        }
        if (game_buttons & 1) {
            word_368 = 1;
            pickup_10ab8(d7);
            return;
        }
        if (slots_30c[word_2cc] >= 0)
            drop_item_10b80();
        if (word_35c != 0) {
            player_end(d7);
            return;
        }
    }
    pickup_scan_105dc(d7);
}

/* ---- walking ---- */

/* orig $0124DE: the frame is kept and mapped through frame_map_125ac; the machine goes on at $1263E (states 5 and 6) */
static void state_5_map(int16_t d7)
{
    word_19a = d7;
    player_after((int16_t)frame_map_125ac[d7]);
}

/* orig $011FCA (state 0, walking to the left) */
static void state_11fca(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1 && ((player_x & 30) < 8 || tile[385] != 1)) {
        fall_begin(11);
        return;
    }
    if (game_stick & 8) {
        player_state = 2;
        player_end(0);
        return;
    }
    if (game_stick & 2) {
        if (!record_near_player()) {
            tile = player_tile_ptr();
            if ((player_x & 31) >= 12 && tile[385] == 2) {
                if (!(game_stick & 12)) {
                    player_x += 32;
                    ladder_mount(13, 26);
                    return;
                }
            } else if ((player_x & 31) <= 20 && tile[384] == 2) {
                ladder_mount(13, 26);
                return;
            }
        }
        pickup_enter(19);
        return;
    }
    if (game_stick & 1) {
        if (game_stick & 4) {
            jump_begin(8, 0);
            return;
        }

        int16_t d0 = player_x & 31;

        if (d0 <= 20) {
            if (tile[0] == 2) {
                ladder_mount(14, 26);
                return;
            }
            if (d0 >= 12 && tile[1] == 2) {
                player_x += 32;
                ladder_mount(14, 26);
                return;
            }
        } else if (tile[1] == 2) {
            player_x += 32;
            ladder_mount(14, 26);
            return;
        }
    }
    if ((game_buttons & 1) && claim_free_entries() == 0) {
        player_state = 6;
        state_5_map(0);
        return;
    }
    if (word_19e != 0) {
        d7 = (int16_t)((d7 + 1) & 7);
        word_19e = 0;
        player_x -= 4;
        player_end(d7);
        return;
    }
    if (game_stick & 4) {
        if ((player_x & 31) == 0 && (tile[-1] == 1 || tile[127] == 1 || tile[255] == 1)) {
            player_end(d7);
            return;
        }
        player_x -= 4;
        if (d7 == 2 || d7 == 6)
            play_sound(1, 2);
        word_19e = 1;
        word_19e = 0;
        if (d7 > 7) {
            d7 = 7;
            player_x += 4;
        }
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (game_stick & 1) {
        word_35e = 0;
        player_state = 26;
        player_end(-1);
        return;
    }
    player_end(d7 == 0 ? d7 : 57);
}

/* orig $012290 (state 1, walking to the right) */
static void state_12290(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1 && ((player_x & 31) < 8 || tile[385] != 1)) {
        fall_begin(12);
        return;
    }
    if (game_stick & 4) {
        player_state = 3;
        player_end(2);
        return;
    }
    if (game_stick & 2) {
        if (!record_near_player()) {
            tile = player_tile_ptr();

            int16_t d0 = player_x & 31;

            if (d0 >= 12 && tile[385] == 2) {
                player_x += 32;
                ladder_mount(13, 28);
                return;
            }
            if (d0 <= 20) {
                if (!(game_stick & 12) && tile[384] == 2) {
                    ladder_mount(13, 28);
                    return;
                }
            }
        }
        pickup_enter(18);
        return;
    }
    if (game_stick & 1) {
        if (game_stick & 8) {
            jump_begin(9, 0);
            return;
        }

        int16_t d0 = player_x & 31;

        if (d0 <= 20) {
            if (tile[0] == 2) {
                ladder_mount(14, 28);
                return;
            }
            if (d0 >= 12 && tile[1] == 2) {
                player_x += 32;
                ladder_mount(14, 28);
                return;
            }
        } else if (tile[1] == 2) {
            player_x += 32;
            ladder_mount(14, 28);
            return;
        }
    }
    if ((game_buttons & 1) && claim_free_entries() == 0) {
        player_state = 5;
        state_5_map(0);
        return;
    }
    if (word_19e != 0) {
        word_19e = 0;
        player_x += 4;
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (game_stick & 8) {
        if ((player_x & 31) < 8 && (tile[1] == 1 || tile[129] == 1 || tile[257] == 1)) {
            player_end(d7);
            return;
        }
        player_x += 4;
        if (d7 == 2 || d7 == 6)
            play_sound(1, 2);
        word_19e = 1;
        word_19e = 0;
        if (d7 > 7) {
            d7 = 7;
            player_x -= 4;
        }
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (game_stick & 1) {
        word_35e = 0;
        player_state = 26;
        player_end(1);
        return;
    }
    player_end(d7 == 0 ? d7 : 45);
}

/* orig $0124BA (state 5, shooting to the right) and $012508 (state 6, to the left): the frame goes on; at 3 the shots are spawned; from 5 on, while
 * fire is held (and a shot can be made) it begins again, otherwise the player goes back to walking (state 1 or 0) */
static void state_124ba(int16_t d7)
{
    d7++;
    if (d7 == 3)
        spawn_player_shots();
    if (d7 >= 5) {
        if ((game_buttons & 1) && claim_free_entries() == 0) {
            state_5_map(0);
            return;
        }
        player_state = 1;
        state_12290(2);
        return;
    }
    state_5_map(d7);
}

static void state_12508(int16_t d7)
{
    d7++;
    if (d7 == 3)
        spawn_player_shots_left();
    if (d7 >= 5) {
        if ((game_buttons & 1) && claim_free_entries() == 0) {
            state_5_map(0);
            return;
        }
        player_state = 0;
        state_11fca(2);
        return;
    }
    state_5_map(d7);
}

/* orig $012538 (state 2, turning to the right) */
static void state_12538(int16_t d7)
{
    if (game_stick & 4) {
        player_state = 3;
        player_end(d7);
        return;
    }
    d7++;
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    player_state = 1;
    player_end(6);
}

/* orig $012560 (state 3, turning to the left) */
static void state_12560(int16_t d7)
{
    if (game_stick & 8) {
        player_state = 2;
        player_end(d7);
        return;
    }
    d7--;
    if (d7 >= 0) {
        player_end(d7);
        return;
    }
    player_state = 0;
    player_end(6);
}

/* orig $012582 (state 4) */
static void state_12582(int16_t d7)
{
    if (game_stick & 8) {
        player_state = 2;
        player_end(2);
        return;
    }
    if (game_stick & 4) {
        player_state = 3;
        player_end(0);
        return;
    }
    state_11cd2(d7);
}

/* orig $01039E (state 26, at the foot of a ladder): up looks for pickups and, at a ladder tile, mounts it (state 14) */
static void state_1039e(int16_t d7)
{
    d7 = 0;
    if (game_stick & 8) {
        player_state = 28;
        player_end(d7);
        return;
    }
    if (game_stick & 4) {
        player_state = 27;
        player_end(d7);
        return;
    }
    if (game_buttons & 1) {
        use_lever_3c94();
        player_end(d7);
        return;
    }
    word_35e = 0;
    if (!(game_stick & 1)) {
        state_11cd2(d7);
        return;
    }
    check_pickups();
    if (flag_1f0 != 0) {
        player_end(d7);
        return;
    }

    uint8_t *tile = player_tile_ptr();

    if ((player_x & 31) >= 16) {
        if (tile[1] != 2 || tile[129] != 2) {
            player_end(d7);
            return;
        }
        player_x += 32;
    } else if (tile[0] != 2 && tile[128] != 2) {
        player_end(d7);
        return;
    }
    ladder_mount(14, 26);
}

/* orig $010368: the player for one frame: the routine of the state named by player_state runs (see the table at $10280): 0 and 1 walk, 2 to
 * 4 turn, 5 and 6 shoot, 8 and 9 jump, 10 to 12 fall, 13 and 14 climb, 16 and 17 land, 18 and 19 handle things, 20 to 23 jump and fall
 * firing, 24 and 25 shoot on a ladder, 26 mounts a ladder, 27 and 28 leave it.  The state 7 and those over 28 point into data and are not
 * states */
void player_control(void)
{
    int16_t d7 = word_19a;
    int16_t state = player_state;

    if (state != 18 && state != 19)
        game_step_5496();
    if (code_3436 == 20081 || flag_290 >= 0) {
        player_end(d7);
        return;
    }
    switch (state) {
    case 0:  state_11fca(d7); break;
    case 1:  state_12290(d7); break;
    case 2:  state_12538(d7); break;
    case 3:  state_12560(d7); break;
    case 4:  state_12582(d7); break;
    case 5:  state_124ba(d7); break;
    case 6:  state_12508(d7); break;
    case 8:  state_11390(d7); break;
    case 9:  state_115bc(d7); break;
    case 10: state_10d44(d7); break;
    case 11: state_10bb2(d7); break;
    case 12: state_10eb6(d7); break;
    case 13: state_11a94(d7); break;
    case 14: state_11d12(d7); break;
    case 15: state_11cd2(d7); break;
    case 16: landing(d7, 1); break;
    case 17: landing(d7, 0); break;
    case 18: state_pickup(d7, 1); break;
    case 19: state_pickup(d7, 0); break;
    case 20: state_118e6(d7); break;
    case 21: state_117ac(d7); break;
    case 22: state_111a0(d7); break;
    case 23: state_11048(d7); break;
    case 24: state_11a24(d7); break;
    case 25: state_11a5c(d7); break;
    case 26: state_1039e(d7); break;
    case 27:
        player_state = 0;
        player_end(2);
        break;
    case 28:
        player_state = 1;
        player_end(2);
        break;
    default:
        break;
    }
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
    const uint8_t *map = line_text;
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

        shot_prev_x = x;
        shot_prev_y = y;
        x += (int16_t)shot_deltas[direction * 2];
        y += (int16_t)shot_deltas[direction * 2 + 1];
        int16_t column = (int16_t)(shot->active - word_fb2c - 1);

        y += shot_offsets_6a4[column];
        shot_offsets_6a4[column]++;
        shot->x = x;
        shot->y = y;
        x -= scroll_x;
        y -= scroll_y;
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

/* orig $018560 (with d2 = d3 = 0): the first free record of records_3a0a6 (first word negative) is started at the screen place (x, y) with no
 * speed and a count of 58 */
static void start_debris_at(int16_t x, int16_t y)
{
    for (int i = 0; i < 20; i++) {
        int16_t *record = &records_3a0a6[i * 6];

        if (record[0] < 0) {
            record[0] = 0;
            record[1] = x + scroll_x;
            record[2] = y + scroll_y;
            record[3] = 0;
            record[4] = 0;
            record[5] = 58;
            flag_2a8++;
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
    const uint8_t *tile = ptr_0b86 + 8192;

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
    int16_t px = (int16_t)(x + 8 + scroll_x + 2);
    int16_t py = (int16_t)(y + 4 + scroll_y);
    int16_t top = player_y;

    if (word_19c >= 18 && word_19c <= 19)
        top += 24;
    if (py < top || px < player_x)
        return;
    if (px > (int16_t)(player_x + 32) || py > (int16_t)(player_y + 48))
        return;
    if (flag_aa8 != 0)
        return;
    player_energy -= 2;
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
    struct Slot8 *bolt = (struct Slot8 *)ptr_0cce;

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
            glyph_draw(x - scroll_x, y - scroll_y, 69 + bolt->w[3]);
            continue;
        }
        bolt->w[0] = x;
        x -= scroll_x;
        y -= scroll_y;
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
    uint32_t *p = (uint32_t *)ptr_0cce;

    for (int i = 0; i < 40; i++)
        *p++ = 0xFFFFFFFF;
}

/* orig $0088CC: the 180 bytes of records_12a0 are cleared */
void clear_records_12a0(void)
{
    uint8_t *p = (uint8_t *)records_12a0;

    for (int i = 0; i < 180; i++)
        *p++ = 0;
}

/* orig $00890C: the 4280 bytes of records_1354 are cleared */
void clear_records_1354(void)
{
    uint8_t *p = (uint8_t *)records_1354;

    for (int i = 0; i < 4280; i++)
        *p++ = 0;
}

/* orig $00D650: the word at +4 of the ten records of records_113c is cleared */
void clear_records_113c(void)
{
    for (int i = 0; i < 10; i++)
        records_113c[i].w[2] = 0;
}

/* orig $00D87E: word_10ae is cleared and so is the byte at +9 of the ten records (12 bytes) at records_1036 */
void clear_records_1036(void)
{
    word_10ae = 0;
    for (int i = 0; i < 10; i++)
        records_1036[i * 12 + 9] = 0;
}

/* orig $00D6A0 (with $00D69C, $00DE26 and $00E392, which set up its arguments): the first word of ten records of 10 bytes at `base` is
 * set to -1, and the routine at $D6B8 is switched off (its first word becomes an rts) */
void reset_slots_10(int16_t *base)
{
    for (int i = 0; i < 10; i++)
        base[i * 5] = -1;
    code_d6b8 = 0x4E75;
}

/* orig $00D69C */
void reset_slots_fd0(void)
{
    reset_slots_10(slots_fd0);
}

/* orig $00E392: the routine at $E3A2 is switched off, then as $D69C for the records at $11B4 */
void reset_slots_e392(void)
{
    code_e3a2 = 0x4E75;
    reset_slots_10(slots_11b4);
}

/* orig $00DE26: the routines at $DE54 and $E142 are switched off; in level 2 the 128 words of table_ed0 are -1; then as $D69C for the
 * records at $11B4 */
void reset_slots_de26(void)
{
    code_de54 = 0x4E75;
    code_e142 = 0x4E75;
    if (param_index == 2)
        for (int i = 0; i < 128; i++)
            table_ed0[i] = -1;
    reset_slots_10(slots_11b4);
}

/* orig $00DAEA: the 140 bytes at bytes_10b0 are set to $FF, the routine at $DB04 is switched off */
void fill_bytes_10b0(void)
{
    for (int i = 0; i < 140; i++)
        bytes_10b0[i] = 0xFF;
    code_db04 = 0x4E75;
}

/* orig $00F9A6: the 200 bytes of flags_39fde are set to $FF */
void fill_flags_39fde(void)
{
    uint32_t *p = (uint32_t *)flags_39fde;

    for (int i = 0; i < 50; i++)
        *p++ = 0xFFFFFFFF;
}

/* orig $00FA46: word_26a is the word of table_fa5c that the level (from 1) names */
void set_pickup_glyph(void)
{
    word_26a = table_fa5c[(int16_t)(param_index - 1)];
}

/* orig $003F2A: the first long of the ten effects (10 bytes each) is -1, flag_2ac is cleared */
void clear_effects(void)
{
    for (int i = 0; i < 10; i++) {
        effects[i * 5] = -1;
        effects[i * 5 + 1] = -1;
    }
    flag_2ac = 0;
}

/* orig $007464: ten records of 8 bytes at records_737e: a long -1 and a long 0; word_73ce is cleared */
void clear_records_737e(void)
{
    for (int i = 0; i < 10; i++) {
        records_737e[i * 2] = 0xFFFFFFFF;
        records_737e[i * 2 + 1] = 0;
    }
    word_73ce = 0;
}

/* orig $017C68: the forty records of 6 bytes at table_1a30c are set to -1 */
void fill_table_1a30c(void)
{
    for (int i = 0; i < 120; i++)
        table_1a30c[i] = 0xFFFF;
}

/* orig $017C7C: the hundred longs of marker_table that are not -1 are looked at (a marker's cell gets 109 in the map's other half and 1
 * in the cell, and the marker is -1); they all are -1 at the start of a level */
void clear_markers(void)
{
    for (int i = 0; i < 100; i++) {
        if (marker_table[i] == 0xFFFFFFFF)
            continue;
        uint8_t *cell = (uint8_t *)(uintptr_t)marker_table[i];

        cell[-8192] = 109;
        cell[0] = 1;
        marker_table[i] = 0xFFFFFFFF;
    }
}

/* orig $018400: twenty records of 22 bytes at records_1a154 get -1 in their first long; word_186be is cleared */
void clear_records_1a154(void)
{
    for (int i = 0; i < 20; i++)
        *(uint32_t *)(records_1a154 + i * 22) = 0xFFFFFFFF;
    word_186be = 0;
}

/* orig $00725A: flag_721c is cleared and the thirty words of records_721e are -1 */
void clear_records_721e(void)
{
    flag_721c = 0;
    for (int i = 0; i < 30; i++)
        records_721e[i] = -1;
}

/* orig $0077BE: the word at +4 of the 42 records (10 bytes) at records_7544 is -1; the routines at $77E6 and $7906 are switched off */
void reset_records_7544(void)
{
    for (int i = 0; i < 42; i++)
        records_7544[i * 5 + 2] = -1;
    code_77e6 = 0x4E75;
    code_7906 = 0x4E75;
}

/* orig $00FA64: the first word of the five records (24 bytes) of records_18740 is cleared, so is the word at +6 of the ten
 * creature types (36 bytes) of table_199ac; then clear_records_1036 */
void clear_creature_state(void)
{
    for (int i = 0; i < 5; i++)
        *(uint16_t *)(records_18740 + i * 24) = 0;
    for (int i = 0; i < 10; i++)
        *(uint16_t *)(table_199ac + i * 36 + 6) = 0;
    clear_records_1036();
}

/* orig $00FA9C: the first word of each of the three lists of shots is -1 and the rest of it (72 words) is cleared; word_214 is -1 */
void clear_shot_lists(void)
{
    uint16_t *p;

    slot_ref_6b6 = -1;
    p = &shot_lists_raw[1];
    clear_72_words(p);
    slot_ref_748 = -1;
    clear_72_words((uint16_t *)((uint8_t *)&slot_ref_748 + 2));
    slot_ref_7da = -1;
    clear_72_words((uint16_t *)((uint8_t *)&slot_ref_7da + 2));
    word_214 = -1;
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
    } while (sound_busy_207a0 != 0);
}

/* orig $016C14 and $016C1E: the 24 bytes at $16BCC (the same twelve words as the ones that are at $16BB4 at the start: the original
 * copies them again) are copied to $16BB4 (the data the game reads there) */
void copy_block_16bb4(void)
{
    static const uint16_t words[12] = { 0x000C, 0xFFFD, 0x000C, 0x0000, 0x000C, 0x0003, 0xFFF4, 0xFFFD, 0xFFF4, 0x0000, 0xFFF4, 0x0003 };

    for (int i = 0; i < 12; i++)
        ((uint16_t *)block_16bb4)[i] = words[i];
}

/* orig $0017DBA: the 25 records of 24 bytes at ptr_0c9e get -1 in their first two words and in the byte at +4; the cells
 * marked by the records are given back */
void clear_cell_records(void)
{
    struct PathRecord *record = ptr_0c9e;

    for (int i = 0; i < 25; i++, record++) {
        record->x = -1;
        record->y = -1;
        record->kind = -1;
    }
    restore_saved_bytes((struct SavedByte *)entry_table, 49);
}

/* orig $00F398: the 506 bytes at bytes_dest become words at words_source (the opposite of narrow_words_to_bytes), and the last four
 * words are then 11, 0, 6 and 0 */
void widen_bytes_to_words(void)
{
    uint16_t *to = (uint16_t *)words_source;
    const uint8_t *from = bytes_dest;

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
void sort_records_19b14(void)
{
    uint8_t *base = (uint8_t *)records_19b14;

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
                const uint8_t *a2 = (const uint8_t *)table_14cf8 + d1 * 10;
                const uint8_t *a3 = (const uint8_t *)table_14cf8 + d2 * 10;
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
    uint16_t seed = random_seed;
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
    random_seed = seed;
    return seed;
#undef ROXR
}

/* the two bytes the original reads from $FF8207 and $FF8209 as the seed of its random numbers: the video address registers of the Atari ST
 * the game came from, an address the Amiga does not have (the emulated machine gives $64 and $FF; a port without such an address uses
 * these) */
#ifdef GODS_SDL
#define SEED_HIGH 0x64
#define SEED_LOW 0xFF
#else
#define SEED_HIGH (*(volatile uint8_t *)GUEST(0xFF8207))
#define SEED_LOW (*(volatile uint8_t *)GUEST(0xFF8209))
#endif

/* orig $0062E6: the table of 256 random words at ptr_0cc6 is made (the seed comes from two bytes of the address space that the
 * program reads: they were the video address of the Atari ST the game came from); one word at an even offset up to 254 and
 * one up to 510 are cleared, word_2e6 is cleared */
void random_table_make(void)
{
    uint16_t *table = (uint16_t *)ptr_0cc6;
    uint16_t or_all;

    do {
        random_seed = (uint16_t)((SEED_HIGH << 8) | SEED_LOW);
        or_all = 0;
        for (int i = 0; i < 256; i++) {
            uint16_t r = random_next();

            or_all |= r;
            table[i] = r;
        }
    } while (or_all == 0);
    word_2e6 = 0;
    {
        uint16_t offset = (uint16_t)(random_next() & 254);

        *(uint16_t *)((uint8_t *)table + offset) = 0;
        offset = (uint16_t)(random_next() & 510);
        *(uint16_t *)((uint8_t *)table + offset) = 0;
    }
}

/* orig $00D5EE: when the password is used (word_0266) the map of the level is searched for the cells 35 and 36 (level 1) that the
 * entries of table_ed0 are made of (goes on at $00D5FE, not run); otherwise the first two longs of table_ed0 are -1 */
void scan_cells_d5ee(void)
{
    uint32_t *entry = (uint32_t *)table_ed0;

    if (word_0266 != 0)
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
    if (param_index == 1) {
        const int32_t *entry = (const int32_t *)table_ed0;

        if (entry[0] >= 0) {
            /* goes on at $00D5B6 (not run) */
            return;
        }
    }
    index_1ea = 0;
}

/* orig $00F3C0 is assign_addresses (menu.c). orig $00F302: the entry list of the level (god01.pat, 5400 bytes) is loaded and given
 * its addresses, and then the 506 words are made bytes: `names` is the file names of the level.  With the password ($266) the
 * file is not read; with a failed read of the level's map ($286 clear) the original loads the files again: not run */
void load_pattern_data(const uint8_t *names)
{
    if (word_0266 == 0) {
        load_unpacked((const char *)names + NAME_PATTERN, entry_list);
        file_longs(entry_list, 100);
        assign_addresses();
    }
    if (flag_286 == 0) {
        /* goes on at $00F330 (not run) */
        return;
    }
    narrow_words_to_bytes();
}

/* orig $0148D0: nothing is done while flag_286 is set (goes on at $0148D6, not run, otherwise) */
void check_flag_286(void)
{
    if (flag_286 == 0)
        return;   /* goes on at $0148D6 */
}

/* orig $014852: 40 words at `table` (0x50 bytes), and the 40 texts of 40 bytes that follow them, are put in the pool of texts at text_pool
 * (ptr_0cf6): a word that is not negative gets the text's offset from ptr_0b66 into the 40 words that are copied to `to` + 80; the pool's
 * end is kept in ptr_0b62 */
void read_text_table_14852(const uint16_t *table, uint16_t *to)
{
    uint8_t *pool = text_pool;
    const uint8_t *text = (const uint8_t *)table + 80;
    uint16_t offset = (uint16_t)((uint32_t)(uintptr_t)pool - (uint32_t)(uintptr_t)ptr_0b66);

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
    ptr_0b62 = pool;
}

/* orig $014814: 40 longs at `table` are offsets of texts (from the start of the table; none when 0 or negative); the texts are put
 * into the pool at ptr_0b62 and ptr_0cba gets, for each, the word that is its offset from ptr_0b66 (-1 for none); the pool's end is
 * kept in ptr_0b62.  Returns where the data goes on: 1840 bytes after the start of the table */
const uint8_t *read_text_table_14814(const uint8_t *table)
{
    uint16_t *to = (uint16_t *)ptr_0cba;
    uint8_t *pool = ptr_0b62;
    uint16_t offset = (uint16_t)((uint32_t)(uintptr_t)pool - (uint32_t)(uintptr_t)ptr_0b66);
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
    ptr_0b62 = pool;
    return table + 1840;
}

/* orig $014794: the file of the objects (`names` + 22: objects.001, 6336 bytes) is loaded; 132 records of 10 bytes (a long and a
 * long, a word that is the name's offset from ptr_0b66) are made in table_14cf8 from it, the names (two strings each) go to ptr_0e5c;
 * a record whose name is shorter than 3 characters has -1 for the offset.  The end of the names is the start of the text pool */
void load_objects_file(const uint8_t *names)
{
    uint8_t *from;
    uint16_t offset = 0;
    uint8_t *to_start;
    uint8_t *out;

    ptr_0b66 = ptr_0e5c;
    load_unpacked((const char *)names + NAME_OBJECTS, picture_buffer);
    from = picture_buffer;
    out = ptr_0b66;
    to_start = (uint8_t *)table_14cf8;
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
    ptr_0b62 = out;
    text_pool = out;
}

/* orig $014888: the file of the weapons (`names` + 34: weapons.001, 880 bytes) is loaded onto object_slots; the third word of each of
 * the 11 slots (the word at +8) is kept and put back after restore_slot_words has put the saved words back */
void load_weapons_file(const uint8_t *names)
{
    int16_t kept[11];
    struct Object *objects = (struct Object *)object_slots;

    for (int i = 0; i < 11; i++)
        kept[i] = objects[i].last_value;
    load_unpacked((const char *)names + NAME_WEAPONS, object_slots);
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
    if (word_0266 != 0)
        return;
    load_weapons_file(names);
    load_objects_file(names);
}

/* orig $01955E: twenty entries of three words (x, y, kind) at table_1c28a are copied to table_19456; each with a not negative x puts
 * a cell in the map (ptr_0b86): the byte of param_b ($251) for kind 0 and of param_b ($253) for another, and, 8192 bytes on, 0 or 1
 * (the kind toggled) */
void place_pickups(void)
{
    const uint16_t *from = table_1c28a;
    uint16_t *to = table_19456;

    word_356 = 1;
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
        uint8_t *cell = ptr_0b86 + (int16_t)(x >> 5) + (int16_t)(y << 3);

        if (kind == 0)
            *cell = (uint8_t)param_b;
        else
            *cell = (uint8_t)(param_b >> 16);
        cell += 8192;
        kind ^= 1;
        *cell = (uint8_t)kind;
    }
}

/* orig $00F3DA: the level's map file (named by ptr_0b8a) is loaded to the background's place: two words (word_1ec, word_1ee), word_1ec
 * words for the copper colours (ptr_0cda), 8192 words of map (ptr_0b86), the record table, the texts (read_text_table_14814) and 2400
 * bytes (load_buffer_2400); the copper list is made (copper_list_make) */
void load_level_map(void)
{
    const uint16_t *p;
    uint16_t *to;

    load_unpacked((const char *)ptr_0b8a, background);
    p = (const uint16_t *)background;
    word_1ec = file_word(p++);
    word_1ee = file_word(p++);
    to = (uint16_t *)ptr_0cda;
    {
        uint16_t n = (uint16_t)(word_1ec - 1);

        do {
            *to++ = file_word(p++);
        } while (n-- != 0);
    }
    to = (uint16_t *)ptr_0b86;
    for (int i = 0; i < 8192; i++)
        *to++ = *p++;
    copy_record_table((const struct Rec6 *)p);
    p += 600;
    p = (const uint16_t *)read_text_table_14814((const uint8_t *)p);
    load_buffer_2400((const uint8_t *)p);
    copper_list_make();
}

/* orig $00530A: the copper list of the level's colours at copper_colors is made from word_1ec colours at ptr_0cda (word_1ee is the
 * spacing of the lines): each colour is a move to COLOR01 (the colour doubled: the original's colours are ST 3-bit ones) and a wait for
 * the line, with, between the colours, a move of the colour half way between them; the list ends with the three moves the program's
 * copper lists have (copper_move_a and b and $8A0000).  The colours are saved (copper_colors_save) */
void copper_list_make(void)
{
    uint16_t *list = copper_colors;
    const uint16_t *source = (const uint16_t *)ptr_0cda;
    uint16_t spacing = word_1ee;
    int16_t count = (int16_t)(word_1ec - 1);
    uint16_t line;

    wait_vblank();
    copper_move_a = 0x00840000;
    copper_move_b = 0x00865246;
    if (count >= 0) {
        int16_t half = (int16_t)(spacing + 1) >> 1;

        word_1ee = (uint16_t)half;
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
                line = (uint16_t)(line + word_1ee);
                *list++ = wait;
                *list++ = 0xFF00;
                *list++ = 386;
                *list++ = (uint16_t)(blue | green | red);
            }
            *list++ = (uint16_t)((uint16_t)(line << 8) | 1);
            line = (uint16_t)(line + word_1ee);
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
    word_1ee = spacing;
}

/* orig $017A7E: the picture file `name` is read to picture_buffer (32034 bytes) and copied to 34 bytes before the screen buffer */
void load_picture_to_screen(const uint8_t *name)
{
    const uint16_t *from;
    uint16_t *to = (uint16_t *)(screen_buffer - 34);

    load_unpacked((const char *)name, picture_buffer);
    from = (const uint16_t *)picture_buffer;
    for (int i = 0; i < 16017; i++)
        *to++ = *from++;
}

/* orig $017B4A: `count` + 1 blocks of 32 by 16 pixels (four planes: the words at +0 and +8 bytes of each of the 16 rows) are cut out of the
 * picture on the screen, from (x, y) on, to `to`, which goes on in ptr_0168; x goes on by 32 and wraps at 320 to the next row of blocks (16
 * lines) */
static void cut_blocks(int16_t x, int16_t y, int16_t count, uint16_t *to)
{
    for (; count >= 0; count--) {
        const uint8_t *base = screen_buffer + (int32_t)y * 160 + (int16_t)(x >> 1);

        for (int plane = 0; plane < 4; plane++) {
            const uint8_t *row = base + plane * 2;

            for (int r = 0; r < 16; r++, row += 160) {
                *to++ = *(const uint16_t *)row;
                *to++ = *(const uint16_t *)(row + 8);
            }
        }
        ptr_0168 = (uint8_t *)to;
        x = (int16_t)(x + 32);
        if (x >= 320) {
            x = 0;
            y = (int16_t)(y + 16);
        }
    }
}

/* orig $017AAA: the background's tile picture `name` (bits1a.pi1) is made the tiles ptr_0c70 points to: the screen of the picture is cut
 * (cut_blocks) into blocks, 120 at the most from the first file and, for the levels that have more, the rest from the file of
 * the second picture (xtra1a.pi1) */
void load_background_tiles(const uint8_t *name)
{
    uint8_t *top = memory_base + 34048;
    int16_t blocks;
    int16_t count;

    background = top - 33792;
    blocks = (int16_t)table_17a6e[(int16_t)((param_index - 1) * 2) + word_0266];
    background = top - ((uint32_t)(uint16_t)blocks << 8);
    ptr_0c70 = background - 256;
    word_276 = 0;
    load_picture_to_screen(name);
    count = (int16_t)(blocks - 1);
    if (count >= 119)
        count = 119;
    word_2fa = (uint16_t)count;
    cut_blocks(0, 0, count, (uint16_t *)background);
    if (blocks <= 120)
        return;
    blocks = (int16_t)(blocks - 120);
    load_picture_to_screen(level_names + NAME_XTRA);
    count = (int16_t)(blocks - 1);
    word_2fa = (uint16_t)count;
    cut_blocks(0, 0, count, (uint16_t *)ptr_0168);
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
    scroll_x = x;
    scroll_y = y;
}

/* orig $019170: the first three words of ten records of records_19334 are -1.  The map (ptr_0b86) is looked at: a cell that is the
 * byte param_a >> 16 or the one param_a & 255 makes a block of 2 by 3 cells: the cell and the ones under it get the byte of
 * param_a & 255 and 10 and 20 more, the ones to the right of them the byte of param_b & 255 (word param_c); the collision map (8192 bytes
 * on) gets a 1 for the three cells at the left */
void mark_cells_19170(void)
{
    uint8_t wanted_a = (uint8_t)(param_a >> 16);
    uint8_t wanted_b = (uint8_t)param_a;
    uint8_t other = (uint8_t)param_c;
    uint8_t *cell = ptr_0b86;

    for (int i = 0; i < 10; i++) {
        records_19334[i].w[0] = -1;
        records_19334[i].w[1] = -1;
        records_19334[i].w[2] = -1;
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

/* orig $00FB9A: 256 bytes at table_1c9e4 are copied 1024 bytes on (copy_1cde4), and the 32 pairs of words at table_1c9e4 (at +0 and +256 of 32
 * bytes each) become 32 pairs of bytes at bytes_fbf2: the first divided by 1366, less 5, plus 16; the second shifted right
 * by 10, less 5, plus 24 */
void make_pairs_fb9a(void)
{
    const uint8_t *table = table_1c9e4;
    uint8_t *out = bytes_fbf2;

    for (int i = 0; i < 256; i++)
        copy_1cde4[i] = table[i];
    for (int n = 0; n < 32; n++) {
        int32_t first = *(int16_t *)(table + n * 32);
        int32_t second = *(int16_t *)(table + 256 + n * 32);
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

/* orig $003DE0: the flags_1c056 entry (six bytes: the long (x << 16) | y and a word) for the point (x, y): its place or 0 when there is none.
 * The original leaves d1 negative when none is found; here the caller looks at the result */
struct LeverFlag *find_flag_entry(int16_t x, int16_t y)
{
    for (int i = 0; i < 64; i++)
        if (flags_1c056[i].x == (uint16_t)x && flags_1c056[i].y == (uint16_t)y)
            return &flags_1c056[i];
    return 0;
}

/* orig $003D86: a record of the record table (200 of 6 bytes) of kind 3 (a kind of table_14cf8) takes the flag word of its
 * entry in flags_1c056 (found by its place), or -1 when it has none; word_2ba becomes -1 */
void fix_records_3d86(void)
{
    struct Rec6 *record = record_table;

    for (int i = 0; i < 200; i++, record++) {
        int16_t kind = (int16_t)record->w[2];

        if (kind < 0 || kind >= 192)
            continue;
        if ((int16_t)table_14cf8[kind].w[2] != 3)
            continue;
        struct LeverFlag *entry = find_flag_entry((int16_t)record->w[0], (int16_t)record->w[1]);

        if (entry == 0)
            record->w[2] = 0xFFFF;
        else
            entry->state = record->w[2];
    }
    word_2ba = -1;
}

/* orig $00F91E (with $00F994 inlined): the same records of kind 3 are looked at: one whose place has no flags_1c056 entry is turned off (kind -1), and so is the
 * second of two that have the same place (the first 64 places are kept in a buffer on the stack) */
void find_duplicate_records(void)
{
    uint32_t seen[64];
    struct Rec6 *record = record_table;

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

        if (*(int16_t *)((uint8_t *)table_14cf8 + (int16_t)offset + 4) != 3)
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
 * the chosen slot (word_2cc) is the cut one */
void item_panels_make(void)
{
    for (int i = 0; i < 4; i++)
        panel_slot_plain((uint8_t *)image_table[115 + i].data, item_slots[i]);
    panel_slot_cut((uint8_t *)image_table[115 + (int16_t)word_2cc].data, item_slots[(int16_t)word_2cc]);
}

/* orig $01EFCC (with $01F5DA inlined: the test of bit 0 of flags_0ddd): image number `image` of the image table is drawn at (x, y) with the clip rectangles ptr_0de0 points to (flags_0ddd bit 3).
 * What did not run (bit 0 and bit 5 of flags_0ddd, a negative height, the draw without a clip list) is not written */
void image_draw_clipped(int16_t x, int16_t y, uint16_t image)
{
    const struct Image *entry;
    int16_t width;
    int16_t height;
    const uint8_t *source;
    uint16_t bytes;
    const int16_t *clip;

    if (flags_0ddd & 1)
        return;   /* goes on at $01F5E2 */
    word_d86 = 0;
    word_d88 = 0;
    if (flags_0ddd & 2)
        word_d86 = (uint16_t)(x & 15);
    entry = &image_table[image];
    width = entry->neg_width;
    height = (int16_t)entry->height;
    source = (const uint8_t *)entry->data;
    bytes = (uint16_t)((width < 0 ? -width : width) + 1) >> 3;
    long_dd8 = (uint32_t)bytes * (uint16_t)(height + 1);
    word_d82 = 0;
    if (width < 0) {
        width = (int16_t)-width;
        word_d82 = (uint16_t)~word_d82;
    }
    if (height < 0)
        return;   /* goes on at $01F02C */
    word_dd6 = 0;
    word_dd4 = 0;
    if (flags_0ddd & 0x20)
        return;   /* goes on at $01F05E */
    if (!(flags_0ddd & 8))
        return;   /* goes on at $01F06C: no clip list */
    for (clip = (const int16_t *)ptr_0de0; clip[0] >= 0; clip += 4) {
        uint16_t kept = word_dd4;

        blit_clipped(x, y, width, height, source, clip);
        word_dd4 = kept;
        word_dd6 = 0;
    }
}

/* orig $005E50: the screen is filled with ones, image 127 (the digits) is drawn at (0, 0) and its sixteen... ten digits, 8 rows of four bytes
 * (one for each plane), are cut out of it into digit_font */
void hud_digits_load(void)
{
    uint32_t *fill = (uint32_t *)screen_buffer;
    const uint8_t *from;
    uint8_t *out = digit_font;

    for (int i = 0; i < 8000; i++)
        *fill++ = 0xFFFFFFFF;
    word_dde = 1;
    image_draw_clipped(0, 0, 127);
    blitter_wait();
    from = screen_buffer;
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

/* orig $00F526: the state of the game is set for the start of a level: the work area is cleared, the player is put at the start
 * of the level, the variables and tables are reset (many of the small routines of this module), the routines that are off in the
 * level are switched off or on (their first words are rts or nop), and the item slots and the records are made */
void level_state_init(void)
{
    uint32_t spot;

    clear_work_area();
    flags_3b1 &= (uint8_t)~4;
    play_sound(0, 0);
    sound_enabled = 1;
    flag_2b0 = 0;
    word_a68 = 0;
    word_19c = 4;
    word_370 = 0;
    player_x = start_x;
    player_y = start_y;
    if (flag_8890 >= 0) {
        word_8882 = (uint16_t)player_x;
        word_8884 = (uint16_t)(player_y - 40);
    }
    spot = (((uint32_t)(uint16_t)player_x << 16) | (uint16_t)player_y) & 0xFF80FF80;
    for (int i = 0; i < 12; i += 2) {
        points_b3a[i] = (int16_t)(spot >> 16);
        points_b3a[i + 1] = (int16_t)spot;
    }
    words_2a2[0] = start_x;
    words_2a2[1] = start_y;
    scroll_to_player(start_x, start_y);
    word_19a = 0;
    frame_toggle = 0;
    frame_phase = 0;
    word_1f8 = 0;
    word_1ba = 0;
    flag_1f0 = 0;
    flag_b24 = 0;
    message_state = 0;
    tick_hold_296 = 0;
    flag_aa8 = 0;
    flag_210 = 0;
    word_a54 = 0;
    countdown_220 = 0;
    word_22c = 0;
    word_22e = 0;
    flag_230 = 0;
    fall_distance = 0;
    word_3a1fa = 0;
    flag_236 = 0;
    word_238 = 0;
    flag_258 = 0;
    word_294 = 0;
    word_280 = 0;
    word_3a6 = 0;
    timer_306 = 0;
    word_025c = 1;
    flags_3b0 &= (uint8_t)~1;
    flags_3b0 &= (uint8_t)~4;
    flags_3b1 &= (uint8_t)~1;
    word_3436 = 0x4E75;
    flag_290 = -1;
    word_17e9c = 0x4E75;
    word_4a2 = 0;
    code_c374 = 0x4E75;
    ptr_0bc6 = (const uint16_t *)(uintptr_t)table_143b8[(int16_t)(param_index - 1)];
    word_2aa = scroll_y;
    mark_cells_19170();
    place_pickups();
    make_pairs_fb9a();
    ptr_0c70 = background - 256;
    if (word_037c != 0) {
        do_nothing();
        load_background_tiles(level_names + NAME_BITS);
        word_037c = 0;
        do_nothing();
    }
    if (word_0248 != 0) {
        seed_282 = 0x4B79C278;
        if (param_index == 4)
            word_284 = 1;
        word_2cc = 3;
        word_2ce = 3;
        slots_30c[0] = -1;
        slots_30c[1] = -1;
        slots_30c[2] = -1;
        slots_30c[3] = -1;
        item_panels_make();
        for (int i = 0; i < 4; i++)
            score_words[i] = 0;
        high_score_baa = (int32_t)table_5b9e[(int16_t)(param_index - 1)];
        high_score_bae = (int32_t)table_5bb2[(int16_t)(param_index - 1)];
        word_27c = (uint16_t)((param_index - 1) * 4);
        word_27e = (uint16_t)((param_index - 1) * 4);
        flag_2d0 = 1;
        player_energy = 24;
        clear_shot_lists();
        copy_block_16bb4();
        end_flag_8890();
        item_count = 2;
        score_total_1fc = 0;
        code_ba34 = 0x4E75;
        flags_3b0 &= (uint8_t)~0x40;
    }
    word_272 = (int16_t)table_1527a[(int16_t)(param_index - 1)];
    word_268 = 0;
    energy_shown = 0;
    word_2a6 = -1;
    word_a9a = -1;
    clear_creature_state();
    clear_records_721e();
    clear_records_1a154();
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
    clear_records_12a0();
    clear_records_1354();
    clear_records_737e();
    reset_records_7544();
    code_8a54 = 0x4E71;
    code_9ae6 = 0x4E71;
    code_6ed0 = 0x4E71;
    stop_game_sounds();
    ptr_0b76 = (const uint8_t *)(uintptr_t)table_1077a[(int16_t)(param_index - 1)];
    set_pickup_glyph();
    for (int i = 0; i < 20; i++)
        records_3a0a6[i * 6] = -1;
    flag_2a8 = 0;
    for (int i = 0; i < 240; i++)
        records_86c[i] = -1;
    flag_a4c = 0;
    word_a4e = 0;
    word_a52 = 0;
    word_1f2 = 4;
    word_354 = -1;
    records_316[0] = -1;
    records_316[3] = -1;
    records_316[6] = -1;
    records_316[9] = -1;
    word_2c0 = -1;
    fall_kind = -1;
    word_2be = 0;
    panel_blank((uint8_t *)image_table[115].data);
    panel_blank((uint8_t *)image_table[116].data);
    panel_blank((uint8_t *)image_table[117].data);
    tick_count = 0;
    for (int i = 0; i < 16; i++)
        bits_1970a[i] = 0;
    if (param_index == 1) {
        table_7a6e[0] = 530;
        table_7a6e[1] = 531;
        table_7a6e[2] = 532;
        table_7a6e[3] = 531;
    } else {
        table_7a6e[0] = 523;
        table_7a6e[1] = 524;
        table_7a6e[2] = 525;
        table_7a6e[3] = 524;
    }
    find_duplicate_records();
    for (int i = 0; i < 200; i++) {
        struct Slot8 *to = &records_19b14[i];
        const struct Rec6 *from = &record_table[i];

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
    game_read_input();
    frame_toggle = (uint16_t)~frame_toggle;
    frame_phase++;
    if ((int16_t)frame_phase >= 2)
        frame_phase = 0;
    if (!(game_buttons & 1))
        fire_latch = 0;
    energy_old = player_energy;
    fill_buffer();
    draw_background();
    draw_records_19334();
    do_nothing_b();
    do_nothing_b();
    redraw_record_images();
    activate_records();
    update_records();
    game_step_4070();
    run_object_lists();
    fn_15cc4_c();
    step_836c();
    value_c6c = 3030;
    flag_20e = -1;
    update_creatures();
    game_step_73d2();
    game_nothing();
    game_nothing();
    draw_effects();
    update_falling_thing();
    draw_marks_316();
    scroll_x_a = scroll_x;
    scroll_y_a = scroll_y;
    player_control();
    scroll_x_b = scroll_x;
    scroll_y_b = scroll_y;
    scroll_x = scroll_x_a;
    scroll_y = scroll_y_a;
    fn_16aee_c();
    do_nothing_b();
    step_190f2();
    do_nothing_b();
    update_records_12a0();
    game_nothing();
    mark_player_cells();
    update_records_1354();
    draw_sprites_721e();
    do_nothing_b();
    unit0_update();
    unit1_update();
    unit2_update();
    unit3_update();
    unit4_update();
    do_nothing_b();
    step_d898();
    move_records_3a0a6();
    draw_sprite_8472();
    update_bolts();
    step_1843e();
    update_score_display();
    draw_score_panel();
    update_item_panel();
    draw_shutter();
    update_energy_display();
    draw_energy_bar();
    draw_item_row();
    scroll_x = scroll_x_b;
    scroll_y = scroll_y_b;
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
        word_185bc = (int16_t)(word_185bc - 1);
        if (word_185bc == 0)
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
        word_4a2--;
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
    const struct PathDescription *description = &ptr_0c9a[number];
    struct PathRecord *record = &ptr_0c9e[number];
    int16_t x, y, tx = 0, ty = 0;

    if ((description->waypoint[0].x | description->waypoint[1].x | description->waypoint[2].x | description->waypoint[3].x) == 0)
        return;
    entry->state = 0xFFFF;
    word_4a2++;
    word_17e9c = 0x4E71;
    if (record->kind < 0) {
        x = description->x;
        y = description->y;
    } else {
        x = record->x;
        y = record->y;
    }
    word_185bc = (int16_t)(description->speed + 1);
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
 * -1).  The kinds 0 and 1 go on to the next waypoint ($18106: not run) */
void path_continue(struct PathRecord *record, const struct PathDescription *description)
{
    path_resume(&record->state);
    record->x = record->state.x;
    record->y = record->state.y;
    if (record->state.count >= 0)
        return;
    const struct PathPoint *point = &description->waypoint[record->waypoint - 1];

    record->x = point->x;
    record->y = point->y;
    if (record->kind <= 1)
        return;   /* goes on at $18106 */
    record->waypoint = (int8_t)0xFF;
}

/* orig $012710: the cells of the map (ptr_0b86 + 8192) at the player are looked at, three rows of one column from the player's x + 16 (to a
 * multiple of 32) and, when his x is 16 or more into a column of 32, of the column before it too: a cell above 2 is an entry
 * (cell_entry_runs) */
void player_tile_scan(void)
{
    int16_t x = (int16_t)((player_x + 16) & -32);
    int16_t y = (int16_t)(player_y & -16);
    const uint8_t *base = ptr_0b86 + 8192 + (x >> 5) + (int16_t)(y << 3);
    const uint8_t *cell = base;

    for (int row = 0; row < 3; row++, cell += 128)
        if (*cell > 2)
            cell_entry_runs(cell);
    if ((player_x & 31) < 16)
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
    text_y = 70;
    text_draw_centered(text);
    copper_setup_done = 1;
    display_reset();
    screens_flip();
    fade_in((const uint16_t *)(background + 2), 0);
}

/* orig $012D44: the text "LOADING LEVEL n" (the level's number is put in the 15th character) is shown (show_text_screen) */
static void loading_level_screen(void)
{
    static const char text[] = "LOADING LEVEL ";

    for (int i = 0; i < 14; i++)
        level_text[i] = (uint8_t)text[i];
    level_text[14] = (uint8_t)(param_index + 93);   /* the original adds 93 to the number: level 1 is '^' */
    level_text[15] = 0;
    show_text_screen((const char *)level_text);
}

/* the tables of the level that the file calfils brings (7000 bytes from data_1aa66 on, loaded from the disk): the entries of the cells (ptr_0cbe, 253), the groups of creatures (ptr_0d0e, 100)
 * and the things on paths (path_descriptions, 25) have words, which are made the machine's own */
static void level_tables_from_disk(void)
{
    for (int i = 0; i < 253; i++)
        file_words(&ptr_0cbe[i], 2);
    for (int i = 0; i < 100; i++) {
        struct CreatureEntry *entry = &ptr_0d0e[i];

        file_words(&entry->start_x, 2);
        file_words(&entry->delay_step, 1);
        file_words(&entry->template_index, 1);
    }
    for (int i = 0; i < 25; i++) {
        struct PathDescription *description = &path_descriptions[i];

        file_words(&description->x, 2);
        file_words(description->waypoint, 8);
    }
    file_words(flags_1c056, 3 * 64);
    file_words(table_1c28a, 60);   /* the pick-ups of the level */
}

/* orig $00323C: when the small font asked for it (menu_state_25a) it is loaded again: the asset set of the level's sprites */
static void reload_sprites(void)
{
    if (menu_state_25a == 0)
        return;
    asset_flag = -1;
    flags_0ddd |= 0x10;
    assets_load((const char *)level_names + NAME_SPRITES, palette_words, asset_end_0ca2, (struct AssetEntry *)asset_table_0ca6);
    palette_words[0] = 0;
    menu_state_25a = 0;
}

/* orig $001C65E: the two longs of table_1c678 that the level names (8 bytes each) go to ptr_0b7e and ptr_0b82 */
static void set_level_pointers(void)
{
    const uint32_t *from = &table_1c678[(int16_t)((param_index - 1) * 2)];

    ptr_0b7e = (uint8_t *)(uintptr_t)from[0];
    ptr_0b82 = (uint8_t *)(uintptr_t)from[1];
}

/* orig $00651E and $002452 to $002C08 (the rts at $016E3E that it calls is not written): Begin Quest.  The level is set up from the files of the disk: the level's names are made from
 * the template, the objects, weapons, text and map files are loaded, the sprites of the level, the pictures; the state is
 * made (level_state_init), two frames are drawn (draw_start_frame) behind the loading screen, which is then faded in: the game loop goes
 * on from here.  (The original puts the stack back to its saved value at $2464; that is not needed here) */
void begin_quest(void)
{
    uint8_t *names = level_names;
    const uint8_t *template = level_name_template;
    uint8_t letter = 'a';
    uint8_t c;

    param_index = 1;
    word_0248 = 1;
    menu_active = 0;
    word_0266 = 0;
    reset_marker_table();
    clear_entry_table();
    clear_cell_records();
    do_nothing();
    background_draw();
    loading_level_screen();
    background = memory_base;
    select_params();
    if (word_0266 != 0)
        letter = 'b';
    do {
        c = *template++;
        if (c == '?')
            c = (uint8_t)('0' + param_index);
        if (c == '*')
            c = letter;
        *names++ = c;
    } while ((int8_t)c >= 0);
    *names++ = *template++;
    names = level_names;
    save_slot_words();
    load_level_objects(names);
    flag_286 = -1;
    load_unpacked((const char *)names + NAME_ALFILS, background);
    for (int i = 0; i < 7000; i++)
        data_1aa66[i] = background[i];
    level_tables_from_disk();
    read_text_table_14852((const uint16_t *)(background + 7000), (uint16_t *)(data_1aa66 + 7000));
    place_pickups();
    {
        uint32_t start = level_starts[(int16_t)(param_index - 1)];

        start_x = (int16_t)(start >> 16);
        start_y = (int16_t)start;
    }
    ptr_0b8a = names;
    load_level_map();
    load_background_tiles(names + NAME_BITS);
    random_table_make();
    scan_cells_d5ee();
    check_flag_286();
    build_table_14cf8();
    if (word_0266 == 0) {
        entry_list[0].address = 0xFFFFFFFF;
        entry_list[0].size = 0xFFFFFFFF;
    }
    load_pattern_data(names);
    flags_0ddd |= 0x10;
    if (word_0266 == 0) {
        const uint32_t *source = &shift_tables[(int16_t)((param_index - 1) * 10)];

        line_text = ptr_0b5a;
        ptr_03ac = ptr_0b5e;
        asset_directory = ptr_0b52;
        if (param_index == 4) {
            line_text = (const uint8_t *)GUEST(226236);
            ptr_03ac = (uint8_t *)GUEST(227836);
            asset_directory = (uint8_t *)entry_list + 2820;
        }
        asset_mem_end = asset_end_28ec;
        asset_table_end = (struct AssetEntry *)asset_table_28f0;
        asset_flag = 0;
        shift_bits = source[0];
        shift_count = 31;
        shift_source = &source[1];
        assets_load((const char *)objects_name, palette_words, asset_mem_end, asset_table_end);
        asset_flag = -1;
        asset_end_0ca2 = asset_mem_end;
        asset_table_0ca6 = (uint8_t *)asset_table_end;
        if (word_0384 == 0) {
            ptr_26de = names + NAME_SPRITES;
            assets_load((const char *)names + NAME_SPRITES, palette_words, asset_mem_end, asset_table_end);
            menu_state_25a = 0;
        }
    }
    set_level_pointers();
    word_dde = 1;
    word_dd2 = 176;   /* the original's value from the start (the bytes of a row of the screen, and the table at $0D9A... of the draw routines) */
    flags_0ddd = 0;
    flags_0ddd |= 2;
    flags_0ddd |= 8;
    ptr_0de0 = (const int16_t *)GUEST(296);
    if (word_0384 != 0)
        return;   /* goes on at $013568 */

    /* $002AAA */
    copper_colors_restore();
    hud_digits_load();
    clear_entry_table();
    word_276 = 0;
    message_shown = 0;
    tick_hold_26c = 0;
    clear_work_area();
    demo_mode = 0;
    reload_sprites();
    word_226 = 0;
    word_228 = 0;
    message_shown = 0;
    message_arg = 0;
    level_state_init();
    widen_bytes_to_words();
    callback_4e0 = (void (*)(void))GUEST(45010);
    flag_23a = 0;
    code_c58e = 0x4E75;
    code_cb0e = 0x4E75;
    code_c406 = 0x4E75;
    code_bc1a = 0x4E75;
    sort_records_19b14();
    fill_flags_39fde();
    activate_records_now();
    frame_wait = 2;
    word_1de = 0;
    fill_cce_ones();
    update_energy_display();
    if (menu_active != 0) {
        if (param_index != 1)
            return;   /* goes on at $00334E */
        menu_active = 0;
    }
    file_read((const char *)sound1_name, picture_buffer);
    fade_out(0, 0xFFFF);
    copper_setup_done = 0;
    draw_start_frame();
    draw_start_frame();
    stop_game_sounds();
    play_sound(9, 127);
    screens_flip();
    palette_words[0] = 0;
    fade_in(palette_words, 1);
    word_dde = 1;
    word_ddc = 0;
    flags_0ddd |= 8;
    flags_0ddd |= 2;
    tick_hold_26c = 0;
    word_0248 = 0;
    drives_idle();
    if (flag_aa8 != 0)
        play_sound(60, 2);
    else
        stop_sound(60);
    if (param_index == 3)
        play_sound(66, 1);
    game_loop();
}
