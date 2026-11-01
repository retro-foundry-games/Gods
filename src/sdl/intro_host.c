/* Gods SDL port, the intro: the Amiga routines of intro_amiga.asm in C, and the music.
 *
 * Compiled with the program's headers and flags (the structs have the 68000's layout), without SDL: intro_sdl.c is the machine around it.
 * The blitter is not emulated: the copies, the clears and the masked image draw are done directly.  The music is a ProTracker player that
 * runs from the vertical blank (50 ticks a second, as the original's replayer does) and mixes the four channels to stereo (the Amiga's
 * left, right, right, left); it plays `module` and does not use the chip's audio registers.  Effects: arpeggio, slides, tone portamento,
 * vibrato, sample offset, volume slide, position jump, volume, pattern break, speed; the E effects are ignored. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>

#include "intro.h"
#include "intro_host.h"

/* ---- the machine ------------------------------------------------------------------------------------------------- */

static volatile int interrupts_enabled;
static volatile int fire_key;

void sdl_blit_run(void)
{
}

void ih_set_fire(int down)
{
    fire_key = down;
}

/* what is shown: the screen the copper list names, and its colours */
const uint16_t *ih_shown_screen(void)
{
    return (const uint16_t *)((const uint16_t *)(uintptr_t)custom->cop1lc == copper_b.words ? (void *)&screen_b : (void *)&screen_a);
}

uint16_t ih_color(int index)
{
    return custom->color[index];
}

void blitter_wait(void)
{
}

/* orig $001B7C: copy the 20 x 800 words at src to dst */
void blit_copy(const void *src, void *dst)
{
    memcpy(dst, src, 20 * 800 * 2);
}

/* orig $001BC6: only the right-hand 9 words of each row */
void blit_copy_right(const void *src, void *dst)
{
    for (int row = 0; row < 800; row++)
        memcpy((uint8_t *)dst + row * 40 + 22, (const uint8_t *)src + row * 40 + 22, 18);
}

/* orig $0030D4: clear the front screen */
void screen_clear(void)
{
    memset(screen_front, 0, 40000);
}

/* orig $0030EE: copy the back screen over the front screen */
void screen_restore(void)
{
    memcpy(screen_front, screen_back, 40000);
}

/* orig $001D54 and $001D7E: set or clear the whole fifth bitplane of the front screen */
void plane5_set(void)
{
    memset((uint8_t *)screen_front + 0x7d00, 0xff, 8000);
}

void plane5_clear(void)
{
    memset((uint8_t *)screen_front + 0x7d00, 0, 8000);
}

/* orig $0032B6 and $0032CA: an image `words` words wide (its four planes `plane` bytes apart) through a mask made of its planes ORed, to
 * the screen (rows of 40 bytes, planes plane_stride apart), shifted `shift` pixels to the right (one word wider then); with draw_flag the
 * fifth plane gets the mask cleared too */
static void blit_image_words(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows, unsigned words)
{
    unsigned bits = shift & 0xff;
    unsigned out_words = words + (bits ? 1 : 0);
    int16_t plane_stride = (int16_t)display.blit.plane_stride;
    unsigned planes = draw_flag ? 5 : 4;
    uint16_t last_mask = bits ? 0 : 0xffff;

    display.blit.extra_modulo = 0;
    for (unsigned i = 0; i < planes; i++)
        for (unsigned r = 0; r < rows; r++) {
            uint16_t *out = (uint16_t *)(dst + (int)i * plane_stride + r * 40u);
            uint32_t previous_a = 0, previous_b = 0;

            for (unsigned j = 0; j < out_words; j++) {
                unsigned mj = j >= words ? j - words : j, mr = j >= words ? r + 1 : r;
                uint16_t a = 0, b = i < 4 ? *(const uint16_t *)(src + i * plane + r * words * 2u + 2 * j) : 0;
                uint16_t shifted_a, shifted_b;

                if (mr < rows)
                    for (unsigned k = 0; k < 4; k++)
                        a |= *(const uint16_t *)(src + k * plane + mr * words * 2u + 2 * mj);
                if (j == out_words - 1)
                    a &= last_mask;
                shifted_a = (uint16_t)(((previous_a << 16 | a) >> bits) & 0xffff);
                shifted_b = (uint16_t)(((previous_b << 16 | b) >> bits) & 0xffff);
                previous_a = a;
                previous_b = b;
                out[j] = (uint16_t)((shifted_a & shifted_b) | (~shifted_a & out[j]));
            }
        }
}

void blit_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_image_words(dst, src, plane, shift, rows, 1);
}

void blit_image_wide(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows)
{
    blit_image_words(dst, src, plane, shift, rows, 2);
}

/* the music's busy wait and the long wait before the program starts: the host has no use for them */
void delay(void)
{
}

void startup_delay(void)
{
}

void interrupts_on(void)
{
    interrupts_enabled = 1;
}

