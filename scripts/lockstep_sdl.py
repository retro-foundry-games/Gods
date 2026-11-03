#!/usr/bin/env python3
"""Lockstep of the Amiga build and the SDL port on level 1: both start the level from the menu's Begin Quest and are given the recorded keys (captures/input/level1-run1.txt, by
iteration of the game loop, shifted by the 128 iterations its state is into the level), and the game's variables ($100-$FFF) are written at the top of every iteration.

    1. the Amiga build (the C disk in the emulator):
       tools/engine9000/ami9000_host projects/gods/host/gods.lua --headless --script-args "rebuilt joy1 press=space@2800 joy=6450:fire,6470:none iterhook=ADDR
           iterkeysfile=captures/input/level1-run1-iterkeys-gods.txt itervars=PROJECT/native/build/lock_amiga.iter frames=17500 shots=/tmp/x"
       (ADDR is `_game_frame` in native/build/menu.sym plus $C0000, in hex)
    2. the SDL port:
       GODS_ITERMAX=3000 GODS_ITERKEYS=../captures/input/level1-run1-iterkeys.txt GODS_ITERDUMP=$PWD/build/lock_sdl.iter native/build/sdl/gods-sdl-dev --disks local --skip-intro
           --headless --frames 20000 --keys 300:40:1,320:40:0
    3. uv run python projects/gods/scripts/lockstep_sdl.py     # the first iteration at which a variable differs

The SDL's memory is the host's: the words and longs of the variables are put in the Amiga's order from the list of what is swapped (native/build/sdl/guest_fixups.h), and a pointer
into the low 64 KiB is put back.
"""
import re
import struct
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import compare_iter as C  # noqa: E402

N = C.N
# (pointers are the host's own addresses; password_slots is what the disk image has; the message line is timed by frames)
TIME_ONLY = {"shift_source", "restart_address", "password_slots", "message_next", "message_hold", "message_wide", "message_arg", "message_x", "message_shown", "message_text", "message_current", "message_state", "message_timer", "flag_236",
             "word_2e6"}


def byte_variables() -> set[int]:
    """The addresses of the variables that are bytes or arrays of bytes (a word variable over them is an alias: those bytes are left out of the comparison)."""
    covered = set()
    for line in (PROJECT / "native/src/menu.symbols").read_text().splitlines():
        m = re.match(r"(\w+)\s+0x([0-9a-f]+)\s+(?:const\s+|volatile\s+)*(u?int8_t|char)\s+%s(?:\[(\d*)\])?", line)
        if m:
            addr, count = int(m.group(2), 16), int(m.group(4)) if m.group(4) else 1
            covered.update(range(addr, addr + max(count, 1)))
    return covered


def canonical(sdl: bytes) -> bytes:
    fixups = [(int(a, 16), int(w), int(c)) for a, w, c in re.findall(r"\{ (0x[0-9a-f]+), (\d+), (\d+) \}", (PROJECT / "native/build/sdl/guest_fixups.h").read_text())
              if 0x100 <= int(a, 16) < 0x1000]
    out = bytearray(sdl)
    for it in range(len(sdl) // N):
        done = set()   # (variables overlap: a byte is put in order once, as guest.c does when it loads the image)
        base = it * N - 0x100
        end = (it + 1) * N
        for addr, width, count in fixups:
            for k in range(count):
                size = 2 if width == 2 else 4
                o = base + addr + size * k
                if o + size > end:
                    break
                if any(b in done for b in range(o, o + size)):
                    continue
                done.update(range(o, o + size))
                if width == 2:
                    out[o], out[o + 1] = out[o + 1], out[o]
                else:   # a long (4) or a pointer (5: four bytes, a value in the moved low 64 KiB is put back)
                    v = struct.unpack("<I", bytes(out[o:o + 4]))[0]
                    if width == 5 and 0x01000000 <= v < 0x01010000:
                        v -= 0x01000000
                    out[o:o + 4] = struct.pack(">I", v)
    return bytes(out)


def main() -> int:
    amiga = (PROJECT / "native/build/lock_amiga.iter").read_bytes()
    sdl = canonical((PROJECT / "native/build/lock_sdl.iter").read_bytes())
    n = min(len(amiga), len(sdl)) // N
    # the bytes that a byte variable and a word variable share are not looked at: put them equal in both
    alias = set()
    fixed = [(int(a, 16), int(w), int(c)) for a, w, c in re.findall(r"\{ (0x[0-9a-f]+), (\d+), (\d+) \}", (PROJECT / "native/build/sdl/guest_fixups.h").read_text())]
    bytes_of_variables = byte_variables()
    for addr, width, count in fixed:
        size = 2 if width == 2 else 4
        for k in range(count):
            span = set(range(addr + size * k, addr + size * (k + 1)))
            if span & bytes_of_variables:
                alias |= span
    amiga, sdl = bytearray(amiga), bytearray(sdl)
    for it in range(n):
        for a in alias:
            if 0x100 <= a < 0x1000:
                sdl[it * N + a - 0x100] = amiga[it * N + a - 0x100]
    rows = [r for r in C.differences(bytes(amiga), bytes(sdl), TIME_ONLY) if not r[1].startswith("?")]
    print(f"{n} iterations compared (the Amiga's {len(amiga) // N}, the SDL's {len(sdl) // N})")
    if not rows:
        print("no difference in the named variables")
        return 0
    for f, name, addr, count, values in rows[:25]:
        print(f"iteration {f:5d}  {name:20s} ${addr:04x}  differs in {count} iterations; there Amiga {values >> 8:02x}, SDL {values & 255:02x}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
