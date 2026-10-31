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
typedef int8_t s8;
typedef int16_t s16;

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
extern volatile u8 fade_done;          /* $2948  set to $FF when a palette fade has finished */
extern u16 palette[32];                /* $294A  colour table, copied to COLOR00..31 by the interrupt */
extern volatile u16 music_enabled;     /* $1DD2  non-zero: the interrupt plays the music */
extern volatile u16 fire_pressed;      /* $1DD4  set by the interrupt when a fire button is seen */
extern volatile u16 input_seen;        /* $1E3C  set together with fire_pressed, stops the button polling */
extern volatile u32 ticks;             /* $1E3E  frames since the intro started, counted by the interrupt */

void game_start(void);   /* orig $000CC8: what follows a fire button; not executed in the recording */

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

/* ---- the ProTracker module and its replayer ---------------------------------------------------------------------- */

struct Sample {
    u8 name[22];
    u16 length;       /* in words */
    u16 volume;       /* finetune and volume, read together */
    u16 loop_start;   /* in words */
    u16 loop_length;  /* in words */
};

struct Module {
    u8 title[20];
    struct Sample sample[31];
    u8 song_length;
    u8 restart;
    u8 order[128];
    u8 tag[4];
    u8 patterns[];    /* 1024 bytes per pattern: 64 rows of four 4-byte cells */
};
_Static_assert(offsetof(struct Module, order) == 0x3b8, "module layout");
_Static_assert(offsetof(struct Module, patterns) == 0x43c, "module layout");

struct Channel {
    u8 cell[4];       /* +00 the current pattern cell: sample/period high, period low, sample low/effect, parameter */
    u32 sample;       /* +04 start of the sample data */
    u16 length;       /* +08 length in words */
    u32 loop;         /* +0A loop start address */
    u16 loop_length;  /* +0E loop length in words */
    u16 period;       /* +10 */
    u16 volume;       /* +12 */
    u16 dma;          /* +14 this channel's DMA bit */
    u8 porta_down;    /* +16 tone portamento direction */
    u8 porta_speed;   /* +17 */
    u16 porta_target; /* +18 */
    u8 vibrato;       /* +1A speed and depth */
    u8 vibrato_pos;   /* +1B */
};
_Static_assert(sizeof(struct Channel) == 0x1c, "channel layout");

#define module ((struct Module *)0x1b6f2)
extern struct Channel channel[4];      /* $28D0 */
extern u32 sample_start[31];           /* $2854  start of each sample's data, set by music_init */
extern u16 period_table[];             /* $2800  the 37 periods of the ProTracker table, three octaves */
extern u8 vibrato_table[32];           /* $27E0 */
extern u8 music_speed;                 /* $284C  ticks per row */
extern u8 music_order;                 /* $284D  position in the order table */
extern u16 music_row;                  /* $284E  byte offset of the row in the pattern, 16 per row */
extern u8 music_tick;                  /* $2850 */
extern u8 music_jump;                  /* $2851  pattern break requested */
extern u16 music_dma;                  /* $2852  DMA bits of the channels started on this row */

/* orig $002242: find where the sample data starts (after the last pattern used), record each sample's address and
 * silence the four channels */
void music_init(void)
{
    const u8 *order = module->order;
    u8 highest = 0, entry = 0;   /* highest pattern number seen so far, and the entry being looked at */
    u16 count = 0x7f;
    u8 *data;
    int i;

    for (;;) {
        highest = entry;
        count--;   /* each new highest pattern costs one entry of the scan */
        for (;;) {
            entry = *order++;
            if ((s8)entry > (s8)highest)
                break;
            if (--count == 0xffff)
                goto scanned;
        }
    }
scanned:
    data = module->patterns + ((u32)(u8)(highest + 1) << 10);   /* the sample data follows the last pattern */
    for (i = 0; i < 31; i++) {
        *(u32 *)data = 0;
        sample_start[i] = (u32)data;
        data += (u32)module->sample[i].length * 2;
    }
    music_speed = 6;
    custom->aud[0].vol = 0;
    custom->aud[1].vol = 0;
    custom->aud[2].vol = 0;
    custom->aud[3].vol = 0;
    music_order = 0;
    music_tick = 0;
    music_row = 0;
}

