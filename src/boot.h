/* Gods, the menu program: the start */
#ifndef MENU_BOOT_H
#define MENU_BOOT_H

#include <stdint.h>

void display_setup(void);        /* orig $039FA6 */
void sound_init(void);           /* orig $03A18A */
void chips_setup(void);          /* orig $039FDC */
void hardware_setup(void);       /* orig $031FC0 */
void data_pointers_init(void);   /* orig $01AA3C */
void memory_map_init(void);      /* orig $03A060 */
void display_state_reset(void);  /* orig $01F5C4 */
void menu_boot(void);            /* orig $039E50 */

#endif
