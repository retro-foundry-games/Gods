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
  The program's memory is one struct (`struct Guest`, `native/src/guest_struct.h`, generated from `menu.symbols` by `scripts/gen_guest_struct.py`): every variable is a member at the offset of its
  original address, so the Amiga build links the struct at address 0 and the SDL and browser builds have it as an ordinary object (plus the program's heap). The sources say `guest.NAME`.
  A save state is that block (`guest_state_write/read` in `sdl/guest.c`); the lockstep resumes the port from the original's RAM (`ram=f5-K.ram`: copied in at the offsets, the words made native and
  the addresses of the data made the block's own) or from the port's own save (`.port`).

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
