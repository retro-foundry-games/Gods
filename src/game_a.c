/* Gods, the game: the routines at $0000-$7FFF (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */
#include "menu.h"
#include "sound.h"

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

/* orig $005496: word_2bc is set to 1 unless word_2c0 is 12 (the original goes on at $54A6 then, which did not run) */
void game_step_5496(void)
{
    if (word_2c0 != 12) {
        word_2bc = 1;
        return;
    }
    /* goes on at $54A6 */
}

/* orig $0073D2: nothing is done while word_73ce is 0; the original goes on at $73D8 otherwise, which did not run */
void game_step_73d2(void)
{
    if (word_73ce == 0)
        return;
    /* goes on at $73D8 */
}

/* orig $005608: nothing is done while word_2ce and word_2cc are equal; the original goes on at $5612 otherwise, which did not run */
void game_step_5608(void)
{
    if (word_2ce == word_2cc)
        return;
    /* goes on at $5612 */
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
 * the caller left; the caller (at $4654) leaves it clear */
void score_add(int16_t points)
{
    int carry = 0;

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
