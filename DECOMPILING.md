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

A worktree has no `native/build`, `local` or `captures` and no host binary.  Make them first (once):

    W=$(pwd); R=/home/boran/git/amiga-decomp
    mkdir -p projects/gods/native/build
    cp $R/projects/gods/native/build/{menu-ram.bin,menu-image.bin,game-ram.bin,keyboard_patch.bin} projects/gods/native/build/
    ln -s $R/projects/gods/local projects/gods/local; ln -s $R/projects/gods/captures projects/gods/captures
    export AMI_HOST=$R/tools/engine9000/ami9000_host
    PY=$R/.venv/bin/python        # use this instead of `uv run python`

Every command is run from the worktree's root (`$W`); the scripts find their own files.  The build runs the compiler through `nix develop`.
Commit in your worktree (message ending `[skip ci]`); do not push.