/* orig $002CDE: wait for the next vertical blank */
void vbl_wait(void)
{
    uint16_t seen = vblank.count;

    while (vblank.count == seen)
        sched_yield();
}

/* orig $000DC4: the loader that brings the next part of the game: here the menu program */
void load_next_stage(void)
{
    ih_start_menu();
    for (;;)
        sched_yield();
}

/* ---- the music --------------------------------------------------------------------------------------------------- */

static const int16_t sine[32] = { 0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
                                  255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24 };

struct Voice {
    const int8_t *data;
    unsigned length, loop_start, loop_length;   /* bytes */
    uint32_t position;     /* 16.16 */
    unsigned period, volume, sample;
    int porta_target, porta_speed, vibrato_speed, vibrato_depth, vibrato_pos, arpeggio_note;
    uint8_t cell[4];
    unsigned offset_memory;
};

static struct Voice voice[4];
static unsigned pt_speed, pt_tick_count, pt_order, pt_row, pt_break_row;
static int pt_jump_order, pt_active;
static const int8_t *sample_data[32];
static unsigned sample_bytes[32];

/* ProTracker's periods for the notes (finetune 0): three octaves */
static unsigned note_period(unsigned period, int semitones)
{
    for (int i = 0; i < 36; i++)
        if (period >= period_table[i]) {
            int n = i + semitones;

            if (n < 0)
                n = 0;
            if (n > 35)
                n = 35;
            return period_table[n];
        }
    return period;
}

void music_init(void)
{
    unsigned patterns = 0;
    const uint8_t *data = module.data;

    memset(voice, 0, sizeof voice);
    for (unsigned i = 0; i < module.song_length && i < 128; i++)
        if (module.order[i] >= patterns)
            patterns = module.order[i] + 1u;
    data += patterns * 1024u;
    for (unsigned s = 0; s < 31; s++) {
        sample_data[s] = (const int8_t *)data;
        sample_bytes[s] = module.sample[s].length * 2u;
        data += sample_bytes[s];
    }
    pt_speed = 6;
    pt_tick_count = pt_speed;   /* the first tick reads a row */
    pt_order = 0;
    pt_row = 0;
    pt_break_row = 0;
    pt_jump_order = -1;
    pt_active = 1;
}

static void start_note(struct Voice *v, unsigned sample, unsigned period)
{
    v->sample = sample;
    v->data = sample_data[sample - 1];
    v->length = sample_bytes[sample - 1];
    v->loop_start = module.sample[sample - 1].loop_start * 2u;
    v->loop_length = module.sample[sample - 1].loop_length * 2u;
    v->period = period;
    v->position = 0;
}

static void read_row(void)
{
    const uint8_t *pattern = module.data + module.order[pt_order] * 1024u + pt_row * 16u;

    for (unsigned c = 0; c < 4; c++) {
        struct Voice *v = &voice[c];
        const uint8_t *cell = pattern + c * 4;
        unsigned sample = (cell[0] & 0xf0) | (cell[2] >> 4);
        unsigned period = (cell[0] & 0x0f) << 8 | cell[1];
        unsigned effect = cell[2] & 0x0f, parameter = cell[3];

        memcpy(v->cell, cell, 4);
        if (sample != 0 && sample <= 31) {
            v->sample = sample;
            v->volume = module.sample[sample - 1].volume & 0xff;
            if (v->volume > 64)
                v->volume = 64;
        }
        if (period != 0) {
            if (effect == 3 || effect == 5) {
                v->porta_target = (int)period;
            } else {
                start_note(v, v->sample ? v->sample : 1, period);
                v->vibrato_pos = 0;
            }
        }
        v->arpeggio_note = 0;
        switch (effect) {
        case 3:
            if (parameter)
                v->porta_speed = (int)parameter;
            break;
        case 4:
            if (parameter & 0xf0)
                v->vibrato_speed = parameter >> 4;
            if (parameter & 0x0f)
                v->vibrato_depth = parameter & 0x0f;
            break;
        case 9:
            if (parameter)
                v->offset_memory = parameter;
            if (v->data)
                v->position = (uint32_t)(v->offset_memory * 256u) << 16;
            break;
        case 0xb:
            pt_jump_order = (int)parameter;
            pt_break_row = 0;
            break;
        case 0xc:
            v->volume = parameter > 64 ? 64 : parameter;
            break;
        case 0xd:
            pt_break_row = (parameter >> 4) * 10 + (parameter & 15);
            if (pt_jump_order < 0)
                pt_jump_order = (int)pt_order + 1;
            break;
        case 0xf:
            if (parameter > 0 && parameter < 32)
                pt_speed = parameter;
            break;
        default:
            break;
        }
    }
}

