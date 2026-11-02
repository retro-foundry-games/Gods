#!/usr/bin/env python3
"""Play level 1 in the SDL port; every iteration of the game loop is checked against the original, and the port is stopped (paused) at the first difference.

    python3 projects/gods/scripts/lockstep_play.py [--every SECONDS] [--scale N] [--no-window]

What it does:
  1. the SDL port is started in a window (native/build/sdl/gods-sdl, built by `make sdl` in projects/gods/native) with GODS_RECORD=native/build/play: it goes through the intro,
     starts Begin Quest by itself (the fire key at frame 300) and plays level 1 from its beginning.  The first 128 iterations (about 8 seconds, the message "recording keys from
     iteration 129: play now" says when) are not played by you: the original's start is a state that far into the level, so the keys you press before are let go.
  2. your keys are recorded by iteration of the game loop (play.keys) and the variables $100-$FFF at the top of every iteration (play.iter).
  3. every few seconds the recording so far is given to the original (the emulator, from projects/gods/local/save/gods-level1-1m.state, the keys at the same iterations) and the
     variables of both are compared iteration by iteration (the same comparison as lockstep_sdl.py: words in the Amiga's order, the bytes that a byte and a word variable share
     and the variables that only count time are left out).
  4. at the first difference the SDL port is paused (SIGSTOP: the window freezes), the iteration, the variables and their values are printed, and the recording is kept in
     native/build/play.* (run again with `--check` to compare it again).  Press Enter to let the port go on, or end it.

    python3 projects/gods/scripts/lockstep_play.py --check      # compare the last recording (native/build/play.keys and play.iter) once and print the result
"""
import argparse
import os
import re
import select
import signal
import subprocess
import sys
from pathlib import Path

import numpy as np

SCRIPTS = Path(__file__).resolve().parent
PROJECT = SCRIPTS.parent
ROOT = PROJECT.parent.parent
sys.path.insert(0, str(SCRIPTS))
import compare_iter as C  # noqa: E402
import lockstep_sdl as L  # noqa: E402

N = C.N
BUILD = PROJECT / "native" / "build"
HOST = ROOT / "tools" / "engine9000" / "ami9000_host"
SCRIPT = PROJECT / "host" / "menu_takeover.lua"
SDL = BUILD / "sdl" / "gods-sdl"
PLAY = BUILD / "play"
OFFSET = 128                      # the original's state is this many iterations into the level
KEYSYM = {0x4C: 273, 0x4D: 274, 0x4E: 275, 0x4F: 276, 0x40: 32}   # raw Amiga key codes -> what menu_takeover.lua takes (up, down, right, left, space)


def alias_bytes() -> set[int]:
    fixed = [(int(a, 16), int(w), int(c)) for a, w, c in re.findall(r"\{ (0x[0-9a-f]+), (\d+), (\d+) \}", (BUILD / "sdl" / "guest_fixups.h").read_text())]
    covered = L.byte_variables()
    alias: set[int] = set()
    for addr, width, count in fixed:
        size = 2 if width == 2 else 4
        for k in range(count):
            span = set(range(addr + size * k, addr + size * (k + 1)))
            if span & covered:
                alias |= span
    return {a - 0x100 for a in alias if 0x100 <= a < 0x1000}


ALIAS = None
IGNORE = None


def ignored_columns() -> np.ndarray:
    """The columns (bytes of $100-$FFF) that are not compared: unnamed ones, the ones that only count time, and the aliases."""
    global ALIAS, IGNORE
    if IGNORE is None:
        ALIAS = alias_bytes()
        mask = np.zeros(N, dtype=bool)
        for i in range(N):
            name = C.name_of(0x100 + i)
            if name.startswith("?") or name in L.TIME_ONLY or i in ALIAS:
                mask[i] = True
        IGNORE = mask
    return IGNORE


def keys_for_original(keys_path: Path, out: Path, last_iteration: int) -> int:
    """The recorded keys ("I CODE V": at the top of SDL iteration I) as menu_takeover.lua has them ("n k keysym state": at the top of iteration n + 1 of the original, n = I - 129)."""
    lines, skipped = [], 0
    if keys_path.exists():
        for line in keys_path.read_text().split("\n"):
            parts = line.split()
            if len(parts) != 3:
                continue
            iteration, code, value = int(parts[0]), int(parts[1], 16), int(parts[2])
            if iteration > last_iteration:
                continue
            if code in KEYSYM:
                lines.append(f"{iteration - OFFSET - 1} k {KEYSYM[code]} {value} 3")
            else:
                skipped += 1
    out.write_text("\n".join(lines) + "\n")
    return skipped


def run_original(iterations: int, keys_file: Path, out: Path) -> bytes:
    frames = 3 * iterations + 400
    args = f"mode=orig state=gods-level1-1m.state kbd=1 iterkeys={keys_file.relative_to(PROJECT)} itervars=1 until={frames} out={out}"
    subprocess.run([str(HOST), str(SCRIPT), "--headless", "--script-args", args], capture_output=True, cwd=ROOT, check=False)
    return Path(str(out) + ".iter").read_bytes()


