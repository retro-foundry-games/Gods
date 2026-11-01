/* Gods, the game: the player of one frame, the state machine of $10368 and everything the states of its table (at $10280) go on with.  Each
 * function says where the original has it (`orig $ADDR`).
 *
 * The state (player_state) names a routine of the table; it gets the animation frame (word_19a, d7 in the original) and ends in $1263A, which
 * keeps the frame and goes on at $1263E (scroll and draw).  A routine here is a C function with the frame as its parameter; `player_end(d7)`
 * is the original's jump to $1263A.  The routines of the states 10, 11, 12, 22 and 23 (falling), 8, 9, 20 and 21 (jumping) and 13 and 14
 * (ladders) are each other's mirror images but are written out as they are in the original, with the differences it has. */
#include "menu.h"
#include "sound.h"

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
        uint16_t *entry = 0;

        for (int e = 0; e < 64; e++) {
            uint16_t *p = (uint16_t *)flags_1c056 + 3 * e;

            if ((((uint32_t)p[0] << 16) | p[1]) == place) {
                entry = p;
                break;
            }
        }
        if (entry == 0)
            continue;
        if (!record_touches_player(record))
            continue;
        if (kind->w[0] == 288 || kind->w[0] == 290 || kind->w[0] == 292 || kind->w[0] == 232) {
            ptr_0ba2 = entry;
            word_2ba = 0;
        } else {
            record->w[2] ^= 1;
            entry[2] ^= 1;
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
