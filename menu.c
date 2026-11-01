/* Gods, the menu program: what is not placed in a module of its own yet.  The modules are disk.c, alloc.c, decrunch.c,
 * input.c and screen.c (menu.h includes their headers). */
#include "menu.h"

/* orig $00F518: clear the 80 longs the work area pointer points at */
void clear_work_area(void)
{
    uint32_t *p = work_area;
    int i;

    for (i = 0; i < 80; i++)
        *p++ = 0;
}

/* orig $01CF26: the request that was waiting, or 0; it is taken, so it is gone afterwards */
uint32_t take_pending_request(void)
{
    uint32_t request = pending_request;

    if (request != 0)
        pending_request = 0;
    return request;
}
