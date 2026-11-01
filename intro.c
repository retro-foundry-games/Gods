/* Gods (Bitmap Brothers, 1991), PAL original disks: the game, decompiled.
 *
 * Built with m68k-amigaos-gcc for a 68000 A500 (see flake.nix).  The data is in intro_data.c, declared in intro.h, and is
 * placed at the original addresses by intro.ld.  Every function carries its original address as `orig $ADDR` so it can be matched
 * with the original code.  Progress and method: PLAN.md.  Evidence for each recovered fact: analysis/. */
#include "intro.h"

/* The original is hand-written 68000 and the intro is timed to the frame, so the code that runs every frame (drawing,
 * the blitter helpers, the interrupt and the music) is compiled for speed; the rest for size. */
#define HOT

/* ---- palette ------------------------------------------------------------------------------------------------------- */

/* orig $002EFE: clear the colour table and ask for it to be copied to the chip */
HOT void palette_clear(void)
{
    int i;

    for (i = 0; i < 32; i++)
        palette_ptr[i] = 0;
    vblank.palette_dirty = 0xffff;
}

/* orig $003022: colour table entry `index` = `value` (the original takes them in D0 and D1) */
HOT void palette_set(uint16_t index, uint16_t value)
{
    palette_ptr[index] = value;
}

/* orig $003036: colour table entry `index` */
HOT uint16_t palette_get(uint16_t index)
{
    return palette_ptr[index];
}

/* ---- screens ----------------------------------------------------------------------------------------------------- */

/* orig $001C18: unscramble the picture at `pic` through the front screen and store it back, plane by plane, in
 * place.  Pictures are stored with the four low planes interleaved word by word. */
void picture_deinterleave(uint16_t *pic)
{
    const uint16_t *src = pic;
    struct Screen *dst = screen_front;
    int row, w, p;
    uint32_t *from = (uint32_t *)screen_front;
    uint32_t *to = (uint32_t *)pic;
    int i;

    for (row = 0; row < SCREEN_ROWS; row++)
        for (w = 0; w < SCREEN_WORDS; w++)
            for (p = 0; p < 4; p++)
                dst->plane[p][row][w] = *src++;
    for (i = 0; i < 8000; i++)
        *to++ = *from++;
}

/* orig $001432: put the title picture on the front screen */
HOT void picture_show_title(void)
{
    blit_copy(graphics.title.data, screen_front);
}

/* orig $0016C0: put the intro background on the front screen and clear the highlight plane */
HOT void background_show(void)
{
    blit_copy(graphics.background.data, screen_front);
    plane5_clear();
}

/* orig $0016DE: as background_show, but only the right-hand part */
HOT void background_show_right(void)
{
    blit_copy_right((const uint8_t *)graphics.background.data, (uint8_t *)screen_front);
    plane5_clear();
}

/* orig $000718: nothing (an `rts` that other code can call) */
void nothing(void)
{
}


/* ---- palette fades ------------------------------------------------------------------------------------------------- */
/* Each fade works on the 16 colours at palette_ptr and waits `frames` + 1 vertical blanks per step.  The target colour
 * table holds the colours at half scale (each component is doubled before it is compared). */

