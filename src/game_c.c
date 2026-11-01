/* Gods, the game: the routines at $10000-$167FF (what game_frame in game.c calls and what that calls).  Each function says where the original has
 * it (`orig $ADDR`). */
#include "menu.h"
#include "sound.h"

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
static void shot_hit_tile(const struct Object *object, struct Shot *shot, int16_t x, int16_t y)
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
        } else {
            return;   /* goes on at $167E0, $167F2 or $16858 (draw types 1 to 3), which did not run */
        }
    }

    int16_t frame = object->frame;

    if (object->anim_mode >= 0) {
        frame++;
        if (frame > object->max_frame)
            frame = 0;
        object->frame = frame;
    }
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
    case 2:
        break;
    default:
        break;   /* goes on at $168AE, $15DB2, $1645C or $1661E */
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

/* ---- the player: the state machine of $10368.  The state (player_state) names a routine of the table at $10280; it gets the animation
 * frame (word_19a, d7 in the original) and ends in $1263A, which keeps the frame and goes on at $1263E (scroll and draw).  A routine here
 * is a C function with the frame as its parameter; `player_end(d7)` is the original's jump to $1263A. ---- */

static void player_after(int16_t frame);
static void state_12290(int16_t d7);

/* orig $01263A */
static void player_end(int16_t d7)
{
    word_19a = d7;
    player_after(d7);
}

/* orig $01263E: the tiles around the player are looked at ($12710), the scroll follows the player (sideways when he is within
 * scroll_margin of the screen's edge, up and down toward the middle) and the player's sprite is drawn: the first sprite of his state plus
 * the frame (52 is shown as 25, 54 as 35 and 55 as 38) */
static void player_after(int16_t d7)
{
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

    int16_t sprite = (int16_t)(state_table[player_state * 2] + d7);

    if (sprite == 52)
        sprite = 25;
    else if (sprite == 54)
        sprite = 35;
    else if (sprite == 55)
        sprite = 38;
    draw_player_sprite(sprite);
}

/* orig $011CD2 (state 15): the player stands on a tile of type 1 (three rows down) -> ends; the rest did not run ($11CE0) */
static void state_11cd2(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] == 1)
        player_end(d7);
    /* else goes on at $11CE0 */
}

/* orig $011ED6: a step up the ladder: the frame goes on, the player 6 rows up; ends when the tile below is a ladder (type 2) */
static void ladder_up_11ed6(int16_t d7)
{
    word_360 = 0;
    word_362 = 0;
    if (word_376 != 0)
        return;   /* goes on at $11F08 */
    d7++;
    if (d7 > 3)
        return;   /* goes on at $11EFE */
    word_37a = player_y;
    player_y -= 6;

    uint8_t *tile = player_tile_ptr();

    if (tile[128] == 2)
        player_end(d7);
    /* else goes on at $11F24 */
}

/* orig $011F3C: a jump to the right begins (state 9): sound 40, the frame 12 */
static void jump_right_11f3c(void)
{
    player_state = 9;
    jump_start_x = player_x;
    jump_step = 0;
    play_sound(40, 2);
    player_end(12);
}

/* orig $011D12 (state 14, on a ladder) */
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
        if (game_buttons & 1)
            return;   /* goes on at $11D60 */
        if (game_stick & 1) {
            jump_right_11f3c();
            return;
        }
        if (game_stick & 2)
            return;   /* goes on at $11D7C */
    }
    if (game_stick & 4)
        return;   /* goes on at $11DDC */
    if ((game_buttons & 1) && word_370 != 0)
        return;   /* goes on at $11E60 */
    if (game_stick & 2)
        return;   /* goes on at $11EC0 */
    player_end(d7);
}

