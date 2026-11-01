# Gods, SDL3 port (menu program)

The menu program's C (`native/src/*.c`, the same files the Amiga build uses) runs on a Linux desktop. The Amiga's chips, the disks and
the sound are replaced by the C in this folder; nothing of the original's 68000 code runs.

## Build and run

You need the original disk images `gods-disk1.adf` and `gods-disk2.adf` in `projects/gods/local/` (or any folder, see `--disks`).

    make -C projects/gods/native/src/sdl                                                    # build (it enters the flake's shell itself)
    projects/gods/native/build/sdl/gods-sdl --disks projects/gods/local                     # run

or both: `make -C projects/gods/native/src/sdl run` (the shell is the one of `native/flake.nix`; the
flake has the 32-bit gcc and the 32-bit SDL3 the port needs).

Options: `--scale N` (window size, default 3), `--disks FOLDER`.  Test runs without a window:

    gods-sdl --headless --frames 800 --dump /tmp/gods_ --dump-every 100 --keys 300:4d:1,310:4d:0,400:40:1,420:40:0

writes the picture of every 100th frame as `/tmp/gods_NNNNN.ppm` and presses the keys given as `frame:raw key code (hex):1 down or 0 up`.

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
| `gen_guest.py` | from `menu.symbols`: the variables as absolute symbols, stubs for the original's code (`fn_...`, `original_...`), the table list |
| `guest.c` | the memory at the Amiga's addresses (the first 64 KiB at `0x01000000 +` : Linux does not map them), the chips' register pages, the menu image |
| `files.c` | the disks (ADF): the directory in sector 22, `file_read`, the packer, the menu image (`bm.2` of disk 1, unpacked to `$84`) |
| `blitter.c` | the blitter (copy and logic modes, shifts, masks, modulos) and `blit_image`/`blit_image_wide` of the asm |
| `render.c` | the display: the copper lists' effect from the program's state (screen shown, scroll, colours, message line) |
| `display_sdl.c` | `amiga/display.c` for the host (no copper scroll, a vertical blank wait that yields) |
| `irq.c` | the vertical blank (50 Hz from the main thread), the keyboard, `random_word`, `sr_set` |
| `shims.c` | `message_draw` (reads text at address 0 on the Amiga) |
| `sound.c` | the sound (see below) |
| `main.c` | the window, the 50 Hz loop, the keys; the program runs on its own thread, the vertical blank is raised from the main thread |

## What works

* the main menu (Begin Quest, Enter Password, Continue), the top ten after 750 idle frames, the password screen with its grid and the keyboard;
* fades, the loading message, the background pictures and the sprites (blitter), the pointer moved with the cursor keys.

## What does not

* Begin Quest, Continue and what follows the password screen go on in the original's 68000 code, which is not in C: the port logs
  `original routine ... called` and returns to the menu.  Starting the game is not possible yet.
* The intro is not ported.
* Words that the original code writes at start-up and the unpacked menu image lacks (the lists of asset file names, the name of the first
  asset set, the second copper list) are set in `guest.c`; the three saved passwords of the high score file are read in the
  Amiga's byte order and may be wrong (a fresh disk has none).
* The copper lists are not run: the message line's backgrounds and the scroll are emulated from the program's variables.
* **Sound is silent.** `sound.c` has the entry points of the asm (`play_sound`, `sound_tick`, `audio_irq`, `voice_start`) as empty
  routines.  What a sound player needs: the sequencer's tracks (`$21C3A`, the table of sound numbers `$20BCE`) are in the menu image, but
  the instrument records at `$3A38A` (sample address, lengths, loop) and the three sound files (`sound1.bin` to `sound3.bin`) are put in
  place by the original's start-up code, which is not in C, so there is nothing to play yet.
