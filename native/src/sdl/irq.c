/* Gods SDL port: the interrupts and the small asm routines of amiga.asm.  The main thread raises the vertical blank interrupt
 * 50 times a second (irq_vbl), the program runs on its own thread, as it runs under the interrupt on the Amiga. */
#include <stdint.h>
#include <stdio.h>

#include "guest.h"

extern void input_poll(void);
extern void frame_tick(void);
#include "game.h"
extern const uint8_t keymap[128];

static volatile int interrupts_on;

/* amiga.asm: the status register; the program sets $2000 when the vectors and the chips are ready */
void sr_set(uint16_t value)
{
    interrupts_on = (value & 0x0700) == 0;
}

/* amiga.asm: the stack is changed and the code entered: not needed on the host */
void jump_with_stack(void (*address)(void), uint8_t *stack)
{
    (void)stack;
    address();
}

/* orig $01CFF8: the vertical blank interrupt (level 3): the pointer and buttons, the colours, the frame counts and the work of every frame */
void vbl_irq(void)
{
    input_poll();
    if (guest.palette_dirty) {
        guest.palette_dirty = 0;
        for (int i = 0; i < 16; i++)
            REG16(0x180 + 2 * i) = guest.shadow_table[i];
    }
    frame_tick();   /* the original jumps to the routine at $016C30 */
    /* the counts the program waits on come last: on the Amiga the interrupt is not interrupted by the program, here the program's thread
     * runs at the same time and must not see a frame whose work (tick_count, the sound, the message) is not done */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    guest.vbl_count++;
    if (guest.vbl_saturating != 0x7fff)
        guest.vbl_saturating++;
}

void irq_vbl(void)
{
    if (interrupts_on)
        vbl_irq();
}

/* the keyboard interrupt of the original is a stub here: irq_key() does what key_translate (orig $01E50C) does */
void keyboard_irq(void)
{
}

/* the keys of a recorded play (GODS_RECORD=PREFIX, see scripts/lockstep_play.py): the window's key events wait in a queue and the program's thread takes them at the top of
 * an iteration of the game loop, so that the iteration they belong to is known and the original can be given the same keys at the same iteration */
static struct { int raw, down; } key_queue[256];
static volatile unsigned key_head, key_tail;
int keys_deferred;
int keys_space_is_fire;   /* the lockstep: the original (with the keyboard patch) takes the space bar as the fire button only, not as a typed character */

static void key_apply(int raw, int down);

void irq_key(int raw, int down)
{
    if (!keys_deferred) {
        key_apply(raw, down);
        return;
    }
    unsigned head = key_head;

    if (head - key_tail < 256) {
        key_queue[head & 255].raw = raw;
        key_queue[head & 255].down = down;
        __atomic_store_n(&key_head, head + 1, __ATOMIC_RELEASE);
    }
}

/* the events that came since the last call (oldest first): `take` is told each (it says whether it is applied); returns how many were */
int keys_take(int (*take)(int raw, int down, void *arg), void *arg)
{
    int n = 0;

    while (key_tail != __atomic_load_n(&key_head, __ATOMIC_ACQUIRE)) {
        int raw = key_queue[key_tail & 255].raw, down = key_queue[key_tail & 255].down;

        key_tail++;
        if (!take || take(raw, down, arg)) {
            key_apply(raw, down);
            n++;
        }
    }
    return n;
}

/* the key with the Amiga raw code `raw`: pressed or let go */
static void key_apply(int raw, int down)
{
    uint8_t code = (uint8_t)(raw & 0x7f);

    /* the cursor keys are the port's joystick (see input_poll: not in the original): the stick, not typed characters; the space bar is a typed character again */
    if ((code >= 0x4c && code <= 0x4f) || (code == 0x40 && keys_space_is_fire)) {
        guest.key_down[code] = down ? 0xff : 0;
        return;
    }
    if (code != 0x60 && code != 0x61)
        guest.pending_request = (guest.pending_request & 0xff00ffffu) | ((uint32_t)(code | (down ? 0 : 0x80)) << 16);
    if (down) {
        uint8_t character = keymap[code];

        guest.key_down[code] = 0xff;
        if ((guest.key_down[0x61] | guest.key_down[0x60]) != 0 && character < 0x7b) {
            if (character >= 0x40)
                character &= 0xdf;
            else if (character >= 0x20)
                character &= 0xef;
        }
        guest.pending_request = (guest.pending_request & 0xffffff00u) | character;
    } else {
        guest.pending_request = 0;
        guest.key_down[code] = 0;
    }
}

/* orig $00633E: the next word of a table, round and round */
uint16_t random_word(void)
{
    uint16_t *index = &guest.random_index;
    uint16_t value = *(uint16_t *)(guest.random_words + *index);

    *index = (uint16_t)((*index + 2) & 0x1ff);
    return value;
}
