"""Build native/build/takeover_glue.bin: the shims that let host/menu_takeover.lua send the original's calls of a routine to its C
version.  Each shim has the original's register interface (arguments in registers, all registers kept, the negative flag
clear), pushes the arguments for the C function, calls it and returns to the original's caller.

    uv run python projects/gods/scripts/build_glue.py
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_menu as BM  # noqa: E402

GLUE_BASE = 0xEA000
# name: (C function, [registers pushed as arguments, last argument first])
SHIMS = {
    "load_file": ("load_file", ["d0", "a0"]),            # orig $01D3A6: d0 the destination, a0 the name
    "load_unpacked": ("load_unpacked", ["d0", "a0"]),    # orig $002702
    "load_directory": ("load_directory", []),            # orig $002988
    "display_reset": ("display_reset", []),              # orig $01D36E
    "drives_idle": ("drives_idle", []),                  # orig $01D966
    "display_setup": ("display_setup", []),              # orig $039FA6
    "chips_setup": ("chips_setup", []),                  # orig $039FDC
    "memory_map_init": ("memory_map_init", []),          # orig $03A060
    "display_state_reset": ("display_state_reset", []),  # orig $01F5C4
    "data_pointers_init": ("data_pointers_init", []),    # orig $01AA3C
    "sound_init": ("sound_init", []),                    # orig $03A18A
    "game_frame": ("game_frame", []),                    # orig $002C20: the loop's body once; then the loop goes on at its top
    "assets_load": ("assets_load", ["a4", "a3", "a2", "a1"]),   # orig $01ECA2: a3 and a4 come back as where the data and the table ended
}
TAIL = {"game_frame": ["        jmp $2c20.l"]}
AFTER = {"assets_load": ["        movea.l ($d3e).w,a3", "        movea.l ($d3a).w,a4"]}


def main() -> None:
    syms = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"^\s+(\w+) = 0x([0-9a-f]+),", (BM.BUILD / "menu_syms.lua").read_text(), re.M)}
    lines = []
    for n, (name, (func, regs)) in enumerate(SHIMS.items()):
        lines += ([f"        org ${GLUE_BASE:x}"] if n == 0 else ["        cnop 0,64"]) + [f"glue_{name}:", "        movem.l d0-d7/a0-a6,-(sp)",
                                                                                              f"        move.l sp,(${GLUE_BASE + 0x400:x}).l",
                                                                                              f"        lea ${GLUE_BASE + 0x10000:x},sp"]   # the C runs on a stack of its own: the original's is in memory it loads files to
        lines += [f"        move.l {r},-(sp)" for r in regs]
        lines += [f"        jsr ${syms[func]:x}", f"        lea {4 * len(regs)}(sp),sp" if regs else "",
                  f"        movea.l (${GLUE_BASE + 0x400:x}).l,sp", "        movem.l (sp)+,d0-d7/a0-a6", ] + AFTER.get(name, []) + TAIL.get(name, ["        move #0,ccr", "        rts"]) + [""]
    src = BM.BUILD / "takeover_glue.asm"
    src.write_text("\n".join(lines) + "\n")
    r = BM.B.nix(f"cd {BM.SRC} && vasmm68k_mot -Fbin -no-opt -quiet -o {BM.BUILD}/takeover_glue.bin {src} ")
    if r.returncode:
        sys.exit(r.stdout + r.stderr)
    (BM.BUILD / "takeover_glue.lua").write_text("return {\n  base = 0x%x,\n%s}\n" % (GLUE_BASE, "".join(
        f"  {name} = 0x{GLUE_BASE + 0x40 * n:x},\n" for n, name in enumerate(SHIMS))))
    print("glue:", ", ".join(SHIMS), "at", hex(GLUE_BASE))


if __name__ == "__main__":
    main()
