/* Gods (Bitmap Brothers, 1991), PAL original disks: the game, decompiled.
 *
 * Built with m68k-amigaos-gcc for a 68000 A500 (see flake.nix); the game's variables live at their original
 * addresses, defined by gods.ld.  Every function carries its original address as `orig $ADDR` so it can be matched
 * with the original code.  Progress and method: PLAN.md.  Evidence for each recovered fact: analysis/. */
#include <stddef.h>
#include <stdint.h>   /* uintptr_t */

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* ---- Amiga custom chip registers, base $DFF000 ---------------------------------------------------------------- */

struct AudioChannel {
    u32 lc;           /* sample address */
    u16 len, per, vol, dat, pad[2];
};

struct Custom {
    u16 bltddat, dmaconr, vposr, vhposr, dskdatr, joy0dat, joy1dat, clxdat;      /* 000 */
    u16 adkconr, pot0dat, pot1dat, potgor, serdatr, dskbytr, intenar, intreqr;   /* 010 */
    u32 dskpt;                                                                   /* 020 */
    u16 dsklen, dskdat, refptr, vposw, vhposw, copcon, serdat, serper, potgo, joytest;
    u16 pad038[4];                                                               /* 038 */
    u16 bltcon0, bltcon1, bltafwm, bltalwm;                                      /* 040 */
    u32 bltcpt, bltbpt, bltapt, bltdpt;                                          /* 048 */
    u16 bltsize, bltcon0l, bltsizv, bltsizh;                                     /* 058 */
    u16 bltcmod, bltbmod, bltamod, bltdmod;                                      /* 060 */
    u16 pad068[4];                                                               /* 068 */
    u16 bltcdat, bltbdat, bltadat;                                               /* 070 */
    u16 pad076[4], dsksync;                                                      /* 076 */
    u32 cop1lc, cop2lc;                                                          /* 080 */
    u16 copjmp1, copjmp2, copins, diwstrt, diwstop, ddfstrt, ddfstop;            /* 088 */
    u16 dmacon, clxcon, intena, intreq, adkcon;                                  /* 096 */
    struct AudioChannel aud[4];                                                  /* 0A0 */
    u32 bplpt[6];                                                                /* 0E0 */
    u16 pad0f8[4];
    u16 bplcon0, bplcon1, bplcon2, bplcon3, bpl1mod, bpl2mod, pad10c[2];         /* 100 */
    u16 bpldat[6];                                                               /* 110 */
    u16 pad11c[2];
    u32 sprpt[8];                                                                /* 120 */
    u16 spr[8][4];                                                               /* 140 */
    u16 color[32];                                                               /* 180 */
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
#define ADDR(p) ((u32)(uintptr_t)(p))   /* a pointer as the chip wants it: a 32-bit address */

#define DMAF_SETCLR 0x8000
#define DMAF_BLITHOG 0x0400     /* blitter priority over the CPU */
#define DMAF_BLTBUSY 0x4000     /* in dmaconr */

/* ---- the screen -------------------------------------------------------------------------------------------------- */

enum { SCREEN_WORDS = 20, SCREEN_ROWS = 200, SCREEN_PLANES = 5 };

/* 320x200, five bitplanes, each plane stored whole (8000 bytes) one after the other */
struct Screen {
    u16 plane[SCREEN_PLANES][SCREEN_ROWS][SCREEN_WORDS];
};

/* ---- game variables, at their original addresses (gods.ld) ------------------------------------------------------- */

extern u16 *palette_ptr;              /* $1AF6  the 32-word colour table that the interrupt copies to the chip */
extern volatile u16 palette_dirty;              /* $2CC2  non-zero: copy the colour table to COLOR00..31 at the next VBL */
extern volatile u16 vbl_saturating;             /* $2CC4  frames counter, stops at $7FFF */
extern volatile u16 vbl_count;                 /* $2CC6  frames counter, wraps */
extern struct Screen *screen_front;    /* $2940  the screen being drawn to */
extern struct Screen *screen_back;     /* $2944  the second screen */

/* ---- palette ------------------------------------------------------------------------------------------------------- */

/* orig $002EFE: clear the colour table and ask for it to be copied to the chip */
void palette_clear(void)
{
    int i;

    for (i = 0; i < 32; i++)
        palette_ptr[i] = 0;
    palette_dirty = 0xffff;
}

/* orig $003022: colour table entry `index` = `value` (the original takes them in D0 and D1) */
void palette_set(u16 index, u16 value)
{
    palette_ptr[index] = value;
}

/* orig $003036: colour table entry `index` */
u16 palette_get(u16 index)
{
    return palette_ptr[index];
}

/* ---- timing -------------------------------------------------------------------------------------------------------- */

/* orig $002CDE: wait for the next vertical blank (the interrupt increments vbl_count) */
void vbl_wait(void)
{
    u16 seen = vbl_count; /* volatile: the interrupt changes it */

    while (vbl_count == seen)
        ;
}

/* ---- blitter -------------------------------------------------------------------------------------------------------- */

/* orig $002F1E: wait until the blitter has finished (and give it priority while waiting) */
void blitter_wait(void)
{
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
    while (custom->dmaconr & DMAF_BLTBUSY)
        ;
    custom->dmacon = DMAF_BLITHOG;
}

/* orig $001B7C: copy 800 rows of 20 words (four planes) from `src` to `dst` with the blitter, A -> D */
void blit_copy(const void *src, void *dst)
{
    blitter_wait();
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltdpt = ADDR(dst);
    custom->bltapt = ADDR(src);
    custom->bltdmod = 0;
    custom->bltamod = 0;
    custom->bltcon1 = 0;
    custom->bltcon0 = 0x09f0;            /* use A and D, D = A */
    custom->dmacon = DMAF_BLITHOG;
    custom->bltsize = (800 << 6) | 20;   /* starts the blit */
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
}

/* orig $001BC6: as blit_copy, but only the right-hand 9 words of each row (the left 11 words are skipped) */
void blit_copy_right(const u8 *src, u8 *dst)
{
    src += 0x16;
    dst += 0x16;
    blitter_wait();
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltdpt = ADDR(dst);
    custom->bltapt = ADDR(src);
    custom->bltdmod = 0x16;
    custom->bltamod = 0x16;
    custom->bltcon1 = 0;
    custom->bltcon0 = 0x09f0;
    custom->dmacon = DMAF_BLITHOG;
    custom->bltsize = (800 << 6) | 9;
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
}

/* ---- screens ----------------------------------------------------------------------------------------------------- */

/* orig $0030D4: clear the front screen */
void screen_clear(void)
{
    u32 *p = (u32 *)screen_front;
    int i;

    for (i = 0; i < 10000; i++)
        *p++ = 0;
}

/* orig $0030EE: copy the back screen over the front screen */
void screen_restore(void)
{
    const u32 *from = (const u32 *)screen_back;
    u32 *to = (u32 *)screen_front;
    int i;

    for (i = 0; i < 10000; i++)
        *to++ = *from++;
}

/* orig $001D54: set the whole fifth bitplane of the front screen (the highlight plane) */
void plane5_set(void)
{
    u32 *p = (u32 *)screen_front->plane[4];
    int i;

    for (i = 0; i < 2000; i++)
        *p++ = 0xffffffff;
}

/* orig $001D7E: clear the fifth bitplane of the front screen */
void plane5_clear(void)
{
    u32 *p = (u32 *)screen_front->plane[4];
    int i;

    for (i = 0; i < 2000; i++)
        *p++ = 0;
}

/* Pictures are stored with the four low planes interleaved word by word.  orig $001C18: unscramble the picture at
 * `pic` through the front screen and store it back, now plane by plane, in place. */
void picture_deinterleave(u16 *pic)
{
    const u16 *src = pic;
    struct Screen *dst = screen_front;
    int row, w, p;
    u32 *from = (u32 *)screen_front;
    u32 *to = (u32 *)pic;
    int i;

    for (row = 0; row < SCREEN_ROWS; row++)
        for (w = 0; w < SCREEN_WORDS; w++)
            for (p = 0; p < 4; p++)
                dst->plane[p][row][w] = *src++;
    for (i = 0; i < 8000; i++)
        *to++ = *from++;
}

/* orig $001432: put the title picture on the front screen */
void picture_show_title(void)
{
    blit_copy((const u8 *)0x3a62 + 0x22, screen_front);
}

/* orig $0016C0: put the intro background on the front screen and clear the highlight plane */
void background_show(void)
{
    blit_copy((const u8 *)0x134a6 + 0x22, screen_front);
    plane5_clear();
}

/* orig $0016DE: as background_show, but only the right-hand part */
void background_show_right(void)
{
    blit_copy_right((const u8 *)0x134a6 + 0x22, (u8 *)screen_front);
    plane5_clear();
}

/* orig $000718: nothing (an `rts` that other code can call) */
void nothing(void)
{
}
