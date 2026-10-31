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
typedef int32_t s32;

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

/* A stored picture: a header word, its 16 colours, then the four low bitplanes interleaved word by word */
struct Picture {
    u16 header;
    u16 palette[16];
    u16 data[];
};

/* ---- game variables, at their original addresses (gods.ld) ------------------------------------------------------- */

extern u16 *palette_ptr;              /* $1AF6  the 32-word colour table that the interrupt copies to the chip */
extern volatile u16 palette_dirty;              /* $2CC2  non-zero: copy the colour table to COLOR00..31 at the next VBL */
extern volatile u16 vbl_saturating;             /* $2CC4  frames counter, stops at $7FFF */
extern volatile u16 vbl_count;                 /* $2CC6  frames counter, wraps */
extern struct Screen *screen_front;    /* $2940  the screen being drawn to */
extern struct Screen *screen_back;     /* $2944  the second screen */
extern struct Picture title_picture;      /* $3A62 */
extern struct Picture background_picture; /* $134A6 */
extern volatile u8 fade_done;          /* $2948  set to $FF when a palette fade has finished */
extern u16 palette[32];                /* $294A  colour table, copied to COLOR00..31 by the interrupt */
extern volatile u16 music_enabled;     /* $1DD2  non-zero: the interrupt plays the music */
extern volatile u16 fire_pressed;      /* $1DD4  set by the interrupt when a fire button is seen */
extern volatile u16 input_seen;        /* $1E3C  set together with fire_pressed, stops the button polling */
extern volatile u32 ticks;             /* $1E3E  frames since the intro started, counted by the interrupt */

void game_start(void);   /* orig $000CC8: what follows a fire button; not executed in the recording */
void intro_story(void);  /* orig $000F1E */

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

/* orig $001C18: unscramble the picture at `pic` through the front screen and store it back, plane by plane, in
 * place.  Pictures are stored with the four low planes interleaved word by word. */
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
    blit_copy(title_picture.data, screen_front);
}

/* orig $0016C0: put the intro background on the front screen and clear the highlight plane */
void background_show(void)
{
    blit_copy(background_picture.data, screen_front);
    plane5_clear();
}

