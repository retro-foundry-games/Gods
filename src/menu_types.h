/* Gods, the menu program: the types of its data (menu_symbols.h declares the data). */
#ifndef MENU_TYPES_H
#define MENU_TYPES_H

#include <stdint.h>

/* the two lists the allocator keeps, as the original has them next to each other */
struct MemLists {
    uint32_t b_head;    /* start of the second list, 2 KB after the first */
    uint32_t a_head;    /* start of the first list */
    uint32_t a_tail;    /* ... where it ends (it is empty: the same) */
    uint32_t b_tail;
    uint32_t end;       /* 2 KB after the second */
};

/* an entry of the table assets_load makes: a sprite cut out of a picture */
struct AssetEntry {
    int16_t width_negative;   /* its width in pixels, negated */
    int16_t height_less_one;
    uint8_t *planes;          /* where its four planes are */
};

/* a voice of the sound system (its object is 148 bytes; the fields the start-up sets are named, the rest belongs to the sequencer) */
struct Voice {
    uint16_t dma_on;          /* +0   the write to DMACON that starts the channel */
    uint16_t dma_bit;
    uint16_t int_on;          /* +4   ... and to INTENA that enables its interrupt */
    uint16_t int_bit;
    struct AudioChannel *channel;   /* +8 the channel's registers */
    uint16_t pad_c;
    uint16_t field_e;
    uint8_t pad_10[8];
    uint8_t *buffer;          /* +0x18 */
    const uint8_t *sample_a;  /* +0x1c */
    const uint8_t *sample_b;  /* +0x20 */
    uint8_t pad_24[4];
    uint16_t field_28;
    uint8_t pad_2a[0x68];
    uint16_t number;          /* +0x92 */
};

/* a record of 8 bytes of the game's tables, a record of 12 bytes */
struct Slot8 { int16_t w[4]; };
struct Slot12 { int16_t w[6]; };
/* an entry of a list of bytes to put back: where, and the value */
struct __attribute__((packed)) SavedByte { uint8_t *address; uint16_t value; };

/* records of the tables the program keeps: their size is what the loops step by */
struct Rec6 { uint16_t w[3]; };
struct Rec10 { uint16_t w[5]; };
struct Rec80 {
    uint8_t before[12];
    uint16_t value_c;      /* +$0C */
    uint8_t between[16];
    uint16_t value_1e;     /* +$1E */
    uint8_t after[48];
};
struct Entry8 {
    uint32_t address;      /* the address the entry's data gets (negative: the list ends here) */
    uint32_t size;
};

/* an image of the table of images, as the intro has it: the data of one image is its planes, one after the other */
struct Image {
    int16_t neg_width;     /* minus the width in pixels */
    uint16_t height;       /* in rows, less one */
    const uint16_t *data;
};

/* game_c.c: the records of the objects (80 bytes each, eleven at 0x14988) and of the lists of shots they own */
struct Object {
    int16_t w0;             /* +0 */
    int16_t draw_type;      /* +2 how a shot is drawn (0 a glyph, 1 a sprite, 2 and 3 two of them) */
    int16_t frame;          /* +4 */
    int16_t first_value;    /* +6 */
    int16_t last_value;     /* +8 */
    int16_t w10;
    int16_t base_frame;     /* +12 */
    int16_t anim_mode;      /* +14 negative: no animation */
    uint8_t pad_16[4];
    uint32_t fire_kind;     /* +20 which routine of the table at $15220 the object calls */
    uint32_t handler;       /* +24 which routine of the table at $15260 runs its list */
    int16_t max_frame;      /* +28 */
    int16_t alt_frame;      /* +30 */
    uint8_t name1[16];      /* +32 */
    uint8_t name2[24];      /* +48 */
    uint8_t hits_tiles;     /* +72 */
    uint8_t pad_73;
    int16_t w74;            /* +74 */
    uint8_t pad_76[4];
};
struct Shot { int16_t active; int16_t x; int16_t y; int16_t direction; };
struct ShotList { int16_t object; struct Shot shot[18]; };   /* 146 bytes: the number of the object (negative: none) and its shots */


