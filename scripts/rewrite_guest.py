#!/usr/bin/env python3
"""One-off: the program's variables are members of struct Guest (guest_struct.h): `word_0248` becomes `guest.word_0248` in the C sources (not in comments, strings, member accesses)."""
import re
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SRC = PROJECT / "native" / "src"
names = set()
for line in (SRC / "menu.symbols").read_text().split("\n"):
    if not line.strip() or line.startswith("#") or line.split()[0] == "table":
        continue
    decl = line.split(None, 2)[2]
    if re.match(r".*?\s*%s\(void\)", decl) and "(*" not in decl:
        continue
    names.add(line.split()[0])
TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|[A-Za-z_]\w*|\.\.\.|->|\.|\s+|[^\sA-Za-z_]', re.S)


def rewrite(text: str) -> tuple[str, int]:
    out = []
    count = 0
    prev = ""   # the last significant token
    for m in TOKEN.finditer(text):
        t = m.group(0)
        if t.startswith("/*") or t.startswith("//") or t[0] in "\"'":
            out.append(t)
            continue
        if t in names and prev not in (".", "->"):
            out.append("guest." + t)
            count += 1
        else:
            out.append(t)
        if not t.isspace():
            prev = t
    return "".join(out), count


def main() -> None:
    files = [p for p in list(SRC.glob("*.c")) + list(SRC.glob("*.h")) + list((SRC / "sdl").glob("*.c")) + list((SRC / "sdl").glob("*.h")) + list((SRC / "amiga").glob("*.c")) + list((SRC / "amiga").glob("*.h"))
             if p.name not in ("menu_symbols.h", "guest_struct.h", "menu_types.h", "data.c", "intro.c", "intro_data.c", "intro.h", "intro_host.c", "intro_host.h", "intro_view.c", "intro_amiga.asm")]
    for p in sorted(files):
        text = p.read_text()
        # the host's hand declarations of variables go
        kept = []
        for line in text.split("\n"):
            m = re.match(r"^extern\s+[^;(]*?\b(\w+)(?:\[[^\]]*\])?;", line)
            if m and m.group(1) in names:
                continue
            kept.append(line)
        new, n = rewrite("\n".join(kept))
        if new != text:
            p.write_text(new)
            print(f"{p.relative_to(SRC)}: {n}")


if __name__ == "__main__":
    main()
