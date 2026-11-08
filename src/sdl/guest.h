/* Gods SDL port: the host side's view of the guest memory and the chips (host code: it does not include the game's headers) */
#ifndef GODS_SDL_GUEST_H
#define GODS_SDL_GUEST_H

#include <stdint.h>
#include <stdio.h>

uint8_t *guest_ptr(uint32_t address);
uint8_t *guest_member_ptr(uint32_t address);                  /* the member of struct Gods that has an address of the original's memory (0: none; never a table) */
int guest_state_write(FILE *f);                              /* the port's own save state: the block of the program and the chips' registers (0 when done) */
int guest_state_read(FILE *f);
void guest_set_heap(void);                                   /* heap_base: the heap of the block (the loader leaves it in a vector slot) */
void guest_set_mask_buffer(void);                            /* mask_buffer: the area of the original's boot code */
void *guest_resolve(uint32_t address);                       /* where an address of the original's memory is here (a table, or the block) */
uint32_t guest_address(uint32_t address);
uint32_t guest_offset(const void *p);                        /* the offset in the block: what an address of the original's memory is */
void guest_init(void);                                       /* the memory and the chips' registers cleared (once) */
void guest_reset(void);                                      /* the memory as the loader leaves it for a program: cleared (the tables are not in the block: guest_resolve finds them) */
/* guest_load.c */
int guest_load_level_image(const char *path);                  /* a RAM image of the original in level 1 (a test) */
void guest_load_image(const uint8_t *image, uint32_t size);  /* the menu program's image, at $84 */

/* files.c: the disks */
int files_open(const char *folder);                          /* 0 when both disks are there */
uint8_t *files_find(const char *name, int disk, uint32_t *size);   /* a file of a disk (0 or 1) by name: points into the disk image */
int files_read(const char *name, int prefer_disk, uint8_t *dest);  /* copy a file (whole sectors) to dest; -1 if missing */
uint8_t *files_unpack(const uint8_t *packed, uint32_t *size);      /* the program's packer: malloc'ed output */
int files_load_menu_image(void);                             /* bm.2 of disk 1 unpacked into the guest memory */

/* the chips: custom registers and CIA as the program sees them */
extern volatile uint8_t host_custom_raw[0x1000];
extern volatile uint8_t host_cia_raw[0x2000];   /* from $BFD000 */
#define REG16(offset) (*(volatile uint16_t *)(host_custom_raw + (offset)))
#define REG32(offset) (*(volatile uint32_t *)(host_custom_raw + (offset)))
#define CIAA_PRA_REG (host_cia_raw[0x1001])

/* display.c */
#define SCREEN_W 320
#define SCREEN_H 256
void display_latch_colors(void);                              /* ... and the colours, after the vertical blank interrupt has written them */
void display_latch(void);                                      /* the display registers as the frame starts (before the vertical blank is signalled) */
void display_render(uint32_t *pixels);                       /* one frame: the copper list and the bitplanes to ARGB pixels */

/* irq.c (the game thread runs the program, the main thread the interrupts) */
void irq_vbl(void);
void irq_key(int raw_code, int down);

/* sound.c */
void sound_mix(int16_t *out);   /* a frame of the sound effects: 882 stereo samples */
void sound_reset(void);

#endif
