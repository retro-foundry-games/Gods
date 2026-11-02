#!/usr/bin/env python3
"""You play the original, the SDL port follows it, and both are stopped at the first difference.

    python3 projects/gods/scripts/lockstep_live.py [--scale N] [--state NAME.state]

  * the original (tools/engine9000/ami9000_host, host/menu_takeover.lua mode=orig) starts from the saved state of level 1 (128 iterations into it) in a window: you play it with the keyboard
    (arrows and space).  It appends the keys the game reads, by iteration of the game loop, to native/build/live.keys (the only file that stays), and sends the variables $100-$FFF at the
    top of every iteration through a named pipe.
  * the SDL port (native/build/sdl/gods-sdl --headless, GODS_FOLLOW=native/build/live, as fast as it goes) boots level 1 from its beginning, runs the 128 iterations the original's state
    is already into the level, and then follows: it does not begin an iteration before the original has begun its own (live.count says how many), and is given the original's keys at the
    same iterations.  Its variables go through a second pipe.
  * this script compares the variables of both as they arrive, iteration by iteration (words in the Amiga's order; the bytes that a byte and a word variable share and the variables that
    only count time are left out), and at the first difference pauses both (SIGSTOP: the window freezes), prints the iteration, the variables and their values.  Enter lets both go on, q ends them.

The original's window: Ctrl+Alt lets go of the mouse, F12 ends it.  Python and the files are in native/build/live.*
"""
import argparse
import os
import select
import signal
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
import lockstep_play as P  # noqa: E402

N, OFFSET, BUILD, ROOT, PROJECT = P.N, P.OFFSET, P.BUILD, P.ROOT, P.PROJECT
LIVE = BUILD / "live"


