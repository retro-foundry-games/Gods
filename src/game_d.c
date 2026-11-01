/* Gods, the game: the routines at $417C and $44D2 and what they call.  Each function says where the original has it (`orig $ADDR`). */
#include "menu.h"

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

/* orig $017100 (with the common blit of $017208 written out): an image five words wide (the part of the screen is `dst`, its four planes
 * `plane` bytes apart from `src`, `rows` + 1 rows high, shifted `shift` pixels to the right) is drawn with the mask the blitter
 * makes of its planes in mask_buffer.  blit_extra_modulo is cleared */
void blit_image_5wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    uint16_t size = (uint16_t)((rows << 6) | 5);
    uint16_t dst_mod = 44 - 10;
    uint16_t first_mask = 0xffff, last_mask = 0;
    uint16_t con0 = 0x0fca, con1 = 0;
    uint16_t a_mod = 0, b_mod;
    const uint8_t *s = src;
    uint8_t *mask = (uint8_t *)mask_buffer;
    uint8_t *d = dst, *dd = dst;

    blit_extra_modulo = 0;
    shift &= 0xff;

    custom->dmacon = 0x8400;
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    /* the mask: the planes 0, 1 and 2 ORed ... */
    custom->bltapt = ADDR(s);
    s += (int16_t)plane;
    custom->bltbpt = ADDR(s);
    s += (int16_t)plane;
    custom->bltcpt = ADDR(s);
    s += (int16_t)plane;
    custom->bltdpt = ADDR(mask);
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltdmod = 0;
    custom->bltcon1 = 0;
    custom->bltcon0 = 4094;
    custom->bltamod = 0;
    custom->bltbmod = 0;
    custom->bltcmod = 0;
    custom->bltsize = size;
    /* ... and the plane 3 */
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltbmod = 0;
    custom->bltcon0 = 3580;
    custom->bltapt = ADDR(s);
    custom->bltbpt = ADDR(mask);
    custom->bltdpt = ADDR(mask);
    custom->bltsize = size;
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->bltafwm = first_mask;
    custom->bltalwm = last_mask;
    if (shift == 0) {
        custom->bltcon0 = con0;
        custom->bltalwm = 0xffff;
        a_mod = 0;
        custom->bltamod = a_mod;
        custom->bltcon1 = 0;
        b_mod = a_mod + blit_extra_modulo;
        custom->bltbmod = b_mod;
    } else {
        while (custom->dmaconr & DMAF_BLTBUSY)
            ;
        con1 = (uint16_t)(shift << 12);
        custom->bltcon1 = con1;
        con0 = con1 | 0x0fca;
        custom->bltcon0 = con0;
        /* blit_extra_modulo is 0 here (not negative): one word wider, the destination modulo less 2 */
        dst_mod -= 2;
        size++;
        a_mod = (uint16_t)-2;
        custom->bltamod = a_mod;
        b_mod = a_mod + 0;
        custom->bltbmod = b_mod;
    }
    custom->bltcmod = dst_mod;
    custom->bltdmod = dst_mod;

    /* the four planes of the image: the mask (A), the plane (B) and the screen (C and D) */
    int16_t stride = (int16_t)blit_plane_stride;
    s = src;
    for (int i = 0; i < 4; i++) {
        if (i != 0) {
            d += stride;
            dd += stride;
            s += (int16_t)plane;
            while (custom->dmaconr & DMAF_BLTBUSY)
                ;
        }
        custom->bltcpt = ADDR(d);
        custom->bltbpt = ADDR(s);
        custom->bltapt = ADDR(mask);
        custom->bltdpt = ADDR(dd);
        if (i == 3)
            custom->dmacon = 0x0400;
        custom->bltsize = size;
    }
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
    fn_12710();

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
        fn_10976();              /* orig $010976, game_e writes it */
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
        score_add(points >> 3);
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
            value_1296 = 0x121c;
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
    value_1296 = 0x121c;
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
