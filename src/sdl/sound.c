/* Gods SDL port: the sound effects, at a higher layer than the original.  The original's sound system (amiga.asm: four voices, a
 * sequencer that reads the command bytes of the tracks, an envelope and slide engine, the interrupt that feeds Paula) is not ported.  What is
 * done here: play_sound(id, priority) looks the id up in the table of sounds ($20BCE, 6 bytes each: the number of voices less one, the
 * track of each voice, a mode), the tracks ($21C3A) are followed note by note once a frame (the commands that set the volume, the
 * tempo and the lengths, instrument, transposition, calls, loops, jump and end), and each note plays the instrument's sample ($3A38A
 * records: a flag, where the loop starts, the length in words, the sample, the period table to use) at the period of the note (the
 * tables at $2080E) in a software mixer: Paula's rate (3546895 / period), 8 bit samples, voices 0 and 3 left, 1 and 2 right.
 * Not done: the volume envelopes (a note plays at its full volume), the period and volume slides, the random variations of some sounds.
 * The data is the program's own, typed: the table of sounds (struct SoundRecord), the track addresses (sound_tracks), the instruments (struct SoundInstrument) and the period
 * tables (sound_periods) are tables of menu.symbols, native words; the tracks themselves are the commands in the program's image, bytes.  The voices' flags are the variables
 * sound_busy_207a0 and voice_ids_207a6. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"

#define VOICES 4
#define STACK 16

/* an address of the program's memory as a pointer (the guest memory is mapped at the Amiga's addresses, the first 64 KiB apart: the tracks and samples are all above) */
#define ADDRESS(a) ((const void *)GUEST(a))   /* (an address of the original's memory) */

struct SoundVoice {
    int active;
    unsigned id, priority, number;   /* number: the track the sound started the voice with (the original keeps it at $207A6 for stop_sound) */
    const uint8_t *pc, *start;     /* the track */
    const uint8_t *stack[STACK];
    int sp;
    struct { const uint8_t *pc; unsigned count; } loop[STACK];
    int loops;
    unsigned wait, duration, volume, max_volume;
    int transposition;
    uint32_t tempo_mult;
    /* the note that sounds */
    const int8_t *sample;
    unsigned length, loop_start;   /* bytes */
    int looping;
    int finished;                  /* a sample that is not looped has played to its end: it sounds again with the next note (note_on), the pointer stays */
    unsigned period, play_volume;
    uint32_t position;             /* 16.16 */
    const uint16_t *period_table;
    const struct SoundInstrument *instrument;
};

static struct SoundVoice sound_voice[VOICES];
static unsigned tempo = 1;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* the track of a sound number: the address in sound_tracks ($21C3A) */
static const uint8_t *track_of(unsigned number)
{
    return number < sizeof sound_tracks / sizeof sound_tracks[0] ? (const uint8_t *)ADDRESS(sound_tracks[number]) : 0;
}

static void note_on(struct SoundVoice *v, unsigned note)
{
    unsigned index = note * 4 + (unsigned)v->transposition;

    if (!v->instrument || !v->period_table)
        return;
    v->period = v->period_table[index];
    v->play_volume = v->volume;
    v->position = 0;
    v->finished = 0;
}

static void set_instrument(struct SoundVoice *v, unsigned n)
{
    const struct SoundInstrument *record = n < sizeof sound_instruments / sizeof sound_instruments[0] ? &sound_instruments[n] : 0;

    if (!record)
        return;
    v->instrument = record;
    v->looping = record->flags < 0;
    v->loop_start = record->loop_start;
    v->length = record->length * 2u;
    v->sample = record->sample ? (const int8_t *)ADDRESS(record->sample) : 0;
    /* orig $020300 (select_period_table): the low byte of the long picks the table: 0 the first, 1 the second, anything else the third (the data holds the tables' addresses there,
     * whose low bytes are not 0 or 1: the original plays every instrument with the third table) */
    unsigned which = record->period_table & 0xff;

    v->period_table = &sound_periods[which == 0 ? 0 : which == 1 ? 48 : 96];
}

/* read the commands of the track up to the next note or rest: the notes of the original's run_sequencer, without the slides and envelopes */
static void run_track(struct SoundVoice *v)
{
    for (int guard = 0; guard < 400; guard++) {
        const uint8_t *p = v->pc;
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
            if (!v->pc) {
                v->active = 0;
                return;
            }
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
            if (!v->pc) {
                v->active = 0;
                return;
            }
            break;
        default:
            v->pc++;   /* a command this port does not know: its byte is skipped */
            break;
        }
    }
    v->active = 0;
}

