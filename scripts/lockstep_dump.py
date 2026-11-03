#!/usr/bin/env python3
"""Run host/gods_lockstep.lua (headless) on a recorded play until it pauses at the first difference, then save the memory of both sides:

    python3 projects/gods/scripts/lockstep_dump.py REPLAY.txt OUTDIR [--frames N] [--pause F]

OUTDIR gets om.bin and pm.bin (the memory from $10000, 448 KB: the original's and the port's), ov.bin and pv.bin (the variables $100-$FFF) and the lockstep's log.  The port's words
are the host's (little endian); the original's are the machine's.  `tools/ami9000_host/ctl.py` is used through the control socket, `mem ADDR LEN` is the port's peer command.
"""
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def main() -> int:
    replay, out = sys.argv[1], Path(sys.argv[2])
    pause = sys.argv[sys.argv.index("--pause") + 1] if "--pause" in sys.argv else None
    frames = sys.argv[sys.argv.index("--frames") + 1] if "--frames" in sys.argv else "6000"
    out.mkdir(parents=True, exist_ok=True)
    sock, log = out / "ls.sock", out / "lockstep.log"
    sock.unlink(missing_ok=True)
    with open(log, "w") as f:
        host = subprocess.Popen([str(ROOT / "tools/engine9000/ami9000_host"), "--headless", "--frames", frames, "--control", str(sock), str(ROOT / "projects/gods/host/gods_lockstep.lua"),
                                 "--script-args", f"replay={replay}" + (f" continue=all pause={pause}" if pause else "")], stdout=f, stderr=subprocess.STDOUT, cwd=ROOT, start_new_session=True)
        try:
            for _ in range(600):
                if "DIFFERENT" in log.read_text() or "lockstep stopped" in log.read_text():
                    break
                time.sleep(0.5)
            else:
                print("no difference found")
                return 1
            ctl = [sys.executable, str(ROOT / "tools/ami9000_host/ctl.py"), str(sock)]
            for name, base, length in (("m", 0x10000, 0x70000), ("v", 0x100, 0xF00), ("l", 0x4c00, 0x600)):
                line = (f"local p=ami.import('amiga/lockstep').peer; p:send('mem {base:x} {length:x}\\n'); local h=p:readline(); local n=tonumber(h:match('mem (%d+)')); "
                        f"ami.save_file('{out}/p{name}.bin', p:read(n)); ami.save_file('{out}/o{name}.bin', ami.read({base}, {length})); return n")
                subprocess.run(ctl + [line], check=True, capture_output=True)
            print([l for l in log.read_text().splitlines() if "DIFFERENT" in l or "lockstep stopped" in l][0])
        finally:
            host.terminate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
