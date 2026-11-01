/* Gods, the menu program: the unpacker for the program's packed files */
#ifndef MENU_DECRUNCH_H
#define MENU_DECRUNCH_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

uint8_t *unpack(uint8_t *to, const uint8_t *from);                       /* orig $01E260 */

#endif
