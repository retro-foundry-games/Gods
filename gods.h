/* Gods: the game's data types and the declarations of its data (defined in data.c).
 *
 * The linker script gods.ld places the data at the addresses the original keeps it at, so memory matches the
 * original; the C code names none of those addresses. */
#ifndef GODS_H
#define GODS_H

#include <stddef.h>
#include <stdint.h>

/* ---- the screen -------------------------------------------------------------------------------------------------- */

enum { SCREEN_WORDS = 20, SCREEN_ROWS = 200, SCREEN_PLANES = 5 };

/* 320x200, five bitplanes, each plane stored whole (8000 bytes) one after the other */
struct Screen {
    uint16_t plane[SCREEN_PLANES][SCREEN_ROWS][SCREEN_WORDS];
};

/* A stored picture: a header word, its 16 colours, then the four low bitplanes interleaved word by word */
struct Picture {
    uint16_t header;
    uint16_t palette[16];
    uint16_t data[16000];
};

/* ---- game variables, at their original addresses (gods.ld) ------------------------------------------------------- */

/* Variables that sit next to each other in the original are structs, so memory matches. */

/* the colour table that the interrupt copies to the chip, and the flag that a fade sets when it is done */
struct PaletteState {
    volatile uint8_t fade_done;     /* $FF when a palette fade has finished */
    uint8_t unused;
    uint16_t colour[32];            /* copied to COLOR00..31 by the interrupt when vblank.palette_dirty is set */
};

/* counters that the vertical blank interrupt maintains */
struct Vblank {
    volatile uint16_t palette_dirty;    /* non-zero: copy the colour table to the chip at the next vertical blank */
    volatile uint16_t saturating;       /* frames since the last screen flip, stops at $7FFF */
    volatile uint16_t count;            /* frames, wraps */
};

struct Flags {
    volatile uint16_t music_enabled;    /* non-zero: the interrupt plays the music */
    volatile uint16_t fire_pressed;     /* set by the interrupt when a fire button is seen */
};

struct GameClock {
    volatile uint16_t input_seen;       /* set together with fire_pressed: stops the button polling */
    volatile uint32_t ticks;            /* frames since the intro started, counted by the interrupt */
};

/* where the music replayer is */
struct MusicState {
    uint8_t speed;      /* ticks per row */
    uint8_t order;      /* position in the order table */
    uint16_t row;       /* byte offset of the row in the pattern, 16 per row */
    uint8_t tick;
    uint8_t jump;       /* a pattern break was requested */
    uint16_t dma;       /* DMA bits of the channels started on this row */
};

extern struct Vblank vblank;
extern struct Flags flags;
extern struct GameClock game_clock;
extern struct MusicState music;
extern uint16_t *palette_ptr;              /* $1AF6  the 32-word colour table that the interrupt copies to the chip */
extern struct Screen *screen_front;    /* $2940  the screen being drawn to */
extern struct Screen *screen_back;     /* $2944  the second screen */

void game_start(void);   /* orig $000CC8: what follows a fire button; not executed in the recording */

/* ---- the ProTracker module and its replayer ---------------------------------------------------------------------- */

struct Sample {
    uint8_t name[22];
    uint16_t length;       /* in words */
    uint16_t volume;       /* finetune and volume, read together */
    uint16_t loop_start;   /* in words */
    uint16_t loop_length;  /* in words */
};

struct Module {
    uint8_t title[20];
    struct Sample sample[31];
    uint8_t song_length;
    uint8_t restart;
    uint8_t order[128];
    uint8_t tag[4];
    uint8_t data[];        /* the patterns (1024 bytes each: 64 rows of four 4-byte cells), then the sample data */
};
_Static_assert(offsetof(struct Module, order) == 0x3b8, "module layout");
_Static_assert(offsetof(struct Module, data) == 0x43c, "module layout");