/* orig $0025A6: effect 3 (tone portamento): set the target period and the direction */
static void tone_portamento_setup(struct Channel *ch)
{
    u16 target = *(u16 *)ch->cell & 0xfff;

    ch->porta_target = target;
    ch->porta_down = 0;
    if (target == ch->period) {
        ch->porta_target = 0;
    } else if ((s16)target < (s16)ch->period) {
        ch->porta_down = 1;
    }
}

/* orig $002412: start of a row for one channel.  Read the pattern cell at `offset`, start the sample it names and
 * run the effects that act when the row begins.  Returns the offset of the next cell. */
static u32 channel_row(struct Channel *ch, volatile struct AudioChannel *hw, const u8 *patterns, u32 offset)
{
    u8 number;
    u8 effect;

    *(u32 *)ch->cell = *(const u32 *)(patterns + offset);
    offset += 4;
    number = (ch->cell[2] >> 4) | (ch->cell[0] & 0xf0);
    if (number != 0) {
        const struct Sample *smp = &module->sample[number - 1];

        ch->sample = sample_start[number - 1];
        ch->length = smp->length;
        ch->volume = smp->volume;
        if (smp->loop_start != 0) {
            ch->loop = ch->sample + (u16)(smp->loop_start << 1);
            ch->length = smp->loop_start + smp->loop_length;
        } else {
            ch->loop = ch->sample;
        }
        ch->loop_length = smp->loop_length;
        hw->vol = ch->volume;
    }
    if ((*(u16 *)ch->cell & 0xfff) != 0) {
        if ((ch->cell[2] & 0xf) == 3) {
            tone_portamento_setup(ch);
        } else {
            ch->period = *(u16 *)ch->cell & 0xfff;
            custom->dmacon = ch->dma;           /* stop the channel until the new sample is in place */
            ch->vibrato_pos = 0;
            hw->lc = ch->sample;
            hw->len = ch->length;
            hw->per = ch->period & 0xfff;
            music_dma |= ch->dma;
        }
    }
    effect = ch->cell[2] & 0xf;
    switch (effect) {
    case 0xd:   /* pattern break */
        music_jump = ~music_jump;
        break;
    case 0xb:   /* position jump */
        music_order = ch->cell[3] - 1;
        music_jump = ~music_jump;
        break;
    case 0xc:   /* set volume */
        if (ch->cell[3] > 0x40)
            ch->cell[3] = 0x40;
        hw->vol = ch->cell[3];
        break;
    case 0xf: { /* set speed */
        u16 speed = ch->cell[3] & 0x1f;

        if (speed != 0) {
            music_tick = 0;
            music_speed = speed;
        }
        break;
    }
    default:
        break;
    }
    return offset;
}

/* orig $002682: the effects that run on every tick after the first of a row */
static void channel_tick(struct Channel *ch, volatile struct AudioChannel *hw)
{
    u8 effect;

    if ((*(u16 *)&ch->cell[2] & 0xfff) == 0) {
        hw->per = ch->period;   /* orig $00267A */
        return;
    }
    effect = ch->cell[2] & 0xf;
    switch (effect) {
    case 0: {   /* arpeggio, orig $00234A */
        u16 step = music_tick % 3;
        u16 semitones;
        const u16 *p;
        int n;

        if (step == 0) {
            hw->per = ch->period;
            return;
        }
        semitones = step == 2 ? (ch->cell[3] & 0xf) : (ch->cell[3] >> 4);
        for (p = period_table, n = 0x24; n >= 0; n--, p++) {
            u16 shifted = p[semitones];

            if ((s16)ch->period >= (s16)p[0]) {
                hw->per = shifted;
                return;
            }
        }
        return;
    }
    case 1:     /* portamento up, orig $002708 */
        ch->period -= ch->cell[3];
        if ((s16)(ch->period & 0xfff) < 0x71)
            ch->period = (ch->period & 0xf000) | 0x71;
        hw->per = ch->period & 0xfff;
        return;
    case 2:     /* portamento down, orig $00273A */
        ch->period += ch->cell[3];
        if ((s16)(ch->period & 0xfff) >= 0x358)
            ch->period = (ch->period & 0xf000) | 0x358;
        hw->per = ch->period & 0xfff;
        return;
    case 3:     /* tone portamento, orig $0025CC */
        if (ch->cell[3] != 0) {
            ch->porta_speed = ch->cell[3];
            ch->cell[3] = 0;
        }
        if (ch->porta_target == 0)
            return;
        if (ch->porta_down == 0) {
            ch->period += ch->porta_speed;
            if ((s16)ch->porta_target <= (s16)ch->period) {
                ch->period = ch->porta_target;
                ch->porta_target = 0;
            }
        } else {
            ch->period -= ch->porta_speed;
            if ((s16)ch->porta_target >= (s16)ch->period) {
                ch->period = ch->porta_target;
                ch->porta_target = 0;
            }
        }
        hw->per = ch->period;
        return;
    case 4: {   /* vibrato, orig $00262C */
        u16 amount;
        u16 per;

        if (ch->cell[3] != 0)
            ch->vibrato = ch->cell[3];
        amount = (vibrato_table[(ch->vibrato_pos >> 2) & 0x1f] * (ch->vibrato & 0xf)) >> 6;
        per = (s8)ch->vibrato_pos < 0 ? ch->period - amount : ch->period + amount;
        hw->per = per;
        ch->vibrato_pos += (ch->vibrato >> 2) & 0x3c;
        return;
    }
    default:
        break;
    }
    hw->per = ch->period;
    if (effect == 0xa) {   /* volume slide, orig $0026C6 */
        u8 up = ch->cell[3] >> 4;

        if (up != 0) {
            ch->volume += up;
            if ((s16)ch->volume >= 0x40)
                ch->volume = 0x40;
        } else {
            ch->volume -= ch->cell[3] & 0xf;
            if ((s16)ch->volume < 0)
                ch->volume = 0;
        }
        hw->vol = ch->volume;
    }
    }

