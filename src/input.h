/* Gods, the menu program: the fire buttons, the joystick and the mouse */
#ifndef MENU_INPUT_H
#define MENU_INPUT_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

uint32_t take_pending_request(void);                                     /* orig $01CF26 (menu.c) */

uint16_t next_request(void);                                             /* orig $01CEF0 */
void input_poll(void);                                                   /* orig $01E39C */

#endif
