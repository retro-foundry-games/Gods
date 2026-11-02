# Decompiling the game run (how it is done here)

The game run is the code that runs while level 1 is played.  Its executed instructions are `analysis/coverage-game.txt`
(a scripted run and a human run); the human run's input is `captures/input/level1-run1.txt`.  What is decompiled is the executed
code (a branch to code that did not run is marked `/* goes on at $ADDR */` and ends the C function).

## The list of what is left

    MENU_RAM=game-ram.bin MENU_COVERAGE=coverage-game.txt MENU_TODO_LIMIT=500 uv run python projects/gods/scripts/menu_dis.py --todo

lists the components (a component is code connected by branches) that no C has yet, small ones first.
`... menu_dis.py 0xADDR [0xADDR ...]` shows the disassembly of the component an address is in, with what it calls and (starred `*`)
which of its instructions ran.  `native/build/game-ram.bin` is the program's RAM in the middle of the run.

## Writing a function

* C only, in the module you are given (`native/src/game_X.c` with its header `game_X.h`; `game.c` is not yours).  No inline asm.
  stdint types.  A comment `/* orig $ADDR: what it does */` before every function; `orig $A, $B and $C` when one function has several
  entries.  Names by what the code does; a provisional name is `fn_` and the address.  A small helper that cannot be a C function
  (it passes its results in registers or the condition codes, or jumps out) is written into its caller (say so in the caller's
  comment: `/* inlined: $12BDA */` and the tool counts it as done).
* Data: every variable the original keeps at an absolute address is a line in `native/src/menu.symbols`: `name 0xADDRESS type %s  comment`
  (the `%s` is where the name goes), or `table name 0xADDR uint8_t|uint16_t|uint32_t COUNT  comment` for constant data (tables are
  copied from `native/build/game-ram.bin` into `menu_data.c`; use a plain variable for something the code changes).  Before you add
  a line, search the file for the address (`grep -i 0x0000ABCD`): reuse the name that is there.  Types of records are in
  `menu_types.h` (add yours).  Use `volatile` for what an interrupt changes.  The generated files (`menu_symbols.h`, `menu.ld`,
  `menu_data.c`, `amiga/menu_amiga.asm`) are made by the build; do not edit them by hand.
* The program is a 68000 program: arithmetic is on 16-bit words most of the time (`addw`, `subqw`); signed comparisons (`blt`, `bgt`,
  `bge`, `ble`) are signed, `bcs/bcc/bhi/bls` unsigned.  Copy the width exactly.  `d0`-`d7` arguments in a function's registers become
  C parameters; results that the caller uses come back as the return value.

## Checking it against the original

1. `uv run python projects/gods/scripts/build_menu.py` builds the C (all modules) into `native/build/menu.blob` and fails on a C error.
2. Add a case for each function to `host/menu_cases.lua`, inserted before the final `}`:
   `{ name = "function_name", orig = 0xADDR, args = { "d0.w", "a0" }, ret = "d0.w", blitter = true, scenario = "game" },`
   (`args`: the registers the arguments come in, `.w` and `.b` for the low part; `ret`: the register of the result if the caller uses it;
   `blitter = true` when the function draws).  The oracle (`host/menu_oracle.lua`) hooks the original's entry in the running original,
   calls the original and the C on the same machine state, and compares the result register and all of chip RAM.
3. `uv run python projects/gods/scripts/check_menu.py function_name ...` runs one oracle run for each (a minute or two each) and must
   say `ok 24 differ 0` (or all the calls, when there are fewer).  A DIFF names the memory bytes that differ: that is your error.
   Run several functions in one command; it can go on in the background.
4. A function whose results are the condition codes or whose entry is in the middle of another cannot be called by the oracle: check it
   inside its caller, or by the lockstep run below.
