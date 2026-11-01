"""Read the original menu program: a function's disassembly with what it calls, who calls it and what is decompiled.

    uv run python projects/gods/scripts/menu_dis.py ADDR [ADDR ...]    # a function by its entry (hex), the code up to its last
                                                                        # executed instruction, executed instructions marked *
    uv run python projects/gods/scripts/menu_dis.py --todo             # the functions not decompiled yet, the leaves first

The code is the program's RAM while the menu is on show (build/menu-ram.bin: frame 8,000 of captures/menu_ref); the executed
instructions are those of analysis/coverage-menu-handling.txt (the menu on show, the selection moved, the password screen and
a start of Begin Quest); what is decompiled is read from the `orig $ADDR` comments of native/*.c.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "scripts"))
import intro_analysis as G  # noqa: E402
import menu_analysis as M  # noqa: E402

RAM = (PROJECT / "build" / "menu-ram.bin").read_bytes()
COVERAGE = PROJECT / "analysis" / "coverage-menu-handling.txt"
NATIVE = PROJECT / "native"
MENU_C = ["menu", "disk", "alloc", "decrunch", "input", "screen", "draw", "display", "message"]


def decompiled() -> dict[int, str]:
    """address -> function name, from the `orig $ADDR` comment that precedes each function of the menu's C."""
    out: dict[int, str] = {}
    for m in MENU_C:
        text = (NATIVE / f"{m}.c").read_text()
        for c in re.finditer(r"/\* orig \$([0-9A-Fa-f]+)[^\n]*(?:\n \*[^\n]*)*\*/\n(?:static )?[\w\s\*]*?\b(\w+)\([^)]*\)\n\{", text):
            out[int(c.group(1), 16)] = c.group(2)
    # the routines that are the original's instructions in menu_amiga.asm: `; void name(...), orig $ADDR` (or after the name)
    asm = (NATIVE / "menu_amiga.asm").read_text()
    for a in re.finditer(r"^; void (\w+)\([^\n]*orig \$([0-9A-Fa-f]+)", asm, re.M):
        out[int(a.group(2), 16)] = a.group(1)
    return out


def asm_ranges() -> list[tuple[int, int]]:
    """The code that menu_amiga.asm has from the original: `orig $FIRST-$LAST` in its comments."""
    text = (NATIVE / "menu_amiga.asm").read_text()
    return [(int(a, 16), int(b, 16)) for a, b in re.findall(r"orig \$([0-9A-Fa-f]{6})-\$([0-9A-Fa-f]{6})", text)]


def stable() -> dict[int, tuple[bytes, str, str]]:
    cov = M.coverage(COVERAGE)
    dec = G.decode(cov)
    return {pc: v[0] for pc, v in dec.items() if v and RAM[pc:pc + len(v[0][0])] == v[0][0] and any(v[0][0])}


def listing(lo: int, hi: int) -> list[tuple[int, str]]:
    tmp = PROJECT / "build" / "menu_dis.tmp"
    tmp.write_bytes(RAM[lo:hi])
    r = subprocess.run(["nix", "develop", str(NATIVE), "--command", "m68k-amigaos-objdump", "-D", "-b", "binary", "-m", "m68k:68000",
                        f"--adjust-vma={lo:#x}", str(tmp)], capture_output=True)
    out = []
    for line in r.stdout.decode().split("\n"):
        m = re.match(r"\s*([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(.*)$", line)
        if m:
            out.append((int(m.group(1), 16), m.group(3)))
        elif re.match(r"\s*[0-9a-f]+:\t[0-9a-f ]+$", line.rstrip()):
            pass
    return out


def graph(st):
    comps, entries, calls = G.functions(st)
    func_of = {}
    for root, pcs in comps.items():
        for p in pcs:
            func_of[p] = root
    entry_of = {}
    for root, pcs in comps.items():
        e = sorted(x for x in pcs if x in entries)
        entry_of[root] = e
    callees: dict[int, set[int]] = {}
    callers: dict[int, set[int]] = {}
    for pc, (_, mn, ops) in st.items():
        if mn.split(".")[0] in ("bsr", "jsr"):
            t = G.branch_target(mn, ops)
            if t in st:
                callees.setdefault(func_of[pc], set()).add(func_of[t])
                callers.setdefault(func_of[t], set()).add(func_of[pc])
    return comps, entries, func_of, entry_of, callees, callers


def show(addr: int, st, names) -> None:
    comps, entries, func_of, entry_of, callees, callers = graph(st)
    if addr not in func_of:
        print(f"{addr:x} is not an executed instruction")
        return
    root = func_of[addr]
    pcs = sorted(comps[root])
    print(f"; function component {pcs[0]:x}..{pcs[-1]:x}, {len(pcs)} instructions, entries {[hex(e) for e in entry_of[root]]}")
    print("; calls:", ", ".join(f"{min(comps[c]):x}" + (f"({names[x]})" if (x := min(comps[c])) in names else "") for c in sorted(callees.get(root, []))))
    print("; called from:", ", ".join(f"{p:x}" for c in callers.get(root, []) for p in sorted(comps[c])
                                       if any(G.branch_target(st[p][1], st[p][2]) in entry_of[root] for _ in [0]))[:300])
    lo, hi = pcs[0], pcs[-1] + 8
    done = set(st)
    for a, text in listing(lo, hi):
        if a > pcs[-1]:
            break
        mark = "*" if a in done else " "
        note = ""
        t = re.search(r"(?:bsr|jsr|bra|b\w\w|db\w+)\w*\s+0x([0-9a-f]+)", text)
        if t and int(t.group(1), 16) in names:
            note = f"   ; {names[int(t.group(1), 16)]}"
        print(f"{a:06x}{mark} {text}{note}")


def todo(st, names) -> None:
    comps, entries, func_of, entry_of, callees, callers = graph(st)
    rows = []
    for root, pcs in comps.items():
        if any(e in names for e in entry_of[root]) or any(a <= min(pcs) <= b for a, b in asm_ranges()):
            continue
        rows.append((len(callees.get(root, [])), len(pcs), min(pcs), entry_of[root]))
    done_n = sum(1 for p in st if p in names)
    print(f"{len(comps)} components, {len(rows)} not decompiled; leaves first:")
    for ncallee, n, a, e in sorted(rows)[:60]:
        print(f"{a:06x} {n:4d} instr  calls {ncallee}  entries {[hex(x) for x in e[:3]]}")


def main() -> None:
    st = stable()
    names = decompiled()
    if len(sys.argv) > 1 and sys.argv[1] == "--todo":
        todo(st, names)
        return
    for a in sys.argv[1:]:
        show(int(a, 16), st, names)
        print()


if __name__ == "__main__":
    main()
