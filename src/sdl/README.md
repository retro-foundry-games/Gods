# Gods, SDL3 port (intro, menu, level 1 as a test)

The C of the intro (`intro.c`) and of the menu program and the game (`menu.c` ... `game_*.c`), the same files the Amiga build uses, runs
on a Linux desktop as one program: `gods-sdl` goes intro, menu, level in one process, one window and one audio stream.  The Amiga's
chips, the disks and the sound are replaced by the C in this folder; nothing of the original's 68000 code runs.

## Build and run

You need the original disk images `gods-disk1.adf` and `gods-disk2.adf` in `projects/gods/local/` (or in the folder `--disks` names).

    make -C projects/gods/native sdl          # build   (the flake's shell with the 32-bit gcc and SDL3 is entered by itself)
    make -C projects/gods/native sdl-run      # run     (builds first; the same as build/sdl/gods-sdl --disks projects/gods/local)

Fire (space, left control, return) ends the intro as on the Amiga ("LOADING GAME") and the menu comes up in the same window.  Options:
`--disks FOLDER`, `--scale N` (window size, default 3), `--skip-intro` (or `--start menu`), `--start level` (needs `--level1-image`).  F10 quits.

How one program does what the loader does between the Amiga's programs: the intro and the menu program are separate programs with the same
names (`glyph_draw`, `screens_flip`, `blit_image` ...), so the intro is linked into one object whose symbols are local but a few entry points;
each program runs on a thread of its own; when the intro asks for the next part its thread ends, the guest memory and the chip registers
are cleared, the menu image is unpacked from disk 1 again and the menu starts on a new thread, while the window, the audio stream and the
main loop (vertical blank, picture, sound, keys) go on.

### Begin Quest and level 1 (a test, until the C Begin Quest exists)

The menu's Begin Quest, Continue and the code after the password screen are the original's 68000 code, which has no C yet.  On the host
they call `host_begin_quest()` (`shims.c`): without more it logs `Begin Quest: the C is not there yet` and the menu goes on.  The C version
takes its place when `original_begin` and the others get one (the hook functions are weak).  To see the level now, start it from a RAM
image of the original (for testing only: nothing is embedded, it is not the default):

    projects/gods/native/build/sdl/gods-sdl --disks projects/gods/local --level1-image projects/gods/native/build/game-ram.bin --start level

