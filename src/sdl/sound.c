/* Gods SDL port: the sound effects, at a higher layer than the original.  The original's sound system (menu_amiga.asm: four voices, a
 * sequencer that reads the command bytes of the tracks, an envelope and slide engine, the interrupt that feeds Paula) is not ported.  What is
 * done here: play_sound(id, priority) looks the id up in the table of sounds ($20BCE, 6 bytes each: the number of voices less one, the
 * track of each voice, a mode), the tracks ($21C3A) are followed note by note once a frame (the commands that set the volume, the
 * tempo and the lengths, instrument, transposition, calls, loops, jump and end), and each note plays the instrument's sample ($3A38A
 * records: a flag, where the loop starts, the length in words, the sample, the period table to use) at the period of the note (the
 * tables at $2080E) in a software mixer: Paula's rate (3546895 / period), 8 bit samples, voices 0 and 3 left, 1 and 2 right.
 * Not done: the volume envelopes (a note plays at its full volume), the period and volume slides, the random variations of some sounds.
 * The data is the Amiga's (big endian): in the RAM of the original's level run (--level1-image); the menu program on its own has no instrument
 * records (the original's start-up writes them), so the menu is silent. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest.h"

#define VOICES 4
#define STACK 16

static unsigned be16(uint32_t address)
{
    const uint8_t *p = guest_ptr(address);

    return (unsigned)(p[0] << 8 | p[1]);
}

static uint32_t be32(uint32_t address)
{
    return (uint32_t)be16(address) << 16 | be16(address + 2);
}

struct SoundVoice {
    int active;
    unsigned id, priority;
    uint32_t pc, start;            /* the track */
    uint32_t stack[STACK];
    int sp;
    struct { uint32_t pc; unsigned count; } loop[STACK];
    int loops;
    unsigned wait, duration, volume, max_volume;
    int transposition;
    uint32_t tempo_mult;
    /* the note that sounds */
    const int8_t *sample;
    unsigned length, loop_start;   /* bytes */
    int looping;
    unsigned period, play_volume;
    uint32_t position;             /* 16.16 */
    unsigned period_table;
    uint32_t instrument;
};

static struct SoundVoice voice[VOICES];
static unsigned tempo = 1;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* the track of a sound number: the pointer in the table at $21C3A */
static uint32_t track_of(unsigned number)
{
    return be32(0x21c3a + 4 * number);
}

static void note_on(struct SoundVoice *v, unsigned note)
{
    uint32_t table = v->period_table == 0 ? 0x2080e : v->period_table == 1 ? 0x2086e : 0x208ce;
    unsigned index = (note * 4 + (unsigned)v->transposition) * 2;

    if (!v->instrument)
        return;
    v->period = be16(table + index);
    v->play_volume = v->volume;
    v->position = 0;
}

static void set_instrument(struct SoundVoice *v, unsigned n)
{
    uint32_t record = 0x3a38a + n * 14;
    uint32_t sample = be32(record + 6);

    v->instrument = record;
    v->looping = (int16_t)be16(record) < 0;
    v->loop_start = be16(record + 2);
    v->length = be16(record + 4) * 2u;
    v->sample = sample ? (const int8_t *)guest_ptr(sample) : 0;
    v->period_table = be32(record + 10) & 0xff;   /* (the original looks at the low byte of the long only) */
}

/* read the commands of the track up to the next note or rest: the notes of the original's run_sequencer, without the slides and envelopes */
static void run_track(struct SoundVoice *v)
{
    for (int guard = 0; guard < 400; guard++) {
        const uint8_t *p = guest_ptr(v->pc);
        unsigned op = *p++;

        v->pc++;
        if (op < 0x80) {
            note_on(v, op);
            v->wait = v->duration;
            return;
        }
        switch (op) {
        case 0x80:
            v->max_volume = v->volume = *p & 0x3f;
            v->pc++;
            break;
        case 0x84: case 0xa0: case 0xa4: case 0xa8: case 0x9c:
            v->pc++;
            break;
        case 0x88:
            v->pc = v->start;
            break;
        case 0x8c:
            v->duration = *p * tempo;
            v->pc++;
            break;
        case 0x90:
            v->wait = v->duration;
            return;
        case 0x94:
            tempo = *p ? 0x2ee / *p : tempo;
            v->pc++;
            break;
        case 0x98: {
            unsigned n = *p++, sum = 0;

            for (unsigned i = 0; i < n; i++)
                sum += *p++;
            v->duration = sum * tempo;
            v->pc += 1 + n;
            break;
        }
        case 0xac:
            v->active = 0;
            return;
        case 0xb0:
            if (v->sp < STACK)
                v->stack[v->sp++] = v->pc + 1;
            v->pc = track_of(*p);
            break;
        case 0xb4:
            if (v->sp > 0)
                v->pc = v->stack[--v->sp];
            break;
        case 0xb8:
            v->transposition = *p ? v->transposition + (int8_t)*p : 0;
            v->pc++;
            break;
        case 0xbc:
            v->transposition = (int8_t)*p;
            v->pc++;
            break;
        case 0xc0: {
            unsigned count = *p;

            v->pc++;
            if (count && v->loops < STACK) {
                v->loop[v->loops].pc = v->pc;
                v->loop[v->loops].count = count;
                v->loops++;
            }
            break;
        }
        case 0xc4:
            if (v->loops > 0) {
                if (--v->loop[v->loops - 1].count == 0)
                    v->loops--;
                else
                    v->pc = v->loop[v->loops - 1].pc;
            }
            break;
        case 0xc8: case 0xcc:
            if (op == 0xc8)
                v->pc++;
            break;
        case 0xd0:
            set_instrument(v, *p);
            v->pc++;
            break;
        case 0xd4:
            v->pc = track_of(*p);
            break;
        default:
            v->pc++;   /* a command this port does not know: its byte is skipped */
            break;
        }
    }
    v->active = 0;
}

