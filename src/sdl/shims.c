/* Gods SDL port: routines of the program whose behaviour depends on the Amiga's memory, replaced here (this object is linked before
 * the program's, whose own copies are not used).  Each is the program's routine with the one difference said.  Compiled with the
 * program's headers and flags. */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "menu.h"

/* orig $016ECA (message.c): draw message_text in the work area, 40 characters at most, centred, and start the colours fading in.
 * The program calls it with message_text 0 when it fades a message out with nothing to follow: on the Amiga the text is then read
 * from address 0 (the first bytes of the memory, which are 0: an empty text); here it is an empty text. */
void message_draw(void)
{
    static const uint8_t empty[1] = { 0 };
    const uint8_t *text = message_text ? message_text : empty;
    int16_t length = 0;

    while (*text++ != 0)
        length++;
    message_wide = -1;

    int16_t start = (40 - length) >> 1;

    message_x = start - 8;
    if (start < 0) {
        start = 0;
        message_x = 0;
        message_wide = 320;
        length = 40;
    }
    uint8_t *to = (uint8_t *)work_area + 40 + start;

    text = message_text ? message_text : empty;
    for (int i = 0; i < length; i++) {
        const uint8_t *glyph = font_8x8 + (*text++ << 3);

        /* (the rows are words of the machine: the shifting and the ticker of the message work on words, a byte of the row is the high or the low half of its word) */
        for (int row = 0; row < 7; row++) {
            uint16_t *word = (uint16_t *)(to - ((uintptr_t)to & 1) + 40 * row);
            int low = (int)((uintptr_t)to & 1);

            *word = (uint16_t)(low ? (*word & 0xff00) | glyph[row] : (*word & 0x00ff) | (glyph[row] << 8));
        }
        to++;
    }
    message_shown = 1;
    message_words[8] = 0x0320;
    message_words[9] = 0x0532;
    message_words[10] = 0x0754;
    message_words[11] = 0x0765;
    message_words[0] = message_words[4];
    message_words[1] = message_words[5];
    message_words[2] = message_words[6];
    message_words[3] = message_words[7];
    message_state = -1;
    message_timer = 10;
}

/* the Amiga's game_loop is the original's loop at $2C20 sent to game_frame (begin_quest ends by entering it): on the host it is the
 * loop of game_frame, for ever, with the interrupts on */
extern void sr_set(uint16_t value);

extern int keys_deferred;
extern int keys_take(int (*take)(int raw, int down, void *arg), void *arg);

/* GODS_RECORD=PREFIX: a play that is checked against the original afterwards (scripts/lockstep_play.py).  The variables at the top of every iteration go to PREFIX.iter and the
 * keys, taken at the top of an iteration, to PREFIX.keys ("N CODE V" as GODS_ITERKEYS has them).  The first RECORD_FROM iterations are not played by the original (its start is a state
 * that far into the level): keys that come before are let go. */
#define RECORD_FROM 128
static FILE *record_keys;
static unsigned record_iteration;

static int record_key(int raw, int down, void *arg)
{
    (void)arg;
    if (record_iteration <= RECORD_FROM)
        return !down;   /* (a key let go is let go) */
    if (record_keys) {
        fprintf(record_keys, "%u %x %d\n", record_iteration, raw & 0x7f, down ? 1 : 0);
        fflush(record_keys);
    }
    return 1;
}

volatile int gods_waiting;   /* the program waits for the original (GODS_FOLLOW): the machine's time does not go on */
unsigned gods_iteration;   /* the iteration of the loop now running (for the debugger) */