static void run_effects(void)
{
    for (unsigned c = 0; c < 4; c++) {
        struct Voice *v = &voice[c];
        unsigned effect = v->cell[2] & 0x0f, parameter = v->cell[3];

        switch (effect) {
        case 0:
            if (parameter) {
                unsigned phase = pt_tick_count % 3;

                v->arpeggio_note = phase == 0 ? 0 : phase == 1 ? (int)(parameter >> 4) : (int)(parameter & 15);
            }
            break;
        case 1:
            if (v->period > parameter + 113u)
                v->period -= parameter;
            else
                v->period = 113;
            break;
        case 2:
            v->period += parameter;
            if (v->period > 856)
                v->period = 856;
            break;
        case 3:
        case 5:
            if (v->porta_target) {
                if ((int)v->period > v->porta_target) {
                    v->period -= (unsigned)v->porta_speed;
                    if ((int)v->period < v->porta_target)
                        v->period = (unsigned)v->porta_target;
                } else if ((int)v->period < v->porta_target) {
                    v->period += (unsigned)v->porta_speed;
                    if ((int)v->period > v->porta_target)
                        v->period = (unsigned)v->porta_target;
                }
            }
            if (effect == 5)
                goto volume_slide;
            break;
        case 4:
        case 6:
            v->vibrato_pos = (v->vibrato_pos + v->vibrato_speed) & 63;
            if (effect == 6)
                goto volume_slide;
            break;
        case 0xa:
        volume_slide:
            if (parameter >> 4)
                v->volume = v->volume + (parameter >> 4) > 64 ? 64 : v->volume + (parameter >> 4);
            else if (v->volume >= (parameter & 15))
                v->volume -= parameter & 15;
            else
                v->volume = 0;
            break;
        default:
            break;
        }
    }
}

/* one tick of the music (the vertical blank) */
static void music_step(void)
{
    if (!pt_active)
        return;
    if (pt_tick_count >= pt_speed) {
        pt_tick_count = 0;
        read_row();
        pt_row++;
        if (pt_jump_order >= 0 || pt_row >= 64) {
            if (pt_jump_order >= 0)
                pt_order = (unsigned)pt_jump_order;
            else
                pt_order++;
            pt_row = pt_break_row;
            pt_break_row = 0;
            pt_jump_order = -1;
            if (pt_order >= module.song_length)
                pt_order = module.restart < module.song_length ? module.restart : 0;
        }
    } else {
        run_effects();
    }
    pt_tick_count++;
}

/* mix `frames` stereo frames (signed 16 bit, interleaved) of the music */
void ih_music_mix(int16_t *out, int frames, int rate)
{
    memset(out, 0, (size_t)frames * 4);
    if (!flags.music_enabled || !pt_active)
        return;
    for (unsigned c = 0; c < 4; c++) {
        struct Voice *v = &voice[c];
        int left = (c == 0 || c == 3);
        unsigned period = v->period;

        if (!v->data || !period)
            continue;
        if (v->vibrato_speed || v->vibrato_depth)
            if ((v->cell[2] & 0x0f) == 4 || (v->cell[2] & 0x0f) == 6)
                period = (unsigned)((int)period + (sine[v->vibrato_pos & 31] * v->vibrato_depth >> 7) * ((v->vibrato_pos & 32) ? -1 : 1));
        if (v->arpeggio_note)
            period = note_period(period, v->arpeggio_note);
        if (period < 113)
            period = 113;
        {
            uint32_t step = (uint32_t)(((uint64_t)3546895 << 16) / ((uint64_t)period * (unsigned)rate));

            for (int i = 0; i < frames; i++) {
                unsigned index = v->position >> 16;

                if (v->loop_length > 2) {
                    if (index >= v->loop_start + v->loop_length) {
                        v->position -= (uint32_t)v->loop_length << 16;
                        index -= v->loop_length;
                    }
                } else if (index >= v->length) {
                    v->data = 0;
                    break;
                }
                {
                    int sample = (v->data[index] * (int)v->volume) >> 1;   /* -4096 .. 4064 */

                    out[2 * i + (left ? 0 : 1)] = (int16_t)(out[2 * i + (left ? 0 : 1)] + sample * 2);
                }
                v->position += step;
            }
        }
    }
}

/* ---- the interrupt ----------------------------------------------------------------------------------------------- */

/* orig $001DD6: the vertical blank: the music, the clock, the fire button, the colours */
void irq_level3(void)
{
    if (flags.music_enabled)
        music_step();
    game_clock.ticks++;
    if (!game_clock.input_seen && fire_key) {
        flags.fire_pressed = 1;
        game_clock.input_seen = 1;
    }
    if (vblank.palette_dirty) {
        vblank.palette_dirty = 0;
        for (int i = 0; i < 32; i++)
            custom->color[i] = display.palette.colour[i];
    }
    if (vblank.saturating != 0x7fff)
        vblank.saturating++;
    vblank.count++;
}

/* the main thread raises the vertical blank */
void ih_vbl(void)
{
    if (interrupts_enabled)
        irq_level3();
}
