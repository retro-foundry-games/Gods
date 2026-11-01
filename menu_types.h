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

#endif
