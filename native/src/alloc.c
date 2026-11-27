/* Gods, the menu program: the memory the program hands out */
#include "alloc.h"

/* orig $01D48E: the program's memory starts at a base the loader leaves in the vector slot at $2C.  Two lists are set up in
 * it, each with a head and a tail that are both its start, in 2 KB each: the first long of the first is cleared, all of
 * the second.  The 16-bit additions are the original's: only the low word of the address takes the carry. */
void alloc_init(void)
{
    uint32_t base = guest.heap_base;
    uint32_t *second;
    int i;

    guest.mem_lists.a_tail = base;
    guest.mem_lists.a_head = base;
    *(uint32_t *)base = 0;
    base = (base & 0xffff0000) | ((base + 0x800) & 0xffff);
    guest.mem_lists.b_tail = base;
    guest.mem_lists.b_head = base;
    second = (uint32_t *)base;
    base = (base & 0xffff0000) | ((base + 0x800) & 0xffff);
    guest.mem_lists.end = base;
    for (i = 0; i < 0x200; i++)
        *second++ = 0;
}