struct Channel {
    uint8_t cell[4];       /* +00 the current pattern cell: sample/period high, period low, sample low/effect, parameter */
    uint32_t sample;       /* +04 start of the sample data */
    uint16_t length;       /* +08 length in words */
    uint32_t loop;         /* +0A loop start address */
    uint16_t loop_length;  /* +0E loop length in words */
    uint16_t period;       /* +10 */
    uint16_t volume;       /* +12 */
    uint16_t dma;          /* +14 this channel's DMA bit */
    uint8_t porta_down;    /* +16 tone portamento direction */
    uint8_t porta_speed;   /* +17 */
    uint16_t porta_target; /* +18 */
    uint8_t vibrato;       /* +1A speed and depth */
    uint8_t vibrato_pos;   /* +1B */
};
_Static_assert(sizeof(struct Channel) == 0x1c, "channel layout");

extern struct Module module;           /* the music: the header, then the patterns and the sample data */
extern struct Channel channel[4];      /* $28D0 */
extern uint32_t sample_start[31];           /* $2854  start of each sample's data, set by music_init */
extern const uint16_t period_table[38];       /* $2800  the 36 periods of the ProTracker table, three octaves, and a zero */
extern const uint8_t vibrato_table[32];           /* $27E0 */

struct Image {
    int16_t neg_width;    /* minus the width in pixels */
    uint16_t height;       /* in rows, less one */
    const uint16_t *data;
};

extern uint16_t font_offset;                /* added to a character to find the glyph: $30 selects the second large font */

void glyph_draw(uint16_t x, uint16_t y, uint16_t index);              /* orig $0039DC */
void sprite_draw(uint16_t x, uint16_t y, uint16_t index);             /* orig $003950 */

extern uint8_t draw_flag;                   /* set while the sprite and text routines should also draw the shadow */
extern uint16_t scroll_y;                   /* the line being scrolled */
extern uint16_t scroll_step;

/* the two screens, drawn to and shown in turn, and the copper list for each (the copper program that points the
 * bitplanes at its screen).  A list ends with the copper wait for a position that never comes. */
struct CopperList {
    uint16_t words[40];
    uint16_t end[2];
};

extern struct Screen screen_a, screen_b;
extern struct CopperList copper_a, copper_b;

/* the CPU's exception vectors, which the hardware reads from address 0.  The game uses the first 42 (the rest of the
 * table holds the copper lists). */
enum { VECTOR_SPRITE_TARGET = 14, VECTOR_LEVEL3 = 27 };   /* level 3 is the vertical blank interrupt */
extern volatile uint32_t vectors[42];

/* an entry of the table that describes where each image is on the glyph sheet (the unpacker's input) */
struct ImageSource {
    uint16_t width_m1;      /* width in pixels, less one */
    uint16_t height_m1;     /* rows, less one */
    uint16_t unused;
    uint16_t x;             /* position on the sheet, in pixels */
    uint16_t y;
};

union ImageTable {
    struct {
        uint16_t count;
        struct ImageSource entry[132];
    } source;
    struct Image image[132];   /* what orig $002D4C turns it into */
};

/* The pictures and the table of images sit one after the other in memory. */
struct Graphics {
    struct Picture title;          /* the title picture */
    struct Picture glyph_sheet;    /* the sheet the glyphs and letters are cut from at the start of the program */
    struct Picture background;     /* the background of the story */
    union ImageTable images;       /* the table of images: the unpacker's input, converted in place into its output */
};

extern struct Graphics graphics;

/* what the masked blit keeps between calls: the size of one bitplane of the screen, an extra modulo and the mask it
 * builds from the planes of the image (the original has room for 400 words) */
struct BlitState {
    uint32_t plane_stride;
    uint16_t extra_modulo;
    uint16_t mask[396];
};

/* the colour table and the blitter's working state sit next to each other */
struct DisplayData {
    struct PaletteState palette;
    struct BlitState blit;
};
extern struct DisplayData display;
extern uint16_t glyph_plane_size;        /* non-zero: the size in bytes of one plane of a glyph, otherwise worked out from its height */
extern const uint16_t blank_plane[513];  /* zeros: the source for the fifth plane when drawing text */

#endif
