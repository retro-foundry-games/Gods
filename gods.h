/* Gods: the game's data types and the declarations of its data (defined in data.c).
 *
 * The linker script gods.ld places the data at the addresses the original keeps it at, so memory matches the
 * original; the C code names none of those addresses. */
#ifndef GODS_H
#define GODS_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

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
    u8 data[];        /* the patterns (1024 bytes each: 64 rows of four 4-byte cells), then the sample data */
};
_Static_assert(offsetof(struct Module, order) == 0x3b8, "module layout");
_Static_assert(offsetof(struct Module, data) == 0x43c, "module layout");

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

extern struct Module module;           /* the music: the header, then the patterns and the sample data */
extern struct Channel channel[4];      /* $28D0 */
extern u32 sample_start[31];           /* $2854  start of each sample's data, set by music_init */
extern const u16 period_table[38];       /* $2800  the 36 periods of the ProTracker table, three octaves, and a zero */
extern const u8 vibrato_table[32];           /* $27E0 */
extern u8 music_speed;                 /* $284C  ticks per row */
extern u8 music_order;                 /* $284D  position in the order table */
extern u16 music_row;                  /* $284E  byte offset of the row in the pattern, 16 per row */
extern u8 music_tick;                  /* $2850 */
extern u8 music_jump;                  /* $2851  pattern break requested */
extern u16 music_dma;                  /* $2852  DMA bits of the channels started on this row */

struct Image {
    s16 neg_width;    /* minus the width in pixels */
    u16 height;       /* in rows, less one */
    const u16 *data;
};

extern struct Image images[];          /* the table of images (built by orig $002D4C) */
extern u16 font_offset;                /* added to a character to find the glyph: $30 selects the second large font */

void glyph_draw(u16 x, u16 y, u16 index);              /* orig $0039DC */
void sprite_draw(u16 x, u16 y, u16 index);             /* orig $003950 */

extern u8 draw_flag;                   /* set while the sprite and text routines should also draw the shadow */
extern u16 scroll_y;                   /* the line being scrolled */
extern u16 scroll_step;

/* the two screens, drawn to and shown in turn, and the copper list for each (the copper program that points the
 * bitplanes at its screen).  A list ends with the copper wait for a position that never comes. */
struct CopperList {
    u16 words[40];
    u16 end[2];
};

extern struct Screen screen_a, screen_b;
extern struct CopperList copper_a, copper_b;

/* the CPU's exception vectors, which the hardware reads from address 0.  The game uses the first 42 (the rest of the
 * table holds the copper lists). */
enum { VECTOR_SPRITE_TARGET = 14, VECTOR_LEVEL3 = 27 };   /* level 3 is the vertical blank interrupt */
extern volatile u32 vectors[42];

#endif
