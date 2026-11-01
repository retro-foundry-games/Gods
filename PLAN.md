# Gods: plan and status

Gods (Bitmap Brothers, 1991, PAL original disks, Kickstart 1.3 A500). The user's latest direction overrides
anything older here; see also [AGENTS.md](AGENTS.md).

## Direction (from the user)

- The game is **one C file per program**: the intro is `src/intro.c` (its data in `intro_data.c`, header `intro.h`, link script
  `intro.ld`, assembly `intro_amiga.asm`), the menu will be `src/menu.c` the same way.  Each is built with `m68k-amigaos-gcc` from the `src/` flake
  (`nix develop --command ...`), for the A500 (68000).
- It is **real decompiled C**: typed structs and globals at the original addresses, real parameters and return
  values. No emulated register file, no flag/`op_` helpers (same architecture: the compiler produces the flags), no
  `rd8/wr16`-style accessors.
- **Every function carries its original address** in a comment, e.g. `/* orig $002CF4 */`, so functions can be matched.
- **No inline asm** in the game. The loader (boot block, trackloader, decruncher) is the one exception: it is
  assembly in a separate `loader.asm`, assembled with `vasm` (`vasmm68k_mot`, in the `src/` flake shell).
- **The C runs from boot.** No original game code executes and there is no switching between original and C at run
  time. Boot method: a new bootable ADF whose boot block and loader come from `loader.asm`.
- **Verification is lockstep** against the recording `captures/ref100k`, from frame 0, comparing the screen and chip
  RAM every frame. No scripted key presses: the original loops its attract mode and does not reach a menu in 100,000
  frames, so the first goal is to match the original's first 100,000 frames, not to reach a menu.
- Use the real cores (ami9000, amiberry) as oracles. Unicorn was tried and dropped: it computed condition codes
  wrongly for `move.w #imm,abs`.

## Done

| What | Where |
| --- | --- |
| Project manifest, README, AGENTS, branch `game/gods` | `project.json`, `README.md`, `AGENTS.md` |
| Media (original PAL ADFs, WHDLoad archive, Kickstart 1.3) in ignored storage | `local/` |
| Run script: boots both disks (DF0/DF1 via `local/gods.m3u`); flags `load= snap= shots= frames= ram= record= compare= until= screen= cover=` | `host/gods.lua` |
| Reference recording, 100,000 frames, 170 MiB (RAM 84 MiB + screens 83 MiB): per-frame screen hash and chip-RAM CRC, zstd XOR-delta RAM and screens, keyframe every 1,000 frames | `captures/ref100k/` (ignored) |
| Recorder / comparer / RAM extractor, shared and game-agnostic | `tools/amiga/frame_recorder.py` |
| Executed-code coverage over the 100,000 frames: 2,104 instructions | `analysis/coverage-100k.txt` |
| Resident-code analysis: stable instruction set, function partition | `scripts/intro_analysis.py` |
| FTP upload helper (anonymous, writable) | `scripts/ftp.py` (framework level) |
| `ami.call` oracle skeleton | `host/liftcheck.lua` |

Verified facts:

- The recording is **deterministic**: 12,000 frames re-ran with identical screen hashes and RAM CRCs.
- Unattended, the original shows the intro, then loops the attract sequence (hero story, credits, GODS title). No
  menu appears within 100,000 frames; snapshots at frames 11,000, 55,000 and 100,000 are in `local/save/`.
- **Resident game code** is 1,472 instructions in 42 functions at `$0718–$3A60`, unchanged from frame 1,729 to the
  end of the recording. It calls nothing outside itself (no Kickstart calls). External entries: `$0B12` (main program,
  reached by `jmp` from `$0084`), `$1DD6` (level-3 interrupt handler, vector `$6C`) and `$0718` (an `rts` stub).
- **Loader code** (632 further executed instructions) is overwritten by the game and lives in clusters at
  `$0084–$0900`, `$1566–$1650`, `$59E8`, `$76DFC` and `$7FF00`. It is a raw-MFM trackloader with its own disk DMA, a
  copy-protection check and a relocated decruncher called with `jsr (a0)`.
