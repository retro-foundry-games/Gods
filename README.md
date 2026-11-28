# Gods

Gods (1991, The Bitmap Brothers / Renegade) on the Amiga, reconstructed in C: the intro and the game, as two programs that run on the Amiga and, for a
Linux desktop and the browser, on SDL3. The method is the `amiga-porting` skill (`~/git/retrofoundry/amiga_porting`, `skill/SKILL.md`).

## Branches

- `develop` is this whole repository: the port in `native/` and the tools that check it against the original.
- `master` is exactly `git subtree split --prefix=native` of `develop`: the port alone, which is what is published (and what GitHub Pages is built from).

Publishing: `git subtree push --prefix=native origin master`. A pull request merged on `master` is brought onto `develop` with `git am --directory=native` of its patch.

## Layout

    native/     the reconstruction (see native/README.md and native/src/sdl/README.md)
    host/       ami9000 Lua scripts: the original, the lockstep with the port, the per-function oracle
    tools/      generators of the checked-in sources and the check drivers
    local/      git-ignored: the disks, the Kickstart, savestates
    captures/   git-ignored: recordings, inputs, RAM dumps
    PLAN.md  DECOMPILING.md   plan, status and the method as applied to this game

Coverage and other reviewed reports are read from `analysis/`.

## Build

| | |
|---|---|
| Amiga | `make -C native` builds `build/intro` and `build/game`; `make -C native adf` makes a disk |
| SDL (desktop) | `make -C native sdl` builds `build/sdl/gods-sdl` (the game) and `gods-sdl-dev` (the lockstep and the tests) in `native/` |
| Browser | `nix build ./native#web`; `python3 native/scripts/serve_web.py result` serves it with the headers the page needs |

The toolchain is the flake of `native/`. `native/src/sdl/README.md` describes the port, its options and its layout in memory.

## Checking against the original

The emulator is `ami9000` of the `amiga_porting` repository, expected beside this one (`../amiga_porting/ami9000/build/ami9000_host`; build it as its
README says). `local/` holds `system/kick34005.A500` (Kickstart 1.3, CRC32 c4f0f55f), `gods-disk1.adf`, `gods-disk2.adf`, `gods.m3u`
(`printf 'gods-disk1.adf\ngods-disk2.adf\n' > local/gods.m3u`) and the savestates in `local/save/`.

    AMI=../amiga_porting/ami9000/build/ami9000_host
    python3 ../amiga_porting/ami9000/pidrun.py -- $AMI host/gods.lua                       the original, in a window
    python3 ../amiga_porting/ami9000/pidrun.py -- $AMI --scale 2 host/gods_lockstep.lua    the original beside the port (options at the top of the script)
    uv run python tools/check_menu.py [NAME ...]                                           per function, original against C

## License

Gods was written by The Bitmap Brothers and published by Renegade; this repository holds a reconstruction of its code in C and the tools used to check it against the original. The disks and the Kickstart are not part of it and are not distributed.

The reconstruction and the tools are free software: you can redistribute them and/or modify them under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version. See [LICENSE](LICENSE).

The pixel scaling code in `native/src/sdl/pixel_scale.*` builds on [ZXSC](https://github.com/binaryfoundry/zxsc) (MIT, Copyright (c) 2019 paul alexander welch); `native/src/sdl/third_party/ZXSC-LICENSE.txt` keeps its license.
