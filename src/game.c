/* Gods, the game: what runs while a level is played.  The loop that calls it all is at $2C20; the routines are written in the
 * order they were found, named by what they do (a provisional name is `fn_` and the address). */
#include "menu.h"

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

/* orig $006136: the first of the twenty records of 8 bytes ptr_0cce points at whose fourth word is not negative (the others are
 * free), or 0.  The original goes on with the record at $6186, which did not run */
struct Slot8 *find_record_cce(void)
{
    struct Slot8 *record = (struct Slot8 *)ptr_0cce;

    for (int i = 0; i < 20; i++, record++)
        if (record->w[3] >= 0)
            return record;
    return 0;
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

/* orig $018A10: the routine the kind (0, 1, ...) names in dispatch_18be0 is called */
void call_by_kind(int16_t kind)
{
    dispatch_18be0[kind]();
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
    fn_6002();
    if (flag_26e)
        fn_30c6();
    fn_178c6();
    fn_1924c();
    fn_194d0();
    fn_17cc0();
    fn_ba34();
    if (flag_23a)
        callback_4e4();
    fn_17e9c();
    if (param_index == 2 || flag_23a != 1) {
        fn_3ea4();
        fn_417c();
        fn_7faa();
        fn_4070();
    }
    fn_15b04();
    fn_15cc4();
    fn_836c();
    value_c6c = 3030;
    flag_20e = -1;
    fn_6ed0();
    if (flag_23a == 0) {
        fn_73d2();
        fn_77e6();
        fn_7906();
        fn_3f8e();
        fn_18d14();
    }
    scroll_x_a = scroll_x;
    scroll_y_a = scroll_y;
    if (flag_290 >= 0)
        fn_33d4();
    else
        fn_10368();
    fn_9912();
    scroll_x_b = scroll_x;
    scroll_y_b = scroll_y;
    scroll_x = scroll_x_a;
    scroll_y = scroll_y_a;
    fn_16aee();
    if (flag_23a == 0) {
        fn_db04();
        fn_190f2();
        fn_e142();
        fn_de54();
        fn_9ae6();
        fn_3436();
        fn_3c56();
        fn_8a54();
        fn_7292();
        fn_e3a2();
        fn_e486();
        fn_e76c();
        fn_ea52();
        fn_ed38();
        fn_f01e();
        fn_d47e();
        fn_d314();
        fn_d6b8();
        fn_d898();
    } else {
        callback_4e0();
        fn_c58e();
        fn_cb0e();
        fn_c406();
    }
    fn_bc1a();
    fn_3dfc();
    fn_8452();
    fn_6136();
    fn_1843e();
    fn_c374();
    fn_5842();
    fn_57fc();
    fn_5608();
    fn_54c4();
    fn_5e02();
    fn_5d76();
    fn_3806();
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
        goto activate;
    }
    d0 = scroll_y - word_2aa;
    if (d0 < 0)
        d0 = -d0;
    if (!(d0 & 0x20))
        return;
activate:
    flag_b24 = 1;
    word_2aa = scroll_y;

    int16_t x0 = (scroll_x & -32) - 64;
    int16_t y0 = (scroll_y & -16) - 48;
    int16_t x1 = (scroll_x & -32) + 384;
    int16_t y1 = (scroll_y & -16) + 240;

    for (int i = 0; i < 200; i++) {
        struct Slot8 *record = &records_19b14[i];
        uint8_t *last_byte = (uint8_t *)record + 7;

        if (record->w[2] < 0 || record->w[0] < x0 || record->w[0] > x1 || record->w[1] < y0 || record->w[1] > y1) {
            record->w[3] = 0;
            continue;
        }
        if (!(*last_byte & 1)) {
            *last_byte |= 1;
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
    value_1296 = 0x121c;
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

/* orig $0058C8: one digit (0 to 15) of digit_font is put in the four planes (150 bytes apart) of the score image, 8 rows of a byte, a
 * row 10 bytes on */
static void draw_score_digit(uint8_t *to, uint16_t digit)
{
    const uint8_t *from = digit_font + (digit << 5);

    for (int row = 0; row < 8; row++) {
        to[0] = *from++;
        to[150] = *from++;
        to[300] = *from++;
        to[450] = *from++;
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
    fn_5af6();
}