- On disk 1 the loader is in plain bytes: boot block code at byte offset `$0E`, loader stage at `$638`, trackreader at
  `$6DE`, MFM decoder at `$B0E`, decruncher at `$52A00`. So `loader.asm` can reproduce those sectors byte for byte.
- Disk 2 has no filesystem; both disks are read by raw sector offsets. Disk 1 track 1 is empty (the skipped
  protection track).

## Steps

1. **Understand the data and the functions.** Disassemble the 42 resident functions with capstone and work out the
   globals and structs. Learned so far:
   - `$2CC2` redraw flag, `$2CC4`/`$2CC6` frame counters (incremented in the level-3 interrupt).
   - `$1AF6` palette-table pointer; `$2940`, `$2944` display-buffer pointers (`$30D4` fills one with zero, `$30EE`
     copies `$2944` to `$2940`, 10,000 longs).
   - `$1B7C` blitter copy, `$2F1E` blitter wait, `$2CDE` waits for the frame counter to change.
   - `$2CF4` loads the palette and updates the counters; `$1DD6`/`$22E6` run the per-frame work.
2. **Decompile into `intro.c`**, leaves first, building the structs as they become clear. Each function: address
   comment, typed signature, plain C.
3. **Per-function checking (optional, to confirm).** Compile the C, load it into slow RAM and call it with
   `ami.call` beside the original with identical register/stack inputs, comparing the return value and changed chip
   RAM. `host/liftcheck.lua` is the starting point.
4. **Write `loader.asm`:** the boot block, trackloader and decruncher as vasm Motorola-syntax source, with the
   original sector bytes as the check. Add the small code that loads the compiled C and enters it.
5. **Build the bootable ADF:** a script writes the `loader.asm` output over the boot sectors, keeps the original
   data sectors, and adds the C binary to free sectors. Check where the C lives in memory: chip RAM is almost full
   at the title screen, so it probably lives in slow RAM (`$C00000`).
6. **Lockstep:** run the new ADF with `ami9000_host projects/gods/host/gods.lua ... compare=captures/ref100k`
   (a `--script-args` flag the script already supports). Fix the first differing frame and byte range, repeat. Mask
   dead stack below SP if the real stack lives elsewhere; keep the mask documented.

## Build (verified)

- `src/intro.c` (code), `src/intro.h` (data types and declarations), `src/intro_data.c` (the data, C initializers
  copied from the original's initial image, frame 1746 of the recording) and `src/intro.ld` (generated with
  `intro_data.c` by `scripts/gen_intro_data.py`).
- Compile each with `m68k-amigaos-gcc -m68000 -Os -ffreestanding -nostdlib -fdata-sections -c`, link with
  `m68k-amigaos-ld -m amiga -T intro.ld intro.o intro_data.o`. In a hunk executable an object's offset in the data hunk is its
  address, so `intro.ld` lays the data hunk out at the original addresses (`. = ADDR;`, `SUBALIGN(1)`); the loader loads
  it at address 0, copying only the listed objects (`src/intro.objects`) so the rest of the original image stays.
  The code hunk follows separately. All 23 data objects checked at their original addresses.
- Hunk sections are rounded up to 4 bytes, so variables that sit next to each other in the original are structs
  (`struct Vblank`, `struct MusicState`, `struct Graphics`, ...) that cover the same bytes.

## Lockstep status (latest)

Run: `python3 scripts/build_adf.py` then
`ami9000_host host/gods.lua --headless --script-args "rebuilt record=captures/rebuilt until=100400 screen=1"` and compare
the per-frame hashes with the reference at the constant offset the loader's extra disk reads cause (169 frames: found by
matching screen hashes, see `tools/amiga/frame_recorder.py`); `scripts/compare_events.py` compares the
game's variables.

- The C runs from **chip RAM** like the original: the machine is given 1 MB chip RAM (`puae_chipmem_size=2`) and the code
  sits at `$80000`, above the original's 512 KB (which is full).  The C's own stack is at `$90000`; the stack is not
  compared.  The CPU is the normal 7 MHz one.
