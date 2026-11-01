/* Gods, the menu program: loading files from the disks.  The drives, the track reads and the directory search are the
 * original's code (menu_amiga.asm, the file layer); the calls into it and what is built on it are C. */
#ifndef MENU_FILE_H
#define MENU_FILE_H

#include <stdint.h>

#include "menu_symbols.h"

/* menu_amiga.asm, orig $01D536: read the file `name` to `dest`; -1 when it could not be read, otherwise 0 */
int32_t file_read(const char *name, void *dest);
/* menu_amiga.asm, orig $01D966: switch the drives that are present off again */
void drives_idle(void);

void load_file(const char *name, void *dest);                     /* orig $01D3A6 */
void load_unpacked(const char *name, void *dest);                 /* orig $002702 */
void load_directory(void);                                        /* orig $002988 */
void assets_load(const char *keyword, uint16_t *palette, uint8_t *memory, struct AssetEntry *table);   /* orig $01ECA2 */
void display_reset(void);                                         /* orig $01D36E */

#endif
