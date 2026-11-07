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

/* the sound system's data, in the program's image: a record of the table of sounds (6 bytes), and an instrument (14 bytes) */
struct SoundRecord {
    uint8_t flags;       /* bit 7: not started again while it plays; the low bits: the number of voices it uses, less one */
    uint8_t track[4];    /* the track each voice plays (0: none, 22: the silent one: it stops the voice) */
    uint8_t variant;     /* 5: two random numbers are taken; 6: more until the choice (0 to 5) is not the last; 9: one, and the record of a variant (0 to 2) follows; other: one */
};
_Static_assert(sizeof(struct SoundRecord) == 6, "sound record");

struct SoundInstrument {
    int16_t flags;            /* negative: the sample loops */
    uint16_t loop_start;      /* where the loop starts (bytes of the sample) */
    uint16_t length;          /* in words */
    uint32_t sample;          /* the address of the 8 bit sample */
    uint32_t period_table;    /* the low byte picks one of the three tables of 48 periods (sound_periods): 0, 1, or the third (the data holds the tables' addresses: always the third) */
};
_Static_assert(sizeof(struct SoundInstrument) == 14, "sound instrument");

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


/* the waves of enemies (the original's "units": five records of 24 bytes at $18740, take_record_to_18740 makes one from a record of the level's script of 8 bytes, UnitScript, and a
 * template of the creature templates; the members of a unit, UnitMember, follow a path of the level's pattern file) */
struct UnitScript {
    uint8_t count;        /* +0 how many members */
    uint8_t path;         /* +1 which path (an entry of entry_list) they follow */
    uint16_t spacing;     /* +2 the delay between one member and the next */
    uint8_t life;         /* +4 the hit points of a member, and the score it gives */
    int8_t template_index;/* +5 which CreatureTemplate */
    uint8_t speeds;       /* +6 bits 4 to 7: how the shots are aimed, 0 to 3: how often they are made */
    uint8_t drop;         /* +7 what the last one drops ($FF none) */
};
_Static_assert(sizeof(struct UnitScript) == 8, "unit script");

struct __attribute__((packed)) Unit {
    uint16_t state;       /* +0 0 free, 2 made, 3 going (bit 1: in use, bit 0: going) */
    uint8_t count;        /* +2 */
    uint8_t path;         /* +3 */
    uint16_t spacing;     /* +4 */
    uint8_t life;         /* +6 */
    uint8_t aim;          /* +7 */
    uint8_t rate;         /* +8 */
    uint8_t draw;         /* +9 which draw routine of the table at $6EA8 */
    uint32_t move;        /* +10 the template's movement routine */
    int16_t box_width;    /* +14 the box the member is hit in */
    int16_t box_height;   /* +16 */
    uint32_t step;        /* +18 the template's step routine */
    uint8_t drop;         /* +22 */
    uint8_t pad;          /* +23 */
};
_Static_assert(sizeof(struct Unit) == 24, "unit");

/* a member of a walking group (the records of 18 bytes at $12A0, struct WalkGroup, each have ten members of 24 bytes at asset_directory + 240 a record: the first 24 bytes of a struct MonsterPart) */
struct WalkMember {
    int16_t x;               /* +0 */
    int16_t y;               /* +2 */
    int16_t frame;           /* +4 the step of its picture (negative: just hit, then counts up while it dies) */
    int16_t timer;           /* +6 */
    int16_t hp;              /* +8 0 free, -2 and -3 about to show up, other negative dying */
    int16_t mode;            /* +10 which movement it is in (the table at $99DC) */
    int16_t unused_12;       /* +12 */
    int16_t fire_timer;      /* +14 */
    int16_t item;            /* +16 the kind of object it carries (negative none) */
    int16_t vy;              /* +18 the step of its jump or fall (the table at $A358) */
    int16_t item_x;          /* +20 where the object it carries is drawn */
    int16_t item_y;          /* +22 */
};
_Static_assert(sizeof(struct WalkMember) == 24, "walk member");

/* a group of walkers (18 bytes, ten of them at $12A0): 0 free, 1 running, 2 to start; the type of its monsters (a struct MonsterType of the level's file, 50 on from the ones of the other groups) */
struct WalkGroup {
    uint16_t state;          /* +0 */
    struct MonsterType *type;   /* +2 */
    uint8_t unused_6[10];    /* +6 */
    uint8_t dropped;         /* +16 the group has dropped what it carries */
    uint8_t unused_17;
};
_Static_assert(sizeof(struct WalkGroup) == 18, "walk group");

/* the path planner of the walkers ($A492): a place that a walker could come to, where it is ($4BE, $4C0 are what it cost and the first step of the way) */
struct PlanItem {
    int16_t x;
    int16_t y;
    int16_t cost;
    int16_t dir;
};

