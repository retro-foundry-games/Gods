/* Gods, the game: the shot routines of the object lists at $168AE and $16A66 (what game_c.c's update_shots and run_object_list do not
 * cover).  Each function says where the original has it (`orig $ADDR`). */
#include "menu.h"
#include "sound.h"

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
