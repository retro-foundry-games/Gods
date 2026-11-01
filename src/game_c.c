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