/* orig $0022E6: advance the music by one tick (called by the interrupt) */
void music_step(void)
{
    volatile struct AudioChannel *hw = custom->aud;
    int i;

    music_tick++;
    if ((s8)music_tick < (s8)music_speed) {
        for (i = 0; i < 4; i++)
            channel_tick(&channel[i], &hw[i]);
        goto advance;
    }
    music_tick = 0;
    {
        const u8 *patterns = module->patterns;
        u32 offset = (u32)module->order[music_order] << 10;

        offset = (u16)(offset + music_row);
        music_dma = 0;
        for (i = 0; i < 4; i++)
            offset = channel_row(&channel[i], &hw[i], patterns, offset);
    }
    {
        volatile int wait;

        for (wait = 0; wait < 0x12d; wait++)
            ;   /* let the channels stop before the new samples are started */
        custom->dmacon = music_dma | DMAF_SETCLR;
        for (wait = 0; wait < 0x12d; wait++)
            ;
    }
    for (i = 3; i >= 0; i--) {   /* the loops that follow the start of each sample */
        hw[i].lc = channel[i].loop;
        hw[i].len = channel[i].loop_length;
    }
    music_row += 0x10;
    if (music_row == 0x400) {
        music_row = 0;
        music_jump = 0;
        music_order = (music_order + 1) & 0x7f;
        if (music_order == module->song_length)
            music_order = 0;
    }
advance:
    while (music_jump != 0) {   /* a pattern break or position jump: start the next pattern now */
        music_row = 0;
        music_jump = 0;
        music_order = (music_order + 1) & 0x7f;
        if (music_order == module->song_length)
            music_order = 0;
    }
}

/* ---- palette fades ------------------------------------------------------------------------------------------------- */
/* Each fade works on the 16 colours at palette_ptr and waits `frames` + 1 vertical blanks per step.  The target colour
 * table holds the colours at half scale (each component is doubled before it is compared). */

