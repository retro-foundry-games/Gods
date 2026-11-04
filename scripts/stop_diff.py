#!/usr/bin/env python3
"""Compare the two sides of a lockstep stop saved by gods_lockstep.lua (captures/stops/stop-fFRAME.*).

    python3 scripts/stop_diff.py captures/stops/stop-f5566 [START [LENGTH [SIZE]]]

The original's RAM (.ram, big endian) is compared with the port's memory (.portmem: the program's own words, native) from START (default
0x100) for LENGTH bytes (default up to $40000), as words (SIZE 2, the default), longs (4) or bytes (1).  The runs that differ are listed with
the name of the nearest symbol of native/src/menu.symbols at or below the address."""
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent


def symbols():
    table = []
    for line in (HERE / "native/src/menu.symbols").read_text().splitlines():
        m = re.match(r"(?:table\s+)?(\w+)\s+0x([0-9a-fA-F]+)\s", line)
        if m:
            table.append((int(m.group(2), 16), m.group(1)))
    return sorted(table)


def name_of(table, address):
    best = None
    for a, n in table:
        if a > address:
            break
        best = (a, n)
    return f"{best[1]}+{address - best[0]:#x}" if best else "?"


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    base = sys.argv[1]
    start = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x100
    port = Path(base + ".portmem").read_bytes()
    orig = Path(base + ".ram").read_bytes()
    length = int(sys.argv[3], 0) if len(sys.argv) > 3 else len(port) - start
    size = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    table = symbols()
    runs, last = [], None
    for a in range(start, min(start + length, len(port)) - size + 1, size):
        o = int.from_bytes(orig[a:a + size], "big")
        p = int.from_bytes(port[a:a + size], "little")
        if o != p:
            if runs and last is not None and a - last <= size * 2:
                runs[-1][1].append((o, p))
            else:
                runs.append([a, [(o, p)]])
            last = a
    for a, pairs in runs:
        print(f"{a:06x} {name_of(table, a)}: " + " ".join(f"{o:0{size * 2}x}/{p:0{size * 2}x}" for o, p in pairs[:12]) + (" ..." if len(pairs) > 12 else ""))
    print(f"{len(runs)} runs differ")


main()
