/* The 68000 has no 32 bit multiply and divide: the code that the optimiser turns into shifts and word multiplies calls libgcc's routines when it is not (-O0 in the
 * DEBUG=1 build).  The program is linked without libgcc, so these are the few it needs.  Shift and add or subtract, slow and small; compiled with -O2 (never -O0:
 * the compiler must not call the routine it is in). */
#include <stdint.h>

uint32_t __mulsi3(uint32_t a, uint32_t b)
{
    uint32_t result = 0;

    while (b) {
        if (b & 1)
            result += a;
        a <<= 1;
        b >>= 1;
    }
    return result;
}

static uint32_t divide(uint32_t n, uint32_t d, uint32_t *remainder)
{
    uint32_t quotient = 0, rest = 0;

    for (int bit = 31; bit >= 0; bit--) {
        rest = (rest << 1) | ((n >> bit) & 1);
        if (rest >= d) {
            rest -= d;
            quotient |= (uint32_t)1 << bit;
        }
    }
    *remainder = rest;
    return quotient;
}

uint32_t __udivsi3(uint32_t n, uint32_t d)
{
    uint32_t rest;

    return d ? divide(n, d, &rest) : 0;
}

uint32_t __umodsi3(uint32_t n, uint32_t d)
{
    uint32_t rest = 0;

    if (d)
        divide(n, d, &rest);
    return rest;
}

int32_t __divsi3(int32_t n, int32_t d)
{
    uint32_t un = n < 0 ? -(uint32_t)n : (uint32_t)n, ud = d < 0 ? -(uint32_t)d : (uint32_t)d;
    uint32_t q = __udivsi3(un, ud);

    return (n < 0) != (d < 0) ? -(int32_t)q : (int32_t)q;
}

int32_t __modsi3(int32_t n, int32_t d)
{
    uint32_t un = n < 0 ? -(uint32_t)n : (uint32_t)n, ud = d < 0 ? -(uint32_t)d : (uint32_t)d;
    uint32_t r = __umodsi3(un, ud);

    return n < 0 ? -(int32_t)r : (int32_t)r;
}
