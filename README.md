# Gods

Gods (1991, The Bitmap Brothers / Renegade) on the Amiga. This project follows the
framework's [recomp-first workflow](../../docs/recomp/porting_game.md): obtain an
executable baseline by static recompilation, qualify it against original
execution, then replace translated routines with meaning-backed source.

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
