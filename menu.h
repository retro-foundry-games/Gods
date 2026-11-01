/* Gods: the menu program (the part of the game the loader reads after a fire button), as C.
 *
 * It is a separate program from the intro (intro.c): the loader reads it into memory and its data sits at the addresses
 * the original keeps it at; menu.symbols lists those, menu_symbols.h declares them and menu.ld gives the names their
 * addresses.  The functions here are the original's, each with its address in a comment (`orig $ADDR`). */
#ifndef MENU_H
#define MENU_H

#include <stddef.h>
#include <stdint.h>

#include "hw.h"
#include "menu_symbols.h"

/* ---- memory and screens ------------------------------------------------------------------------------------------ */

void clear_work_area(void);                                              /* orig $00F518 */
uint32_t take_pending_request(void);                                     /* orig $01CF26 */
void clear_screen_buffer(uint32_t *buffer);                              /* orig $01CFAC */
void copy_screen_buffer(const uint32_t *from, uint32_t *to);             /* orig $01CFE4 */
const uint16_t *find_color01_move(const uint16_t *list);                 /* orig $01D22A */
void shadow_set(uint16_t index, uint16_t value);                         /* orig $01D2D8 */
uint16_t shadow_get(uint16_t index);                                     /* orig $01D2EA */
void copy_sectors(const uint32_t *from, uint32_t *to, uint16_t sectors); /* orig $01D4E4 */

/* ---- disk: MFM and the track buffer -------------------------------------------------------------------------------- */

void clear_track_slots(uint8_t *track);                                  /* orig $01DE42 */
uint32_t mfm_decode_long(const uint32_t *odd_even);                      /* orig $01DE76 */
uint32_t mfm_checksum(const uint8_t *track);                             /* orig $01DE9C */
uint32_t *mfm_encode_long(uint32_t value, uint32_t *to);                 /* orig $01E03A */
uint8_t *unpack(uint8_t *to, const uint8_t *from);                       /* orig $01E260 */

/* ---- the chips ---------------------------------------------------------------------------------------------------- */

void blitter_wait(void);                                                 /* orig $01DFD8 */
void input_poll(void);                                                   /* orig $01E39C */

#endif
