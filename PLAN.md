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
`ami9000_host host/gods.lua --headless --script-args "rebuilt compare=captures/ref100k [survey] until=N"`
(or `record=DIR` and compare the per-frame hashes). The rebuilt disk is the original disk with its loader patched to load
the C (see `scripts/build_adf.py`); its load takes a different time, so the rebuilt run is shifted by a constant number of
frames (369 at 7 MHz, found by matching screen hashes).

- With that shift the rebuilt game matches the original's screens **exactly for 1,312 consecutive frames** (credits, logo,
  hero picture and the fades) and then diverges at original frame 3545, the start of the story's text scroll.
- The C touches the chips as the original does (blitter, BLTPRI, CPU fills, music delays); the first scroll step still ends
  about 4,000 chipset cycles (6% of a frame) later than the original's, so the flip lands a frame late.
- `ami.cycles()` (new in the host) counts chipset clock units, 70,937 to a PAL frame. Per-function costs, same window:
  the C's blit routine takes 1,447 per call against 1,096, `glyph_draw` 304 against 170, the bitplane clear 21,000 against
  16,000 (gcc will not emit `move.l d,(a0)+` for unrolled stores).
- The original run on a 14 MHz cycle-exact CPU reproduces the 7 MHz recording frame for frame (2,200 frames, 0 differ), so
  a faster cycle-exact CPU (`--opt puae_cpu_compatibility=exact --opt puae_cpu_multiplier=4`) is a valid way to absorb the
  C's extra run time. On it the rebuilt game still diverges at the same place, with an irregular 1-2-3 frame scroll cadence,
  which points at a timing dependence in the rebuilt flip/scroll loop that the original does not have. Not yet explained.
- Memory is not compared yet: the compare tool has `--survey` and `--mask`, but the mask (vectors, stack zone, loader buffers,
  the C blob's neighbourhood) has not been written; the boot phase differs because the disk differs.
- Next: find the cadence difference at the start of the scroll (trace `scroll_y`, `vblank.saturating` and the flips in both
  on the fast machine), then the memory mask, then run the full 100,000 frames. The from-scratch ADF (own boot block, only
  the C and its data) is still to do and would remove the loader-patch offset.

## Open questions

- Per-function checking against the original (step 3), or only the full lockstep run?
- Where the C binary is stored on the disk and where the code hunk runs (steps 4 and 5): slow RAM is the candidate.
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
# RAM of any recorded frame
python3 tools/amiga/frame_recorder.py --extract projects/gods/captures/ref100k FRAME out.bin
```

Python tools use `uv run python ...` (capstone and unicorn are in `pyproject.toml`). The m68k toolchain is
`nix develop projects/gods/native --command ...`.
