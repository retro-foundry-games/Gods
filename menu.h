/* Gods: the menu program (the part of the game the loader reads after a fire button), as C.
 *
 * It is a separate program from the intro (intro.c): the loader reads it into memory and its data sits at the addresses
 * the original keeps it at; menu.symbols lists those, menu_symbols.h declares them and menu.ld gives the names their
 * addresses.  The program is split in modules by what they do: disk.c (the disk layer), alloc.c (memory), decrunch.c (the
 * unpacker), input.c (buttons, joystick, mouse), screen.c (buffers, colours, copper, blitter), draw.c (images and text); menu.c has what is not
 * placed yet.  Every function says where the original has it (`orig $ADDR`). */
#ifndef MENU_H
#define MENU_H

#include <stddef.h>
#include <stdint.h>

#include "alloc.h"
#include "decrunch.h"
#include "draw.h"
#include "disk.h"
#include "hw.h"
#include "input.h"
#include "menu_symbols.h"
#include "screen.h"

void clear_work_area(void);                                              /* orig $00F518 */
uint32_t take_pending_request(void);                                     /* orig $01CF26 */
void do_nothing(void);                                                   /* orig $0053EC and $016D74 */
void reset_marker_table(void);                                           /* orig $017CAC */
void copy_record_table(const struct Rec6 *from);                         /* orig $00F474 */
void clear_entry_table(void);                                            /* orig $017DF8 */
void narrow_words_to_bytes(void);                                        /* orig $00F382 */
void assign_addresses(void);                                             /* orig $00F3C0 */
void load_buffer_2400(const uint8_t *from);                              /* orig $00F488 */
void select_params(void);                                                /* orig $0029B4 */
void build_table_14cf8(void);                                            /* orig $004C90 */
void save_slot_words(void);                                              /* orig $01494C */
void restore_slot_words(void)                                            /* orig $01496A */;

#endif