def launch(command: list[str], env: dict, cwd: Path) -> tuple[subprocess.Popen, int]:
    child = subprocess.Popen(command, env=env, cwd=cwd, start_new_session=True)
    return child, os.pidfd_open(child.pid)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scale", type=int, default=3)
    parser.add_argument("--state", default="gods-level1-1m.state")
    parser.add_argument("--test", action="store_true", help="the original without a window, given the keys of the recording captures/input/level1-run1-iter.txt (a test of this script)")
    args = parser.parse_args()

    # nothing is kept but the original's keys and a count: the variables of both come through named pipes and are compared as they arrive
    fifos = {name: Path(str(LIVE) + f".{name}.fifo") for name in ("orig", "sdl")}
    for path in [*fifos.values(), Path(str(LIVE) + ".keys"), Path(str(LIVE) + ".count")]:
        path.unlink(missing_ok=True)
    for path in fifos.values():
        os.mkfifo(path)
    # (opened for reading and writing: the writers never block on opening and never see a reader go away)
    pipes = {name: os.open(path, os.O_RDWR | os.O_NONBLOCK) for name, path in fifos.items()}
    count_path = Path(str(LIVE) + ".count")

    def publish(n: int) -> None:
        tmp = Path(str(count_path) + ".tmp")
        tmp.write_text(str(n))
        tmp.replace(count_path)

    publish(0)
    extra = "iterkeys=captures/input/level1-run1-iter.txt" if args.test else ""
    window = ["--headless"] if args.test else ["--scale", str(args.scale)]
    original, original_fd = launch(
        [str(P.HOST), *window, str(P.SCRIPT), "--script-args", f"mode=orig state={args.state} kbd=1 live={LIVE} {extra} until={4500 if args.test else 10000000} out=/dev/null"],
        dict(os.environ), ROOT)
    sdl, sdl_fd = launch([str(P.SDL), "--disks", "../local", "--skip-intro", "--headless", "--keys", "300:40:1,320:40:0"],
                         dict(os.environ, GODS_FOLLOW=str(LIVE), GODS_FAST="1", SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"), BUILD.parent)
    poller = select.poll()
    poller.register(original_fd, select.POLLIN)
    poller.register(sdl_fd, select.POLLIN)
    for fd in pipes.values():
        poller.register(fd, select.POLLIN)
    print("the original is in its window (level 1, state of 128 iterations in): play it with the arrows and space.  The SDL port follows it; both stop at the first difference.")
    buffers = {"orig": bytearray(), "sdl": bytearray()}
    skip_sdl = OFFSET * N          # (the iterations of the port that the original's state is already past)
    compared, received, orig_total = 0, 0, 0
    original_ended, last_progress, last_debug = False, time.monotonic(), 0.0
    try:
        while True:
            for fd, _ in poller.poll(300):
                if fd == sdl_fd:
                    print("\nthe SDL port ended")
                    raise KeyboardInterrupt
                if fd == original_fd and not original_ended:
                    print("\nthe original ended: the SDL port is compared as far as it goes")
                    original_ended = True
                    poller.unregister(original_fd)
            for name, fd in pipes.items():
                try:
                    data = os.read(fd, 1 << 20)
                except BlockingIOError:
                    continue
                if name == "sdl" and skip_sdl:
                    cut = min(skip_sdl, len(data))
                    skip_sdl -= cut
                    data = data[cut:]
                buffers[name] += data
                if name == "orig":
                    orig_total += len(data)
            if orig_total // N > received:
                received = orig_total // N
                publish(received)
            if original_ended and time.monotonic() - last_progress > 8:
                print(f"\nthe SDL port does not go on: {compared} iterations compared; the original sent {orig_total // N}, the port {len(buffers['sdl']) // N} (+{OFFSET - skip_sdl // N} skipped)")
                break
            if os.environ.get("LIVE_DEBUG") and time.monotonic() - last_debug > 1:
                last_debug = time.monotonic()
                print(f"\n[orig {orig_total // N} sdl {len(buffers['sdl']) // N} skip {skip_sdl // N} count {count_path.read_text()}]", flush=True)
            ready = min(len(buffers["orig"]), len(buffers["sdl"])) // N
            if ready == 0:
                continue
            last_progress = time.monotonic()
            orig = np.frombuffer(bytes(buffers["orig"][: ready * N]), dtype=np.uint8).reshape(-1, N)
            sdl_rows = np.frombuffer(P.L.canonical(bytes(buffers["sdl"][: ready * N])), dtype=np.uint8).reshape(-1, N)
            differ = orig != sdl_rows
            differ[:, P.ignored_columns()] = False
            rows = np.where(differ.any(axis=1))[0]
            if len(rows) == 0:
                compared += ready
                del buffers["orig"][: ready * N]
                del buffers["sdl"][: ready * N]
                print(f"\rno difference in {compared} iterations", end="", flush=True)
                continue
            row = int(rows[0])
            for fd in (original_fd, sdl_fd):
                signal.pidfd_send_signal(fd, signal.SIGSTOP)
            seen, found = set(), []
            for col in np.where(differ[row])[0]:
                name = P.C.name_of(0x100 + int(col))
                if name not in seen:
                    seen.add(name)
                    found.append((name, 0x100 + int(col), int(orig[row, col]), int(sdl_rows[row, col])))
            iteration = compared + row + 1
            print(f"\nDIFFERENCE at iteration {iteration} of the original (the SDL port's {iteration + OFFSET}): both are paused")
            for name, address, o, s in found[:20]:
                print(f"   {name:22s} ${address:04x}  original {o:02x}  SDL {s:02x}")
            print("(the original's keys are in native/build/live.keys)  Enter lets both go on, q ends them")
            if sys.stdin.readline().strip().lower() == "q":
                break
            for fd in (original_fd, sdl_fd):
                signal.pidfd_send_signal(fd, signal.SIGCONT)
            compared += ready   # (the iterations up to the difference are not looked at again)
            del buffers["orig"][: ready * N]
            del buffers["sdl"][: ready * N]
    except KeyboardInterrupt:
        pass
    finally:
        for child, fd in ((original, original_fd), (sdl, sdl_fd)):
            try:
                signal.pidfd_send_signal(fd, signal.SIGCONT)
                os.killpg(child.pid, signal.SIGTERM)
            except (ProcessLookupError, OSError):
                pass
            os.close(fd)
        for fd in pipes.values():
            os.close(fd)
        for path in fifos.values():
            path.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