/* orig $002DC8: 15 steps; every component that differs from the target goes up by one */
void palette_fade_in(const u16 *target, u16 frames)
{
    int step, i;
    u16 wait;

    for (step = 0; step < 15; step++) {
        for (i = 15; i >= 0; i--) {
            u16 colour = palette_get(i);
            u16 want = target[i] + target[i];

            if (((want & 0x00f) - (colour & 0x00f)) & 0xffff)
                colour += 0x001;
            if (((want & 0x0f0) - (colour & 0x0f0)) & 0xffff)
                colour += 0x010;
            if (((want & 0xf00) - (colour & 0xf00)) & 0xffff)
                colour += 0x100;
            palette_set(i, colour);
        }
        palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
    fade_done = 0xff;
}

/* orig $002E52: 8 steps; every component below the target goes up by one or two */
void palette_fade_in_fast(const u16 *target, u16 frames)
{
    int step, i;
    u16 wait;

    for (step = 0; step < 8; step++) {
        for (i = 15; i >= 0; i--) {
            u16 colour = palette_get(i);
            u16 want = target[i] + target[i];
            s16 gap;

            gap = (want & 0x00f) - (colour & 0x00f);
            if (gap > 0)
                colour += gap == 1 ? 0x001 : 0x002;
            gap = (want & 0x0f0) - (colour & 0x0f0);
            if (gap > 0)
                colour += gap == 0x10 ? 0x010 : 0x020;
            gap = (want & 0xf00) - (colour & 0xf00);
            if (gap > 0)
                colour += gap == 0x100 ? 0x100 : 0x200;
            palette_set(i, colour);
        }
        palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
    fade_done = 0xff;
}

/* orig $002F3A: 15 steps; every component above the target goes down by one */
void palette_fade_to(const u16 *target, u16 frames)
{
    int step, i;
    u16 wait;

    for (step = 0; step < 15; step++) {
        for (i = 15; i >= 0; i--) {
            u16 colour = palette_get(i);
            u16 want = target[i] + target[i];

            if ((s16)((want & 0x00f) - (colour & 0x00f)) < 0)
                colour -= 0x001;
            if ((s16)((want & 0x0f0) - (colour & 0x0f0)) < 0)
                colour -= 0x010;
            if ((s16)((want & 0xf00) - (colour & 0xf00)) < 0)
                colour -= 0x100;
            palette_set(i, colour);
        }
        palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
    fade_done = 0xff;
}

/* orig $002FC4: 15 steps; every non-zero component goes down by one, so the colours fade to black */
void palette_fade_out(u16 frames)
{
    int step, i;
    u16 wait;

    for (step = 0; step < 15; step++) {
        for (i = 15; i >= 0; i--) {
            u16 colour = palette_get(i);

            if (colour & 0x00f)
                colour -= 0x001;
            if (colour & 0x0f0)
                colour -= 0x010;
            if (colour & 0xf00)
                colour -= 0x100;
            palette_set(i, colour);
        }
        palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
}

/* ---- showing the finished screen --------------------------------------------------------------------------------- */

/* orig $00307A: show the screen that was drawn, and start drawing on the other one.  The copper list for the buffer
 * that is now on show lives at $A8 or $FC. */
void screens_flip(void)
{
    struct Screen *shown = screen_back;
    u32 copper;

    if (fire_pressed != 0)
        game_start();   /* not executed in the recording: a fire button was pressed */
    screen_back = screen_front;
    screen_front = shown;
    copper = (u32)screen_back == 0x6c780 ? 0xfc : 0xa8;
    custom->cop1lc = copper;
    vbl_saturating = 0;
    vbl_wait();
}

/* orig $003070: wait until `frames` vertical blanks have passed since the last flip, then flip */
void screens_flip_after(s16 frames)
{
    while (frames >= (s16)vbl_saturating)
        ;
    screens_flip();
}

/* ---- the vertical-blank interrupt -------------------------------------------------------------------------------- */

#define INTF_VERTB 0x0020
#define CIAA_PRA (*(volatile u8 *)0xbfe001)   /* bits 6 and 7: the two fire buttons, 0 when pressed */

/* orig $002CF4: the tail of the interrupt: refresh the chip's colours and count frames */
static void vbl_housekeeping(void)
{
    if (palette_dirty != 0) {
        int i;

        palette_dirty = 0;
        for (i = 0; i < 32; i++)
            custom->color[i] = palette[i];
    }
    if (vbl_saturating != 0x7fff)
        vbl_saturating++;
    vbl_count++;
}

/* orig $001DD6: level 3 interrupt (vertical blank).  The original begins with `move #$2700,sr`, which C cannot
 * write; only VERTB is enabled (INTENA is $C020) so no other interrupt can come in and it is not needed. */
__attribute__((interrupt)) void irq_level3(void)
{
    if ((custom->intreqr & INTF_VERTB) && music_enabled != 0)
        music_step();
    custom->intreq = 0x70;   /* clear COPER, VERTB and BLIT */
    ticks++;
    if (input_seen == 0) {
        u8 pra = CIAA_PRA;

        if (!((pra & 0x80) && (pra & 0x40))) {   /* a fire button is down */
            fire_pressed = 1;
            input_seen = 1;
        }
    }
    vbl_housekeeping();
}
