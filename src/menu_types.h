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

#endif
