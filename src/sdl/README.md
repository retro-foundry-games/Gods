# Gods, SDL3 port (intro, menu, level 1 as a test)

The C of the intro (`intro.c`) and of the menu program and the game (`menu.c` ... `game_*.c`), the same files the Amiga build uses, runs
on a Linux desktop as one program: `gods-sdl` goes intro, menu, level in one process, one window and one audio stream.  The Amiga's
chips, the disks and the sound are replaced by the C in this folder; nothing of the original's 68000 code runs.

## Build and run

You need the original disk images `gods-disk1.adf` and `gods-disk2.adf` in `projects/gods/local/` (or in the folder `--disks` names).

    make -C projects/gods/native sdl          # build   (the flake's shell with the 32-bit gcc and SDL3 is entered by itself)
    make -C projects/gods/native sdl-run      # run     (builds first; runs build/sdl/gods-sdl, which finds the disks in projects/gods/local)

Two programs are built from the same machine (`host.c`): `gods-sdl` (`main.c`) runs the game with `--disks`, `--interpolate` and `--widescreen`, no environment variables and no control channel;
`gods-sdl-dev` (`devmain.c`, with `dev_game.c` for the game's thread) has everything the tests need: the options below, the lockstep with the original (`--lockstep`, see
`projects/gods/host/gods_lockstep.lua`), the quicksave (F5) and quickload (F9), `--quickload FILE` and the `GODS_*` environment variables.

Fire (space, left control, return) ends the intro as on the Amiga ("LOADING GAME") and the menu comes up in the same window.  Options of `gods-sdl-dev`:
`--disks FOLDER`, `--scale N` (window size, default 3), `--skip-intro` (or `--start menu`), `--start level` (needs `--level1-image`).  F10 quits.

### Widescreen (16:9)

Both executables accept `--widescreen`; the browser launcher has a
**Widescreen (16:9)** checkbox. This is the only extra aspect ratio. It works
independently of `--interpolate` / **Smooth motion**.

    build/sdl/gods-sdl --disks DIR --widescreen --interpolate

The presentation surface shows 400 level pixels across rather than 320.
Its 400x270 raster is displayed at 400x225, preserving the original 6:5 pixel
proportions and giving exactly 16:9. The original 320x256 raster is centred in
that surface; the HUD and message text retain their original pixels. Intro,
menu, shop and result artwork keeps its original size and proportions.

The wider camera stays inside the 4096-pixel level. The original camera,
352-pixel bitplanes, saved-state layout and all gameplay rules remain intact.
The game thread snapshots the same map/tiles used by `draw_background`
(`$178C6` / `$17972`), plus the portions of actual glyph, sprite and clipped
image draws beyond the original bitmap. Rendering never runs those routines
again. Masks, fixed spike bases and foreground draw order are retained.

Enemy activation, projectile removal and other visibility rules still use the
original gameplay viewport. The additional view can therefore reveal their
original appearance/disappearance boundaries. It does not activate enemies
earlier, extend weapon range or change collision, physics or game speed.
Development captures and the lockstep protocol retain their 320x256 format.

    make -C src/sdl test-widescreen

With the original disk images, the native lockstep test compares 300 machine
ticks of walking and firing, checking every original raster pixel and all
named gameplay members from `$100` through `$1D3FF` (unused padding excluded):

    make -C src/sdl test-widescreen-gameplay GODS_DISKS="DIR"

### Pixel scaling

Desktop and browser output use the pixel resampler from F/A-18 Interceptor's
`native/src/platform/pixel_scale.c`, adapted in `pixel_scale.c` here. This is
enabled in both the original and interpolated rendering modes.

It enlarges the original bitmap with nearest sampling, then selects and blends
box-filtered mip levels for the displayed pixel footprint. Integral pixel
dimensions use an exact nearest copy; fractional dimensions retain sharp pixel
interiors and filter their boundaries. Gods keeps its existing pixel aspect,
palettes and source resolution. The preferred enlargement is 12x, reduced to
fit the source renderer's texture-size limits and its 256 MiB GPU / 16 MiB
software texture budget. Targets are prepared before the game clock starts.

Desktop windows support resizing and high DPI. The browser uses F/A-18's
physical-pixel sizing and snaps the canvas size and origin to the display grid,
including resize, fullscreen, high DPI and visual-viewport zoom. SDL renders
directly to those pixels rather than enlarging a fixed canvas through CSS.