/* the original keeps, for each voice, the priority of the sound it plays at $207A0 (a byte each; 0: free), which the game waits on (stop_game_sounds): kept the same here, under the lock */
static void sync_busy(void)
{
    volatile uint8_t *busy = (volatile uint8_t *)&guest.sound_busy_207a0;
    volatile uint8_t *ids = guest.voice_ids_207a6;

    for (int i = 0; i < VOICES && i < 4; i++) {
        busy[i] = sound_voice[i].active ? (uint8_t)sound_voice[i].priority : 0;
        ids[i] = sound_voice[i].active ? (uint8_t)sound_voice[i].number : 0;
    }
}

static void start_voice(struct SoundVoice *v, unsigned number, unsigned id, unsigned priority)
{
    memset(v, 0, sizeof *v);
    v->active = 1;
    v->id = id;
    v->number = number;
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
    memset(sound_voice, 0, sizeof sound_voice);
    tempo = 1;
    pthread_mutex_unlock(&lock);
}

/* orig $01FACC: the random number d0 scaled to 0..max (a division by a step of 65535 / (max + 1) + 1) */
static unsigned scale_random(unsigned r, unsigned max)
{
    unsigned step = (0xffffu / (max + 1) + 1) & 0xffff;

    if (r == 0)
        r = 1;
    return (r / step) & 0xffff;
}

/* orig $01FB38..$01FBA8: the random numbers that the sound's variation takes from the program's table ($633E) before a voice is started: the
 * record's byte 5 says how many (5: two, 6: more until the choice (0..5) is not the last one, 9: one and then the record of a variant
 * (0..2) is looked at in the same way, other values not zero: one).  The variation itself is not made (the tracks are not played with it) but the
 * index into the table must go on as the original's */
static void take_variation_randoms(const struct SoundRecord *record)
{
    for (;;) {
        unsigned variant = record->variant;
        unsigned r;

        if (variant == 0)
            return;
        r = random_word() & 0xffff;
        if (variant == 5) {
            random_word();
            return;
        }
        if (variant == 6) {
            unsigned choice = scale_random(r, 5) & 0xff;

            if (choice == guest.sound_last_variant)
                continue;
            guest.sound_last_variant = (uint8_t)choice;
            return;
        }
        if (variant == 9) {
            record += scale_random(r, 2) + 1;
            continue;
        }
        return;
    }
}

/* orig $01FAF6: start the sound `id` with the priority `priority` */
void play_sound(uint16_t id, uint16_t priority)
{
    const struct SoundRecord *record = &sound_records[id];
    unsigned count, flags;

    if (id >= sizeof sound_records / sizeof sound_records[0] || sound_tracks[1] == 0)
        return;   /* no such sound, or no tracks: the sound data is not in memory */
    pthread_mutex_lock(&lock);
    flags = record->flags;
    count = (flags & 0x7f) + 1;
    if (flags & 0x80) {   /* a sound that is not started again while it plays; its priority is 126 whatever the call says ($1FB1A) */
        for (int i = 0; i < VOICES; i++)
            if (sound_voice[i].active && sound_voice[i].id == id)
                goto done;
        priority = 0x7e;
    }
    take_variation_randoms(record);
    {
        /* orig $01FBAC..: the tracks are given to the voices, the first pass to the free ones (their busy byte 0) in the order of the voices, one track each until all have one, the
         * second to the busy ones whose priority is not higher than the sound's (a priority 0 takes any), also in order; when the tracks do not all find a voice nothing is started.  The voices that
         * were given a track in the first pass are still free for the second (their busy byte is set when all is done) */
        unsigned assigned[VOICES] = { 0 };
        unsigned busy[VOICES];
        unsigned last = count - 1 < 3 ? count - 1 : 3;
        unsigned at = 0;
        int placed = 0;

        for (int i = 0; i < VOICES; i++)
            busy[i] = sound_voice[i].active ? sound_voice[i].priority : 0;
        for (int i = 0; i < VOICES && !placed; i++) {
            if (busy[i] != 0)
                continue;
            assigned[i] = record->track[at];
            if (at == last)
                placed = 1;
            else
                at++;
        }
        for (int i = 0; i < VOICES && !placed; i++) {
            if (busy[i] == 0 || (priority != 0 && priority < busy[i]))
                continue;
            assigned[i] = record->track[at];
            if (at == last)
                placed = 1;
            else
                at++;
        }
        if (!placed)
            goto done;
        for (int i = 0; i < VOICES; i++) {
            unsigned number = assigned[i];

            if (number == 0)
                continue;
            if (number == 0x16) {   /* the silent track: the voice stops */
                sound_voice[i].active = 0;
                sound_voice[i].play_volume = 0;
                continue;
            }
            start_voice(&sound_voice[i], number, id, priority);
        }
    }
done:
    sync_busy();
    pthread_mutex_unlock(&lock);
}


