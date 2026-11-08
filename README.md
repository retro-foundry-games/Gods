# Gods

A reconstruction in C of the Amiga game *Gods* (The Bitmap Brothers, 1991): the intro and the game, as two programs that run on the Amiga and, for a Linux desktop and the browser, on SDL3.

The game's variables are the members of one struct, `struct Gods` (`src/game.h`, generated from `src/game.symbols`), so its state can be saved and loaded as one block.  `src/game.h` holds the types, that struct and the declarations; `src/data.c` the original's constant tables.

## Build

| | |
|---|---|
| Amiga | `make` builds `build/intro` and `build/game` with `m68k-amigaos-gcc` (the flake's shell has it) |
| SDL (desktop) | `make sdl` builds `build/sdl/gods-sdl` (the game) and `build/sdl/gods-sdl-dev` (the options and tests); `make sdl-run` runs it |
| SDL without the original's memory layout | `make sdl MATCH_MEMORY_LAYOUT=0` (the struct without its padding) |
| Browser | `nix build .#web` (see `web/` and `src/sdl/README.md`) |

You need the original disk images `gods-disk1.adf` and `gods-disk2.adf` to play: they are not part of this repository.  `gods-sdl --disks DIR` names the folder; the default is the working folder.

`src/sdl/README.md` describes the SDL port.

## Layout

- `src/` the C of the game (`game.c`, `menu.c`, `boot.c`, ...), the generated `game.h`, `data.c` and `game.ld`, the intro's data
- `src/amiga/` what only the Amiga build has (the loader, the hardware, `memory.c`)
- `src/sdl/` the SDL3 port: the machine around the program, the sound mixer, the disks
- `web/` the page of the browser build

The generators (`../scripts/gen_game_symbols.py`, `gen_guest_struct.py`, `gen_game_amiga_asm.py`) and the tests that compare the port with the original running in an emulator live in the repository this was split from; the files they write are checked in here.

## License

GNU General Public License, version 2 or (at your option) any later version.  See `COPYING`.
