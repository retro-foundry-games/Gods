/* Gods, the menu program: the screens of the menu */
#ifndef MENU_UI_H
#define MENU_UI_H

#include <stdint.h>

void menu_screen_draw(void);       /* orig $0066CC */
void top_ten_screen(void);         /* orig $0068DE */
void password_screen(void);        /* orig $006B4C */
void menu_main(void);              /* orig $0063A2 */

#endif