/* orig $002DC8: 15 steps; every component that differs from the target goes up by one */
void palette_fade_in(const uint16_t *target, uint16_t frames)
{
    int step, i;
    uint16_t wait;

    for (step = 0; step < 15; step++) {
        for (i = 15; i >= 0; i--) {
            uint16_t colour = palette_get(i);
            uint16_t want = target[i] + target[i];

            if (((want & 0x00f) - (colour & 0x00f)) & 0xffff)
                colour += 0x001;
            if (((want & 0x0f0) - (colour & 0x0f0)) & 0xffff)
                colour += 0x010;
            if (((want & 0xf00) - (colour & 0xf00)) & 0xffff)
                colour += 0x100;
            palette_set(i, colour);
        }
        vblank.palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
    display.palette.fade_done = 0xff;
}

/* orig $002E52: 8 steps; every component below the target goes up by one or two */
void palette_fade_in_fast(const uint16_t *target, uint16_t frames)
{
    int step, i;
    uint16_t wait;

    for (step = 0; step < 8; step++) {
        for (i = 15; i >= 0; i--) {
            uint16_t colour = palette_get(i);
            uint16_t want = target[i] + target[i];
            int16_t gap;

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
        vblank.palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
    display.palette.fade_done = 0xff;
}

/* orig $002F3A: 15 steps; every component above the target goes down by one */
void palette_fade_to(const uint16_t *target, uint16_t frames)
{
    int step, i;
    uint16_t wait;

    for (step = 0; step < 15; step++) {
        for (i = 15; i >= 0; i--) {
            uint16_t colour = palette_get(i);
            uint16_t want = target[i] + target[i];

            if ((int16_t)((want & 0x00f) - (colour & 0x00f)) < 0)
                colour -= 0x001;
            if ((int16_t)((want & 0x0f0) - (colour & 0x0f0)) < 0)
                colour -= 0x010;
            if ((int16_t)((want & 0xf00) - (colour & 0xf00)) < 0)
                colour -= 0x100;
            palette_set(i, colour);
        }
        vblank.palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
    display.palette.fade_done = 0xff;
}

/* orig $002FC4: 15 steps; every non-zero component goes down by one, so the colours fade to black */
void palette_fade_out(uint16_t frames)
{
    int step, i;
    uint16_t wait;

    for (step = 0; step < 15; step++) {
        for (i = 15; i >= 0; i--) {
            uint16_t colour = palette_get(i);

            if (colour & 0x00f)
                colour -= 0x001;
            if (colour & 0x0f0)
                colour -= 0x010;
            if (colour & 0xf00)
                colour -= 0x100;
            palette_set(i, colour);
        }
        vblank.palette_dirty = 0xffff;
        for (wait = 0; wait <= frames; wait++)
            vbl_wait();
    }
}

/* ---- showing the finished screen --------------------------------------------------------------------------------- */

/* orig $00307A: show the screen that was drawn, and start drawing on the other one.  The copper list for the buffer
 * that is now on show lives at $A8 or $FC. */
HOT void screens_flip(void)
{
    struct Screen *shown = screen_back;

    if (flags.fire_pressed != 0)
        game_start();   /* not executed in the recording: a fire button was pressed */
    screen_back = screen_front;
    screen_front = shown;
    custom->cop1lc = ADDR(screen_back == &screen_b ? copper_b.words : copper_a.words);
    vblank.saturating = 0;
    vbl_wait();
}

/* orig $003070: wait until `frames` vertical blanks have passed since the last flip, then flip */
HOT void screens_flip_after(int16_t frames)
{
    while (frames >= (int16_t)vblank.saturating)
        ;
    screens_flip();
}

#define INTF_VERTB 0x0020

/* ---- drawing images ------------------------------------------------------------------------------------------------ */

#define BLTCON0_USEABCD 0x0f00
#define BLTCON0_USEABD 0x0d00
#define MINTERM_A_OR_B_OR_C 0xfe     /* D = A | B | C */
#define MINTERM_A_OR_B 0xfc          /* D = A | B */
#define MINTERM_COOKIE_CUT 0xca      /* D = A ? B : C: B where the mask A is set, the background C elsewhere */

/* orig $0039DC: draw glyph or image `index` of the table at pixel (x, y) on the front screen, clipped at the top
 * and bottom (the image is one word wide) */
HOT void glyph_draw(uint16_t x, uint16_t y, uint16_t index)
{
    const struct Image *img = &graphics.images.image[index];
    int16_t last = img->height;     /* the last row */
    const uint16_t *data;
    uint8_t *dst;
    uint16_t plane;

    if (flags.fire_pressed != 0)
        game_start();   /* not executed in the recording */
    /* clip first: most calls of a scrolling line are above or below the screen and need nothing else */
    if ((int16_t)y < 0) {
        int16_t skip = -(int16_t)y;

        last -= skip;
        if (last < 0)
            return;
        data = img->data + skip;   /* two bytes a row */
        dst = (uint8_t *)screen_front + ((int16_t)(x & 0xfff0) >> 3) + (int16_t)((uint32_t)y * 40) + skip * 40;
    } else {
        int16_t over = y - (199 - last);

        if (over > 0) {
            last -= over;
            if (last < 0)
                return;
        }
        data = img->data;
        dst = (uint8_t *)screen_front + ((int16_t)(x & 0xfff0) >> 3) + (int16_t)((uint32_t)y * 40);
    }
    plane = glyph_plane_size != 0 ? glyph_plane_size : (img->height + 1) * 2;
    blit_image(dst, (const uint8_t *)data, plane, x & 0xf, last + 1);
}

/* orig $003950: the same for an image two words wide.  The original's clipping at the top looks unfinished: it moves
 * the destination by the number of rows skipped in bytes and does not skip any image data. */
HOT void sprite_draw(uint16_t x, uint16_t y, uint16_t index)
{
    const struct Image *img = &graphics.images.image[index];
    int16_t last = img->height;
    uint8_t *dst;

    if (flags.fire_pressed != 0)
        game_start();   /* not executed in the recording */
    if ((int16_t)y < 0) {
        int16_t skip = -(int16_t)y;

        last -= skip;
        if (last < 0)
            return;
        dst = (uint8_t *)screen_front + ((int16_t)(x & 0xfff0) >> 3) + (int16_t)((uint32_t)y * 40) + skip;
    } else {
        int16_t over = y - (199 - last);

        if (over > 0) {
            last -= over;
            if (last < 0)
                return;
        }
        dst = (uint8_t *)screen_front + ((int16_t)(x & 0xfff0) >> 3) + (int16_t)((uint32_t)y * 40);
    }
    blit_image_wide(dst, (const uint8_t *)img->data, (img->height + 1) * 4, x & 0xf, last + 1);
}

/* orig $002D4C: cut the images out of the glyph sheet.  `sheet` is the sheet's pixels, four planes interleaved word by
 * word, 80 words a row.  Each image is written to `out` as four planes one after the other, and its entry in the
 * table is rewritten in place from the description of where to find it to the size and the address of the data. */
static void images_unpack(const uint16_t *sheet, uint16_t *out, union ImageTable *table)
{
    const struct ImageSource *in = table->source.entry;
    struct Image *entry = table->image;
    uint16_t count = table->source.count;

    do {
        uint16_t width_m1 = in->width_m1;
        uint16_t height_m1 = in->height_m1;
        uint16_t offset = (in->x >> 1) + (uint16_t)((uint16_t)(in->y << 5) + (uint16_t)(in->y << 7));   /* y * 160 + x / 2 bytes */
        const uint16_t *from = (const uint16_t *)((const uint8_t *)sheet + (int16_t)offset);
        uint16_t words_m1 = width_m1 >> 4;
        uint16_t skip = (uint16_t)((0x13 - words_m1) << 3);                 /* to the start of the next row */
        uint16_t plane = (uint16_t)((uint16_t)((words_m1 + 1) << 3) * (uint16_t)(height_m1 + 1)) >> 2;   /* bytes in one plane */
        uint16_t row, col;

        entry->neg_width = -(int16_t)width_m1;
        entry->height = height_m1;
        entry->data = out;
        entry++;
        in++;
        for (row = 0; row <= height_m1; row++) {
            for (col = 0; col <= words_m1; col++) {
                *out = *from++;
                *(uint16_t *)((uint8_t *)out + plane) = *from++;
                *(uint16_t *)((uint8_t *)out + plane * 2) = *from++;
                *(uint16_t *)((uint8_t *)out + plane * 3) = *from++;
                out++;
            }
            from = (const uint16_t *)((const uint8_t *)from + (int16_t)skip);
        }
        out = (uint16_t *)((uint8_t *)out + plane * 3);
    } while (--count != 0);
}

/* orig $000E06: prepare the pictures and the images.  The glyph sheet is copied to the back screen and cut up there
 * (the images are written over the sheet's place), and the two interleaved pictures are turned into planes. */
void intro_setup(void)
{
    const uint8_t *from = (const uint8_t *)&graphics.glyph_sheet;
    uint8_t *to = (uint8_t *)screen_back;
    int i;

    for (i = 0; i < 0x7d22; i++)
        *to++ = *from++;
    images_unpack(((struct Picture *)screen_back)->data, (uint16_t *)&graphics.glyph_sheet, &graphics.images);
    picture_deinterleave(graphics.background.data);
    picture_deinterleave(graphics.title.data);
}

/* ---- text ---------------------------------------------------------------------------------------------------------- */

/* one entry of the image table: the glyphs of both fonts and the title letters */

/* orig $0038E0: draw `text` in the large font, 16 pixels per character, from pixel column x */
HOT void text_draw_at(const char *text, uint16_t x, uint16_t y)
{
    for (;;) {
        int16_t ch = (int8_t)*text++;

        if (ch == 0)
            return;
        if (ch != ' ')
            glyph_draw(x, y, ch + font_offset - 'A');
        x += 16;
    }
}

/* orig $0038C6: draw `text` in the large font, centred on the screen */
HOT void text_draw(const char *text, uint16_t y)
{
    uint16_t width = (uint16_t)-8;   /* the original starts at -8 and adds 8 for every byte, the terminator included */
    const char *p = text;

    do {
        width += 8;
    } while (*p++ != 0);
    text_draw_at(text, 0xa0 - width, y);
}

/* orig $003926: draw `text` in the small font, 8 pixels per character, from pixel column x */
HOT void text_small_at(const char *text, uint16_t x, uint16_t y)
{
    for (;;) {
        int16_t ch = (int8_t)*text++;

        if (ch == 0)
            return;
        if (ch != ' ')
            glyph_draw(x, y, ch + 9);
        x += 8;
    }
}

/* orig $00390C: draw `text` in the small font, centred on column $F6 (the right-hand part of the screen) */
HOT void text_small_line(const char *text, int16_t y)
{
    uint16_t width = (uint16_t)-8;   /* the original starts at -8 and adds 4 for every byte, the terminator included */
    const char *p = text;

    do {
        width += 4;
    } while (*p++ != 0);
    text_small_at(text, 0xf6 - width, y);
}

/* ---- the intro: a script of timed steps ----------------------------------------------------------------------------- */
/* The program runs the intro once, waits for the end of its music, fades out and starts again.  Each step waits for
 * `game_clock.ticks` (counted by the interrupt) to reach a value, so the sequence is driven by the time since the intro began. */


/* the colours the intro fades to (the same table appears twice in the original) */
static const uint16_t palette_intro[16] = {
    0x000, 0x343, 0x111, 0x122, 0x334, 0x444, 0x554, 0x777, 0x200, 0x420, 0x530, 0x641, 0x750, 0x031, 0x762, 0x233,
};
/* the white that the fade-in-fast starts from */
static const uint16_t palette_white[16] = {
    0x000, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee,
};
static const uint16_t palette_white_text[16] = {
    0x000, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xeee, 0xffff,
};

static void wait_ticks(uint32_t until)
{
    while ((int32_t)game_clock.ticks < (int32_t)until)
        ;
}

/* four lines of the story, shown one by one scrolling up the screen; the last entry is NULL */
struct Verse {
    const char *line[5];
};

static const struct Verse verse[8] = {
    { { "any man may", "earn through", "skill and courage", "a single favour", 0 } },
    { { "if he will face", "the fear of the", "ancient city", 0 } },
    { { "and slay the four", "great guardians", "who have stolen", "our citadel", 0 } },
    { { "our hero stands", "before the gods", "and asks that", 0 } },
    { { "if he fulfils", "their quest", "he will join them", "as an equal", 0 } },
    { { "only the hope", "of the", "heroGs failure" /* G draws an apostrophe */, "comforts them", 0 } },
    { { "the warrior", "departs for the", "city of legends", 0 } },
    { { "preparing to", "carve one more", "story on", "its walls", 0 } },
};

/* orig $0016A0: draw the lines of a verse from scroll_y downwards, 12 rows apart */
HOT static void verse_draw(const struct Verse *v)
{
    int16_t y = scroll.y;
    int i;

    for (i = 0; v->line[i] != 0; i++) {
        text_small_line(v->line[i], y);
        y += 12;
    }
}

/* orig $001608: scroll a verse up the screen from below, one row per frame */
HOT static void verse_scroll(const struct Verse *v)
{
    scroll.y = 0xc8;
    scroll.limit = 0xffc4;
    scroll.step = 1;
    do {
        scroll.y--;
        background_show_right();
        verse_draw(v);
        screens_flip_after(1);
    } while ((int16_t)scroll.y > -60);
}

/* "Renegade presents a Bitmap Brothers game": each line fades in and out */
struct PresentsPage {
    const char *text;
    uint32_t fade_in_at;     /* game_clock.ticks */
    uint32_t fade_out_at;
};

static const struct PresentsPage presents[4] = {
    { "RENEGADE", 0x00, 0x5a },
    { "PRESENTS", 0xc2, 0x120 },
    { "A BITMAP BROTHERS", 0x180, 0x1d8 },
    { "GAME", 0x241, 0x299 },
};

/* orig $00190C */
static void intro_presents(void)
{
    int i;

    screen_clear();
    font_offset = 0x30;
    for (i = 0; i < 4; i++) {
        if (i != 0)
            screen_clear();
        text_draw(presents[i].text, 0x50);
        screens_flip();
        wait_ticks(presents[i].fade_in_at);
        palette_fade_in(palette_intro, 1);
        wait_ticks(presents[i].fade_out_at);
        palette_fade_out(1);
    }
}

/* orig $001B16: the seven letters of the title, each drawn from its own image */
static void title_letters(void)
{
    uint16_t x;
    uint16_t index = 0x29;

    draw_flag = 1;
    for (x = 0x3c; x <= 0xfc; x += 0x20)
        sprite_draw(x, 0x10, index++);
}

/* orig $001AFA: the title picture and its letters */
static void title_screen(void)
{
    blit_copy(graphics.title.data, screen_front);
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
    palette_fade_in(palette_white, 0);
    palette_fade_to(palette_intro, 1);
    wait_ticks(0x366);
    palette_ptr += 16;   /* the second set of 16 colours */
    palette_fade_in(graphics.title.palette, 2);
    palette_ptr -= 16;
    wait_ticks(0x3c7);
    palette_fade_in_fast(palette_white, 0);
    palette_fade_to(palette_intro, 1);
    palette_ptr += 16;
    wait_ticks(0x489);
    palette_fade_out(1);
    palette_ptr -= 16;
    wait_ticks(0x4e8);
    palette_fade_out(1);
}

/* orig $0014F6: the hero appears and the story scrolls up the screen */
static void intro_story(void)
{
    int i;

    background_show();
    screens_flip();
    wait_ticks(0x53e);
    palette_fade_in_fast(palette_white_text, 0);
    palette_fade_to(graphics.background.palette, 1);
    background_show();
    wait_ticks(0x608);
    screens_flip();
    wait_ticks(0x669);
    draw_flag = 0;
    verse_scroll(&verse[0]);
    verse_scroll(&verse[1]);
    verse_scroll(&verse[2]);
    wait_ticks(0xd28);
    palette_fade_out(1);
    wait_ticks(0xd63);
    palette_fade_in_fast(palette_white_text, 0);
    palette_fade_to(graphics.background.palette, 1);
    verse_scroll(&verse[3]);
    verse_scroll(&verse[4]);
    for (i = 0; i < 100; i++)
        vbl_wait();
    verse_scroll(&verse[5]);
    verse_scroll(&verse[6]);
    verse_scroll(&verse[7]);
    wait_ticks(0x1900);
    palette_fade_out(3);
}

/* a page of the production credits: up to three lines of text */
struct TeamPage {
    struct {
        const char *text;
        uint16_t y;
    } line[3];
    uint32_t until;          /* game_clock.ticks */
};

static const struct TeamPage team[7] = {
    { { { "PRODUCTION TEAM", 0x50 } }, 0x1986 },
    { { { "CODE", 0x46 }, { "STEVE TALL", 0x64 } }, 0x19ea },
    { { { "GRAPHICS", 0x46 }, { "MARK COLEMAN", 0x64 } }, 0x1a49 },
    { { { "DESIGN", 0x46 }, { "ERIC MATTHEWS", 0x64 }, { "STEVE TALL", 0x7a } }, 0x1aa8 },
    { { { "MUSIC", 0x46 }, { "NATION XII", 0x64 } }, 0x1b07 },
    { { { "MUSIC CODE AND SFX", 0x46 }, { "RICHARD JOSEPH", 0x64 } }, 0x1b66 },
    { { { "ADDITIONAL CODE", 0x46 }, { "MIKE MONTGOMERY", 0x64 } }, 0x1bc5 },
};

/* the fade-in that every page of the credits ends with */
static void team_fade(uint16_t frames)
{
    palette_fade_in_fast(palette_white, 0);
    palette_fade_to(palette_intro, frames);
}

/* orig $000F1E: the title picture with the production credits over it */
static void intro_team(void)
{
    int i, l;

    font_offset = 0;
    draw_flag = 1;
    picture_show_title();
    plane5_set();
    screens_flip();
    palette_ptr += 16;
    wait_ticks(0x1962);
    palette_fade_in(graphics.title.palette, 1);
    palette_ptr -= 16;
    for (i = 0; i < 16; i++)
        display.palette.colour[i] = palette_intro[i] * 2;
    vblank.palette_dirty = 0xffff;

    for (i = 0; i < 7; i++) {
        picture_show_title();
        plane5_set();
        for (l = 0; l < 3 && team[i].line[l].text != 0; l++)
            text_draw(team[i].line[l].text, team[i].line[l].y);
        wait_ticks(team[i].until);
        screens_flip();
        team_fade(1);
    }
    picture_show_title();
    plane5_set();
    title_letters();
    wait_ticks(0x1c24);
    screens_flip();
    team_fade(1);

    screen_restore();
    text_draw("INTO THE WONDERFUL", 0x64);
    wait_ticks(0x1d49);
    screens_flip();
    palette_fade_in_fast(palette_white, 0);
    palette_fade_to(palette_intro, 3);
}

/* orig $000EB8: the whole intro */
void intro_run(void)
{
    intro_presents();
    intro_logo();
    intro_story();
    intro_team();
}

/* ---- the program ---------------------------------------------------------------------------------------------------- */


/* orig $000CC8: what follows a fire button (or the space bar): the music stops, the title picture comes up with
 * "LOADING GAME" and the loader is asked for the next part of the game, which replaces this program.  Entered from the
 * routines that draw (a flip, a glyph, an image), wherever the intro happens to be. */
void game_start(void)
{
    int i;

    flags.fire_pressed = 0;
    font_offset = 0;
    flags.music_enabled = 0;
    custom->dmacon = 0x000f;   /* the four audio channels off */
    for (i = 0; i < 10; i++)
        vbl_wait();
    palette_clear();
    screen_clear();
    screens_flip();
    screen_clear();
    picture_show_title();
    plane5_set();
    screens_flip();
    palette_ptr += 16;
    palette_fade_in(graphics.title.palette, 1);
    palette_ptr -= 16;
    screen_restore();
    plane5_set();
    draw_flag = 1;
    text_draw("LOADING GAME", 0x50);
    screens_flip();
    team_fade(1);
    if (screen_back != &screen_a) {   /* show the right one of the two screens */
        screen_restore();
        screens_flip();
    }
    load_next_stage();
}

/* orig $000B12: the program.  Set up the hardware, start the interrupt, then run the intro over and over.
 *
 * The original switches itself to supervisor mode with a TRAP, sets the stack to $2242 and raises the interrupt mask
 * with `move #$2700,sr`; none of that can be written in C.  The loader enters here already in supervisor mode with a
 * usable stack, and interrupts are held off through INTENA, which is enough because that is the only way they are
 * enabled. */
void game_main(void)
{
    volatile uint32_t i;
    int s;

    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;
    custom->cop1lc = ADDR(copper_a.end);   /* an empty list until the screens are set up */
    custom->cop2lc = ADDR(copper_a.end);
    custom->copjmp1 = 0;
    screen_front = &screen_a;
    screen_back = &screen_b;
    palette_clear();
    for (s = 0; s < 16; s++) {
        custom->color[s] = display.palette.colour[s];
        custom->color[16 + s] = display.palette.colour[s];
    }

    vectors[0] = ADDR(&vectors[VECTOR_SPRITE_TARGET]);
    for (s = 0; s < 8; s++)
        custom->sprpt[s] = ADDR(&vectors[VECTOR_SPRITE_TARGET]);   /* sprite DMA is off; point them somewhere harmless */
    custom->intena = 0x7fff;
    custom->intreq = 0x7fff;
    custom->dmacon = 0x7fff;
    custom->cop1lc = ADDR(copper_b.words);
    custom->cop2lc = ADDR(copper_b.words);
    custom->copjmp1 = 0;
    custom->dmacon = 0x83c0;   /* master, bitplane, copper and blitter DMA on */
    game_clock.ticks = 0;
    vectors[VECTOR_LEVEL3] = ADDR(irq_level3);
    custom->intena = 0xc020;   /* master enable and vertical blank */
    (void)custom->intreqr;
    (void)custom->intenar;
    intro_setup();
    flags.fire_pressed = 0;
    startup_delay();
    for (;;) {
        music_init();
        CIAA_PRA |= 0x02;
        flags.music_enabled = 1;
        game_clock.ticks = 0;
        interrupts_on();
        screen_clear();
        intro_run();
        while ((int32_t)game_clock.ticks < 0x1dd0)
            ;
        flags.music_enabled = 0;
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