The underlying technique comes from ZXSC's `src/speccy/Render.cpp`; its
[MIT notice](third_party/ZXSC-LICENSE.txt) is retained. To check exact integer
pixels, fractional boundaries, tiny outputs and the pixel aspect:

    make -C src/sdl test-pixel-scale

### Interpolated rendering

Both executables accept `--interpolate`:

    build/sdl/gods-sdl --disks DIR --interpolate

This disables desktop VSync and presents as fast as the renderer can run. The
machine still raises vertical blanks at 50 Hz; the original gameplay loop
keeps its existing waits between completed drawings. Extra presentations
interpolate sprite positions and camera scrolling between those drawings.
Input is polled every presentation, and sound advances only on machine ticks.
The default rendering mode is unchanged.

The browser launcher has a **Smooth motion** checkbox for the same interpolation.
Browser presentation follows `requestAnimationFrame`, so its rate is the
display's refresh rate rather than an uncapped desktop loop.

`presentation.c` records the original glyph, sprite and clipped-image blits and
their masks into immutable snapshots at screen flips. It moves the drawings
over the reconstructed background without rerunning game logic or changing
guest state. Scenery interpolates across all visible bitmap rows, including
behind the HUD. The HUD's actual blit masks keep its pixels fixed without
freezing the scenery in the same rows. The message band and animation poses
retain their original pixels. Presentation trails gameplay by one completed drawing (40 ms at 25 Hz),
using the actual interval between flips when a drawing takes longer.
Scene changes, respawns, teleports, loads and long clock gaps discard history.
Scenery, moving paths, pickups and creatures match drawings by their persistent
game records, preserving identity when draw order or visibility changes. A new
or unmatched drawing keeps its current world position and follows the
interpolated camera, so foreground masks remain aligned with the scenery.
The retracting spikes (`draw_thing_flyer`, original `$49F8`) retain their fixed
stem base: interpolation reveals or retracts the stem above that boundary
without moving its lower pixels through the scenery.

Development captures and scripted keys still count 50 Hz machine ticks in this
mode. `--headless`, `--lockstep`, `GODS_FOLLOW` and `GODS_FAST` cannot be combined
with `--interpolate`. To test the compositor without disk images:

    make -C src/sdl test-presentation

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

    projects/gods/native/build/sdl/gods-sdl-dev --disks projects/gods/local --level1-image projects/gods/native/build/game-ram.bin --start level

(or leave `--start level` out and choose Begin Quest in the menu).  The image is the Amiga's big-endian chip RAM: the words and longs of
the program's variables (from the types of `game.symbols`, `gen_fixups.py`), the two screens, the sprites' planes and the background are
swapped to native order; what the C reads in other ways (the level's map, ...) is not known to the port and is left as it is.
`game_frame` is then called in a loop.

### Without a window (tests)

    gods-sdl-dev --headless --frames 1100 --dump /tmp/gods_ --dump-every 100 --keys 200:40:1,210:40:0,800:4d:1,810:4d:0,850:40:1,870:40:0

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
| `gen_guest.py`, `gen_fixups.py` | from `game.symbols`: the variables as absolute symbols, stubs for the original's code (`fn_...`), the table list, the byte order list of the variables |
| `guest.c` | the memory at the Amiga's addresses (the first 64 KiB at `0x01000000 +`: Linux does not map them), the chips' register pages, the menu image, the level image |
| `files.c` | the disks (ADF): the directory in sector 22, `file_read`, the packer, the menu image (`bm.2` of disk 1, unpacked to `$84`) |
| `display.c` | the screens' flip, the colours and fades, the masked-image blits done directly (`blit_masked_image` of draw.c is replaced, and `blit_image`/`blit_image_wide` of the asm) and the picture: the copper lists' effect from the program's state (screen shown, scroll, colours, message line); the chip's blitter itself is not emulated |
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

The bitplanes' words are the machine's own on the host (converted when they are loaded; the blits work on words).  The program's few byte accesses to the pixels of a plane
(cutting the digits out of the screen in `hud_digits_load`, putting them in the score image in `draw_score_digit`) are written as what they are, eight pixels of a
word (`plane_byte`, `set_plane_byte` in game.c: the high half of a word for an even address), so that they mean the same on both machines.

## Debug build

`make sdl DEBUG=1` (or `DEBUG=1 make -C projects/gods/native/src/sdl`) compiles everything with `-O0 -ggdb`; the objects are made again when the flags change, so `make sdl`
without it goes back.  `gdb build/sdl/gods-sdl` (a 32 bit program; the game's memory is mapped at the Amiga's addresses, the first 64 KiB at `0x01000000`).
