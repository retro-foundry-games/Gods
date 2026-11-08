#!/usr/bin/env python3
"""Rename variables and functions: python3 scripts/rename_symbols.py OLD=NEW ...   (or a file of OLD=NEW lines with @FILE)

Whole-word replacement in the sources (native/src, host scripts, README), then menu_symbols.h, menu.ld, data.c and struct Gods are generated again, so menu.symbols is the
one place a variable's name is changed.  Refuses a new name that already exists."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = [p for pattern in ("native/src/**/*.c", "native/src/**/*.h", "native/src/**/*.asm", "native/src/**/*.py", "native/src/menu.symbols", "host/*.lua", "scripts/*.py", "README.md")
         for p in ROOT.glob(pattern) if "build" not in p.parts and p.name not in ("menu_symbols.h", "menu.ld", "data.c")]


def main() -> None:
    pairs = []
    for arg in sys.argv[1:]:
        lines = Path(arg[1:]).read_text().split() if arg.startswith("@") else [arg]
        pairs += [tuple(x.split("=")) for x in lines]
    texts = {p: p.read_text() for p in FILES}
    for old, new in pairs:
        if not any(re.search(rf"\b{old}\b", t) for t in texts.values()):
            raise SystemExit(f"{old}: not found")
        if any(re.search(rf"\b{new}\b", t) for t in texts.values()):
            raise SystemExit(f"{new}: exists already")
        for p in texts:
            texts[p] = re.sub(rf"\b{old}\b", new, texts[p])
    for p, t in texts.items():
        if t != p.read_text():
            p.write_text(t)
    subprocess.run([sys.executable, str(ROOT / "scripts/gen_menu_symbols.py")], check=True)
    subprocess.run([sys.executable, str(ROOT / "scripts/gen_guest_struct.py")], check=True)


if __name__ == "__main__":
    main()
