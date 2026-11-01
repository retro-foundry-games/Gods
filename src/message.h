/* Gods, the menu program: the line of text at the top of the screen */
#ifndef MENU_MESSAGE_H
#define MENU_MESSAGE_H

#include <stdint.h>

#include "menu_symbols.h"

void message_start(uint16_t arg);       /* orig $016DF8 */
void message_tick(void);                /* orig $016E42 */

void message_draw(void);                /* orig $016ECA */
uint16_t message_request(int16_t priority);   /* orig $016D76 */

#endif