struct Planner {
    int16_t cost;            /* +0 $4BE what the place being looked at cost to come to */
    int16_t dir;             /* +2 $4C0 the first step of the way to it */
    int16_t result;          /* +4 $4C2 the first step of the best way to the player (negative none) */
    uint16_t best;           /* +6 $4C4 what that way costs (65535 none) */
    int16_t flag;            /* +8 $4C6 0 the player is the target, 1 a place of the level's list, 2 the things in the list at $121C */
    int16_t queued;          /* +10 $4C8 how many places wait in plan_queue */
    int16_t label;           /* +12 $4CA the first step a jump has */
    int16_t jump;            /* +14 $4CC what a jump cost */
    int16_t visited;         /* +16 $4CE how many places plan_list has */
    struct PlanItem pending[2];   /* +18 $4D0 the place a walk to the right ends at, +26 $4D8 to the left */
};
_Static_assert(sizeof(struct Planner) == 34, "planner");

struct UnitMember {
    int16_t x;            /* +0 */
    int16_t y;            /* +2 */
    int16_t unused_4;     /* +4 */
    int16_t delay;        /* +6 frames to go before it starts */
    const uint8_t *path;  /* +8 where it is on its path (a byte a frame: a step in x in the high nibble, in y in the low) */
    int16_t hp;           /* +12 0 gone, negative dying */
    int16_t anim;         /* +14 the picture (bit 3: faces left), while dying the count */
    int16_t vector;       /* +16 the last step of its path, which it dies going on with */
    int16_t timer;        /* +18 */
};
_Static_assert(sizeof(struct UnitMember) == 20, "unit member");

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

/* the shards the stone faces spit ($0737E, ten records of 8 bytes): where it is, the step of its picture (0 to 3) and the frames it has left (x negative: free) */
struct Shard {
    int16_t x;
    int16_t y;
    int16_t frame;
    int16_t life;
};
_Static_assert(sizeof(struct Shard) == 8, "shard record");

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

/* the ways a walker walks (8 of 8 bytes at $99DC): the address of its routine and the base its picture starts at */
struct WalkMode {
    uint32_t routine;
    uint32_t picture_base;
};
_Static_assert(sizeof(struct WalkMode) == 8, "walk mode");

/* the switches of the level (64 of 6 bytes, loaded with the level's tables): where one is (x, y of its record) and its state, bit 0 of which call_by_kind tests */
struct LeverFlag {
    uint16_t x;
    uint16_t y;
    uint16_t state;
};
_Static_assert(sizeof(struct LeverFlag) == 6, "lever flag");

/* what the end of a level shows and how many spots end it (34 bytes, a list for each level at the addresses table_143b8 names; the list of the first level has four): the number of
 * spots remembered (word_22e) that ends the level, the bonus, the text (the first line of the screen), and the routines to run */
struct __attribute__((packed)) LevelEnd {
    uint16_t spots;
    uint32_t bonus;
    uint8_t text[20];
    uint16_t unused_26;
    uint32_t effects;   /* +28 one bit for each of the routines the level's end runs (bit 8: the things in the slots that are not kept are put away, bit 6: where the list of the teleport gems goes on, ...) */
    uint16_t list_offset;   /* +32 */
};
_Static_assert(sizeof(struct LevelEnd) == 34, "level end");

/* the four lists in one (the second at $14462 is not on a 4 byte line: the linker puts a table of the compiler's alignment there with a gap): $143D8 on */
struct __attribute__((packed)) LevelEndShort {   /* a record without its last word, which the next record's first word is (the fourth of the second level's: 32 bytes) */
    uint16_t spots;
    uint32_t bonus;
    uint8_t text[20];
    uint16_t unused_26;
    uint32_t effects;
};
_Static_assert(sizeof(struct LevelEndShort) == 32, "short level end");

struct __attribute__((packed)) LevelEnds {
    struct LevelEnd l1[4];
    uint16_t pad_14460;
    struct LevelEnd l2[3];
    struct LevelEndShort l2_3;   /* $144C8: its list offset is the next record's spots */
    struct LevelEnd l2_4;        /* $144E8: the level's end (the guardian's) */
    uint16_t l2_end;             /* $1450A: $FFFF */
    struct LevelEnd l3[3];
    struct LevelEnd l4[5];
};

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


/* the monsters of a level: ten records of 428 bytes (struct Monster, at $1354) each run a group of up to ten monsters (struct MonsterPart, 40 bytes, in a block of 400 bytes of its own at
 * ptr_0cee) whose kind is a struct MonsterType of the level's file (22 bytes, at ptr_0ce6).  The group's monsters fly to a target (the player, an object lying about or a zone of the
 * screen), the first of a chain is followed by the others along the places it has been (history), and they shoot and carry the objects they pick up. */