5. The lockstep run drives the whole game with a C routine in place of the original's and compares what is shown:
   `host/menu_takeover.lua mode=game` (the loop body `game_frame` is C, its routines the original's); this is the lead's to run.

## The tools of the checkout in a worktree

A worktree has no `native/build`, `local` or `captures` and no host binary.  Make them first (once).  COPY the data, never
symlink it into the tree (a symlink named `local` or `captures` replaced the real directories when it was merged once):

    W=$(pwd); R=/home/boran/git/amiga-decomp
    mkdir -p projects/gods/native/build projects/gods/captures/input
    cp $R/projects/gods/native/build/{menu-ram.bin,menu-image.bin,game-ram.bin,keyboard_patch.bin} projects/gods/native/build/
    cp -r $R/projects/gods/local projects/gods/local
    cp -r $R/projects/gods/captures/ref100k projects/gods/captures/ 2>/dev/null; cp $R/projects/gods/captures/input/* projects/gods/captures/input/
    export AMI_HOST=$R/tools/engine9000/ami9000_host
    PY=$R/.venv/bin/python        # use this instead of `uv run python`

Every command is run from the worktree's root (`$W`); the scripts find their own files.  The build runs the compiler through `nix develop`.
Commit in your worktree (message ending `[skip ci]`); do not push and do not commit anything in `local/`, `captures/` or `native/build/`.

## What a finished function needs to do (lessons)

* A function that stops at "goes on at $ADDR" is only the executed part.  The game is played with more branches than the coverage
  shows (`analysis/coverage-game.txt` is a scripted run plus a human run of level 1, `captures/input/level1-run1.txt`), so a partial
  function cannot stand in for the original in the C game loop: the lead keeps calling the original for those.  Write the
  continuations too when you can read them (the disassembly tool shows unexecuted code without the star; read it with
  `kuna`-free methods: `menu_dis.py 0xADDR` after adding that run to the coverage, or capstone on `native/build/game-ram.bin`).
* Some `rts`-only routines are patched at run time (a `move.w #$4E75,addr` over a first word turns a routine off); do not treat
  every one-instruction routine as empty.

## Begin Quest (the code from the menu's fire to the first frame of the game loop)

`native/src/game_h.c` has it all in C (`begin_quest`, which `ui.c` calls instead of `original_begin`): the level's set-up ($651E,
$2452 to $2C08 and the routines it calls: the files, the map, the pictures, the copper list, the state, two frames drawn behind
the loading screen).  How it was made and checked:

* Its coverage: `host/gods.lua` from a snapshot of the C disk just before the fire (a run from the boot with `press=space@2800
  snap=6440 snapname=gods-c-menu shots=DIR frames=6440`), then `rebuilt joy1 load=gods-c-menu.state joy=10:fire,30:none
  coverstop=ADDR cover=PATH until=3100`.  `coverstop=` writes the coverage and quits when the program first reaches ADDR (`2c20`
  for the original's start of the loop, the C `game_loop` address of `native/build/menu_syms.lua` for the C); `ram=PATH` writes the RAM
  there too, which `MENU_RAM=... MENU_COVERAGE=coverage-begin.txt menu_dis.py --todo` wants (the original's loader overwrites
  parts of the program, so only the RAM at the end of the window has the code the window ran at its address).  Leave `press=` out of a
  cover run: it is only read by the runs without cover/record.  `analysis/coverage-begin.txt` is the original's code;
  `analysis/coverage-begin-c.txt` what of the original still runs after Begin Quest is C.
* Other flags added to `host/gods.lua` for this: `stopat=ADDR ram=PATH` (the RAM the first time ADDR is reached, then quit),
  `trace=ADDR[,..]` (the registers and the return address the first 6 times) and `watch=ADDR` (a word's changes, by frame).
  A savestate holds the C blob: after every build make a new one.
* The check of the whole: two builds of the disk, one with `original_begin()` and one with `begin_quest()` in `ui.c`, each run
  from its snapshot to the first frame of the loop (`stopat=2c20` and `stopat=<game_loop>`).  The 512 KB chip RAM of the two is the same
  except the interrupt vectors and the jump at $2C22 (the blob's addresses), the frame counters ($1D05A, $1E256) and the stack.
* The oracle (`scenario = "begin_quest"` in `scripts/check_menu.py`: the original from `gods-menu-1m.state`, the fire at frame 20,
  1500 frames) checks the leaves.  The loaders of the files, the copper list and what waits for the vertical blank are in the
  whole check only.
* A function that is only the way that ran says `goes on at $ADDR` where it stops: the other levels (`param_index` 2 to 4), the
  password start (`word_0266`), the other draw routines of the table at $0D9A and the mirrored or shifted draws of `image_draw_clipped`,
  the kinds 0, 1 and 6 at the end of a path ($18106).  A file that cannot be read does not come back (`load_file`), so the
  original's code for that is not written.

## Status of the game run (November 2)

- Executed code of the human level-1 run (analysis/coverage-game.txt, 4456 instructions) and of Begin Quest (analysis/coverage-begin.txt, 4683):
  `menu_dis.py --todo` lists nothing that is not decompiled, except the keyboard patch code at $1E426/$1E608 (not the game's: it is the memory patch
  of scripts/build_keyboard_patch.py).  Every written routine is checked by the oracle on every call of the replay (differ 0; the one remaining
  difference was in the oracle's own comparison of the patched keyboard bytes, now ignored with kbd=1).
- `game_frame` calls no original code except $30C6, $33D4 and $12E08, which the recording never reaches.  Branches of the written routines that
  the recording never reaches are marked `goes on at $ADDR` (or written from the disassembly without a check: the reports of the modules say which).
- Speed: with the game's modules built `-O2` (the Makefile used `-Os` for a while to fit the disk's cylinders) the C `game_frame` loop on the
  C disk runs 667 iterations in 2000 frames, exactly the original's (3 frames an iteration; with `-Os` it was 500, 4 frames).  Count with
  `gods.lua ... count=ADDR` (the original's loop is $2C20, the C one is `_game_frame` in build/menu.sym + $C0000).
- End-to-end check against the original, no replay needed: run the original disk (`onemb joy1`, fire pulses to the level) and the C disk with the
  same joystick script relative to the first iteration of the loop (L about 6195 and 7848), and compare screenshots at L+k: the scenes, scroll
  positions and enemies agree (scores a few points apart: the first iteration is only known to a few frames).  The takeover lockstep
  (`menu_takeover.lua mode=game`) still diverges from the flip ~70 (its glue is slower than the C disk): informative for the first flips only.
- Continue and the password route (`original_continue`, `original_after_password`) are still the original's code; levels 2 to 4 are not decompiled.
