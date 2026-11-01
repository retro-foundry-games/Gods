/* Gods SDL port: the sound.  Silent for now: see README.md (the instrument records and the sound files are put in place by the
 * original's start-up code).  The entry points of the asm (menu_amiga.asm) the program calls are here. */
#include <stdint.h>
#include <stdio.h>

#include "guest.h"

void sound_open(void)
{
}

void sound_close(void)
{
}

void play_sound(uint16_t id, uint16_t priority)
{
    (void)id;
    (void)priority;
}

void voice_start(uint16_t sound, uint16_t voice)
{
    (void)sound;
    (void)voice;
}

void sound_tick(void)
{
}

void audio_irq(void)
{
}
