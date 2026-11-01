"""Compare the game's variables and screens in two recordings frame by frame (the C build against the original).

The C build starts a constant number of frames later than the original (its loader reads the program from disk), so
frame F of the C recording is frame F - OFFSET of the reference.  Only the memory that is the game's own is compared: the
data objects pinned at the original addresses by gods.ld (sizes from build/data.o) and the two screens.  Not compared: the
stack, the loader's buffers, the code (the C is in different memory) and the vectors' contents.

    uv run python projects/gods/scripts/compare_memory.py CAPTURE OFFSET [FIRST [LAST [SKEW]]]

SKEW (default 1): the recording is taken at the end of each frame, which is where the interrupt and the main program
race: whether an update that lands within a few hundred cycles of it is in this frame's snapshot or the next one's depends
on the speed of the code.  An object matches if it equals the reference's at the same frame or at one within SKEW frames.
The frame counter (vblank.count) counts from boot, which differs by the loader's time, and is left out.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "gods"
sys.path.insert(0, str(ROOT / "tools" / "amiga"))
from frame_recorder import RefRam  # noqa: E402

SKIP = {"vectors"}          # the C handler's address is in the vector table, the original's is elsewhere


def objects() -> list[tuple[str, int, int]]:
    sizes: dict[str, int] = {}
    out = subprocess.run(["nix", "develop", str(PROJECT / "native"), "--command", "m68k-amigaos-objdump", "-h",
                          str(PROJECT / "build" / "data.o")], capture_output=True, text=True).stdout
    for m in re.finditer(r"\.(?:data|bss|rodata)\.(\w+)\s+([0-9a-f]{8})", out):
        sizes[m.group(1)] = int(m.group(2), 16)
    result = []
    for line in (PROJECT / "native" / "gods.objects").read_text().split("\n"):
        if line.strip():
            name, addr = line.split()
            if name not in SKIP and name in sizes:
                result.append((name, int(addr, 16), sizes[name]))
    return result


VOLATILE = {"music", "channel", "game_clock", "vblank"}   # the interrupt updates these; a snapshot can catch it half way
SCREENS = {"screen_a", "screen_b"}


def u32(mem: bytes, addr: int) -> int:
    return int.from_bytes(mem[addr:addr + 4], "big")


def compared(name: str, addr: int, size: int, mem: bytes) -> bytes:
    """The bytes of an object that are compared.  Left out: vblank.count (counts from boot), the blitter's mask in
    `display` (the blitter is still writing it when the snapshot is taken), and the screen that is being drawn on (only
    the one on show is compared, the frame hashes cover what the display shows as well)."""
    if name == "vblank":
        return mem[addr:addr + 4]
    if name == "display":
        return mem[addr:addr + 0x48]
    if name in SCREENS:
        shown = u32(mem, SCREEN_BACK)   # screens_flip: the screen that was drawn is shown, and called screen_back
        return mem[addr:addr + size] if shown == addr else b""
    return mem[addr:addr + size]


SCREEN_BACK = 0x2944
COUNT = 0x2CC6


def settled(cur: bytes, prev: bytes | None) -> bool:
    """The interrupt ran once, completely, since the previous snapshot."""
    return prev is not None and (int.from_bytes(cur[COUNT:COUNT + 2], "big") - int.from_bytes(prev[COUNT:COUNT + 2], "big")) & 0xFFFF == 1


def main() -> int:
    cap = PROJECT / "captures" / sys.argv[1]
    offset = int(sys.argv[2])
    first = int(sys.argv[3]) if len(sys.argv) > 3 else 2400
    last = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9
    skew = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    objs = objects()
    mine, ref = RefRam(cap, 1000), RefRam(PROJECT / "captures" / "ref100k", 1000)
    window: dict[int, bytes] = {}
    bad: dict[str, list[int]] = {}
    frames = skipped = 0
    prev_a = None
    f = first - 1
    while f <= last:
        a = mine.get(f)
        for g in range(f - offset - skew - 1 if not window else max(window) + 1, f - offset + skew + 1):
            r = ref.get(g)
            if r is not None:
                window[g] = r
        for g in [g for g in window if g < f - offset - skew - 1]:
            del window[g]
        if a is None or (f - offset + skew) not in window:
            break
        if f >= first:
            frames += 1
            settled_here = settled(a, prev_a) and all(settled(window[g], window.get(g - 1)) for g in range(f - offset - skew, f - offset + skew + 1) if g in window and (g - 1) in window) if False else None
            cands = [g for g in range(f - offset - skew, f - offset + skew + 1) if g in window]
            for name, addr, size in objs:
                if name in VOLATILE:
                    ok_frames = settled(a, prev_a) and any(settled(window[g], window.get(g - 1)) for g in cands)
                    if not ok_frames:
                        skipped += 1
                        continue
                mine_bytes = compared(name, addr, size, a)
                if name in SCREENS and not mine_bytes:
                    continue
                if not any(compared(name, addr, size, window[g]) == mine_bytes for g in cands):
                    bad.setdefault(name, []).append(f)
        prev_a = a
        f += 1
    print(f"{frames} frames compared (rebuilt {first}..{f - 1}, offset {offset}, skew {skew}), {len(objs)} objects; "
          f"{skipped} interrupt-dependent object checks skipped in frames the snapshot caught the interrupt")
    for name, fs in sorted(bad.items(), key=lambda kv: kv[1][0]):
        print(f"  {name}: differs in {len(fs)} frames, first at rebuilt {fs[0]} (reference {fs[0] - offset})")
    if not bad:
        print("NO MEMORY MISMATCH")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