/* the creatures of a level (game.c, take_record_to_199ac): an entry of the table that says where and how a group starts (16 bytes, 25 for each level)... */
struct CreatureEntry {
    int16_t start_x;
    int16_t start_y;
    int8_t direction;
    uint8_t bounds;
    int16_t delay_step;
    uint8_t number;
    uint8_t value;         /* the hit points of one, and the score it gives */
    uint16_t template_index;   /* which CreatureTemplate */
    uint8_t flag_18;
    uint8_t speed;
    uint8_t drop;          /* what the group drops when it is all gone ($FF none) */
    uint8_t pad_15;
};
_Static_assert(sizeof(struct CreatureEntry) == 16, "creature entry");

/* ... and a template: the draw kind, the routines that move and step the creature (addresses in the original program: game.c has them by number) and its box (16 bytes) */
struct CreatureTemplate {
    uint16_t unused;
    uint16_t draw_kind;
    uint32_t move_routine;
    uint16_t box_width;
    uint16_t box_height;
    uint32_t step_routine;
};
_Static_assert(sizeof(struct CreatureTemplate) == 16, "creature template");


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


/* the things that move along a path ($17E0A to $18400): a description (28 bytes, 25 of them, the program's data: where it starts, its picture, its waypoints and how it goes
 * from one to the next)... */
struct PathPoint {
    int16_t x;
    int16_t y;
};

struct PathDescription {
    int16_t x;                    /* +0 where it starts */
    int16_t y;
    int8_t block;                 /* +4 the number of a 256-byte block of the background: its picture */
    int8_t speed;                 /* +5 steps a frame, less one */
    struct PathPoint waypoint[4]; /* +6 (x 0: none) */
    int8_t kind[4];               /* +22 how it goes on from the end of a line, for each of the four ways it can be started */
    uint8_t columns;              /* +26 bits 0 to 2: its picture's width in 32 pixel columns, less one; bit 7: it does not mark the map's cells */
    int8_t rows;                  /* +27 its height in 16 line rows, less one */
};
_Static_assert(sizeof(struct PathDescription) == 28, "path description");

/* the switches of the level (64 of 6 bytes, loaded with the level's tables): where one is (x, y of its record) and its state, bit 0 of which call_by_kind tests */
struct LeverFlag {
    uint16_t x;
    uint16_t y;
    uint16_t state;
};
_Static_assert(sizeof(struct LeverFlag) == 6, "lever flag");

/* ... and a record (24 bytes, 25 of them): where it is, which way it goes and the state of its line (the original keeps the line's registers in the record and leaves
 * the loop of the line by storing the address it goes on at; the four loops, the line steeper or flatter, left or right, are tagged by those addresses) */
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

struct PathRecord {
    int16_t x;
    int16_t y;
    int8_t kind;        /* +4 negative: not in use; 0 and 1 go on to the next waypoint at the end of a line, above 1 stop there */
    int8_t waypoint;    /* +5 the number of the waypoint it goes to, 1 to 4 (negative: it is done with its path) */
    struct PathState state;
};
_Static_assert(sizeof(struct PathRecord) == 24, "path record");


/* the entries of the cells of the level's map (4 bytes: ptr_0cbe, 253 of them, loaded from the disk): what the entry does (a state: how it is run, see run_cell_entry; negative or 0: nothing) and the number of the record it
 * is about */
struct EntryRef {
    uint16_t state;
    uint16_t number;
};

/* the level's script (24 bytes, 100 of them: buffer_cb6, loaded from the disk): up to three conditions (a kind and an argument, call_by_kind), and what happens when they all hold */
struct ScriptRecord {
    struct {
        int16_t kind;
        int16_t argument;
    } condition[3];
    int16_t x;                /* +12 where it is about (a position in the level) */
    int16_t y;                /* +14 */
    uint16_t effect;          /* +16 which routine of dispatch_18ba4 does its part (the top bit: another way) */
    int16_t effect_argument;  /* +18 */
    uint16_t message;         /* +20 the number of a message (the offset is in ptr_0cba), when it has one */
    int16_t unused_22;
};
_Static_assert(sizeof(struct ScriptRecord) == 24, "script record");

#endif
