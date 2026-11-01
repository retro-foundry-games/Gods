/* Gods, the menu program: the fire buttons, the joystick and the mouse */
#ifndef MENU_INPUT_H
#define MENU_INPUT_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

void input_poll(void);                                                   /* orig $01E39C */

#endif