/* orig $0016DE: as background_show, but only the right-hand part */
void background_show_right(void)
{
    blit_copy_right((const u8 *)background_picture.data, (u8 *)screen_front);
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

/* ---- the intro: a script of timed steps ----------------------------------------------------------------------------- */
/* The program runs the intro once, waits for the end of its music, fades out and starts again.  Each step waits for
 * `ticks` (counted by the interrupt) to reach a value, so the sequence is driven by the time since the intro began. */

#define STR(addr) ((const u8 *)(addr))     /* text at an original address */
#define PAL(addr) ((const u16 *)(addr))    /* a table of 16 colours at an original address */

extern u8 draw_flag;                   /* $2CB0  set while the sprite and text routines should also draw the shadow */
extern u16 text_colour;                /* $394A  */
extern u16 scroll_y;                   /* $169A  vertical position of the line being scrolled */
extern u16 scroll_step;                /* $169E  */

void text_draw(const u8 *text, u16 y);                 /* orig $0038C6 */
const u8 *text_line(const u8 *text, s16 y);            /* orig $00390C */
void sprite_draw(u16 x, u16 y, u16 index);             /* orig $003950 */

static void wait_ticks(u32 until)
{
    while ((s32)ticks < (s32)until)
        ;
}

/* orig $0016A0: draw a list of text lines from y downwards, 12 lines apart, until one starts with a byte >= $80 */
static void text_list_draw(const u8 *list)
{
    s16 y = scroll_y;

    while ((s8)*list >= 0) {
        list = text_line(list, y);
        y += 12;
    }
}

/* orig $001608: scroll a list of text lines up the screen from below, one pixel per frame */
static void text_list_scroll(const u8 *list)
{
    scroll_y = 0xc8;
    scroll_step = 1;
    do {
        scroll_y--;
        background_show_right();
        text_list_draw(list);
        screens_flip_after(1);
    } while ((s16)scroll_y > -60);
}

/* orig $00190C: four credit screens, each faded in and out */
static void intro_credits(void)
{
    screen_clear();
    text_colour = 0x30;
    text_draw(STR(0x1da8), 0x50);
    screens_flip();
    palette_fade_in(PAL(0xe98), 1);
    wait_ticks(0x5a);
    palette_fade_out(1);
    screen_clear();
    text_draw(STR(0x1db1), 0x50);
    screens_flip();
    wait_ticks(0xc2);
    palette_fade_in(PAL(0xe98), 1);
    wait_ticks(0x120);
    palette_fade_out(1);
    screen_clear();
    text_draw(STR(0x1dba), 0x50);
    screens_flip();
    wait_ticks(0x180);
    palette_fade_in(PAL(0xe98), 1);
    wait_ticks(0x1d8);
    palette_fade_out(1);
    screen_clear();
    text_draw(STR(0x1dcc), 0x50);
    screens_flip();
    wait_ticks(0x241);
    palette_fade_in(PAL(0xe98), 1);
    wait_ticks(0x299);
    palette_fade_out(1);
}

/* orig $001B16: the seven letters of the title, each drawn from its own image */
static void title_letters(void)
{
    draw_flag = 1;
    sprite_draw(0x3c, 0x10, 0x29);
    sprite_draw(0x5c, 0x10, 0x2a);
    sprite_draw(0x7c, 0x10, 0x2b);
    sprite_draw(0x9c, 0x10, 0x2c);
    sprite_draw(0xbc, 0x10, 0x2d);
    sprite_draw(0xdc, 0x10, 0x2e);
    sprite_draw(0xfc, 0x10, 0x2f);
}

/* orig $001AFA: the title picture and its letters */
static void title_screen(void)
{
    blit_copy(title_picture.data, screen_front);
    plane5_set();
    title_letters();
}

/* orig $001A2A: the title: picture, letters and fades */
static void intro_logo(void)
{
    palette_clear();
    title_screen();
    screens_flip();
    wait_ticks(0x300);
    palette_fade_in(PAL(0x18ac), 0);
    palette_fade_to(PAL(0x18ec), 1);
    wait_ticks(0x366);
    palette_ptr += 16;   /* the second set of 16 colours */
    palette_fade_in(title_picture.palette, 2);
    palette_ptr -= 16;
    wait_ticks(0x3c7);
    palette_fade_in_fast(PAL(0x18ac), 0);
    palette_fade_to(PAL(0x18ec), 1);
    palette_ptr += 16;
    wait_ticks(0x489);
    palette_fade_out(1);
    palette_ptr -= 16;
    wait_ticks(0x4e8);
    palette_fade_out(1);
}

/* orig $0014F6: the hero scrolls in, then lines of the story scroll up the screen */
static void intro_text_scroll(void)
{
    background_show();
    screens_flip();
    wait_ticks(0x53e);
    palette_fade_in_fast(PAL(0x18cc), 0);
    palette_fade_to(background_picture.palette, 1);
    background_show();
    wait_ticks(0x608);
    screens_flip();
    wait_ticks(0x669);
    draw_flag = 0;
    text_list_scroll(STR(0x170a));
    text_list_scroll(STR(0x1746));
    text_list_scroll(STR(0x1774));
    wait_ticks(0xd28);
    palette_fade_out(1);
    wait_ticks(0xd63);
    palette_fade_in_fast(PAL(0x18cc), 0);
    palette_fade_to(background_picture.palette, 1);
    text_list_scroll(STR(0x17b3));
    text_list_scroll(STR(0x17e2));
    {
        int i;

        for (i = 0; i < 100; i++)
            vbl_wait();
    }
    text_list_scroll(STR(0x181b));
    text_list_scroll(STR(0x184e));
    text_list_scroll(STR(0x187b));
    wait_ticks(0x1900);
    palette_fade_out(3);
}

/* orig $000EB8: the whole intro */
void intro_run(void)
{
    intro_credits();
    intro_logo();
    intro_text_scroll();
    intro_story();
}

/* the fade-in that every story page ends with */
static void story_page_fade(u16 frames)
{
    palette_fade_in_fast(PAL(0x18ac), 0);
    palette_fade_to(PAL(0x18ec), frames);
}

/* orig $000F1E: the title picture with the pages of the story over it */
void intro_story(void)
{
    int i;

    text_colour = 0;
    draw_flag = 1;
    picture_show_title();
    plane5_set();
    screens_flip();
    palette_ptr += 16;
    wait_ticks(0x1962);
    palette_fade_in(title_picture.palette, 1);
    palette_ptr -= 16;
    for (i = 0; i < 16; i++)
        palette[i] = PAL(0x18ec)[i] * 2;
    palette_dirty = 0xffff;

    picture_show_title();
    plane5_set();
    text_draw(STR(0x144c), 0x50);
    wait_ticks(0x1986);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    text_draw(STR(0x145c), 0x46);
    text_draw(STR(0x1461), 0x64);
    wait_ticks(0x19ea);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    text_draw(STR(0x146c), 0x46);
    text_draw(STR(0x1475), 0x64);
    wait_ticks(0x1a49);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    text_draw(STR(0x1482), 0x46);
    text_draw(STR(0x1489), 0x64);
    text_draw(STR(0x1497), 0x7a);
    wait_ticks(0x1aa8);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    text_draw(STR(0x14a2), 0x46);
    text_draw(STR(0x14a8), 0x64);
    wait_ticks(0x1b07);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    text_draw(STR(0x14b3), 0x46);
    text_draw(STR(0x14c6), 0x64);
    wait_ticks(0x1b66);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    text_draw(STR(0x14d5), 0x46);
    text_draw(STR(0x14e5), 0x64);
    wait_ticks(0x1bc5);
    screens_flip();
    story_page_fade(1);

    picture_show_title();
    plane5_set();
    title_letters();
    wait_ticks(0x1c24);
    screens_flip();
    story_page_fade(1);

    screen_restore();
    text_draw(STR(0x124e), 0x64);
    wait_ticks(0x1d49);
    screens_flip();
    palette_fade_in_fast(PAL(0x18ac), 0);
    palette_fade_to(PAL(0x18ec), 3);
}

/* ---- the program ---------------------------------------------------------------------------------------------------- */

void intro_setup(void);   /* orig $000E06 */

/* orig $000B12: the program.  Set up the hardware, start the interrupt, then run the intro over and over.
 *
 * The original switches itself to supervisor mode with a TRAP, sets the stack to $2242 and raises the interrupt mask
 * with `move #$2700,sr`; none of that can be written in C.  The loader enters here already in supervisor mode with a
 * usable stack, and interrupts are held off through INTENA, which is enough because that is the only way they are
 * enabled. */
void game_main(void)
{
    volatile u32 *vector = (volatile u32 *)0;
    volatile u32 i;
    int s;

    vector[0x84 / 4] = 0x4e714e71;   /* the loader's jump into the program is overwritten with NOPs */
    vector[0x88 / 4] = 0x4e714e71;
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;
    custom->cop1lc = 0xf8;
    custom->cop2lc = 0xf8;
    custom->copjmp1 = 0;
    screen_front = (struct Screen *)0x763c0;
    screen_back = (struct Screen *)0x6c780;
    palette_clear();
    for (s = 0; s < 16; s++) {
        custom->color[s] = palette[s];
        custom->color[16 + s] = palette[s];
    }
    vector[0x84 / 4] = 0xb9a;   /* the trap vector; the original then executes TRAP #1 */

    vector[0] = 0x38;
    for (s = 0; s < 8; s++)
        custom->sprpt[s] = 0x38;
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;
    custom->cop1lc = 0xfc;
    custom->cop2lc = 0xfc;
    custom->copjmp1 = 0;
    custom->dmacon = 0x83c0;   /* master, bitplane, copper and blitter DMA on */
    ticks = 0;
    vector[0x6c / 4] = (u32)irq_level3;
    custom->intena = 0xc020;   /* master enable and vertical blank */
    (void)custom->intreqr;
    (void)custom->intenar;
    intro_setup();
    fire_pressed = 0;
    for (i = 1000000; i != 0; i--)
        ;
    for (;;) {
        music_init();
        CIAA_PRA |= 0x02;
        music_enabled = 1;
        ticks = 0;
        screen_clear();
        intro_run();
        while ((s32)ticks < 0x1dd0)
            ;
        music_enabled = 0;
        for (s = 0; s < 201; s++)
            vbl_wait();
        palette_ptr += 16;
        palette_fade_out(3);
        palette_ptr -= 16;
        palette_fade_out(3);
        screen_clear();
        screens_flip();
        screen_clear();
        palette_clear();
        for (s = 0; s < 151; s++)
            vbl_wait();
    }
}
