"""Compare two runs of host/menu_takeover.lua: the first frame at which each recorded region differs, and which variables.

    uv run python tools/compare_takeover.py ORIG.txt C.txt [MAXSHOW]
"""
import sys

NAMES = ["screen1", "screen2", "copper", "vars", "system", "sound"]
a = [l.split() for l in open(sys.argv[1], errors="replace") if l[0].isdigit()]
b = [l.split() for l in open(sys.argv[2], errors="replace") if l[0].isdigit()]
show = int(sys.argv[3]) if len(sys.argv) > 3 else 8
first = {}
for x, y in zip(a, b):
    for i, n in enumerate(NAMES):
        if x[1 + i] != y[1 + i] and n not in first:
            first[n] = int(x[0])
print(f"{len(a)} / {len(b)} frames; first difference per region:", first or "none")
for n in ("vars",):
    if n in first:
        f = first[n]
        x = bytes.fromhex(a[f - 1][7]) if len(a) >= f else b""
        y = bytes.fromhex(b[f - 1][7])
        d = [i for i in range(min(len(x), len(y))) if x[i] != y[i]]
        print(f"frame {f}: {len(d)} bytes differ in $100-$FFF:", ", ".join(f"${0x100 + i:x}: {x[i]:02x}/{y[i]:02x}" for i in d[:show]))

# the flips: what was drawn (screen checksum, palette checksum, cursor, message state, x offset) at each screens_flip, in order
fa = next(l for l in open(sys.argv[1], errors="replace") if l.startswith("flips ")).split()[1:]
fb = next(l for l in open(sys.argv[2], errors="replace") if l.startswith("flips ")).split()[1:]
n = min(len(fa), len(fb))
bad = [i for i in range(n) if fa[i] != fb[i]]
print(f"flips: {len(fa)} / {len(fb)}; {len(bad)} of the first {n} differ", "first at" if bad else "", bad[:10])
for i in bad[:3]:
    print("  ", i, fa[i], fb[i])
