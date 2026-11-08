"""Compare two runs of host/menu_takeover.lua itervars=1 (the variables $100-$FFF at the top of each iteration of the game loop):
the first iteration at which a game variable differs (the ones that only count time are told apart by being different from early on).

    uv run python projects/gods/scripts/compare_iter.py ORIG.iter C.iter
"""
import bisect
import re
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
N = 0xF00

N = 0xF00
syms = []
for line in (PROJECT / "native" / "src" / "game.symbols").read_text().split("\n"):
    m = re.match(r"(\w+)\s+0x([0-9a-f]+)\s+(?:const\s+)?(\w+)\s*(\*)?\s*%s(\[(\d+)\])?", line)
    if m:
        name, addr, ty, ptr, _, cnt = m.groups()
        size = 4 if ptr else {"uint8_t": 1, "int8_t": 1, "uint16_t": 2, "int16_t": 2, "uint32_t": 4, "int32_t": 4}.get(ty, 4)
        syms.append((int(addr, 16), size * int(cnt or 1), name))
syms.sort()
starts = [s[0] for s in syms]


def name_of(i: int) -> str:
    j = bisect.bisect_right(starts, i) - 1
    return syms[j][2] if j >= 0 and syms[j][0] <= i < syms[j][0] + syms[j][1] else f"?{i:#x}"


def differences(a: bytes, b: bytes, ignore: set[str] = frozenset()) -> list[tuple[int, str, int, int, int]]:
    """(first iteration, variable, address, differing iterations, bytes) of every variable that differs, the earliest first."""
    n = min(len(a), len(b)) // N
    first, count = {}, {}
    for it in range(n):
        for i in range(N):
            if a[it * N + i] != b[it * N + i] and name_of(0x100 + i) not in ignore:
                first.setdefault(i, it + 1)
                count[i] = count.get(i, 0) + 1
    seen, rows = set(), []
    for f, i in sorted((f, i) for i, f in first.items()):
        nm = name_of(0x100 + i)
        if nm in seen:
            continue
        seen.add(nm)
        rows.append((f, nm, 0x100 + i, count[i], a[(f - 1) * N + i] << 8 | b[(f - 1) * N + i]))
    return rows


if __name__ == "__main__":
    a, b = Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]).read_bytes()
    print(f"{min(len(a), len(b)) // N} iterations compared")
    for f, nm, addr, c, v in differences(a, b)[:40]:
        print(f"iteration {f:5d}  {nm:20s} {addr:#06x}  differs in {c} iterations; values there {v >> 8:02x} / {v & 255:02x}")
