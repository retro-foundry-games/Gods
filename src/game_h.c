/* Gods, the game: what Begin Quest does from the fire button of the menu to the first frame of the game loop: the original's
 * code at $651E, $2452 to $2C08 and the routines it calls that are not in the other game modules.  Each function says where the
 * original has it (`orig $ADDR`).  Where the original goes on at an address that did not run for level 1 (the other levels, a
 * file that cannot be read: load_file does not return then) the comment says `goes on at $ADDR`. */
#include "menu.h"
#include "sound.h"

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
    uint8_t *record = ptr_0c9e;

    for (int i = 0; i < 25; i++, record += 24) {
        *(int16_t *)record = -1;
        *(int16_t *)(record + 2) = -1;
        record[4] = 0xFF;
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
        int16_t value = (int16_t)*table++;

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
        int32_t value = longs[i];

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

        *(uint32_t *)record = *(const uint32_t *)from;
        *(uint32_t *)(record + 4) = *(const uint32_t *)(from + 4);
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
    word_1ec = *p++;
    word_1ee = *p++;
    to = (uint16_t *)ptr_0cda;
    {
        uint16_t n = (uint16_t)(word_1ec - 1);

        do {
            *to++ = *p++;
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
uint8_t *find_flag_entry(int16_t x, int16_t y)
{
    uint32_t key = ((uint32_t)(uint16_t)x << 16) | (uint16_t)y;
    uint8_t *entry = flags_1c056;

    for (int i = 0; i < 64; i++, entry += 6)
        if (*(uint32_t *)entry == key)
            return entry;
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
        uint8_t *entry = find_flag_entry((int16_t)record->w[0], (int16_t)record->w[1]);

        if (entry == 0)
            record->w[2] = 0xFFFF;
        else
            *(uint16_t *)(entry + 4) = record->w[2];
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

/* orig $01F54E (the mask) and $01F4B6 with $01F52C (the cookie cut; $01E356, the blitter wait, is blitter_grab_wait): the image at `source` (four planes, plane_size bytes each, `rows`
 * rows of `words` words; the blitter's modulo for it is modulo_ab) is drawn to `dest` (four planes 8448 bytes apart, modulo_cd) through a
 * mask that is its four planes or'ed (made in mask_buffer by two blits) */
static void blit_masked(const uint8_t *source, uint8_t *dest, int16_t modulo_ab, uint16_t words, int16_t modulo_cd, uint32_t plane_size,
                        uint16_t rows)
{
    uint16_t size = (uint16_t)((rows << 6) | words);

    blitter_grab_wait();
    custom->bltdpt = ADDR(mask_buffer);
    custom->bltapt = ADDR(source);
    custom->bltbpt = ADDR(source + plane_size);
    custom->bltcpt = ADDR(source + 2 * plane_size);
    custom->bltamod = (uint16_t)modulo_ab;
    custom->bltbmod = (uint16_t)modulo_ab;
    custom->bltcmod = (uint16_t)modulo_ab;
    custom->bltdmod = (uint16_t)modulo_ab;
    custom->bltcon1 = 0;
    custom->bltcon0 = 4094;
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    blit_start(size);
    blitter_grab_wait();
    custom->bltcpt = ADDR(source + 3 * plane_size);
    custom->bltbpt = ADDR(mask_buffer);
    custom->bltdpt = ADDR(mask_buffer);
    custom->bltcon0 = 2030;
    blit_start(size);

    blitter_grab_wait();
    custom->bltafwm = 0xFFFF;
    custom->bltalwm = 0xFFFF;
    custom->bltapt = ADDR(mask_buffer);
    custom->bltbpt = ADDR(source);
    custom->bltcpt = ADDR(dest);
    custom->bltdpt = ADDR(dest);
    custom->bltdmod = (uint16_t)modulo_cd;
    custom->bltcmod = (uint16_t)modulo_cd;
    custom->bltamod = (uint16_t)modulo_ab;
    custom->bltbmod = (uint16_t)modulo_ab;
    custom->bltcon1 = 0;
    custom->bltcon0 = 4042;
    blit_start(size);
    for (int plane = 0; plane < 3; plane++) {
        dest += 8448;
        source += plane_size;
        blitter_grab_wait();
        custom->bltbpt = ADDR(source);
        custom->bltapt = ADDR(mask_buffer);
        custom->bltcpt = ADDR(dest);
        custom->bltdpt = ADDR(dest);
        blit_start(size);
    }
}

/* orig $01F0AA: the image (x, y, `width` pixels, `height` + 1 rows, at `source`: the first of its four planes) is clipped by the
 * rectangle `clip` (x0, x1, y0, y1) and drawn on the screen buffer.  Only the way that ran is written: an image that is mirrored
 * (word_d82 negative), the shift in the bits of x (word_d86) and the other draw routines of the table at $0D9A are not */
static void blit_clipped(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *source, const int16_t *clip)
{
    int16_t low = clip[2];
    int16_t high = clip[3];
    int16_t rest;
    int16_t modulo_ab;
    int16_t modulo_cd;
    uint16_t words;
    int16_t offset;
    uint32_t product;

    if (y < low)
        return;   /* goes on at $01F0B6 */
    if (y > high)
        return;
    if ((int16_t)(height + y) > high)
        height = (int16_t)(high - y);
    low = clip[0];
    rest = width;
    if (x <= low) {
        rest = (int16_t)(rest + x);
        if (rest < low)
            return;
        rest = (int16_t)(rest - low);
        offset = (int16_t)(((uint16_t)(low - x) & 0xFFF0) >> 3);
        source += offset;
        x = low;
        word_d88 = 1;
    }
    high = clip[1];
    if (x > high)
        return;
    if ((int16_t)(rest + x) > high) {
        rest = (int16_t)(high - x);
        word_d88 = (uint16_t)(word_d88 + 2);
    }
    words = (uint16_t)rest >> 4;
    rest = (int16_t)(words << 3);   /* the original's d5 */
    if (!(word_d82 & 0x8000))
        return;   /* goes on at $01F13A */
    width = (int16_t)(((uint16_t)width >> 1) & 0xFFF8);
    product = (uint32_t)(uint16_t)(width + 8) * word_dd4;
    source += (int16_t)((uint16_t)product >> 2);
    if (flags_0ddd & 0x20)
        return;   /* goes on at $01F186 */
    modulo_ab = (int16_t)(width - rest);
    modulo_cd = (int16_t)(((uint16_t)(word_dd2 - 8 - rest)) >> 2);
    x = (int16_t)((uint16_t)x & 0xFFF0);
    x = (int16_t)((uint16_t)x >> 3);
    x = (int16_t)(x + (uint16_t)((uint16_t)y * 44));
    if (word_d86 != 0)
        return;   /* goes on at $01F1BE */
    if (word_dde != 1 || (flags_0ddd & 4))
        return;   /* goes on at the routine of the table at $0D9A that word_dde and bit 2 of flags_0ddd name */
    blit_masked(source, screen_buffer + x, (int16_t)(modulo_ab >> 2), (uint16_t)(words + 1), modulo_cd, long_dd8, (uint16_t)(height + 1));
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
                out[0] = from[0];
                out[1] = from[8448];
                out[2] = from[16896];
                out[3] = from[25344];
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

struct PathState {
    uint32_t loop;      /* +6 which loop it goes on in */
    int16_t x;          /* +10 */
    int16_t y;          /* +12 */
    int16_t y_step;     /* +14 1 or -1 */
    int16_t length;     /* +16 d2: the longer side */
    int16_t shorter;    /* +18 d3 */
    int16_t error;      /* +20 d5 */
    int16_t count;      /* +22 d7 */
};

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
static int path_waypoint(const uint8_t *description, uint8_t *record, int k, int16_t *x, int16_t *y)
{
    const int16_t *point = (const int16_t *)(description + 6 + 4 * (k - 1));

    if (point[0] == 0)
        return 0;
    *x = point[0];
    *y = point[1];
    record[5] = (uint8_t)k;
    record[4] = 2;
    return 1;
}

/* orig $01813C (the table at $18284, whose routines are $018154, $018164, $018192, $0181C0, $0181D8, $0181F0, $018208, $018220, $018238, $018250 and $018268 inlined): which waypoint the line of the record goes to next, by the
 * kind in the description's byte at +22 + `variant`: the next one (kinds 2 to 5), the one before (7 to 10), the next of the row (0) or the
 * one before (1 and 6) */
static void path_target(const uint8_t *description, uint8_t *record, int8_t kind, int16_t *x, int16_t *y)
{
    int k;

    if (kind < 0) {
        record[4] = 0xFF;
        word_4a2--;
        return;   /* the target is where the line starts */
    }
    if (kind == 0) {
        record[4] = 0;
        k = (int8_t)record[5] + 1;
        if (k > 4)
            k = 1;
        path_target(description, record, (int8_t)(k + 1), x, y);
        record[4] = 0;
        return;
    }
    if (kind == 1 || kind == 6) {
        record[4] = 1;
        k = (int8_t)record[5] - 1;
        if (k < 1)
            k = 4;
        path_target(description, record, (int8_t)(6 + k), x, y);
        record[4] = 1;
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
void start_record_path(uint16_t *entry, int16_t variant)
{
    int16_t number = (int16_t)entry[1];
    uint8_t *description = ptr_0c9a + number * 28;
    uint8_t *record = ptr_0c9e + number * 24;
    int16_t x, y, tx = 0, ty = 0;
    struct PathState *state = (struct PathState *)(record + 6);

    if ((*(int16_t *)(description + 6) | *(int16_t *)(description + 10) | *(int16_t *)(description + 14) | *(int16_t *)(description + 18)) == 0)
        return;
    entry[0] = 0xFFFF;
    word_4a2++;
    word_17e9c = 0x4E71;
    if ((int8_t)record[4] < 0) {
        x = *(int16_t *)description;
        y = *(int16_t *)(description + 2);
    } else {
        x = *(int16_t *)record;
        y = *(int16_t *)(record + 2);
    }
    word_185bc = (int16_t)((int8_t)description[5] + 1);
    record[5] = 0;
    tx = x;
    ty = y;
    path_target(description, record, (int8_t)description[22 + variant], &tx, &ty);
    path_start(state, x, y, tx, ty);
    *(int16_t *)record = state->x;
    *(int16_t *)(record + 2) = state->y;
}

/* orig $018072 (from $01808E on, with $018092 to $0180C6): the record goes on along its line: the place it reaches is kept in the
 * record; when the line is done the record is at its waypoint, and with a kind above 1 (byte +4) it is done with the path (byte +5
 * -1).  The kinds 0 and 1 go on to the next waypoint ($18106: not run) */
void path_continue(uint8_t *record, const uint8_t *description)
{
    struct PathState *state = (struct PathState *)(record + 6);

    path_resume(state);
    *(int16_t *)record = state->x;
    *(int16_t *)(record + 2) = state->y;
    if (state->count >= 0)
        return;
    int16_t index = (int16_t)(((int8_t)record[5] - 1) << 2);

    *(int16_t *)record = *(const int16_t *)(description + 6 + index);
    *(int16_t *)(record + 2) = *(const int16_t *)(description + 8 + index);
    if ((int8_t)record[4] <= 1)
        return;   /* goes on at $18106 */
    record[5] = 0xFF;
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
            line_text = (const uint8_t *)(uintptr_t)226236;
            ptr_03ac = (uint8_t *)(uintptr_t)227836;
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
    ptr_0de0 = (const int16_t *)(uintptr_t)296;
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
