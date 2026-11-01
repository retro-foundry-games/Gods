/* Gods SDL port: the blitter of the chip, for the routines of the game proper (game_b.c, game_d.c) that program its registers directly.
 * It runs when blit_start() stores BLTSIZE: copy and logic modes (the minterm of BLTCON0 over A, B and C to D), ascending and descending,
 * the shifts of A and B, the first and last word masks, the modulos.  Not the line or fill modes (the program uses neither).  The words of
 * the bitplanes are native words. */
#include <stdint.h>

#include "guest.h"

#define BLTCON0 0x040
#define BLTCON1 0x042
#define BLTAFWM 0x044
#define BLTALWM 0x046
#define BLTCPT 0x048
#define BLTBPT 0x04c
#define BLTAPT 0x050
#define BLTDPT 0x054
#define BLTSIZE 0x058
#define BLTCMOD 0x060
#define BLTBMOD 0x062
#define BLTAMOD 0x064
#define BLTDMOD 0x066
#define BLTCDAT 0x070
#define BLTBDAT 0x072
#define BLTADAT 0x074

static uint16_t minterm(uint8_t lf, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t d = 0;

    for (int i = 0; i < 8; i++)
        if (lf & (1 << i))
            d |= (uint16_t)(((i & 4) ? a : ~a) & ((i & 2) ? b : ~b) & ((i & 1) ? c : ~c));
    return d;
}

void sdl_blit_run(void)
{
    uint16_t con0 = REG16(BLTCON0), con1 = REG16(BLTCON1), size = REG16(BLTSIZE);
    unsigned height = size >> 6 ? size >> 6 : 1024, width = size & 63 ? size & 63 : 64;
    int use_a = con0 >> 11 & 1, use_b = con0 >> 10 & 1, use_c = con0 >> 9 & 1, use_d = con0 >> 8 & 1;
    int ash = con0 >> 12, bsh = con1 >> 12, desc = con1 & 2;
    uint8_t lf = con0 & 0xff;
    uintptr_t apt = REG32(BLTAPT), bpt = REG32(BLTBPT), cpt = REG32(BLTCPT), dpt = REG32(BLTDPT);
    int step = desc ? -2 : 2;
    int amod = (int16_t)REG16(BLTAMOD), bmod = (int16_t)REG16(BLTBMOD), cmod = (int16_t)REG16(BLTCMOD), dmod = (int16_t)REG16(BLTDMOD);

    if (con1 & 1)
        return;   /* line mode */
    for (unsigned y = 0; y < height; y++) {
        uint32_t aold = 0, bold = 0;

        for (unsigned x = 0; x < width; x++) {
            uint16_t a = use_a ? *(uint16_t *)apt : REG16(BLTADAT);
            uint16_t b = use_b ? *(uint16_t *)bpt : REG16(BLTBDAT);
            uint16_t c = use_c ? *(uint16_t *)cpt : REG16(BLTCDAT);
            uint16_t mask = 0xffff, a_shifted, b_shifted, d;

            if (x == 0)
                mask &= REG16(BLTAFWM);
            if (x == width - 1)
                mask &= REG16(BLTALWM);
            a &= mask;
            if (desc) {
                a_shifted = (uint16_t)((((uint32_t)a << 16 | aold) << ash) >> 16);
                b_shifted = (uint16_t)((((uint32_t)b << 16 | bold) << bsh) >> 16);
            } else {
                a_shifted = (uint16_t)(((aold << 16 | a) >> ash) & 0xffff);
                b_shifted = (uint16_t)(((bold << 16 | b) >> bsh) & 0xffff);
            }
            aold = a;
            bold = b;
            d = minterm(lf, a_shifted, b_shifted, c);
            if (use_d)
                *(uint16_t *)dpt = d;
            if (use_a)
                apt += step;
            if (use_b)
                bpt += step;
            if (use_c)
                cpt += step;
            if (use_d)
                dpt += step;
        }
        if (use_a)
            apt += desc ? -amod : amod;
        if (use_b)
            bpt += desc ? -bmod : bmod;
        if (use_c)
            cpt += desc ? -cmod : cmod;
        if (use_d)
            dpt += desc ? -dmod : dmod;
    }
    REG32(BLTAPT) = (uint32_t)apt;
    REG32(BLTBPT) = (uint32_t)bpt;
    REG32(BLTCPT) = (uint32_t)cpt;
    REG32(BLTDPT) = (uint32_t)dpt;
}
