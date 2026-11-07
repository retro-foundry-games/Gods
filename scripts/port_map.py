#!/usr/bin/env python3
"""A map of how much of the original program (bm.2: the menu and the game, $2000 to $21000) has a C counterpart.

The function entries are the targets of `jsr abs.l` and `bsr.w` in a RAM image of the original plus every `orig $ADDR` of the C sources (routines
the program reaches through tables and pointers); a function counts as ported when an `orig $ADDR` comment names an address inside it (or a range
`orig $A ... $B` covering it).  It is an estimate: the comments are the record, and an entry the scan found in data counts too.

    python3 projects/gods/scripts/port_map.py [RAMFILE]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
args = [a for a in sys.argv[1:] if not a.startswith("--")]
RAM = Path(args[0]) if args else ROOT / "captures/stops/stop-f212.ram"
LO, HI, BAND = 0x2000, 0x21000, 0x1000

ram = RAM.read_bytes()
entries = set()
for m in re.finditer(rb"\x4e\xb9\x00([\x00-\xff])([\x00-\xff])([\x00-\xff])", ram[:HI]):   # jsr abs.l
    t = m.group(1)[0] << 16 | m.group(2)[0] << 8 | m.group(3)[0]
    if LO <= t < HI and t % 2 == 0:
        entries.add(t)
for i in range(LO, HI - 4, 2):   # bsr.w
    if ram[i] == 0x61 and ram[i + 1] == 0x00:
        d = int.from_bytes(ram[i + 2 : i + 4], "big", signed=True)
        t = i + 2 + d
        if LO <= t < HI and t % 2 == 0:
            entries.add(t)

orig = set()
ranges = []
for path in list((ROOT / "native/src").glob("*.c")) + list((ROOT / "native/src/sdl").glob("*.c")) + list((ROOT / "native/src/amiga").glob("*.c")):
    text = path.read_text(errors="replace")
    for m in re.finditer(r"orig \$([0-9A-Fa-f]{5,6})((?:[^*\n]{0,60}?(?:to|and|\.\.|-)\s*\$([0-9A-Fa-f]{5,6}))?)", text):
        a = int(m.group(1), 16)
        if LO <= a < HI:
            orig.add(a)
            if m.group(3):
                b = int(m.group(3), 16)
                if a < b < a + 0x2000:
                    ranges.append((a, b))
entries |= orig
ordered = sorted(entries)
covered = set()
for a in orig:
    j = max((e for e in ordered if e <= a), default=None)
    if j is not None:
        covered.add(j)
for a, b in ranges:
    covered |= {e for e in ordered if a <= e <= b}

size = {}
for k, e in enumerate(ordered):
    nxt = ordered[k + 1] if k + 1 < len(ordered) else HI
    size[e] = min(nxt - e, 0x800)

print(f"function entries found: {len(ordered)}; with a C counterpart: {len(covered)} ({100 * len(covered) // len(ordered)}%)")
tot = sum(size.values())
done = sum(size[e] for e in covered)
print(f"their bytes (each capped at 2 KB): {done} of {tot} ({100 * done // tot}%)\n")
print(" band         entries ported  ported bytes %    map")
for b in range(LO, HI, BAND):
    es = [e for e in ordered if b <= e < b + BAND]
    if not es:
        print(f" ${b:05x}-${b + BAND - 1:05x}   -")
        continue
    p = [e for e in es if e in covered]
    by = sum(size[e] for e in es)
    bp = sum(size[e] for e in p)
    pct = 100 * bp // by if by else 0
    print(f" ${b:05x}-${b + BAND - 1:05x}  {len(es):5} {len(p):6}   {pct:3}%   " + "#" * (pct // 5) + "." * (20 - pct // 5))

if "--missing" in sys.argv:
    miss = [e for e in ordered if e not in covered and e < 0x1c000]
    print(f"\nnot ported below $1C000 ({len(miss)}): " + " ".join(f"${e:05x}" for e in miss))
