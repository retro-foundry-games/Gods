#!/usr/bin/env python3
"""Which original function entries have a C counterpart in native/src/intro.c (matched by its `orig $ADDR` comments).

    uv run python tools/decompile_status.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import intro_analysis as G  # noqa: E402


def main() -> int:
    dec = G.decode(G.read_coverage())
    _, stable = G.stable_set(dec)
    _, entries, _ = G.functions(stable)
    text = (G.ROOT / "native" / "src" / "intro.c").read_text()
    # only a comment on a function definition counts, not one on a prototype or inside a body
    done = set()
    for m in re.finditer(r"(?m)^/\* orig \$([0-9A-Fa-f]{6})", text):   # comments that open a line, above a definition
        after = text[m.end() :]
        end = after.find("*/")
        rest = after[end + 2 :] if end >= 0 else after
        first = re.search(r"[{;]", rest)
        if first and first.group() == "{" and "\n}" in rest[first.start() :]:
            done.add(int(m.group(1), 16))
    missing = sorted(e for e in entries if e not in done)
    print(f"{len(entries) - len(missing)} of {len(entries)} function entries have a C function in intro.c")
    if missing:
        print("not yet decompiled: " + " ".join(f"${e:06x}" for e in missing))
    extra = sorted(done - set(entries))
    if extra:
        print("`orig` comments that are not function entries: " + " ".join(f"${e:06x}" for e in extra))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
