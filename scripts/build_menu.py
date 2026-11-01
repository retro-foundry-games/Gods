"""Build the menu program's C (native/src/menu.c) as a blob at an address, for checking it against the original.

Makes native/build/menu (the Makefile compiles the modules of native/src/ and links them with menu.ld, the data names at their
addresses), relocates the code hunk to BASE and writes
native/build/menu.blob (the bytes), native/build/menu.sym (nm) and native/build/menu_syms.lua (function name -> address, for host/menu_oracle.lua).

    uv run python projects/gods/scripts/build_menu.py [BASE]        # BASE in hex, default C0000 (RAM above the original's 512 KB)
"""
from __future__ import annotations

import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_adf as B  # noqa: E402
import gen_menu_amiga_asm  # noqa: E402
import gen_menu_symbols  # noqa: E402

SRC, BUILD = B.SRC, B.BUILD




def main() -> int:
    base = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0xC0000
    BUILD.mkdir(exist_ok=True)
    gen_menu_symbols.main(base)
    gen_menu_amiga_asm.main()
    r = subprocess.run(["make", "-C", str(B.G.ROOT / "native"), "--no-print-directory", "build/menu"], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-3000:])
        return 1
    hunks = B.parse_hunks((BUILD / "menu").read_bytes())
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
