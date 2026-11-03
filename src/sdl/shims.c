/* Gods SDL port: routines of the program whose behaviour depends on the Amiga's memory, replaced here (this object is linked before
 * the program's, whose own copies are not used).  Each is the program's routine with the one difference said.  Compiled with the
 * program's headers and flags. */
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "guest.h"

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

/* the quicksave (F5) and the quickload (F9): the window asks (host_quick_request), the program's thread does it at the top of an iteration of the loop, where nothing of the game
 * is half done on its stack: the memory the program has (the main memory, the low 64 KiB, the chips' registers) is written to a file or read back from it, as the host has it (native
 * words).  The state is what the memory holds; what the host keeps outside it (the sound mixer's voices, the window) is not. */
volatile int host_quick_request;   /* main.c: 1 F5, 2 F9 */
const char *host_quicksave_file = "gods-quicksave.bin";
#define QUICK_MAGIC 0x31535147u   /* "GQS1" */
#define QUICK_END 0x200000u   /* (GUEST_END of guest.c) */
#define QUICK_LOW 0x10000u   /* (GUEST_LOW_LIMIT) */
#define QUICK_MAIN_SIZE (QUICK_END - 0x10000)

static void quick_do(int request)
{
    FILE *f = fopen(host_quicksave_file, request == 1 ? "wb" : "rb");
    uint32_t magic = QUICK_MAGIC;
    int ok = f != 0;

    if (!f) {
        fprintf(stderr, "gods-sdl: quick%s: cannot open %s\n", request == 1 ? "save" : "load", host_quicksave_file);
        return;
    }
    if (request == 1) {
        ok = fwrite(&magic, 4, 1, f) == 1 && fwrite((const void *)(uintptr_t)0x10000, 1, QUICK_MAIN_SIZE, f) == QUICK_MAIN_SIZE
             && fwrite(guest_ptr(0), 1, QUICK_LOW, f) == QUICK_LOW && fwrite((const void *)(uintptr_t)0xdff000, 1, 0x1000, f) == 0x1000;
    } else {
        uint32_t found = 0;

        ok = fread(&found, 4, 1, f) == 1 && found == QUICK_MAGIC;
        if (ok) {   /* (the file is read to the end before the memory is touched) */
            uint8_t *main_memory = malloc(QUICK_MAIN_SIZE), *low = malloc(QUICK_LOW), *chips = malloc(0x1000);

            ok = main_memory && low && chips && fread(main_memory, 1, QUICK_MAIN_SIZE, f) == QUICK_MAIN_SIZE && fread(low, 1, QUICK_LOW, f) == QUICK_LOW
                 && fread(chips, 1, 0x1000, f) == 0x1000;
            if (ok) {
                memcpy((void *)(uintptr_t)0x10000, main_memory, QUICK_MAIN_SIZE);
                memcpy(guest_ptr(0), low, QUICK_LOW);
                memcpy((void *)(uintptr_t)0xdff000, chips, 0x1000);
            }
            free(main_memory);
            free(low);
            free(chips);
        }
    }
    fclose(f);
    fprintf(stderr, "gods-sdl: quick%s %s (%s)\n", request == 1 ? "save" : "load", ok ? "done" : "failed", host_quicksave_file);
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
        {   /* GODS_QUICKTEST=SAVE_ITERATION:LOAD_ITERATION: the F5 and the F9 at those iterations (a test without a window) */
            static unsigned test_save, test_load;
            static int test_read;

            if (!test_read) {
                const char *t = getenv("GODS_QUICKTEST");

                test_read = 1;
                if (t)
                    sscanf(t, "%u:%u", &test_save, &test_load);
            }
            if (test_save && iteration + 1 == test_save)
                host_quick_request = 1;
            if (test_load && iteration + 1 == test_load)
                host_quick_request = 2;
        }
        if (host_quick_request) {
            int request = host_quick_request;

            host_quick_request = 0;
            quick_do(request);
        }
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

/* the quickload (--quickload FILE): begin_quest takes the level from a RAM image of the original instead of loading it */
const char *host_quickload_file;
extern int guest_load_quick(const char *path);

int host_quickload(void)
{
    if (!host_quickload_file)
        return 0;
    if (guest_load_quick(host_quickload_file) != 0) {
        fprintf(stderr, "gods-sdl: cannot read %s\n", host_quickload_file);
        exit(1);
    }
    return 1;
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

/* the original's CPU work (a loop that clears a screen, the drawing of a frame) takes frames of the video; the host does it at once and waits for them */
/* the lockstep with the original (`flipsync OFFSET`): the port's flips wait for the original's (the driver gives the count of the times the original reached its flip, $1D302), so
 * the CPU time of an iteration is not modelled (host_iteration_cpu does nothing); the other waits stay, as the least time the original takes */
volatile int host_flip_sync;
volatile unsigned host_flips_orig;   /* the original's flips since the sync began */
volatile int host_flips_offset;
static unsigned flips_done;
static int flip_bound;   /* the next flip ends an iteration (host_iteration_cpu): it waits for the original's */

void host_flip_wait(void)
{
    extern void wait_vblank(void);

    if (!host_flip_sync)
        return;
    if (flip_bound) {
        while ((int)(flips_done + 1) - host_flips_offset > (int)host_flips_orig)
            wait_vblank();
        flip_bound = 0;
    }
    flips_done++;
}

void host_cpu_frames(unsigned frames)
{
    extern void wait_vblank(void);

    while (frames--)
        wait_vblank();
}

static uint16_t iteration_mark;

int host_lockstep_level;   /* set by main.c: --lockstep-level */

int16_t host_start_level(void)
{
    return host_lockstep_level ? (int16_t)host_lockstep_level : 1;
}

void host_iteration_mark(void)
{
    iteration_mark = vbl_count;
}

/* the drawing and moving of a frame of the game takes the original's CPU about two video frames, so a frame that is not held back by the wait for the last flip (the first
 * after a fade) still ends two frames after it began */
static unsigned iteration_cpu_next;

void host_iteration_cpu_next(unsigned frames)
{
    iteration_cpu_next = frames;
}

void host_iteration_cpu(unsigned frames)
{
    extern void wait_vblank(void);

    if (host_flip_sync) {
        iteration_cpu_next = 0;
        flip_bound = 1;
        return;
    }
    if (iteration_cpu_next) {
        frames = iteration_cpu_next;
        iteration_cpu_next = 0;
    }
    while ((uint16_t)(vbl_count - iteration_mark) < frames)
        wait_vblank();
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
