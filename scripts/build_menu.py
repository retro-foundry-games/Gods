"""Build the menu program's C (native/menu.c) as a blob at an address, for checking it against the original.

Compiles menu.c, links it with menu.ld (the data names at their addresses), relocates the code hunk to BASE and writes
build/menu.blob (the bytes), build/menu.sym (nm) and build/menu_syms.lua (function name -> address, for host/menu_oracle.lua).

    uv run python projects/gods/scripts/build_menu.py [BASE]        # BASE in hex, default C0000 (RAM above the original's 512 KB)
"""
from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_adf as B  # noqa: E402
import gen_menu_amiga_asm  # noqa: E402
import gen_menu_symbols  # noqa: E402

NATIVE, BUILD = B.NATIVE, B.BUILD
FLAGS = "-m68000 -O2 -ffreestanding -fno-tree-loop-distribute-patterns -fno-tree-forwprop -fno-ivopts -nostdlib -fdata-sections -ffunction-sections"


MODULES = ['menu', 'disk', 'alloc', 'decrunch', 'input', 'screen', 'draw', 'display', 'menu_data']   # native/<name>.c


def main() -> int:
    base = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0xC0000
    BUILD.mkdir(exist_ok=True)
    gen_menu_symbols.main(base)
    gen_menu_amiga_asm.main()
    compile_all = " && ".join(f"m68k-amigaos-gcc {FLAGS} -Wall -Wextra -c {m}.c -o {BUILD}/menu_{m}.o" for m in MODULES)
    objects = " ".join(f"{BUILD}/menu_{m}.o" for m in MODULES) + f" {BUILD}/menu_amiga.o"
    r = B.nix(f"cd {NATIVE} && {compile_all} && vasmm68k_mot -Fhunk -no-opt -quiet -o {BUILD}/menu_amiga.o menu_amiga.asm"
              f" && m68k-amigaos-ld -m amiga -T menu.ld -Map {BUILD}/menu.map -o {BUILD}/menu.exe {objects}"
              f" && m68k-amigaos-nm {BUILD}/menu.exe > {BUILD}/menu.sym")
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-3000:])
        return 1
    hunks = B.parse_hunks((BUILD / "menu.exe").read_bytes())
    # the tables (a data hunk at address 0, never loaded: the bytes are in memory already) and the code, linked at `base`
    assert len(hunks) in (1, 2), f"expected the code hunk and perhaps a data hunk, got {len(hunks)}"
    _, code, relocs = hunks[-1]
    assert not relocs, "the code is linked at its address and needs no relocation"
    code = bytearray(code)
    (BUILD / "menu.blob").write_bytes(bytes(code))
    funcs = {m.group(3).lstrip("_"): int(m.group(1), 16) + base
             for m in re.finditer(r"^([0-9a-f]{8}) ([tT]) (\S+)$", (BUILD / "menu.sym").read_text(), re.M)}
    (BUILD / "menu_syms.lua").write_text("return {\n  base = 0x%x,\n  size = %d,\n%s}\n" % (
        base, len(code), "".join(f"  {name} = 0x{addr:x},\n" for name, addr in sorted(funcs.items()))))
    print(f"menu: {len(code)} bytes at ${base:x}, {len(funcs)} functions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
