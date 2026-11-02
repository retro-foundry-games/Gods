/* Gods SDL port: the interrupts and the small asm routines of menu_amiga.asm.  The main thread raises the vertical blank interrupt
 * 50 times a second (irq_vbl), the program runs on its own thread, as it runs under the interrupt on the Amiga. */
#include <stdint.h>
#include <stdio.h>

#include "guest.h"

extern void input_poll(void);
extern void frame_tick(void);
extern uint16_t palette_dirty;
extern uint16_t shadow_table[];
extern volatile uint16_t vbl_saturating;
extern volatile uint16_t vbl_count;
extern volatile uint32_t pending_request;
extern volatile uint8_t key_down[128];
extern const uint8_t keymap[128];
extern uint8_t *ptr_0cc6;

static volatile int interrupts_on;

/* menu_amiga.asm: the status register; the program sets $2000 when the vectors and the chips are ready */
void sr_set(uint16_t value)
{
    interrupts_on = (value & 0x0700) == 0;
}

/* menu_amiga.asm: the stack is changed and the code entered: not needed on the host */
void jump_with_stack(void (*address)(void), uint8_t *stack)
{
    (void)stack;
    address();
}

/* orig $01CFF8: the vertical blank interrupt (level 3): the pointer and buttons, the colours, the frame counts and the work of every frame */
void vbl_irq(void)
{
    input_poll();
    if (palette_dirty) {
        palette_dirty = 0;
        for (int i = 0; i < 16; i++)
            REG16(0x180 + 2 * i) = shadow_table[i];
    }
    frame_tick();   /* the original jumps to the routine at $016C30 */
    /* the counts the program waits on come last: on the Amiga the interrupt is not interrupted by the program, here the program's thread
     * runs at the same time and must not see a frame whose work (tick_count, the sound, the message) is not done */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    vbl_count++;
    if (vbl_saturating != 0x7fff)
        vbl_saturating++;
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

/* the key with the Amiga raw code `raw`: pressed or let go */
void irq_key(int raw, int down)
{
    uint8_t code = (uint8_t)(raw & 0x7f);

    if (code != 0x60 && code != 0x61)
        pending_request = (pending_request & 0xff00ffffu) | ((uint32_t)(code | (down ? 0 : 0x80)) << 16);
    if (down) {
        uint8_t character = keymap[code];

        key_down[code] = 0xff;
        if ((key_down[0x61] | key_down[0x60]) != 0 && character < 0x7b) {
            if (character >= 0x40)
                character &= 0xdf;
            else if (character >= 0x20)
                character &= 0xef;
        }
        pending_request = (pending_request & 0xffffff00u) | character;
    } else {
        pending_request = 0;
        key_down[code] = 0;
    }
}

/* orig $00633E: the next word of a table, round and round */
uint16_t random_word(void)
{
    uint16_t *index = (uint16_t *)(uintptr_t)(0x2e6 + 0x01000000u);
    uint16_t value = *(uint16_t *)(ptr_0cc6 + *index);

    *index = (uint16_t)((*index + 2) & 0x1ff);
    return value;
}
