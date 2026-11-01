/* Gods: the menu program (the part of the game the loader reads after a fire button), as C.
 *
 * It is a separate program from the intro (intro.c): the loader reads it into memory and its data sits at the addresses
 * the original keeps it at; menu.symbols lists those, menu_symbols.h declares them and menu.ld gives the names their
 * addresses.  The program is split in modules by what they do: disk.c (the disk layer), alloc.c (memory), decrunch.c (the
 * unpacker), input.c (buttons, joystick, mouse), screen.c (buffers, colours, copper, blitter); menu.c has what is not
 * placed yet.  Every function says where the original has it (`orig $ADDR`). */
#ifndef MENU_H
#define MENU_H

#include <stddef.h>
#include <stdint.h>

#include "alloc.h"
#include "decrunch.h"
#include "disk.h"
#include "hw.h"
#include "input.h"
#include "menu_symbols.h"
#include "screen.h"

void clear_work_area(void);                                              /* orig $00F518 */
uint32_t take_pending_request(void);                                     /* orig $01CF26 */

#endif
