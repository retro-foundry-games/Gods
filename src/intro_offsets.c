/* The offsets of the fields of intro.h that intro_amiga.asm uses, from the compiler: scripts/intro_offsets.py compiles this file
 * and writes native/intro_offsets.i, which intro_amiga.asm includes, so the assembly cannot drift from the structs.
 * Add a line here for every field the assembly names. */
#include "intro.h"

#define OFFSET(name, type, field) const unsigned short name = offsetof(type, field);
#define SIZE(name, type) const unsigned short name = sizeof(type);

OFFSET(Flags_music_enabled, struct Flags, music_enabled)
OFFSET(Flags_fire_pressed, struct Flags, fire_pressed)

OFFSET(GameClock_input_seen, struct GameClock, input_seen)
OFFSET(GameClock_ticks, struct GameClock, ticks)

OFFSET(Vblank_palette_dirty, struct Vblank, palette_dirty)
OFFSET(Vblank_saturating, struct Vblank, saturating)
OFFSET(Vblank_count, struct Vblank, count)

OFFSET(MusicState_speed, struct MusicState, speed)
OFFSET(MusicState_order, struct MusicState, order)
OFFSET(MusicState_row, struct MusicState, row)
OFFSET(MusicState_tick, struct MusicState, tick)
OFFSET(MusicState_jump, struct MusicState, jump)
OFFSET(MusicState_dma, struct MusicState, dma)

OFFSET(DisplayData_palette_fade_done, struct DisplayData, palette.fade_done)
OFFSET(DisplayData_palette_colour, struct DisplayData, palette.colour)
OFFSET(DisplayData_blit_plane_stride, struct DisplayData, blit.plane_stride)
OFFSET(DisplayData_blit_extra_modulo, struct DisplayData, blit.extra_modulo)
OFFSET(DisplayData_blit_mask, struct DisplayData, blit.mask)

OFFSET(Module_song_length, struct Module, song_length)

OFFSET(Channel_cell, struct Channel, cell)
OFFSET(Channel_sample, struct Channel, sample)
OFFSET(Channel_length, struct Channel, length)
OFFSET(Channel_loop, struct Channel, loop)
OFFSET(Channel_loop_length, struct Channel, loop_length)
OFFSET(Channel_period, struct Channel, period)
OFFSET(Channel_volume, struct Channel, volume)
OFFSET(Channel_dma, struct Channel, dma)
OFFSET(Channel_porta_down, struct Channel, porta_down)
OFFSET(Channel_porta_speed, struct Channel, porta_speed)
OFFSET(Channel_porta_target, struct Channel, porta_target)
OFFSET(Channel_vibrato, struct Channel, vibrato)
OFFSET(Channel_vibrato_pos, struct Channel, vibrato_pos)
SIZE(Channel_size, struct Channel)

OFFSET(AudioChannel_lc, struct AudioChannel, lc)
OFFSET(AudioChannel_len, struct AudioChannel, len)
OFFSET(AudioChannel_per, struct AudioChannel, per)
OFFSET(AudioChannel_vol, struct AudioChannel, vol)
