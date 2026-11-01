"""Gods menu analysis: executed-code coverage of the original's main menu + its recording -> the menu's resident code and
its function structure (the intro's equivalent is gods_analysis.py).

The recording is `captures/menu_ref` (the original, space/fire pressed at frame 2,700, the menu on show from about frame
5,700; see PLAN.md for the command); the coverage is `analysis/coverage-menu.txt`.  An instruction is *stable* if its bytes
are the same at every sampled frame while the menu is on show, so the loader and decruncher, which are overwritten, are not
in it.  An instruction is *known* if the intro's coverage has the same instruction at the same address: it is code the
intro already has (the C versions exist).

    uv run python projects/gods/scripts/menu_analysis.py [out.txt]
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
sys.path.insert(0, str(REPO / "tools" / "amiga"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import gods_analysis as G  # noqa: E402
from frame_recorder import RefRam  # noqa: E402

REC = ROOT / "captures" / "menu_ref"
COVERAGE = ROOT / "analysis" / "coverage-menu.txt"
MENU_FRAMES = [6500, 8000, 9500, 11000, 12500, 13900]   # the menu on show, and later screens of its attract loop


def coverage(path: Path) -> dict[int, list[bytes]]:
    cov: dict[int, list[bytes]] = {}
    for line in path.read_text().split("\n"):
        if line:
            pc, raw = line.split()
            cov.setdefault(int(pc, 16), []).append(bytes.fromhex(raw))
    return cov


def main() -> None:
    cov = coverage(COVERAGE)
    decoded = G.decode(cov)
    ref = RefRam(REC, 1000)
    rams: dict[int, bytes] = {}
    for f in range(MENU_FRAMES[0] - 1, MENU_FRAMES[-1] + 1):
        ram = ref.get(f)
        if f in MENU_FRAMES and ram is not None:
            rams[f] = ram
    stable = {}
    for pc, variants in decoded.items():
        for v in variants:
            if all(r[pc:pc + len(v[0])] == v[0] for r in rams.values()):
                stable[pc] = v
                break
    intro = G.decode(coverage(G.COVERAGE))
    known = {pc for pc, v in stable.items() if any(v[0] == w[0] for w in intro.get(pc, []))}
    comps, entries, calls = G.functions(stable)
    out = [f"{len(stable)} stable instructions, {len(known)} also in the intro, {len(comps)} components, {len(entries)} entries"]
    callers: dict[int, set[int]] = {}
    for pc, (_, mn, ops) in stable.items():
        t = G.branch_target(mn, ops)
        if mn.split(".")[0] in ("bsr", "jsr") and t in stable:
            callers.setdefault(t, set()).add(pc)
    for root, pcs in sorted(comps.items()):
        ent = [e for e in pcs if e in entries]
        size = sum(len(stable[p][0]) for p in pcs)
        new = sum(1 for p in pcs if p not in known)
        out.append(f"{min(pcs):06x}-{max(pcs):06x} {len(pcs):4d} instr {size:5d} bytes new {new:4d} entries "
                   + ",".join(f"{e:x}" for e in ent[:6]) + (" callers " + ",".join(f"{c:x}" for c in sorted(callers.get(ent[0], []))[:4]) if ent and ent[0] in callers else ""))
    text = "\n".join(out) + "\n"
    if len(sys.argv) > 1:
        Path(sys.argv[1]).write_text(text)
    print(text[:6000])


if __name__ == "__main__":
    main()
