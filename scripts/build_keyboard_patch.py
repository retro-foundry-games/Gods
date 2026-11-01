#!/usr/bin/env python3
"""Assemble native/src/amiga/keyboard_patch.asm (the keyboard in the original menu's input) to native/build/keyboard_patch.bin.

    uv run python projects/gods/scripts/build_keyboard_patch.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_adf as B  # noqa: E402


def main() -> int:
    B.BUILD.mkdir(exist_ok=True)
    src = B.SRC / "amiga" / "keyboard_patch.asm"
    r = B.nix(f"vasmm68k_mot -Fbin -no-opt -quiet -o {B.BUILD}/keyboard_patch.bin {src}")
    if r.returncode:
        print(r.stdout, r.stderr)
        return 1
    print(f"keyboard_patch.bin: {(B.BUILD / 'keyboard_patch.bin').stat().st_size} bytes for $1E608")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