- The Amiga-specific routines are the original's instructions in `src/intro_amiga.asm` (the user allowed assembly for them,
  besides `loader.asm`): the blitter work, screen and bitplane fills, the busy-wait loops, the vertical blank interrupt,
  `vbl_wait` and the music replayer.  `scripts/gen_intro_amiga_asm.py` generates it from the resident code of the recording; every
  instruction is assembled again at its original address and compared with the original bytes.  Struct field offsets in
  the assembly come from the compiler (`src/intro_offsets.c` -> `src/intro_offsets.i`, which `intro_amiga.asm` includes), so they follow
  `intro.h`.  Only C-callable
  wrappers (arguments on the stack, `d2-d7/a2-a6` preserved) were added.  Two deliberate differences: the interrupt saves
  `d0`/`d1` with `movem.l` (the original's `movem.w` sign-extends them, which C does not survive), and `lea d16(pc),a6` of
  the music code is `lea abs.w,a6` (same cycles, the data is in another hunk).
- Why assembly: gcc's code for the blit routine, the glyph drawing, the music and the clears took 30% longer than the
  original's; the intro is timed to the frame (the first scroll step has to end before the next interrupt), so frames
  diverged.  Run from fast RAM or on a faster CPU the C was fast enough, but the original's own loops in chip RAM are slower
  than that, so one frame flip fell on the other side of an interrupt.  With the original's instructions in chip RAM the
  timing is the same cycle for cycle.
- **Scope (agreed with the user):** the lockstep covers the game's own frames, from the original's entry into the game
  (`$000B12`, reference frame 1,746) to frame 100,000.  The Kickstart boot and the original loader before that (reference
  frames 0-1,727) cannot match index for index on a 1 MB chip machine: Kickstart sizes and tests the extra RAM, which changes
  its start-up timing (the original disk on 1 MB chip first differs from the reference at frame 56; on 512 KB it matches
  frame for frame), and the C's loader hook reads its blob from disk.  Those frames are reported as not comparable.
- **Frames:** reference frames 1,728-99,999 (98,272 frames) compared against the C build at the constant boot offset (169
  frames), 0 differ.
- **Memory:** `scripts/compare_events.py` compares the pinned data objects (sizes from `build/intro_data.o`: the music module,
  the graphics, the palette and blitter state, both screens, the music and channel state, the clocks and the frame counter)
  at the program's own points in time, not at the end of each frame.  An event is the entry of `vbl_wait` (the original's
  `$002CDE`), recorded with `gods.lua record=DIR hook=ADDRESS`: the interrupt of the frame has finished and the program has
  done its work, so the memory depends on how far the program has got, not on which cycle the end of the frame falls on.
  (At the end of each frame the recorder catches the interrupt, the fade and the blitter half way; compared there, a few
  hundred frames of 100,000 differ by that jitter.)  **All 44,258 events of the 100,000 frames match** (`captures/events_ref`
  from the original against `captures/events_c`).  Not compared: the stack, the vectors (the C handler's address is in them),
  the loader's buffers and the code.
- The first pass of the intro had one tick of difference for 31 events until the C kept interrupts masked as the original
  does: its `sr` stays `$2700` until `move #$2000,sr` at `$000C4A` (`interrupts_on()` in `intro_amiga.asm`), so the vertical blank
  that came in during the start-up delay is pending and is taken as soon as they are enabled.
- Still to do: the from-scratch ADF (own boot block, only the C and its data), decompile `game_start` (the fire button).

## The menu (next part of the game)

After a fire button (here the space bar too) the intro asks the loader for the next part: the original menu program, which
the loader reads and decrunches over the intro.  It is a different program, about twice the size of the intro.

- Recorded: `captures/menu_ref` (the original, left mouse button pressed at frame 2,700, 14,000 frames, RAM per frame; the
  menu is on show from about frame 5,700), `analysis/coverage-menu.txt` (executed instructions, 5,381 of them with the intro's
  and the loader's).  Make it again with `gods.lua --headless --script-args "click=2700 until=14000 record=DIR screen=500 cover=PATH"`.
- `scripts/menu_analysis.py` -> `analysis/menu-functions.txt`: 2,858 stable instructions (resident and unchanged while the menu
  is on show), 107 flow components, 128 entry points; none of it equals the intro's code at the same address.  The largest
  pieces: `$0500-$6440` (413 instructions, its own region of low memory), the screens' code around `$16DF8` and `$1CF26-$1E4FA`
  (about a thousand instructions), `$1FAF6-$20344`, `$39F0A-$3A376`.
- How it will run in the C build: the same way as the intro.  The loader still reads the menu program (code and data) from
  disk into memory; the trampoline's second entry (now `jmp $84`) will enter the C menu instead, which uses the data that is
  in memory at the original addresses (pinned by the linker script) and ignores the original code.

### The menu handling (goal: decompile it)

"Menu handling" is the code that runs while the menu is on show and used: the attract screens (menu, top ten), the selection
moved with the joystick, and the password screen with letters picked: 1,453 instructions in 58 flow components
(`analysis/coverage-menu-handling.txt`; `scripts/menu_dis.py --todo` lists what is not decompiled).  What runs when Begin Quest
starts the game is separate (`analysis/coverage-game-start.txt`, the loader and the game's set-up): functions decompiled
from it are checked in their own scenario.  To use the menu in the emulator the joystick has to be in port 1:
`--opt puae_joyport_order=2143` (the default puts the pad on the mouse port), and `gods.lua` has `joy=F:DIRS`, `move=`,
`click=`, `press=` to drive it; the host got `ami.joystick{up=, down=, left=, right=, fire=}`.

### Decompiling the menu: how it is checked

- The menu program's C is split by what the code does: `disk.c` (the disk layer: MFM, track buffer, sector copy), `alloc.c`
  (memory), `decrunch.c` (the unpacker), `input.c` (buttons, joystick, mouse), `screen.c` (buffers, colour shadow table, copper
  list search, blitter wait); `menu.c` has what is not placed yet.  Each module has its header, `menu.h` includes them all and
  `menu_types.h` has the types of the data.  Its its data is declared from `src/menu.symbols` (name, address, type;
  `scripts/gen_menu_symbols.py` writes `menu_symbols.h` and `menu.ld`, so the C names no address).  `scripts/build_menu.py`
  builds the C into a blob at an address in the RAM above the original's 512 KB.
- `host/menu_oracle.lua` (driven by `scripts/check_menu.py`) checks each decompiled function against the original **on the
  running original**: it hooks the original's entry, saves the machine state, runs the original routine with `ami.call`,
  puts the memory back, runs the C function on the same state with the arguments in the registers the original has them in,
  and compares the result register and all of chip RAM, leaving out what the chips write meanwhile (two runs of the original
  show which addresses; the disk DMA buffer).  Then the state is put back, so the program goes on.  The functions to check are
  listed in `host/menu_cases.lua` (the registers the arguments and the result are in).
- The first batch, 15 leaf functions of the engine layer (MFM decode and encode, the unpacker, mouse and joystick polling,
  buffer copies, the colour shadow table, ...), match on 24 calls each spread over the first 9,000 frames.
- The menu handling is decompiled completely (`menu_dis.py --todo` lists nothing): the screens are in `ui.c` (`menu_main` $63A2,
  `menu_screen_draw`, `top_ten_screen`, `password_screen`), the message fade in `message.c`, the fades and the frame tick in
  `display.c`, the sound system (Paula driver, sequencer) and the vertical blank interrupt are generated asm in
  `menu_amiga.asm`.  Code that did not run in the menu (the disk load of the background, the rest of the password route,
  Begin Quest and Continue) is not there; the first two are `load_background` and `password_screen_setup` guards.
- What waits for the vertical blank cannot be checked by the oracle.  `host/menu_takeover.lua` (compared by
  `scripts/compare_takeover.py`) runs the original from the savestate, and in the other modes hands the main thread to
  the C `menu_main` when the original reaches $63A2 (`mode=c`), or also the interrupt vectors (`mode=irq`: C `vbl_irq`,
  `audio_irq`, `frame_tick`, `input_poll`, the sound code), and records per frame the checksums of the screens, the copper
  lists, variables and sound state, and per `screens_flip` the finished screen.  Result for the idle run (menu, 750 frames,
  fade, top ten, fade, menu): the screens drawn at all 381 flips are the same as the original's, and the sound state is
  the same each frame.  The C code needs 4 frames for what the original does in 3 (the same draw, flip, wait loop), so the
  runs are compared flip by flip, not frame by frame; the palette checksum differs at the flips next to a fade (4 of 381).
  A joystick run (down, fire, then the password screen) gives the same first picks; later picks depend on how many loop
  iterations a held direction gets, which differs with the speed.
- The file loading layer is in too (`MENU_COVERAGE=coverage-menu.txt menu_dis.py --todo`, the run that loads the menu from
  the disks, lists what is left: the code at $2400 that loads the copy protection's stub and runs what it releases).  The
  drive control, the track reads and writes through the disk DMA, the MFM coding and the directory search ($01D4CA-$01E253)
  are generated asm (`file_layer()` in `gen_menu_amiga_asm.py`, every instruction checked against the original's bytes);
  `file.c` has what is built on it: `load_file` ($01D3A6), `load_unpacked` ($002702: the packed file is "c" + name),
  `load_directory` ($002988: tocache.bin), `assets_load` ($01ECA2: pictures are cut into sprites with four planes) and
  `display_reset`; `boot.c` has the start-up ($039E50: chips, memory map, sound, first asset set).  The keyboard interrupt
  ($01E2B8) is generated asm too.  The data the asm keeps its state in is copied from the program as the loader leaves it
  (`host/menu_image.lua` writes `build/menu-image.bin`), not from the RAM of a later frame.
- Checked by running the original from a cold boot to the start of the menu ($006388) with the C versions in place of the
  original's (`host/menu_boot.lua`, shims from `scripts/build_glue.py` that give a C function the original's register
  interface and run it on a stack of its own): the RAM is the same as the original's at that point except for the
  interrupt vectors (C's handlers), the restart address, frame counters and a few hundred bytes of the sound state and the
  stack, which depend on the timing.
- What the program turned out to be: the menu is the game's whole engine (memory, disk and file layer, input, blitter and
  copper helpers) with the menu screens on top, so many leaves are library routines.

## Open questions

- Per-function checking against the original (step 3), or only the full lockstep run?
- Where the C binary is stored on the disk: free sectors of the original disk (the code runs in extra chip RAM at `$80000`).
- `move #imm,sr`, `trap` and `rte` need privileged instructions that plain C cannot express. Plain C replacements
  (interrupt-enable register writes, `__attribute__((interrupt))`) may not be byte-for-byte equivalent in effect.
  Revisit when the loader exists; the user must say if any asm beyond `loader.asm` is acceptable.

## Commands

From the repository root:

```console
# play (window); F12 quits
tools/engine9000/ami9000_host projects/gods/host/gods.lua
# compare the current build against the recording; stops at the first difference
tools/engine9000/ami9000_host projects/gods/host/gods.lua --headless --script-args "compare=$PWD/projects/gods/captures/ref100k until=100000"
# re-record (replaces captures/ref100k; about 7 minutes)
tools/engine9000/ami9000_host projects/gods/host/gods.lua --headless --script-args "record=$PWD/projects/gods/captures/ref100k until=100000 screen=1 cover=$PWD/projects/gods/analysis/coverage-100k.txt"
# the C build: press space (the interrupt takes it as the fire button), wait for the main menu and save a savestate
# (two ways: the script's press=, or the Lua control socket, which any client can drive while the host runs)
tools/engine9000/ami9000_host projects/gods/host/gods.lua --headless --script-args "rebuilt press=space@2700 snap=6000 snapname=gods-c-menu"
tools/engine9000/ami9000_host projects/gods/host/gods.lua --headless --control /tmp/gods.sock --script-args "rebuilt" &
python3 tools/ami9000_host/ctl.py /tmp/gods.sock 'ami.key("space", true)'   # ... later: 'ami.key("space", false)'
python3 tools/ami9000_host/ctl.py /tmp/gods.sock 'ami.snapshot_save("gods-c-menu-ctl.state")'
# start from the savestate (a file in local/save), in a window
tools/engine9000/ami9000_host projects/gods/host/gods.lua --script-args "rebuilt load=gods-c-menu.state"
# RAM of any recorded frame
python3 tools/amiga/frame_recorder.py --extract projects/gods/captures/ref100k FRAME out.bin
```

Python tools use `uv run python ...` (capstone and unicorn are in `pyproject.toml`). The m68k toolchain is
`nix develop projects/gods/src --command ...`.
