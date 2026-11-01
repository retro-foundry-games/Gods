/* Gods, the game: the routines at $8000-$FFFF and $16800 and up (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */
#include "menu.h"

/* orig $00BA34, $00BC1A, $00C374, $00D6B8, $00DB04, $00DE54, $00E142, $00E3A2, $017CC0, $0194D0, $018C24 and $018C2E: nothing;
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

/* orig $01E372: the blitter gets priority over the processor until it is done */
void blitter_grab_wait(void)
{
    custom->dmacon = 0x8400;
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->dmacon = 0x0400;
}

/* orig $017082: one column of a tile is copied by four blits of (rows + 1) rows and two words (A to D, no mask): the source
 * moves by src_step and the destination by dst_step bytes between them */
void blit_tile(int16_t src_step, int16_t dst_step, int16_t rows, const uint8_t *src, uint8_t *dst)
{
    uint16_t size = 1026;

    blitter_grab_wait();
    if (rows != 15)
        size = (uint16_t)(((rows + 1) << 6) | 2);
    custom->bltapt = ADDR(src);
    custom->bltdpt = ADDR(dst);
    custom->dmacon = 0x8400;
    blit_start(size);
    dst += dst_step;
    src += src_step;
    for (int i = 0; i < 2; i++) {
        while (custom->dmaconr & DMAF_BLTBUSY)
            ;
        custom->bltapt = ADDR(src);
        custom->bltdpt = ADDR(dst);
        blit_start(size);
        dst += dst_step;
        src += src_step;
    }
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltapt = ADDR(src);
    custom->bltdpt = ADDR(dst);
    custom->dmacon = 0x0400;
    blit_start(size);
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

/* orig $017044: the blitter is set up for plain copies (A to D, full masks, source modulo 0, destination modulo 40); the
 * original also leaves 64 in d4 and $2100 in d5, the steps between the four blits of a tile */
void blitter_setup_copy(void)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltcon0 = 2544;
    custom->bltcon1 = 0;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltamod = 0;
    custom->bltdmod = 40;
}

/* orig $017814 and $01786E..: the whole screen (4 planes of 192 rows, 44 bytes a row) is copied by the blitter from `from` to `to` (the
 * original waits for the blitter first, clears the first/last word masks and the modulos, and the copy is A to D with no shift) */
