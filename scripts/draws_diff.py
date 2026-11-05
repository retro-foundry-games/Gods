#!/usr/bin/env python3
"""Compare the draw calls of the two sides saved with a lockstep stop (captures/stops/stop-fN.draws).

    python3 scripts/draws_diff.py captures/stops/stop-f5659

O lines: the original's draws (age 0 the last flip interval that is over, 1 the one in progress; the caller's address last); P lines: the port's, by iteration (the two newest).
Printed: the draws that only one side made in each interval (kind 0 glyph, 1 sprite; x, y, image)."""
import sys
from collections import Counter

path = sys.argv[1] if sys.argv[1].endswith(".draws") else sys.argv[1] + ".draws"
orig = {0: Counter(), 1: Counter()}
callers = {}
port = {}
for line in open(path):
    f = line.split()
    if not f:
        continue
    if f[0] == "O":
        age, kind, x, y, index, caller = int(f[1]), int(f[2]), int(f[3]), int(f[4]), int(f[5]), f[6]
        orig[age][(kind, x, y, index)] += 1
        callers[(kind, x, y, index)] = caller
    elif f[0] == "P":
        it, kind, x, y, index = map(int, f[1:6])
        port.setdefault(it, Counter())[(kind, x, y, index)] += 1
iterations = sorted(port)
newest = iterations[-2:] if len(iterations) >= 2 else iterations
for age, it in zip((0, 1), newest if len(newest) == 2 else [None] + newest):
    p = port.get(it, Counter())
    o = orig[age]
    only_o, only_p = o - p, p - o
    label = "previous interval/iteration" if age == 0 else "newest (in progress)"
    print(f"== {label}: original {sum(o.values())} draws, port {sum(p.values())} draws")
    for key, n in sorted(only_o.items()):
        print(f"   only the original: kind {key[0]} x {key[1]} y {key[2]} image {key[3]} x{n} (from ${callers.get(key, '?')})")
    for key, n in sorted(only_p.items()):
        print(f"   only the port:     kind {key[0]} x {key[1]} y {key[2]} image {key[3]} x{n}")