static void start_voice(struct SoundVoice *v, unsigned number, unsigned id, unsigned priority)
{
    memset(v, 0, sizeof *v);
    v->active = 1;
    v->id = id;
    v->priority = priority;
    v->pc = v->start = track_of(number);
    if (!v->pc) {
        v->active = 0;
        return;
    }
    v->volume = v->max_volume = 63;
}

/* the voices are silent again (the menu program starts over) */
void sound_reset(void)
{
    pthread_mutex_lock(&lock);
    memset(voice, 0, sizeof voice);
    tempo = 1;
    pthread_mutex_unlock(&lock);
}

/* orig $01FAF6: start the sound `id` with the priority `priority` */
void play_sound(uint16_t id, uint16_t priority)
{
    uint32_t record = 0x20bce + id * 6u;
    unsigned count, flags;

    if (be32(0x21c3a + 4) == 0)
        return;   /* no tracks: the sound data is not in memory */
    pthread_mutex_lock(&lock);
    flags = guest_ptr(record)[0];
    count = (flags & 0x7f) + 1;
    if (flags & 0x80)   /* a sound that is not started again while it plays */
        for (int i = 0; i < VOICES; i++)
            if (voice[i].active && voice[i].id == id)
                goto done;
    for (unsigned k = 0; k < count && k < 4; k++) {
        unsigned number = guest_ptr(record)[1 + k];
        int chosen = -1;

        if (number == 0)
            continue;
        if (number == 0x16) {   /* the silent track: the voice stops */
            for (int i = 0; i < VOICES; i++)
                voice[i].active = 0;
            continue;
        }
        for (int i = 0; i < VOICES && chosen < 0; i++)
            if (!voice[i].active)
                chosen = i;
        for (int i = 0; i < VOICES && chosen < 0; i++)
            if (voice[i].priority <= priority)
                chosen = i;
        if (chosen >= 0)
            start_voice(&voice[chosen], number, id, priority);
    }
done:
    pthread_mutex_unlock(&lock);
}

void voice_start(uint16_t sound, uint16_t index)
{
    (void)sound;
    (void)index;
}

void audio_irq(void)
{
}

/* orig $01FCB8: once a frame: every voice goes on in its track */
void sound_tick(void)
{
    pthread_mutex_lock(&lock);
    for (int i = 0; i < VOICES; i++) {
        struct SoundVoice *v = &voice[i];

        if (!v->active)
            continue;
        if (v->wait == 0)
            run_track(v);
        if (v->active && v->wait > 0)
            v->wait--;
    }
    pthread_mutex_unlock(&lock);
}

/* mix one frame of the sound (882 stereo samples, signed 16 bit, at 44100 Hz) */
void sound_mix(int16_t *out)
{
    int32_t mix[882 * 2];

    memset(mix, 0, sizeof mix);
    pthread_mutex_lock(&lock);
    for (int i = 0; i < VOICES; i++) {
        struct SoundVoice *v = &voice[i];
        int right = (i == 1 || i == 2);

        if (!v->sample || !v->period || !v->play_volume)
            continue;
        uint32_t step = (uint32_t)(((uint64_t)3546895 << 16) / ((uint64_t)v->period * 44100));

        for (int n = 0; n < 882; n++) {
            unsigned index = v->position >> 16;

            if (index >= v->length) {
                if (!v->looping || v->length <= v->loop_start) {
                    v->sample = 0;
                    break;
                }
                v->position -= (v->length - v->loop_start) << 16;
                index = v->position >> 16;
            }
            mix[2 * n + right] += (v->sample[index] * (int)v->play_volume) * 2;
            v->position += step;
        }
    }
    pthread_mutex_unlock(&lock);
    for (int n = 0; n < 882 * 2; n++)
        out[n] = (int16_t)(mix[n] > 32767 ? 32767 : mix[n] < -32768 ? -32768 : mix[n]);
}