def compare(sdl_raw: bytes, original: bytes, limit: int):
    """(None, [], iterations compared) when the iterations 129..limit of the SDL port equal the original's, else (SDL iteration, [(variable, address, original, SDL)], iterations compared)."""
    sdl = np.frombuffer(L.canonical(sdl_raw[: limit * N]), dtype=np.uint8).reshape(-1, N)[OFFSET:]
    orig = np.frombuffer(original, dtype=np.uint8).reshape(-1, N)
    n = min(len(sdl), len(orig))
    if n == 0:
        return None, [], 0
    differ = sdl[:n] != orig[:n]
    differ[:, ignored_columns()] = False
    rows = np.where(differ.any(axis=1))[0]
    if len(rows) == 0:
        return None, [], n
    row = int(rows[0])
    seen, found = set(), []
    for col in np.where(differ[row])[0]:
        name = C.name_of(0x100 + int(col))
        if name in seen:
            continue
        seen.add(name)
        found.append((name, 0x100 + int(col), int(orig[row, col]), int(sdl[row, col])))
    return row + OFFSET + 1, found, n


def check_once():
    """(first SDL iteration that differs or None, its differing variables, iterations compared), or None when too little is recorded."""
    iter_path = Path(str(PLAY) + ".iter")
    data = iter_path.read_bytes() if iter_path.exists() else b""
    count = len(data) // N - 1            # (the last iteration may be written only in part)
    if count <= OFFSET + 1:
        return None, 0
    keys_file = BUILD / "play.orig.keys"
    keys_for_original(Path(str(PLAY) + ".keys"), keys_file, count)
    original = run_original(count - OFFSET, keys_file, BUILD / "play.orig")
    return compare(data, original, count)


def report(iteration: int, found: list, count: int) -> None:
    print(f"\nDIFFERENCE at iteration {iteration} of the game loop (the original's iteration {iteration - OFFSET}); {count} iterations were compared")
    for name, address, o, s in found[:20]:
        print(f"   {name:22s} ${address:04x}  original {o:02x}  SDL {s:02x}")
    print(f"the recording is native/build/play.keys and play.iter; `lockstep_play.py --check` compares it again")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--every", type=float, default=8.0, help="seconds between the checks")
    parser.add_argument("--scale", type=int, default=3)
    parser.add_argument("--no-window", action="store_true", help="a headless run of the keys of --keys FILE (a test)")
    parser.add_argument("--check", action="store_true", help="compare the last recording once")
    args = parser.parse_args()

    if args.check:
        result = check_once()
        if result is None:
            print("nothing recorded (fewer than 129 iterations)")
            return 2
        iteration, found, count = result
        if iteration is None:
            print(f"no difference in {count} iterations of the game loop (from the 129th on)")
            return 0
        report(iteration, found, count)
        return 1

    for suffix in (".iter", ".keys"):
        Path(str(PLAY) + suffix).unlink(missing_ok=True)
    env = dict(os.environ, GODS_RECORD=str(PLAY))
    child = subprocess.Popen([str(SDL), "--disks", "../local", "--skip-intro", "--scale", str(args.scale), "--keys", "300:40:1,320:40:0"], cwd=BUILD.parent, env=env, start_new_session=True)
    pidfd = os.pidfd_open(child.pid)
    poller = select.poll()
    poller.register(pidfd, select.POLLIN)
    print("gods-sdl started: it plays the intro, starts Begin Quest by itself and then waits 128 iterations (about 8 s) for you; play level 1 from its beginning.")
    last_checked = 0
    try:
        while True:
            if poller.poll(int(args.every * 1000)):
                print("the port ended")
                break
            data_size = Path(str(PLAY) + ".iter").stat().st_size if Path(str(PLAY) + ".iter").exists() else 0
            count = data_size // N - 1
            if count <= OFFSET + 1 or count <= last_checked:
                continue
            result = check_once()
            last_checked = count
            if result is None:
                continue
            iteration, found, checked = result
            if iteration is None:
                print(f"\rno difference in {checked} iterations", end="", flush=True)
                continue
            signal.pidfd_send_signal(pidfd, signal.SIGSTOP)
            report(iteration, found, checked)
            print("the port is paused; Enter lets it go on (and goes on checking), q + Enter ends it")
            answer = sys.stdin.readline().strip().lower()
            if answer == "q":
                break
            signal.pidfd_send_signal(pidfd, signal.SIGCONT)
            last_checked = 10**9   # (the difference stays in the recording: only a new run checks again)
    finally:
        try:
            signal.pidfd_send_signal(pidfd, signal.SIGCONT)
            os.killpg(child.pid, signal.SIGTERM)
        except (ProcessLookupError, OSError):
            pass
        os.close(pidfd)
    result = check_once()
    if result:
        iteration, found, count = result
        if iteration is None:
            print(f"final check: no difference in {count} iterations")
        else:
            report(iteration, found, count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
