#!/usr/bin/env python3
"""Build the Gods ADF whose game is the C in native/src/: boot -> original loader -> C (no original game code runs).

    uv run python projects/gods/scripts/build_adf.py

1. compile intro.c and intro_data.c with m68k-amigaos-gcc and link them with m68k-amigaos-ld -m amiga and intro.ld: the data
   hunk is laid out at the original addresses, the code hunk follows;
2. turn the code hunk into a flat blob for slow RAM: apply its relocations (hunk 0, the data, is at address 0);
3. put a 16-byte trampoline in front of it (a stack in the extra chip RAM, jump to game_main; interrupts stay masked, as in the original, until the game enables them);
4. let gen_loader_asm.py patch the loader and write the disk image with the blob in free sectors.
The data needs no loading: it is the same as what the original loader decrunches to those addresses.
"""
from __future__ import annotations

import os
import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_loader_asm as L  # noqa: E402
import intro_analysis as G  # noqa: E402

SRC = G.ROOT / "native" / "src"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import intro_offsets  # noqa: E402

BUILD = G.ROOT / "native" / "build"
OUT_ADF = G.ROOT / "local" / "gods-c-disk1.adf"
TRAMPOLINE_SIZE = 34
STARTED_FLAG = 0x90010   # a word above the C's stack, outside the blob (the loader's hook copies the blob again)
STACK_TOP = 0x90000       # the C's own stack, in the extra chip RAM above the code


def nix(cmd: str) -> subprocess.CompletedProcess:
    return subprocess.run(["nix", "develop", str(SRC.parent), "--command", "sh", "-c", cmd], capture_output=True, text=True)


def parse_hunks(data: bytes):
    """A hunk executable: [(type, payload bytes or size, {target hunk: [offsets]})]."""
    pos = 0

    def u32():
        nonlocal pos
        v = struct.unpack_from(">I", data, pos)[0]
        pos += 4
        return v

    assert u32() == 0x3F3, "not a hunk executable"
    while (n := u32()) != 0:
        pos += 4 * n
    u32()
    first, last = u32(), u32()
    sizes = [u32() & 0x3FFFFFFF for _ in range(last - first + 1)]
    hunks = []
    for size in sizes:
        t = u32() & 0x3FFFFFFF
        n = u32() & 0x3FFFFFFF
        payload = data[pos:pos + 4 * n] if t in (0x3E9, 0x3EA) else n * 4
        if t in (0x3E9, 0x3EA):
            pos += 4 * n
        relocs: dict[int, list[int]] = {}
        while True:
            t2 = u32()
            if t2 == 0x3F2:
                break
            if t2 == 0x3EC:
                while (cnt := u32()) != 0:
                    target = u32()
                    relocs.setdefault(target, []).extend(u32() for _ in range(cnt))
            elif t2 == 0x3F0:
                while (ln := u32()) != 0:
                    pos += 4 * (ln & 0xFFFFFF) + 4
            elif t2 == 0x3F1:
                pos += 4 * u32()
            else:
                raise SystemExit(f"unexpected hunk block {t2:#x} at {pos:#x}")
        hunks.append((t, payload, relocs))
    return hunks


def main() -> int:
    BUILD.mkdir(exist_ok=True)
    intro_offsets.write()   # the struct field offsets intro_amiga.asm includes, from intro.h
    r = subprocess.run(["make", "-C", str(G.ROOT / "native"), "--no-print-directory", "build/intro"], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-3000:])
        return 1
    syms = {m.group(3).lstrip("_"): int(m.group(1), 16)
            for m in re.finditer(r"^([0-9a-f]{8}) (\w) (\S+)$", (BUILD / "intro.sym").read_text(), re.M)}
    hunks = parse_hunks((BUILD / "intro").read_bytes())
    assert len(hunks) == 2, f"expected a data hunk and a code hunk, got {len(hunks)}"
    (t0, data_hunk, _), (t1, code, relocs) = hunks
    base = L.SLOW_BASE + TRAMPOLINE_SIZE
    code = bytearray(code)
    n_rel = 0
    for target, offsets in relocs.items():
        add = {0: 0, 1: base}[target]          # hunk 0 is the data at address 0, hunk 1 is the code
        for off in offsets:
            old = struct.unpack_from(">I", code, off)[0]
            struct.pack_into(">I", code, off, old + add)
            n_rel += 1
    game_main = base + syms["game_main"]
    # the trampoline: interrupt mask 0, the original's stack, then the game
    tramp_src = BUILD / "trampoline.asm"
    # The stage's last jump goes here every time it loads something.  The first time it is the C game; later it is the part
    # of the game that the C asked the loader for (a fire button or the space bar), which the original runs from $0084.
    menu_syms = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"^\s+(\w+) = 0x([0-9a-f]+),", (BUILD / "game_syms.lua").read_text(), re.M)}
    menu_blob = (BUILD / "game.blob").read_bytes()
    menu_base = menu_syms["base"]
    tramp_src.write_text(f"        org ${L.SLOW_BASE:x}\n        tst.w ${STARTED_FLAG:x}.l\n        bne.b next_stage\n"
                         f"        move.w #1,${STARTED_FLAG:x}.l\n        movea.l #${STACK_TOP:x},a7\n        jmp ${game_main:x}.l\n"
                         f"next_stage:\n        jmp $84.l\n")
    r = subprocess.run(["nix", "develop", str(SRC.parent), "--command", "sh", "-c",
                        f"vasmm68k_mot -Fbin -no-opt -quiet -o {BUILD}/trampoline.bin {tramp_src}"], capture_output=True, text=True)
    tramp = (BUILD / "trampoline.bin").read_bytes()
    assert len(tramp) == TRAMPOLINE_SIZE, f"trampoline is {len(tramp)} bytes"
    blob = tramp + bytes(code)
    (BUILD / "intro.blob").write_bytes(blob)
    print(f"code hunk {len(code)} bytes at ${base:x}, {n_rel} relocations; game_main at ${game_main:x}; blob {len(blob)} bytes")
    return L.main(patched=True, adf_out=OUT_ADF, blob=blob, menu_blob=menu_blob, menu_base=menu_base, menu_entry=menu_syms["menu_entry"])


if __name__ == "__main__":
    raise SystemExit(main())
