# Gods

Gods (1991, The Bitmap Brothers / Renegade) on the Amiga. This project follows the
framework's [recomp-first workflow](../../docs/recomp/porting_game.md): obtain an
executable baseline by static recompilation, qualify it against original
execution, then replace translated routines with meaning-backed source.

## Builds

- Amiga: `make -C projects/gods/native` (`build/intro` and `build/menu`, `make adf` for a disk).
- Desktop SDL port: `make -C projects/gods/native sdl` builds `build/sdl/gods-sdl` (`--disks DIR`, the folder with `gods-disk1.adf` and `gods-disk2.adf`, default the
  working folder; `--help`) and `gods-sdl-dev` (the lockstep with the original and the tests). The port keeps its own `hiscores.bin` in the working folder.
- Browser (WebAssembly): `make -f projects/gods/native/src/sdl/Makefile.web` or `nix build projects/gods/native#web` makes `gods.html`, `gods.js` and `gods.wasm`.
  `python3 projects/gods/native/scripts/serve_web.py [DIR [PORT]]` serves them (default `build/web`, port 8080) with the headers the page needs: the game runs on a thread
  (a web worker), which wants cross-origin isolation, so a static host has to send `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`.
  The page asks for the two ADF files (they stay in the tab); the high scores and level codes are kept in the browser (IndexedDB). Arrows and Space; Esc quits a game to the menu.
  There is no linker script in wasm: the program's variables are macros for the memory at their addresses (`src/sdl/gen_guest.py`, `guest_vars_web.h`) and the program is linked above
  that memory (`--global-base`, no stack in front of the data).

## Status

Scaffold only. No media has been identified, no recording made and no claim of
recompilation or parity exists yet.

## Next steps

1. Place the supplied disk images in ignored `local/`; inventory them
   (hashes, variant, disk count) in `analysis/media_inventory.json`.
2. Write `local/gods.uae` and `local/system/` (Kickstart) for the machine the
   game actually needs; correct `project.json` memory regions from that setup.
3. Establish the loader path (bootblock, custom trackloader, decrunch) per
   [decrunching](../../docs/recomp/decrunching.md), and record the resident-image
   entry point in `recomp/gods.toml`; add `"recomp_manifest"` to `project.json`.
4. `python -m amiga_decomp --project gods run recompile analyze`

Select the project explicitly: `python -m amiga_decomp --project gods info`.
For storage-gated pushes, run
`git config --local --add amigaDecomp.storageProject gods`.
