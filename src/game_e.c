/* Gods, the game: the routines at $6ED0, $12924, $12A9C, $12796, $10976 and $15282 and what they call.  Each function says where the original has it (`orig $ADDR`). */
#include "menu.h"
#include "sound.h"

/* the creatures of a level: ten records of 36 bytes at $199AC (table_199ac), each the type of a group of up to 255 creatures whose 14
 * byte records (struct Creature) it points to.  Its movement and step routines are the original's (addresses in the record); only
 * the ones that ran are written (see call_move / call_step) */
struct Creature {
    int16_t x;
    int16_t y;
    int16_t state;     /* +4 0 or more: the step of its animation, -1: just hit, then counts up to 7 while it dies */
    int16_t delay;     /* +6 negative or positive: frames to go before it appears; 0 it is there */
    int16_t hp;        /* +8 0: gone; negative: dying */
    int16_t direction; /* +10 */
    int16_t timer;     /* +12 */
};

struct CreatureType {
    struct Creature *creatures;   /* +0 */
    int16_t count;                /* +4 */
    uint16_t status;              /* +6 bit 1: in use, bit 0: started */
    int16_t start_x;              /* +8 */
    int16_t start_y;              /* +10 */
    int8_t direction;             /* +12 */
    int8_t bounds;                /* +13 0, 1 none; 2 left, 3 right, 4 top, 5 bottom: the creature is gone when it leaves the screen that way */
    int16_t delay_step;           /* +14 */
    uint8_t number;               /* +16 */
    uint8_t value;                /* +17 its hit points, and the score it gives */
    uint8_t flag_18;              /* +18 */
    uint8_t speed;                /* +19 */
    uint16_t draw_kind;           /* +20 index of the table of draw routines at $6EA8 */
    uint32_t move_routine;        /* +22 */
    int16_t box_width;            /* +26 */
    int16_t box_height;           /* +28 */
    uint32_t step_routine;        /* +30 */
    uint8_t drop;                 /* +34 what the group drops when it is all gone ($FF none) */
    uint8_t pad_35;
};

_Static_assert(sizeof(struct CreatureType) == 36, "creature type record");
_Static_assert(sizeof(struct Creature) == 14, "creature record");

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
 * 4 and 5, $8898 and $88DC, and 7 to 10, $17E0A to $17E16, are not written: 8 ran once in the human replay, in 3128 calls, and is the
 * one call the oracle still sees differ: it finishes the entry and moves a creature along its path with the code at $1813C and
 * $182C4, which is self-modifying) */
void cell_entry_runs(const uint8_t *cell)
{
    run_cell_entry((int16_t)((uint8_t)*cell - 3));
}

/* orig $0127A0: the entry `index` (4 bytes each) of the table at ptr_0cbe is run as cell_entry_runs says */
void run_cell_entry(int16_t index)
{
    uint16_t *entry = (uint16_t *)(ptr_0cbe + (int16_t)(index << 2));

    if ((int16_t)entry[0] <= 0)
        return;

    struct CreatureType *type = (struct CreatureType *)table_199ac;

    for (int i = 0; i < 10; i++, type++)
        if (type->bounds == 1)
            type->status = 0;

    uint32_t routine = table_18714[(int16_t)(entry[0] - 1)];

    if (routine == 0x18900)
        take_record_to_18740(entry);
    else if (routine == 0x18890)
        take_record_to_199ac(entry);
    else if (routine == 0x1895A)
        finish_record_1895a(entry);
    else if (routine == 0x187B8)
        remember_visited_spot();
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
