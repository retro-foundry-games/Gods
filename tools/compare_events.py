"""Compare the game's variables at the program's own points in time: event N of the C build against event N of the original.

An event is the entry of the program's wait for the vertical blank (the original's $002CDE, `vbl_wait` in the C build),
recorded with `gods.lua ... record=DIR hook=ADDRESS`.  At that point the interrupt of the frame has finished and the
program has finished the work it was doing, so the memory is a function of how far the program has got, not of the cycle
the end of the frame happens to fall on (the end-of-frame recordings catch the interrupt and the fade half way).

Compared: every data object that intro.ld pins at the original's address (sizes from native/build/intro_data.o), the screens, the
blitter's scratch mask and the frame counter included.  Not compared: the stack, the loader's buffers,
the code, the vectors.

    uv run python tools/compare_events.py CAPTURE REFERENCE [FIRST [LAST]]
"""
import re
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
from frame_recorder import RefRam  # noqa: E402

SKIP = {"vectors"}          # the C handler's address is in the vector table, the original's is elsewhere


def objects() -> list[tuple[str, int, int]]:
    sizes: dict[str, int] = {}
    out = subprocess.run(["nix", "develop", str(PROJECT / "native"), "--command", "m68k-amigaos-objdump", "-h",
                          str(PROJECT / "native" / "build" / "intro_data.o")], capture_output=True, text=True).stdout
    for m in re.finditer(r"\.(?:data|bss|rodata)\.(\w+)\s+([0-9a-f]{8})", out):
        sizes[m.group(1)] = int(m.group(2), 16)
    result = []
    for line in (PROJECT / "native" / "src" / "intro.objects").read_text().split("\n"):
        if line.strip():
            name, addr = line.split()
            if name not in SKIP and name in sizes:
                result.append((name, int(addr, 16), sizes[name]))
    return result


def part(addr: int, size: int, mem: bytes) -> bytes:
    return mem[addr:addr + size]


def main() -> int:
    mine_dir, ref_dir = Path(sys.argv[1]), Path(sys.argv[2])
    first = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    last = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9
    objs = objects()
    mine, ref = RefRam(mine_dir, 1000), RefRam(ref_dir, 1000)
    bad: dict[str, list[int]] = {}
    n = first
    while n <= last:
        a, b = mine.get(n), ref.get(n)
        if a is None or b is None:
            break
        for name, addr, size in objs:
            if part(addr, size, a) != part(addr, size, b):
                bad.setdefault(name, []).append(n)
        n += 1
    print(f"{n - first} events compared ({first}..{n - 1}), {len(objs)} objects")
    for name, ev in sorted(bad.items(), key=lambda kv: kv[1][0]):
        print(f"  {name}: differs at {len(ev)} events, first at event {ev[0]}")
    if not bad:
        print("NO MEMORY MISMATCH")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