/* orig $011FCA (state 0, walking to the left) */
static void state_11fca(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1)
        return;   /* goes on at $11FD6 */
    if (game_stick & 8)
        return;   /* goes on at $1200C */
    if (game_stick & 2) {
        if (!record_near_player()) {
            tile = player_tile_ptr();

            int16_t d0 = player_x & 31;

            if (d0 >= 12 && tile[385] == 2)
                return;   /* goes on at $12042 */
            d0 = player_x & 31;
            if (d0 <= 20 && tile[384] == 2)
                return;   /* goes on at $12054 */
        }
        player_state = 19;
        word_368 = 0;
        word_1fa = 0;
        word_35c = 0;
        player_end(0);
        return;
    }
    if (game_stick & 1) {
        if (game_stick & 4)
            return;   /* goes on at $120B6 */

        int16_t d0 = player_x & 31;

        if (d0 <= 20) {
            if (tile[0] == 2)
                goto at_ladder;
            if (d0 < 12)
                goto not_up;
        }
        if (tile[1] != 2)
            goto not_up;
        player_x += 32;
at_ladder:
        player_x &= -32;
        word_370 = 0;
        player_state = 14;
        word_376 = 0;
        word_360 = 0;
        word_362 = 0;
        player_end(26);
        return;
    }
not_up:
    if (game_buttons & 1)
        return;   /* goes on at $12134 */
    if (word_19e != 0)
        return;   /* goes on at $121D6 */
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
        if (d7 > 7)
            return;   /* goes on at $121C6 */
        player_end((int16_t)((d7 + 1) & 7));
        return;
    }
    if (game_stick & 1) {
        word_35e = 0;
        player_state = 26;
        player_end(-1);
        return;
    }
    /* else goes on at $12278 */
}

/* orig $0124DE: the frame is kept and mapped through frame_map_125ac; the machine goes on at $1263E (state 5) */
static void state_5_map(int16_t d7)
{
    word_19a = d7;
    player_after((int16_t)frame_map_125ac[d7]);
}

/* orig $012290 (state 1, walking to the right) */
static void state_12290(int16_t d7)
{
    uint8_t *tile = player_tile_ptr();

    if (tile[384] != 1)
        return;   /* goes on at $1229C */
    if (game_stick & 4) {
        player_state = 3;
        player_end(2);
        return;
    }
    if (game_stick & 2)
        return;   /* goes on at $122E6 */
    if (game_stick & 1) {
        if (!(game_stick & 8))
            return;   /* goes on at $123A2 */
        player_state = 9;
        jump_start_x = player_x;
        jump_step = 0;
        play_sound(40, 2);
        player_end(0);
        return;
    }
    if ((game_buttons & 1) && fn_fdba() == 0) {
        player_state = 5;
        state_5_map(0);
        return;
    }
    if (word_19e != 0)
        return;   /* goes on at $1249C */
    if (!(game_stick & 8)) {
        if (game_stick & 1)
            return;   /* goes on at $1220C */
        if (d7 == 0)
            player_end(d7);
        else
            player_end(45);
        return;
    }
    if ((player_x & 31) < 8 && (tile[1] == 1 || tile[129] == 1 || tile[257] == 1)) {
        player_end(d7);
        return;
    }
    player_x += 4;
    if (d7 == 2 || d7 == 6)
        play_sound(1, 2);
    word_19e = 1;
    word_19e = 0;
    if (d7 > 7)
        return;   /* goes on at $1248C */
    player_end((int16_t)((d7 + 1) & 7));
}

/* orig $0124BA (state 5): the frame goes on; at 3 the shots are spawned; from 5 on, while fire is not held, the player goes back to state 1 */
static void state_124ba(int16_t d7)
{
    d7++;
    if (d7 == 3)
        spawn_player_shots();
    if (d7 >= 5) {
        if (!(game_buttons & 1)) {
            player_state = 1;
            state_12290(2);
        }
        return;   /* else goes on at $124D4 */
    }
    state_5_map(d7);
}

/* orig $012538 (state 2) */
static void state_12538(int16_t d7)
{
    if (game_stick & 4)
        return;   /* goes on at $1253E */
    d7++;
    if (d7 < 3) {
        player_end(d7);
        return;
    }
    player_state = 1;
    player_end(6);
}

/* orig $012560 (state 3) */
static void state_12560(int16_t d7)
{
    if (game_stick & 8)
        return;   /* goes on at $12568 */
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
    if (game_stick & 4)
        return;   /* goes on at $1259E */
    state_11cd2(d7);
}