void game_loop(void)
{
    /* the lockstep with the Amiga build (scripts/lockstep_sdl.py): GODS_ITERKEYS=FILE ("N CODE V" lines: at the top of the Nth iteration of the loop the key with the raw
     * code CODE ($4C up, $4D down, $4E right, $4F left, $40 space) is set to V, in the keyboard interrupt's table) and GODS_ITERDUMP=FILE (the variables $100-$FFF at the top of
     * every iteration, as they are in the host's memory) */
    static struct { unsigned iteration, code, value; } keys[100000];
    unsigned key_count = 0, next_key = 0, iteration = 0;
    const char *keys_path = getenv("GODS_ITERKEYS"), *dump_path = getenv("GODS_ITERDUMP");
    FILE *dump = dump_path ? fopen(dump_path, "wb") : 0;
    const char *record = getenv("GODS_RECORD");

    if (record) {
        char name[600];

        snprintf(name, sizeof name, "%s.iter", record);
        dump = fopen(name, "wb");
        snprintf(name, sizeof name, "%s.keys", record);
        record_keys = fopen(name, "w");
        keys_deferred = 1;
    }

    if (keys_path) {
        FILE *f = fopen(keys_path, "r");

        while (f && key_count < 100000 && fscanf(f, "%u %x %u", &keys[key_count].iteration, &keys[key_count].code, &keys[key_count].value) == 3)
            key_count++;
        if (f)
            fclose(f);
    }
    /* GODS_FOLLOW=PREFIX: the SDL port follows the original that is played live (scripts/lockstep_live.py).  PREFIX.keys are the original's keys ("N CODE V", as GODS_ITERKEYS has
     * them), PREFIX.count the number of iterations the original has begun (a short text file) and the variables of the port go to the named pipe PREFIX.sdl.fifo.  An iteration N of
     * the port (the original's is N - 128: it starts that far into the level) is not begun before the original has begun its own, and the machine's time stands still while the
     * port waits (no frame is counted). */
    const char *follow = getenv("GODS_FOLLOW");
    static char follow_count[600], follow_keys[600];   /* (static: the stack of the loop is not safe from the program) */
    static off_t follow_keys_size = -1;

    if (follow) {
        char name[600];

        snprintf(follow_count, sizeof follow_count, "%s.count", follow);
        snprintf(follow_keys, sizeof follow_keys, "%s.keys", follow);
        snprintf(name, sizeof name, "%s.sdl.fifo", follow);
        dump = fopen(name, "wb");
    }
    const char *max_text = getenv("GODS_ITERMAX");   /* GODS_ITERMAX=N: the program ends at the top of the Nth iteration */
    unsigned max_iterations = max_text ? (unsigned)atoi(max_text) : 0;

    /* GODS_ITERREGION=N:ADDR:LEN:PATH (hex ADDR and LEN): the part of the memory at the top of the Nth iteration is written to PATH */
    const char *region_text = getenv("GODS_ITERREGION");
    unsigned region_iteration = 0, region_address = 0, region_length = 0;
    char region_path[512] = "";

    if (region_text && sscanf(region_text, "%u:%x:%x:%511s", &region_iteration, &region_address, &region_length, region_path) != 4)
        region_text = 0;
    sr_set(0x2000);
    for (;;) {
        iteration++;
        gods_iteration = iteration;
        if (max_iterations && iteration > max_iterations)
            exit(0);
        if (follow) {
            struct stat st;
            unsigned begun = 0;

            for (;;) {
                FILE *c = fopen(follow_count, "r");

                if (c) {
                    if (fscanf(c, "%u", &begun) != 1)
                        begun = 0;
                    fclose(c);
                }
                if (begun >= (iteration > 128 ? iteration - 128 : 0))
                    break;
                gods_waiting = 1;
                usleep(1000);
            }
            gods_waiting = 0;
            if (stat(follow_keys, &st) == 0 && st.st_size != follow_keys_size) {
                FILE *f = fopen(follow_keys, "r");

                follow_keys_size = st.st_size;
                key_count = 0;
                while (f && key_count < 100000 && fscanf(f, "%u %x %u", &keys[key_count].iteration, &keys[key_count].code, &keys[key_count].value) == 3)
                    key_count++;
                if (f)
                    fclose(f);
            }
        }
        if (record) {
            record_iteration = iteration;
            if (iteration == RECORD_FROM + 1)
                fprintf(stderr, "gods-sdl: recording keys from iteration %u: play now\n", iteration);
            keys_take(record_key, 0);
        }
        while (next_key < key_count && keys[next_key].iteration <= iteration) {
            key_down[keys[next_key].code] = keys[next_key].value ? 0xff : 0x00;
            next_key++;
        }
        if (region_text && iteration == region_iteration) {
            FILE *f = fopen(region_path, "wb");

            if (f) {
                fwrite((const void *)(uintptr_t)GUEST(region_address), 1, region_length, f);
                fclose(f);
            }
        }
        if (dump) {
            fwrite((const void *)(uintptr_t)GUEST(0x100), 1, 0xF00, dump);
            fflush(dump);
        }
        game_frame();
    }
}

/* ---- Begin Quest on the host -------------------------------------------------------------------------------------------------------
 * The menu's lines call the original's code (original_begin, original_continue, original_after_password), which has no C yet.  On the
 * host they call host_begin_quest(): with --level1-image the level is started from that RAM image of the original (a test) and the game
 * frame runs for ever; without it the call is logged and the menu goes on.  The functions are weak: the C ones, when they exist and
 * keep these names, take their place; when the menu calls another name this hook is not used. */
const char *host_level_image;   /* set by main.c: --level1-image */
extern int guest_load_level_image(const char *path);
extern void sr_set(uint16_t value);

void host_begin_quest(void)
{
    if (!host_level_image) {
        fprintf(stderr, "gods-sdl: Begin Quest: the C is not there yet (try --level1-image)\n");
        return;
    }
    if (guest_load_level_image(host_level_image) != 0) {
        fprintf(stderr, "gods-sdl: cannot read %s\n", host_level_image);
        return;
    }
    sr_set(0x2000);   /* the interrupts on */
    for (;;)
        game_frame();
}

__attribute__((weak)) void original_begin(void)
{
    host_begin_quest();
}

__attribute__((weak)) void original_continue(void)
{
    host_begin_quest();
}

__attribute__((weak)) void original_after_password(void)
{
    host_begin_quest();
}

/* an original routine that is not replicated was reached: once each, on stderr */
void host_missing_routine(uint32_t address)
{
    static uint32_t said[256];
    static unsigned count;

    for (unsigned i = 0; i < count; i++)
        if (said[i] == address)
            return;
    if (count < 256)
        said[count++] = address;
    fprintf(stderr, "gods-sdl: the original's routine at $%X is not replicated and was reached\n", address);
}
