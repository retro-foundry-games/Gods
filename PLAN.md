# Gods: plan and status

Gods (Bitmap Brothers, 1991, PAL original disks, Kickstart 1.3 A500). The user's latest direction overrides
anything older here; see also [AGENTS.md](AGENTS.md).

## Direction (from the user)

- The game is **one C file**, `native/gods.c`, built with `m68k-amigaos-gcc` from the `native/` flake
  (`nix develop --command ...`), for the A500 (68000).
- It is **real decompiled C**: typed structs and globals at the original addresses, real parameters and return
  values. No emulated register file, no flag/`op_` helpers (same architecture: the compiler produces the flags), no
  `rd8/wr16`-style accessors.
- **Every function carries its original address** in a comment, e.g. `/* orig $002CF4 */`, so functions can be matched.
- **No inline asm** in the game. The loader (boot block, trackloader, decruncher) is the one exception: it is
  assembly in a separate `loader.asm`, assembled with `vasm` (`vasmm68k_mot`, in the `native/` flake shell).
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
| Resident-code analysis: stable instruction set, function partition | `scripts/gods_analysis.py` |
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
2. **Decompile into `gods.c`**, leaves first, building the structs as they become clear. Each function: address
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

- `native/gods.c` (code), `native/gods.h` (data types and declarations), `native/data.c` (the data, C initializers
  copied from the original's initial image, frame 1746 of the recording) and `native/gods.ld` (generated with
  `data.c` by `scripts/gen_gods_data.py`).
- Compile each with `m68k-amigaos-gcc -m68000 -Os -ffreestanding -nostdlib -fdata-sections -c`, link with
  `m68k-amigaos-ld -m amiga -T gods.ld gods.o data.o`. In a hunk executable an object's offset in the data hunk is its
  address, so `gods.ld` lays the data hunk out at the original addresses (`. = ADDR;`, `SUBALIGN(1)`); the loader loads
  it at address 0, copying only the listed objects (`native/gods.objects`) so the rest of the original image stays.
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
- The Amiga-specific routines are the original's instructions in `native/amiga.asm` (the user allowed assembly for them,
  besides `loader.asm`): the blitter work, screen and bitplane fills, the busy-wait loops, the vertical blank interrupt,
  `vbl_wait` and the music replayer.  `scripts/gen_amiga_asm.py` generates it from the resident code of the recording; every
  instruction is assembled again at its original address and compared with the original bytes.  Struct field offsets in
  the assembly come from the compiler (`native/offsets.c` -> `native/gods_offsets.i`, which `amiga.asm` includes), so they follow
  `gods.h`.  Only C-callable
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
- **Memory:** `scripts/compare_events.py` compares the pinned data objects (sizes from `build/data.o`: the music module,
  the graphics, the palette and blitter state, both screens, the music and channel state, the clocks and the frame counter)
  at the program's own points in time, not at the end of each frame.  An event is the entry of `vbl_wait` (the original's
  `$002CDE`), recorded with `gods.lua record=DIR hook=ADDRESS`: the interrupt of the frame has finished and the program has
  done its work, so the memory depends on how far the program has got, not on which cycle the end of the frame falls on.
  (At the end of each frame the recorder catches the interrupt, the fade and the blitter half way; compared there, a few
  hundred frames of 100,000 differ by that jitter.)  **All 44,258 events of the 100,000 frames match** (`captures/events_ref`
  from the original against `captures/events_c`).  Not compared: the stack, the vectors (the C handler's address is in them),
  the loader's buffers and the code.
- The first pass of the intro had one tick of difference for 31 events until the C kept interrupts masked as the original
  does: its `sr` stays `$2700` until `move #$2000,sr` at `$000C4A` (`interrupts_on()` in `amiga.asm`), so the vertical blank
  that came in during the start-up delay is pending and is taken as soon as they are enabled.
- Still to do: the from-scratch ADF (own boot block, only the C and its data), decompile `game_start` (the fire button).

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
`nix develop projects/gods/native --command ...`.
