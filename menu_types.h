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

#endif
