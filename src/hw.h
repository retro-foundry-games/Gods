/* The Amiga custom chips, as the programs of Gods use them: the register block at $DFF000. */
#ifndef GODS_HW_H
#define GODS_HW_H

#include <stddef.h>
#include <stdint.h>

/* a channel of the audio hardware, as it sits in the custom chip registers */
struct AudioChannel {
    uint32_t lc;           /* sample address */
    uint16_t len, per, vol, dat, pad[2];
};

struct Custom {
    uint16_t bltddat, dmaconr, vposr, vhposr, dskdatr, joy0dat, joy1dat, clxdat;      /* 000 */
    uint16_t adkconr, pot0dat, pot1dat, potgor, serdatr, dskbytr, intenar, intreqr;   /* 010 */
    uint32_t dskpt;                                                                   /* 020 */
    uint16_t dsklen, dskdat, refptr, vposw, vhposw, copcon, serdat, serper, potgo, joytest;
    uint16_t pad038[4];                                                               /* 038 */
    uint16_t bltcon0, bltcon1, bltafwm, bltalwm;                                      /* 040 */
    uint32_t bltcpt, bltbpt, bltapt, bltdpt;                                          /* 048 */
    uint16_t bltsize, bltcon0l, bltsizv, bltsizh;                                     /* 058 */
    uint16_t bltcmod, bltbmod, bltamod, bltdmod;                                      /* 060 */
    uint16_t pad068[4];                                                               /* 068 */
    uint16_t bltcdat, bltbdat, bltadat;                                               /* 070 */
    uint16_t pad076[4], dsksync;                                                      /* 076 */
    uint32_t cop1lc, cop2lc;                                                          /* 080 */
    uint16_t copjmp1, copjmp2, copins, diwstrt, diwstop, ddfstrt, ddfstop;            /* 088 */
    uint16_t dmacon, clxcon, intena, intreq, adkcon;                                  /* 096 */
    struct AudioChannel aud[4];                                                  /* 0A0 */
    uint32_t bplpt[6];                                                                /* 0E0 */
    uint16_t pad0f8[4];
    uint16_t bplcon0, bplcon1, bplcon2, bplcon3, bpl1mod, bpl2mod, pad10c[2];         /* 100 */
    uint16_t bpldat[6];                                                               /* 110 */
    uint16_t pad11c[2];
    uint32_t sprpt[8];                                                                /* 120 */
    uint16_t spr[8][4];                                                               /* 140 */
    uint16_t color[32];                                                               /* 180 */
};
_Static_assert(offsetof(struct Custom, dmaconr) == 0x002, "custom layout");
_Static_assert(offsetof(struct Custom, intreqr) == 0x01e, "custom layout");
_Static_assert(offsetof(struct Custom, bltcon0) == 0x040, "custom layout");
_Static_assert(offsetof(struct Custom, bltsize) == 0x058, "custom layout");
_Static_assert(offsetof(struct Custom, bltdmod) == 0x066, "custom layout");
_Static_assert(offsetof(struct Custom, bltcdat) == 0x070, "custom layout");
_Static_assert(offsetof(struct Custom, dsksync) == 0x07e, "custom layout");
_Static_assert(offsetof(struct Custom, dmacon) == 0x096, "custom layout");
_Static_assert(offsetof(struct Custom, aud) == 0x0a0, "custom layout");
_Static_assert(offsetof(struct Custom, bplpt) == 0x0e0, "custom layout");
_Static_assert(offsetof(struct Custom, bplcon0) == 0x100, "custom layout");
_Static_assert(offsetof(struct Custom, bpldat) == 0x110, "custom layout");
_Static_assert(offsetof(struct Custom, sprpt) == 0x120, "custom layout");
_Static_assert(offsetof(struct Custom, spr) == 0x140, "custom layout");
_Static_assert(offsetof(struct Custom, color) == 0x180, "custom layout");

#define custom ((volatile struct Custom *)0xdff000)
#define ADDR(p) ((uint32_t)(uintptr_t)(p))   /* a pointer as the chip wants it: a 32-bit address */

#define DMAF_SETCLR 0x8000
#define DMAF_BLITHOG 0x0400     /* blitter priority over the CPU */
#define DMAF_BLTBUSY 0x4000     /* in dmaconr */



#define CIAA_PRA (*(volatile uint8_t *)0xbfe001)   /* bits 6 and 7: the two fire buttons, 0 when pressed */

#endif