/* orig $01159A (state 16, landing) */
static void state_1159a(int16_t d7)
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
    player_state = 1;
    player_end(2);
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
        if (player_x != jump_start_x && (player_x & 31) == 0)
            return;   /* goes on at $1163E */
    }
    if (game_stick & 2)
        return;   /* goes on at $11674 */
    if (jump_step >= 22 && tile_one_three_rows_down() != 0)
        goto land;
    player_y -= (int16_t)jump_deltas[jump_step >> 1];
    if (countdown_220 != 0)
        return;   /* goes on at $1170C */
    if (tile_one_at_player() != 0) {
        player_y &= -16;
        player_y += 16;
        player_state = 12;
        fall_rows_36a = 0;
        air_steps = 0;
        fall_distance = 0;
        player_end(0);
        return;
    }
    if (jump_step >= 22 && tile_one_three_rows_down() != 0)
        goto land;
    if (jump_step >= 4 && (game_buttons & 1) && fn_fdba() == 0 && jump_step < 44)
        return;   /* goes on at $11770 */
    jump_step += 2;
    if (jump_step < 46) {
        player_end(d7);
        return;
    }
    player_state = 12;
    fall_rows_36a = 0;
    air_steps = 0;
    fall_distance = 0;
    player_end(0);
    return;
land:
    player_state = 16;
    player_y &= -16;
    word_1e6 = 0;
    play_sound(16, 3);
    player_end(0);
}

/* orig $010EB6 (state 12, falling to the right) */
static void state_10eb6(int16_t d7)
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
                goto grounded;
            if ((player_y & 15) != 0 && tile[385] == 1)
                goto grounded;
        }
        player_x += 4;
        air_steps++;
    }
grounded:
    tile = player_tile_ptr();
    d7 = 0;
    if (tile[384] != 1 && ((player_x & 28) < 8 || tile[385] != 1)) {
        if (!(game_buttons & 1)) {
            player_end(d7);
            return;
        }
        if (fn_fdba() != 0) {
            player_end(d7);
            return;
        }
        player_state = 22;
        word_36e = 1;
        player_end(1);
        return;
    }
    player_state = 16;
    player_y &= -16;
    word_1e6 = 0;
    d7 = 0;
    play_sound(16, 3);

    int16_t excess = fall_distance - 20;

    if (excess > 0 && flag_aa8 == 0)
        player_energy -= excess >> 1;
    player_end(d7);
}

/* orig $01056A (state 19, in front of a thing the player can take or open) */
static void state_1056a(int16_t d7)
{
    if (!(game_stick & 2)) {
        if (slots_30c[word_2cc] >= 0)
            return;   /* goes on at $10582 (calls $10B80) */
        d7--;
        if (d7 >= 0) {
            player_end(d7);
            return;
        }
        player_state = 0;
        player_end(2);
        return;
    }
    d7 = 1;
    if (word_35c != 0) {
        if (word_1fa == 0)
            return;   /* goes on at $10AB8 */
        if (game_buttons & 1)
            return;   /* goes on at $10AB2 */
        if (slots_30c[word_2cc] >= 0)
            return;   /* goes on at $105C8 (calls $10B80) */
        player_end(d7);
        return;
    }
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
        record->w[2] = -1;
        record->w[3] = 0;
        return;   /* goes on at $10A48 */
    }
    word_1fa = 1;
    player_end(d7);
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
        fn_3c94();
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
    player_x &= -32;
    player_state = 14;
    word_376 = 0;
    word_360 = 0;
    word_362 = 0;
    word_370 = 0;
    player_end(26);
}

/* orig $010368: the player for one frame: the routine of the state named by player_state runs (see the table at $10280): 0 and 1 walk, 2 to
 * 5 turn and shoot, 9 jumps, 12 falls, 14 climbs, 16 lands, 19 handles things, 26 mounts a ladder, 27 and 28 leave it.  The states 6 to 8, 10,
 * 11, 13, 17, 18, 20 to 25 did not run in the recorded play ($11390, $10D44, $10BB2, $11A94, ...) */
void player_control(void)
{
    int16_t d7 = word_19a;
    int16_t state = player_state;

    if (state != 18 && state != 19)
        fn_5496();
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
    case 9:  state_115bc(d7); break;
    case 12: state_10eb6(d7); break;
    case 14: state_11d12(d7); break;
    case 15: state_11cd2(d7); break;
    case 16: state_1159a(d7); break;
    case 19: state_1056a(d7); break;
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
        break;   /* goes on at the state's routine in the table, which did not run */
    }
}
