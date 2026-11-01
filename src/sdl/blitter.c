/* Gods SDL port: the blitter, run when the program starts it (sdl_blit_run, called by blit_start() after the store to BLTSIZE), and the
 * two masked-image routines of the original's asm (blit_image, blit_image_wide, orig $0171E4 and $0171F6) that program it.
 * The words of the bitplanes in memory are native words (the program copies them; the disk's are swapped on loading). */
#include <stdint.h>
#include <string.h>

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

    for (int i = 0; i < 8; i++) {
        if (!(lf & (1 << i)))
            continue;
        d |= (uint16_t)(((i & 4) ? a : ~a) & ((i & 2) ? b : ~b) & ((i & 1) ? c : ~c));
    }
    return d;
}

/* the blit the registers describe (copy and logic modes, ascending and descending, shifts, masks, modulos; not the line or fill modes) */
void blitter_run(void)
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
        return;   /* line mode: not used by the program */
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

void sdl_blit_run(void)
{
    blitter_run();
}

extern uint8_t *mask_buffer;
extern uint16_t blit_extra_modulo;
extern uint32_t blit_plane_stride;

static void set_ptr(unsigned reg, const void *p)
{
    REG32(reg) = (uint32_t)(uintptr_t)p;
}

/* the masked blit of draw.c (blit_masked_image) with the first and last word masks of these two entries: an image `words` words wide
 * (the planes `plane` bytes apart), the mask made of its planes in mask_buffer, then the four planes to the screen */
static void blit_image_words(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows, uint16_t words)
{
    uint16_t size = (uint16_t)((rows << 6) | words);
    uint16_t dst_mod = (uint16_t)(44 - words * 2);
    uint16_t a_mod, b_mod;
    const uint8_t *s = src;
    uint8_t *d = dst;
    int16_t stride;

    blit_extra_modulo = 0;
    shift &= 0xff;
    set_ptr(BLTAPT, s);
    s += (int16_t)plane;
    set_ptr(BLTBPT, s);
    s += (int16_t)plane;
    set_ptr(BLTCPT, s);
    s += (int16_t)plane;
    set_ptr(BLTDPT, mask_buffer);
    REG16(BLTAFWM) = 0xffff;
    REG16(BLTALWM) = 0xffff;
    REG16(BLTDMOD) = 0;
    REG16(BLTCON1) = 0;
    REG16(BLTCON0) = 0x0ffe;
    REG16(BLTAMOD) = 0;
    REG16(BLTBMOD) = 0;
    REG16(BLTCMOD) = 0;
    REG16(BLTSIZE) = size;
    blitter_run();
    REG16(BLTBMOD) = 0;
    REG16(BLTCON0) = 0x0dfc;
    set_ptr(BLTAPT, s);
    set_ptr(BLTBPT, mask_buffer);
    set_ptr(BLTDPT, mask_buffer);
    REG16(BLTSIZE) = size;
    blitter_run();
    REG16(BLTAFWM) = 0xffff;
    REG16(BLTALWM) = 0;
    if (shift == 0) {
        REG16(BLTCON0) = 0x0fca;
        REG16(BLTALWM) = 0xffff;
        a_mod = 0;
        REG16(BLTCON1) = 0;
        b_mod = 0;
    } else {
        uint16_t con1 = (uint16_t)(shift << 12);

        REG16(BLTCON1) = con1;
        REG16(BLTCON0) = con1 | 0x0fca;
        dst_mod -= 2;
        size++;
        a_mod = (uint16_t)-2;
        b_mod = (uint16_t)-2;
    }
    REG16(BLTAMOD) = a_mod;
    REG16(BLTBMOD) = b_mod;
    REG16(BLTCMOD) = dst_mod;
    REG16(BLTDMOD) = dst_mod;
    stride = (int16_t)blit_plane_stride;
    s = src;
    for (int i = 0; i < 4; i++) {
        if (i != 0) {
            d += stride;
            s += (int16_t)plane;
        }
        set_ptr(BLTCPT, d);
        set_ptr(BLTBPT, s);
        set_ptr(BLTAPT, mask_buffer);
        set_ptr(BLTDPT, d);
        REG16(BLTSIZE) = size;
        blitter_run();
    }
}

/* orig $0171E4: draw an image one word wide */
void blit_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_image_words(dst, src, plane, shift, rows, 1);
}

/* orig $0171F6: ... and two words wide */
void blit_image_wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_image_words(dst, src, plane, shift, rows, 2);
}