static void copy_screen(const uint8_t *from, uint8_t *to)
{
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt = ADDR(from);
    custom->bltdpt = ADDR(to);
    custom->bltdmod = 0;
    custom->bltamod = 0;
    custom->bltcon1 = 0;
    custom->bltcon0 = 2544;
    custom->dmacon = 0x0400;
    blit_start(0xC016);
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
void step_18072(uint8_t *a1, const uint8_t *a2)
{
    word_185bc = (int16_t)(int8_t)a2[5] + 1;
    if ((int8_t)a1[5] < 0)
        return;
    path_continue(a1, a2);
}

/* orig $017FA0: the picture of a record (a2: byte +4 is the number of a 256-byte block of the background, +26 and +27 the size
 * in 32 pixel columns and 16 line rows, less one) is made an image at the end of the image table and drawn with sprite_draw at
 * (x, y), less the scroll, in columns and rows */
void draw_record_image(int16_t x, int16_t y, const uint8_t *a2)
{
    struct Image *image = (struct Image *)asset_table_end;
    int16_t columns = a2[26] & 7;
    int16_t rows = (int8_t)a2[27];

    image->height = 15;
    image->data = (const uint16_t *)(background + ((int32_t)(int8_t)a2[4] << 8));
    uint16_t index = (uint16_t)((int16_t)((uint32_t)asset_table_end - (uint32_t)image_table) >> 3);

    x -= scroll_x;
    y -= scroll_y;
    int16_t first_x = x;

    for (int16_t r = rows; r >= 0; r--) {
        columns = a2[26] & 7;
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
struct SavedByte *mark_record_cells(int16_t x, int16_t y, const uint8_t *a2, struct SavedByte *list)
{
    if (a2[26] & 0x80)
        return list;
    int16_t col = (int16_t)(x + 16) & -32;
    int16_t row = (int16_t)(y + 8) & -16;
    uint8_t *cell = ptr_0b86 + 8192 + (col >> 5) + (int16_t)(row << 3);
    uint8_t *line = cell;
    int16_t lines = (int8_t)a2[27];

    for (int16_t r = lines; r >= 0; r--) {
        uint8_t *p = line;

        for (int16_t c = a2[26] & 7; c >= 0; c--) {
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
void take_record_to_199ac(uint16_t *a0)
{
    a0[0] = 0xFFFF;
    uint8_t *entry = ptr_0d0e + (int16_t)(a0[1] << 4);
    uint8_t *slot = table_199ac;
    int i;

    for (i = 0; i < 10; i++, slot += 36)
        if (*(uint16_t *)(slot + 6) == 0)
            break;
    if (i == 10)
        return;
    uint8_t *out = slot + 6;
    const uint16_t *w = (const uint16_t *)entry;

    *(uint16_t *)out = 2;
    *(uint16_t *)(out + 2) = w[0];
    *(uint16_t *)(out + 4) = w[1];
    out[6] = entry[4];
    out[7] = entry[5];
    *(uint16_t *)(out + 8) = w[3];
    out[10] = entry[8];
    out[11] = entry[9];
    int16_t from = (int16_t)(w[5] << 4);
    out[12] = entry[12];
    out[13] = entry[13];
    uint8_t last = entry[14];
    const uint8_t *src = ptr_0b82 + 2 + from;

    *(uint16_t *)(out + 14) = *(const uint16_t *)src;
    *(uint32_t *)(out + 16) = *(const uint32_t *)(src + 2);
    *(uint32_t *)(out + 20) = *(const uint32_t *)(src + 6);
    *(uint32_t *)(out + 24) = *(const uint32_t *)(src + 10);
    out[28] = last;
}


/* orig $018900: like take_record_to_199ac, but the entry's record (its word +2 numbers 8 bytes of ptr_0cb2) is made into the first
 * free one of the five records of 24 bytes of records_18740 (a state 0): state 2, the 8 bytes (the byte +6 split into two
 * nibbles), and 14 bytes copied from the table ptr_0b7e + 2 (16 bytes for each of the signed byte +5); when none is free that is
 * all */
void take_record_to_18740(uint16_t *a0)
{
    a0[0] = 0xFFFF;

    const uint8_t *s = ptr_0cb2 + (int16_t)(a0[1] << 3);
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
static void record_seen(const uint8_t *a1, const uint8_t *a2)
{
    int16_t sx = (int16_t)(*(const int16_t *)a1 - scroll_x);
    int16_t sy = (int16_t)(*(const int16_t *)(a1 + 2) - scroll_y);

    if (sx > -32) {
        if (sx > 320)
            return;
        if (sy > -16) {
            if (sy <= 192)
                flags_3b0 |= 0x80;
            return;
        }
    }
    int16_t wide = (int16_t)(((int8_t)a2[27] << 5) + sx);
    int16_t high = (int16_t)(((a2[26] & 7) << 4) + sy);

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
    uint8_t *a1 = ptr_0c9e;
    uint8_t *a2 = ptr_0c9a;
    int16_t x;
    int16_t y;

    for (int16_t n = 24; n >= 0; n--, a1 += 24, a2 += 28) {
        if ((int8_t)a1[4] < 0)
            continue;
        uint32_t before = *(uint32_t *)a1;

        step_18072(a1, a2);
        if (*(uint32_t *)a1 != before) {
            record_seen(a1, a2);
        }
        x = *(int16_t *)a1;
        y = *(int16_t *)(a1 + 2);

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
static void empty_slots_of_record(const uint16_t *a1)
{
    uint16_t kind = a1[8] & 0x7FFF;

    if (!(kind == 2 || kind == 3 || kind == 4 || kind == 7 || kind == 8))
        return;
    for (int p = 0; p < 6; p += 2) {
        int16_t item = (int16_t)a1[p + 1];

        if (a1[p] == 1 && ((item >= 0x12 && item <= 0x17) || (item >= 0x7F && item <= 0x81)))
            empty_first_slot_with(item);
    }
}

/* orig $018A22, $018A2C and $018A5C: a record whose word +16 has the top bit set empties every slot of item_slots that holds the
 * item each of its pairs of words names (the pair's first word 1) when the item's table_14cf8 entry has the word +4 at 0 or 2 */
static void empty_slots_of_flagged_record(const uint16_t *a1)
{
    if (!(a1[8] & 0x8000))
        return;
    for (int p = 0; p < 6; p += 2) {
        if (a1[p] != 1)
            continue;
        int16_t item = (int16_t)a1[p + 1];
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
static void kind_drop_item(const uint16_t *a1)
{
    int16_t x = (int16_t)a1[6], y = (int16_t)a1[7], number = (int16_t)a1[9];
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
static void kind_drop_192(const uint16_t *a1)
{
    int16_t x = (int16_t)a1[6], y = (int16_t)a1[7], number = (int16_t)(a1[9] + 0xC0);
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
static void kind_place_19334(const uint16_t *a1, int twenty)
{
    struct Slot8 *record = first_free_slot8(records_19334, twenty ? 20 : 10);

    if (record == 0)
        return;
    int16_t x = (int16_t)((a1[6] & 0xFFE0) - 0x10);
    int16_t y = (int16_t)((a1[7] + 8) & 0xFFF0);

    if (record_19334_at(x, y))
        return;
    record->w[0] = x;
    record->w[1] = y;
    record->w[2] = twenty ? 1 : 3;
    record->w[3] = (int16_t)a1[9];
    if (twenty)
        play_sound(7, 5);
}

/* orig $0192A4 and $0192D6 (kind 2): the block of the tile map (ptr_0b86) at the record a1's position (x rounded to 32, y 8
 * further and rounded to 16) is set to six tiles counted up from tile_24d and the three cells of the second plane (+8192)
 * cleared; the sound 7 is played (priority 5) */
static void kind_set_tiles(const uint16_t *a1)
{
    int16_t x = (int16_t)(((a1[6] & 0xFFE0) + 12) & 0xFFE0);
    int16_t y = (int16_t)((a1[7] + 8) & 0xFFF0);
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
static void kind_burst(const uint16_t *a1)
{
    int16_t x0 = (int16_t)(a1[6] - 12), y0 = (int16_t)(a1[7] - 12);
    int16_t x1 = (int16_t)(a1[6] + 12), y1 = (int16_t)(a1[7] + 12);
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
static void kind_free_slot_ref(const uint16_t *a1)
{
    int16_t number = (int16_t)a1[9];

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
void finish_record_1895a(uint16_t *a0)
{
    if (flag_230)
        return;

    const uint16_t *a1 = (const uint16_t *)(buffer_cb6 + (int32_t)((int16_t)a0[1] * 24));

    *(int32_t *)words_300 = -1;
    words_300[2] = -1;
    call_by_kind((int16_t)a1[0], (int16_t)a1[1], &words_300[0]);
    call_by_kind((int16_t)a1[2], (int16_t)a1[3], &words_300[1]);
    call_by_kind((int16_t)a1[4], (int16_t)a1[5], &words_300[2]);
    if ((int16_t)(words_300[0] & words_300[1] & words_300[2]) >= 0)
        return;
    a0[0] = 0xFFFF;

    const int16_t *offset = (const int16_t *)(ptr_0cba + (uint16_t)(a1[10] * 2));

    if (*offset >= 0) {
        message_next = ptr_0b66 + *offset;
        if (*message_next != 0)
            message_request(50);
    }
    empty_slots_of_record(a1);
    empty_slots_of_flagged_record(a1);
    switch (a1[8] & 0x7FFF) {
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
        run_cell_entry((int16_t)(a1[9] - 1));
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
