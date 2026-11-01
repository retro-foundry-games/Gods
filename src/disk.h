/* Gods, the menu program: the disk layer: MFM data as the disk drive reads and writes it, the track buffer, sector copies */
#ifndef MENU_DISK_H
#define MENU_DISK_H

#include <stddef.h>
#include <stdint.h>

#include "amiga/hw.h"
#include "menu_symbols.h"

void clear_track_slots(uint8_t *track);                                  /* orig $01DE42 */
uint32_t mfm_decode_long(const uint32_t *odd_even);                      /* orig $01DE76 */
uint32_t mfm_checksum(const uint8_t *track);                             /* orig $01DE9C */
uint32_t *mfm_encode_long(uint32_t value, uint32_t *to);                 /* orig $01E03A */
void copy_sectors(const uint32_t *from, uint32_t *to, uint16_t sectors); /* orig $01D4E4 */

#endif
