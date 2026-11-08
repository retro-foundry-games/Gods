#!/usr/bin/env python3
"""Lockstep: a recorded play of a level through the original disk and through the C game loop, compared at the top of every iteration.

    1. play (a window, the original disk; the keys are recorded):
         tools/engine9000/ami9000_host projects/gods/host/gods.lua --script-args "onemb joy1 keyboard load=gods-level2-1m.state inputlog=captures/input/level2-run1.txt"
       (levels 2 to 4: save/gods-levelN-1m.state, see DECOMPILING.md; level 1: gods-level1-1m.state with captures/input/level1-run1.txt; arrows move, space is fire)
    2. check:
         uv run python projects/gods/scripts/lockstep.py LEVEL

Steps: the frame-timed recording is played on the original and each key event is noted by the iteration of the game loop it fell into
(menu_takeover.lua mode=orig replay= iterlog=); then the original and the C loop (mode=game) are both given the keys by iteration (the key table is set at
once: the keyboard interrupt is asynchronous and loses one of two close events by the original's own timing) and the variables $100-$FFF are written at the top of
every iteration (itervars=1); compare_iter.py lists the first iteration at which a variable differs.  The ones that only count time (message_*, $CE4/$CE5,
random_index, flag_236) are left out.  A difference in a game variable is a difference of the C: the report names the variable and the iteration; itershot= gives
screenshots at chosen iterations (menu_takeover.lua).
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "gods"
HOST = ROOT / "tools" / "engine9000" / "ami9000_host"
SCRIPT = PROJECT / "host" / "menu_takeover.lua"
BUILD = PROJECT / "native" / "build"
TIME_ONLY = {"message_hold", "message_wide", "message_arg", "message_x", "message_shown", "message_text", "message_current", "message_state",
             "message_timer", "flag_236", "random_index", "?0xce4", "?0xce5"}


def run(mode: str, state: str, extra: str, frames: int, out: Path) -> None:
    args = f"mode={mode} state={state} kbd=1 {extra} until={frames} out={out}"
    subprocess.run([str(HOST), str(SCRIPT), "--headless", "--script-args", args], capture_output=True, cwd=PROJECT.parent.parent)


def main() -> int:
    level = sys.argv[1] if len(sys.argv) > 1 else "1"
    state = f"gods-level{level}-1m.state"
    rel = Path("projects/gods/captures/input") / f"level{level}-run1.txt"
    if not (ROOT / rel).exists():
        print(f"no recording {rel}: play first (see the top of this file)")
        return 2
    frames = max(int(l.split()[0]) for l in (ROOT / rel).read_text().split("\n") if l.strip()) + 100
    iterfile = ROOT / "projects/gods/captures/input" / f"level{level}-run1-iter.txt"
    out_o, out_c = BUILD / f"lock_o{level}.txt", BUILD / f"lock_c{level}.txt"
    subprocess.run([str(HOST), str(SCRIPT), "--headless", "--script-args",
                    f"mode=orig state={state} kbd=1 replay={rel.relative_to('projects/gods')} iterlog={iterfile} until={frames} out={BUILD / 'lock_log.txt'}"],
                   capture_output=True, cwd=PROJECT)
    keys = f"iterkeys={rel.relative_to('projects/gods').with_name(iterfile.name)} itervars=1"
    run("orig", state, keys, frames, out_o)
    run("game", state, keys, frames * 2, out_c)
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import compare_iter
    a, b = Path(str(out_o) + ".iter").read_bytes(), Path(str(out_c) + ".iter").read_bytes()
    rows = compare_iter.differences(a, b, TIME_ONLY)
    print(f"level {level}: {min(len(a), len(b)) // compare_iter.N} iterations of the loop compared")
    if not rows:
        print("no difference in the game's variables")
        return 0
    f, nm, addr, c, v = rows[0]
    print(f"FIRST DIFFERENCE at iteration {f}: {nm} (${addr:x}) original {v >> 8:02x}, C {v & 255:02x}; differs in {c} iterations")
    for f, nm, addr, c, v in rows[1:12]:
        print(f"   then iteration {f}: {nm} (${addr:x})")
    print(f"screenshots at iterations: menu_takeover.lua itershot=N shots=DIR (mode=orig and mode=game, iterkeys={iterfile.name})")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