struct MonsterType {
    int8_t count;         /* +0 how many monsters */
    uint8_t life;         /* +1 hit points of one (doubled for the one the record picked), and its score */
    uint8_t pad_2[2];
    uint16_t flags;       /* +4 bits 0 to 3: who takes objects up (by the kind of object), 4 and 5: how it shoots (0 sideways, 1 aimed at a place near the player, 2 and 3 aimed), 6 and 7: how the chain
                           * follows (0 the places, 2 a circle, 1 and 3 turned), 8: the first to leave is chosen by chance */
    uint8_t fire;         /* +6 low nibble: how often it shoots, high: the speed of the shot */
    uint8_t picture;      /* +7 the template (CreatureTemplate) whose first word is its picture */
    uint8_t mode;         /* +8 the two things it does (low and high nibble: 0 the player, 1 a zone, 2 objects, 3 starts a chain, 4 follows) */
    uint8_t mode_chance;  /* +9 the chance of the high one (of 128) */
    uint8_t chance;       /* +10 */
    int8_t limit_b;       /* +11 the greatest speed in x and y */
    int8_t limit_c;       /* +12 the greatest change of speed */
    uint8_t zone_chance;  /* +13 the chance of going where most of the group's zones are */
    uint8_t delay;        /* +14 frames before the first one shows up */
    uint8_t wall;         /* +15 how far the walls push it away (0 not) */
    int16_t item;         /* +16 what it drops when all are gone, negative none */
    uint16_t start;       /* +18 the cell it starts in: x in the high byte, y in the low (32 by 16 pixels) */
    uint8_t spacing;      /* +20 the delay between one monster and the next */
    uint8_t pad_21;
};
_Static_assert(sizeof(struct MonsterType) == 22, "monster type");

struct MonsterPart {
    int16_t x;            /* +0 */
    int16_t y;            /* +2 */
    int16_t frame;        /* +4 the step of its picture (negative: just hit, then counts up while it dies) */
    int16_t timer;        /* +6 */
    int16_t hp;           /* +8 0 free, -2 and -3 about to show up, other negative dying */
    int16_t mode;         /* +10 */
    int16_t unused_12;    /* +12 */
    int16_t fire_timer;   /* +14 */
    int16_t item;         /* +16 the kind of object it carries (negative none) */
    int16_t vx;           /* +18 */
    int16_t vy;           /* +20 */
    int16_t speed;        /* +22 */
    uint8_t unused_24;    /* +24 */
    int8_t rank;          /* +25 0 free, 1 the first of the chain, then the places behind it */
    uint8_t zone;         /* +26 the zone it was sent to */
    uint8_t new_target;   /* +27 */
    int16_t target_x;     /* +28 */
    int16_t target_y;     /* +30 */
    int16_t item_x;       /* +32 where the object it carries is drawn */
    int16_t item_y;       /* +34 */
    int8_t goal;          /* +36 -1 chooses, 0 the player, 4 follows the chain */
    uint8_t reached;      /* +37 */
    uint8_t anim_timer;   /* +38 */
    uint8_t facing;       /* +39 */
};
_Static_assert(sizeof(struct MonsterPart) == 40, "monster part");

struct Monster {
    uint16_t state;       /* +0 0 free, 1 running, 2 to start */
    struct MonsterType *type;   /* +2 */
    int16_t field_6;      /* +6 */
    uint8_t unused_8[8];  /* +8 */
    uint8_t dropped;      /* +16 the drop of the group has been made */
    int8_t special;       /* +17 the monster that is picked (negative none) */
    int8_t chain;         /* +18 how many are in the chain (0 none) */
    int8_t alive;         /* +19 */
    uint16_t mask;        /* +20 which monsters are in the chain */
    int16_t lead;         /* +22 how many places the first has made (up to 100) */
    struct { int16_t x, y; } history[100];   /* +24 where the first has been */
    uint16_t index;       /* +424 where the next goes (in bytes) */
    uint8_t phase;        /* +426 */
    uint8_t pad_427;
};
_Static_assert(sizeof(struct Monster) == 428, "monster record");


/* the bolts the enemies shoot at the player ($183BC): twenty records of 22 bytes at $1A154.  The bolt goes along a line from where it was shot to where the player was (the
 * Bresenham line of $185BE: the original keeps in the record the address of the loop that goes on with it) by `speed` steps a frame */
struct Bolt {
    uint32_t routine;     /* +0 the original's loop ($185E2 and $18614 going right, $18660 and $18692 going left; negative: free) */
    int16_t x;            /* +4 */
    int16_t y;            /* +6 */
    int16_t dir;          /* +8 the step in y, 1 or -1 */
    int16_t length_x;     /* +10 */
    int16_t length_y;     /* +12 */
    int16_t error;        /* +14 */
    int16_t count;        /* +16 */
    int16_t speed;        /* +18 steps a frame */
    int16_t kind;         /* +20 */
};
_Static_assert(sizeof(struct Bolt) == 22, "bolt");

#endif
