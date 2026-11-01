# Gods, SDL3 port (intro, menu, level 1 as a test)

The C of the intro (`intro.c`) and of the menu program and the game (`menu.c` ... `game_*.c`), the same files the Amiga build uses, runs
on a Linux desktop.  The Amiga's chips, the disks and the sound are replaced by the C in this folder; nothing of the original's 68000
code runs.

## Build and run

You need the original disk images `gods-disk1.adf` and `gods-disk2.adf` in `projects/gods/local/` (or in the folder `--disks` names).

    make -C projects/gods/native sdl          # build (the same as: make -C projects/gods/native/src/sdl)
    make -C projects/gods/native sdl-run      # build and run:  intro, then (fire) the menu
    projects/gods/native/build/sdl/gods-sdl --disks projects/gods/local        # run without make

Both enter the flake's shell themselves (`native/flake.nix`: the 32-bit gcc and the 32-bit SDL3 the port needs).  Two programs are built:

* `gods-sdl` is the intro.  Fire (space, left control, return) ends it as on the Amiga ("LOADING GAME", then) and starts
* `gods-sdl-menu`, the menu program.  `gods-sdl --skip-intro` starts it at once.

Options (the same for both): `--disks FOLDER`, `--scale N` (window size, default 3).  F10 quits.

### Level 1 (a test, until the C Begin Quest exists)

The menu's Begin Quest, Continue and the code after the password screen are the original's 68000 code, which has no C yet: the port logs
`original routine ... called (not in C): skipped` and returns to the menu (the stubs `original_begin` ... come from `menu.symbols`; when
the C versions exist the stubs go away and the menu calls them).  To see the level, start it from a RAM image of the original:

    projects/gods/native/build/sdl/gods-sdl-menu --disks projects/gods/local --level1-image projects/gods/native/build/game-ram.bin

(`game-ram.bin`: the original's chip RAM in level 1, from `scripts/` / `host/`.)  This is for testing only: nothing is embedded and it is not the
default.  The image is the Amiga's big-endian bytes: the words and longs of the program's variables (from the types of `menu.symbols`, see
`gen_fixups.py`), the two screens, the sprites' planes and the background are swapped to native order; what the C reads in other ways (the
level's map, ...) is not known to the port and is left as it is.  `game_frame` is then called in a loop.

### Without a window (tests)

    gods-sdl-menu --headless --frames 800 --dump /tmp/gods_ --dump-every 100 --keys 300:4d:1,310:4d:0,400:40:1,420:40:0

writes the picture of every 100th frame as `/tmp/gods_NNNNN.ppm`, presses the keys `frame:raw key code (hex):1 down or 0 up` (`gods-sdl`:
only the space bar, 40), runs at 50 frames a second.  `GODS_WAV=file` writes the sound as raw signed 16 bit stereo 44100 Hz, `GODS_PLAY=id`
(level image only) starts a sound at once, `GODS_DEBUG=1` counts the game frames.

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
| `Makefile` | builds the program's modules unchanged with `-m32 -fpack-struct=2` (the game's data has the 68000's layout and 32-bit pointers) |
| `gen_guest.py`, `gen_fixups.py` | from `menu.symbols`: the variables as absolute symbols, stubs for the original's code (`fn_...`, `original_...`), the table list, the byte order list of the variables |
| `guest.c` | the memory at the Amiga's addresses (the first 64 KiB at `0x01000000 +`: Linux does not map them), the chips' register pages, the menu image, the level image |
| `files.c` | the disks (ADF): the directory in sector 22, `file_read`, the packer, the menu image (`bm.2` of disk 1, unpacked to `$84`) |
| `blitter.c` | the masked-image blit done directly (`blit_masked_image` of draw.c is replaced) and `blit_image`/`blit_image_wide` of the asm |
| `blitter_regs.c` | the chip's blitter (minterms, shifts, masks, modulos, ascending and descending), run at the store to BLTSIZE (`blit_start`), for `game_b.c` and `game_d.c` that program it |
| `render.c` | the display: the copper lists' effect from the program's state (the screen shown, the scroll, the colours, the message line) |
| `display_sdl.c` | `amiga/display.c` for the host |
| `irq.c` | the vertical blank (50 Hz from the main thread), the keyboard, `random_word`, `sr_set` |
| `shims.c` | `message_draw` (reads text at address 0 on the Amiga), `game_loop` |
| `sound.c` | the sound effects (see below) |
| `main.c` | the menu program's window, the 50 Hz loop, the keys; the program runs on its own thread |
| `intro_sdl.c`, `intro_host.c`, `intro_host.h` | the intro: window and audio; the routines of `intro_amiga.asm` in C, the ProTracker player |

## What works

* the intro, then the menu (boot, intro, fire, menu), with the music of the intro (a ProTracker player: speed, slides, tone portamento,
  vibrato, sample offset, volume slide, jump, break; the E effects are not played);
* the menu: the three lines, the top ten, the password screen, fades, pictures, sprites, the pointer moved by the keys;
* level 1 from the RAM image: the level is drawn (the screen with the hero, the ladder, the status panel) and the program's frame runs.

## What does not

* Begin Quest, Continue and the code after the password screen: not in C yet (see above).  The level cannot be started from the menu.
* Level 1 from the image does not respond to the keys yet: the routines of the frame that are still the original's code (`fn_...`, about
  twenty of them: the player's control among them, as far as I can tell) are skipped, and so is what they draw.  Only what is C draws.
  The image's map and other data of unknown width are not swapped, so some of what the C draws from them may be wrong.
* The copper lists are not run: the split-screen colours of the level (the colour bands) are not shown, the message line is emulated.
* Sound effects: `sound.c` follows the tracks of the original's sequencer note by note (sound table, instruments and period tables of
  the image) and mixes the samples with Paula's rates and the Amiga's left/right; the envelopes, the slides and the random variants of some
  sounds are not done.  It has data only with the level image (the menu program's own start-up writes the instruments, which is not in C),
  so the menu is silent.  A first check wrote non-silent audio for the menu click (sound 33); nothing was listened to.
* The intro's story scroll and its other parts were not compared with the original; the music is not the original's replayer (timing and
  the E effects differ).
* The three saved passwords of the high score file are read in the Amiga's byte order and may be wrong (a fresh disk has none).
