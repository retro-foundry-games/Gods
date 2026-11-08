/* Gods, the Amiga build: the memory routines the C of the program asks of the compiler (there is no C library in the menu program's hunk) */
#include <stddef.h>

/* (the compiler would turn the loop into a call of memset: the routine itself must not be made of one) */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *destination, int value, size_t count)
{
    unsigned char *p = destination;

    while (count--)
        *p++ = (unsigned char)value;
    return destination;
}