/* the quickload (the chip RAM of the original at some moment of a play): the voices that the original had going are given the port's: a voice whose busy byte ($207A0, the priority) is not 0 plays the track its id
 * byte ($207A6) names, from its start (a looped sound goes on as it did), or, with no id (stop_sound has cleared it), the silent track that runs out in 32 frames; the game waits for the voices (level_end_quiet) and
 * stops sounds by their track */
void sound_restore_quick(void)
{
    uint32_t raw = guest.sound_busy_207a0;   /* (a long to the loader: the first voice's byte is the top one) */

    pthread_mutex_lock(&lock);
    for (int i = 0; i < VOICES && i < 4; i++) {
        unsigned busy = (raw >> (24 - 8 * i)) & 0xff;
        unsigned number = guest.voice_ids_207a6[i];

        memset(&sound_voice[i], 0, sizeof sound_voice[i]);
        if (busy == 0)
            continue;
        if (number == 0) {
            start_voice(&sound_voice[i], 22, ~0u, busy);
            sound_voice[i].number = 0;
            continue;
        }

        unsigned id = ~0u;

        for (unsigned k = 0; k < sizeof sound_records / sizeof sound_records[0]; k++)
            if (sound_records[k].track[0] == number) {
                id = k;
                break;
            }
        start_voice(&sound_voice[i], number, id, busy);
    }
    sync_busy();
    pthread_mutex_unlock(&lock);
}

/* orig $01FE3E: the sound track `sound` is started on the voice `index`; stop_sound starts the silent one (22), which stops the voice */
void voice_start(uint16_t sound, uint16_t index)
{
    if (sound != 22 || index >= VOICES)
        return;
    pthread_mutex_lock(&lock);
    struct SoundVoice *v = &sound_voice[index];

    /* the original runs the silent track (22: a note of volume 0 that lasts 32 ticks, then the end) on the voice, and stop_sound has cleared the voice's id: the voice's busy byte
     * ($207A0, the priority of the sound) stays until the track ends, 32 frames later */
    if (v->active) {
        unsigned priority = v->priority;

        start_voice(v, 22, ~0u, priority);   /* (no sound id: the original has cleared the voice's id) */
        v->number = 0;
    } else {
        v->play_volume = 0;
    }
    sync_busy();
    pthread_mutex_unlock(&lock);
}

void audio_irq(void)
{
}

/* orig $01FCB8: once a frame: every voice goes on in its track */
void sound_tick(void)
{
    pthread_mutex_lock(&lock);
    for (int i = 0; i < VOICES; i++) {
        struct SoundVoice *v = &sound_voice[i];

        if (!v->active)
            continue;
        if (v->wait == 0)
            run_track(v);
        if (v->active && v->wait > 0)
            v->wait--;
    }
    sync_busy();
    pthread_mutex_unlock(&lock);
}

/* mix one frame of the sound (882 stereo samples, signed 16 bit, at 44100 Hz) */
void sound_mix(int16_t *out)
{
    int32_t mix[882 * 2];

    memset(mix, 0, sizeof mix);
    pthread_mutex_lock(&lock);
    for (int i = 0; i < VOICES; i++) {
        struct SoundVoice *v = &sound_voice[i];
        int right = (i == 1 || i == 2);

        if (!v->sample || !v->period || !v->play_volume || v->finished)
            continue;
        if (!v->active && v->looping)   /* a track that has ended or been stopped no longer feeds its looped sample (it would sound for ever) */
            continue;
        uint32_t step = (uint32_t)(((uint64_t)3546895 << 16) / ((uint64_t)v->period * 44100));

        for (int n = 0; n < 882; n++) {
            unsigned index = v->position >> 16;

            if (index >= v->length) {
                if (!v->looping || v->length <= v->loop_start) {
                    v->finished = 1;
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
