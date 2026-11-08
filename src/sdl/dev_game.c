/* Gods SDL port, development: what the game's thread does for the lockstep and the other tests (the regular program, main.c, has none of it).  devmain.c calls dev_game_init(); the
 * game's loop (shims.c, game_loop) then calls dev_iteration at the top of every iteration.  Compiled with the program's headers and flags.
 *
 *   GODS_ITERKEYS=FILE ("N CODE V" lines: at the top of the Nth iteration the key with the raw code CODE ($4C up, $4D down, $4E right, $4F left, $40 space) is set to V in the keyboard
 *                      interrupt's table) and GODS_ITERDUMP=FILE (the variables $100-$FFF at the top of every iteration, as they are in the host's memory): scripts/lockstep_sdl.py
 *   GODS_RECORD=PREFIX, GODS_FOLLOW=PREFIX, GODS_ITERMAX=N, GODS_ITERREGION=N:ADDR:LEN:PATH, GODS_QUICKTEST=SAVE:LOAD (below) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "menu.h"
#include "guest.h"

extern void (*host_iteration_hook)(unsigned iteration);   /* shims.c */
extern volatile int gods_waiting;                         /* shims.c */
extern volatile int host_irq_hold;                        /* guest.c */

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
volatile int host_quick_request;   /* devmain.c: 1 F5, 2 F9 */
const char *host_quicksave_file = "gods-quicksave.bin";
#define QUICK_MAGIC 0x31535147u   /* "GQS1" */
#define QUICK_END 0x200000u   /* (GUEST_END of guest.c) */
#define QUICK_LOW 0x10000u   /* (GUEST_LOW_LIMIT) */
#define QUICK_MAIN_SIZE (QUICK_END - 0x10000)

/* the memory of a quicksave (F5, or the lockstep's F5: f5-K.port) read back from the file PATH: 0 when it was read (the interrupt held off while the memory is replaced) */
int host_quick_state_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t found = 0;
    extern volatile int host_irq_hold;
    int ok;

    if (!f)
        return -1;
    ok = fread(&found, 4, 1, f) == 1 && found == QUICK_MAGIC;
    if (ok) {   /* (the file is read to the end before the memory is touched) */
        uint8_t *main_memory = malloc(QUICK_MAIN_SIZE), *low = malloc(QUICK_LOW), *chips = malloc(0x1000);

        ok = main_memory && low && chips && fread(main_memory, 1, QUICK_MAIN_SIZE, f) == QUICK_MAIN_SIZE && fread(low, 1, QUICK_LOW, f) == QUICK_LOW
             && fread(chips, 1, 0x1000, f) == 0x1000;
        if (ok) {
            host_irq_hold = 1;   /* (the interrupt must not run on half of the memory) */
            memcpy((void *)(uintptr_t)0x10000, main_memory, QUICK_MAIN_SIZE);
            memcpy(guest_ptr(0), low, QUICK_LOW);
            memcpy((void *)(uintptr_t)0xdff000, chips, 0x1000);
            host_irq_hold = 0;
        }
        free(main_memory);
        free(low);
        free(chips);
    }
    fclose(f);
    return ok ? 0 : -1;
}

static void quick_do(int request)
{
    FILE *f = fopen(host_quicksave_file, request == 1 ? "wb" : "rb");
    uint32_t magic = QUICK_MAGIC;
    int ok = f != 0;
    extern volatile int host_irq_hold;

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
                host_irq_hold = 1;   /* (the interrupt must not run on half of the memory) */
                memcpy((void *)(uintptr_t)0x10000, main_memory, QUICK_MAIN_SIZE);
                memcpy(guest_ptr(0), low, QUICK_LOW);
                memcpy((void *)(uintptr_t)0xdff000, chips, 0x1000);
                host_irq_hold = 0;
            }
            free(main_memory);
            free(low);
            free(chips);
        }
    }
    fclose(f);
    fprintf(stderr, "gods-sdl: quick%s %s (%s)\n", request == 1 ? "save" : "load", ok ? "done" : "failed", host_quicksave_file);
}


static struct { unsigned iteration, code, value; } keys[100000];
static unsigned key_count, next_key;
static FILE *dump;
static const char *record, *follow;
static char follow_count[600], follow_keys[600];   /* (static: the stack of the loop is not safe from the program) */
static off_t follow_keys_size = -1;
static unsigned max_iterations;
static const char *region_text;
static unsigned region_iteration, region_address, region_length;
static char region_path[512] = "";
static unsigned test_save, test_load;   /* GODS_QUICKTEST=SAVE_ITERATION:LOAD_ITERATION: the F5 and the F9 at those iterations (a test without a window) */

/* the top of an iteration `iteration` (1 is the first) */
/* the weapons the lockstep gives (`weapon ITEM` on its protocol: the original has picked the same up): each is picked up at the top of the next iteration as the player picks up a weapon
 * lying on the ground */
volatile unsigned host_weapon_count;
volatile int16_t host_weapon_items[32];
static unsigned weapons_done;

static void dev_iteration(unsigned iteration)
{
    while (weapons_done != host_weapon_count) {
        int16_t item = host_weapon_items[weapons_done++ & 31];

        guest.word_2b2 = (uint16_t)item;
        pick_up_object(item);
    }
    if (test_save && iteration == test_save)
        host_quick_request = 1;
    if (test_load && iteration == test_load)
        host_quick_request = 2;
    if (host_quick_request) {
        int request = host_quick_request;

        host_quick_request = 0;
        quick_do(request);
    }
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
        guest.key_down[keys[next_key].code] = keys[next_key].value ? 0xff : 0x00;
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
}

void dev_game_init(void)
{
    const char *keys_path = getenv("GODS_ITERKEYS"), *dump_path = getenv("GODS_ITERDUMP"), *max_text = getenv("GODS_ITERMAX"), *quick_text = getenv("GODS_QUICKTEST");

    dump = dump_path ? fopen(dump_path, "wb") : 0;
    record = getenv("GODS_RECORD");
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
    follow = getenv("GODS_FOLLOW");
    if (follow) {
        char name[600];

        snprintf(follow_count, sizeof follow_count, "%s.count", follow);
        snprintf(follow_keys, sizeof follow_keys, "%s.keys", follow);
        snprintf(name, sizeof name, "%s.sdl.fifo", follow);
        dump = fopen(name, "wb");
    }
    max_iterations = max_text ? (unsigned)atoi(max_text) : 0;   /* GODS_ITERMAX=N: the program ends at the top of the Nth iteration */
    /* GODS_ITERREGION=N:ADDR:LEN:PATH (hex ADDR and LEN): the part of the memory at the top of the Nth iteration is written to PATH */
    region_text = getenv("GODS_ITERREGION");
    if (region_text && sscanf(region_text, "%u:%x:%x:%511s", &region_iteration, &region_address, &region_length, region_path) != 4)
        region_text = 0;
    if (quick_text)
        sscanf(quick_text, "%u:%u", &test_save, &test_load);
    host_iteration_hook = dev_iteration;
}