(or leave `--start level` out and choose Begin Quest in the menu).  The image is the Amiga's big-endian chip RAM: the words and longs of
the program's variables (from the types of `menu.symbols`, `gen_fixups.py`), the two screens, the sprites' planes and the background are
swapped to native order; what the C reads in other ways (the level's map, ...) is not known to the port and is left as it is.
`game_frame` is then called in a loop.

### Without a window (tests)

    gods-sdl --headless --frames 1100 --dump /tmp/gods_ --dump-every 100 --keys 200:40:1,210:40:0,800:4d:1,810:4d:0,850:40:1,870:40:0

writes the picture of every 100th frame as `/tmp/gods_NNNNN.ppm` and presses the keys `frame:raw key code (hex):1 down or 0 up` (the frame
count goes on across the programs; this one: fire in the intro, down and fire in the menu), at 50 frames a second.  `GODS_WAV=file` writes
the sound as raw signed 16 bit stereo 44100 Hz.

## Keys

| key | does |
| --- | --- |
| cursor keys | the joystick |
| space | the fire button |
| letters, digits, `.`, `,`, return, backspace | the password screen (as on the Amiga keyboard) |
| F10 | quit |

## What is where

| file | what it does |
| --- | --- |
| `Makefile` | builds the program's modules unchanged with `-m32 -fpack-struct=2` (the game's data has the 68000's layout and 32-bit pointers); links the intro apart |
| `main.c` | the window, the 50 Hz loop, the keys, the audio stream, the change from one program to the next |
| `gen_guest.py`, `gen_fixups.py` | from `menu.symbols`: the variables as absolute symbols, stubs for the original's code (`fn_...`), the table list, the byte order list of the variables |
| `guest.c` | the memory at the Amiga's addresses (the first 64 KiB at `0x01000000 +`: Linux does not map them), the chips' register pages, the menu image, the level image |
| `files.c` | the disks (ADF): the directory in sector 22, `file_read`, the packer, the menu image (`bm.2` of disk 1, unpacked to `$84`) |
| `blitter.c` | the masked-image blit done directly (`blit_masked_image` of draw.c is replaced) and `blit_image`/`blit_image_wide` of the asm |
| `blitter_regs.c` | the chip's blitter (minterms, shifts, masks, modulos, ascending and descending), run at the store to BLTSIZE (`blit_start`), for `game_b.c` and `game_d.c` |
| `render.c` | the menu program's and the game's display: the copper lists' effect from the program's state (screen shown, scroll, colours, message line) |
| `display.c` | `amiga/display.c` for the host: the same fades and flips, the chips' parts the host's |
| `irq.c` | the vertical blank (from the main loop), the keyboard, `random_word`, `sr_set` |
| `shims.c` | `message_draw` (reads text at address 0 on the Amiga), `game_loop`, `host_begin_quest` |
| `sound.c` | the sound effects (see below) |
| `intro_host.c`, `intro_view.c` | the intro: the routines of `intro_amiga.asm` in C, the ProTracker player; its picture |

## What works

* intro, then menu, then (with the image) level, in one window, no restart of the window or the audio between them;
* the intro with its music (a ProTracker player: speed, slides, tone portamento, vibrato, sample offset, volume slide, jump, break; the E effects are not played);
* the menu: the three lines, the top ten, the password screen, fades, pictures, sprites, the pointer moved by the keys;
* level 1 from the RAM image: the level is drawn and the program's frame runs.

## What does not

* Begin Quest from the disks: no C yet (see above).  Only the image starts a level.
* Level 1 from the image: the routines of the frame that are still the original's code (`fn_...`) are skipped, and so is what they draw.
  The image's map and other data of unknown width are not swapped, so some of what the C draws from them may be wrong.
* The copper lists are not run: the split-screen colours of the level are not shown, the message line is emulated.
* Sound effects: `sound.c` follows the tracks of the original's sequencer note by note (sound table, instruments and period tables of the
  image) and mixes the samples with Paula's rates and the Amiga's left/right; the envelopes, the slides and the random variants of some sounds
  are not done.  It has data only with the level image (the menu program's own start-up writes the instruments, which is not in C), so the
  menu is silent.  Nothing was listened to.
* The intro's story scroll and its other parts were not compared with the original; the music is not the original's replayer.
* The three saved passwords of the high score file are read in the Amiga's byte order and may be wrong (a fresh disk has none).

## Pixel bytes

The bitplanes' words are the machine's own on the host (the blits work on words), so the first byte of a word is its low one.  The few places
of the program that read or write the pixels of a plane by bytes (cutting the digits out of the screen in `hud_digits_load`, putting them in the score
image in `draw_score_digit`) go through `PIXEL_BYTE(p)` (`amiga/platform.h`: the pointer itself; `sdl/sdl_platform.h`: the neighbouring byte).
Without it the score shows eight ones.  Any other byte access to a plane's pixels needs the same.

## Debug build

`make sdl DEBUG=1` (or `DEBUG=1 make -C projects/gods/native/src/sdl`) compiles everything with `-O0 -ggdb`; the objects are made again when the flags change, so `make sdl`
without it goes back.  `gdb build/sdl/gods-sdl` (a 32 bit program; the game's memory is mapped at the Amiga's addresses, the first 64 KiB at `0x01000000`).
The Amiga build has the same: `make menu DEBUG=1` / `DEBUG=1 uv run python projects/gods/scripts/build_menu.py` (the code is bigger and slower, the game's timing
is not the same; `amiga/libgcc_debug.c` supplies the 32 bit multiply and divide that -O0 code calls).
