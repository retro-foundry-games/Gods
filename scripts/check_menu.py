"""Check the functions of menu.c against the original, one oracle run each (host/menu_oracle.lua), samples spread over the run.

    uv run python projects/gods/scripts/check_menu.py [NAME ...]     # all the functions in host/menu_cases.lua, or the named ones
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "gods"
import os
HOST = Path(os.environ.get("AMI_HOST", ROOT / "tools" / "engine9000" / "ami9000_host"))   # a worktree has none of its own: AMI_HOST names the main checkout's
SAMPLES = 24
# the oracle run: the original from a savestate at the menu (host/gods.lua snapname=gods-menu-1m), with the menu used: the
# selection moved, the password screen opened and some letters picked, then Begin Quest chosen
STATE = "gods-menu-1m.state"
SCENARIOS = {}
JOY = "20:down,30:none,60:up,70:none,100:down,110:none,140:fire,150:none"
_t = 200
for _m in "right right down fire left up fire down down right fire up fire left left fire".split():
    JOY += f",{_t}:{_m},{_t + 8}:none"
    _t += 30
SCENARIOS["menu"] = (STATE, 1400, JOY)
SCENARIOS["boot"] = (None, 9000, "")   # from a cold start: the original loads the menu (cases with scenario = "boot")
SCENARIOS["idle"] = (STATE, 2600, "")   # nothing pressed: the menu, then the top ten (cases with scenario = "idle")
# the game: level 1 played by a human (captures/input/level1-run1.txt, recorded with gods.lua inputlog=) from the 1 MB snapshot of its start
SCENARIOS["game"] = ("gods-level1-1m.state", 6000, "", "kbd=1 replay=captures/input/level1-run1.txt")
SCENARIOS["begin"] = (STATE, 700, "20:fire,30:none")   # Begin Quest chosen at once: the program starts the game (cases with scenario = "begin")


def scenario_of(name: str) -> str:
    m = re.search(rf'name = "{name}".*?scenario = "(\w+)"', (PROJECT / "host" / "menu_cases.lua").read_text())
    return m.group(1) if m else "menu"


def calls(name: str) -> int:
    """How many times the original calls it in the run, from a first oracle run with no comparisons (samples=0)."""
    scenario = scenario_of(name)
    state, until, joy, extra = (SCENARIOS[scenario] + ('',))[:4]
    out = PROJECT / "native" / "build" / f"menu_oracle_calls_{scenario}.txt"
    if not out.exists():
        subprocess.run([str(HOST), str(PROJECT / "host" / "menu_oracle.lua"), "--headless", "--script-args",
                        f"{'state=' + state if state else ''} until={until} joy={joy} {extra} samples=0 out={out}"], capture_output=True)
    m = re.search(rf"^{name}\s+calls\s+(\d+)", out.read_text(), re.M)
    return int(m.group(1)) if m else 0


def main() -> int:
    cases = re.findall(r'name = "(\w+)"', (PROJECT / "host" / "menu_cases.lua").read_text())
    names = sys.argv[1:] or cases
    bad = 0
    for name in names:
        n = calls(name)
        state, until, joy, extra = (SCENARIOS[scenario_of(name)] + ('',))[:4]
        stride = max(1, n // SAMPLES)
        out = PROJECT / "native" / "build" / f"menu_oracle_{name}.txt"
        out.unlink(missing_ok=True)
        run = subprocess.run([str(HOST), str(PROJECT / "host" / "menu_oracle.lua"), "--headless", "--script-args",
                        f"{'state=' + state if state else ''} until={until} joy={joy} {extra} only={name} samples={SAMPLES} stride={stride} out={out}"], capture_output=True)
        if not out.exists():
            print(f"{name}: the run wrote no result\n" + (run.stdout + run.stderr).decode()[-600:])
            bad += 1
            continue
        text = out.read_text()
        line = next((l for l in text.split("\n") if l.startswith(name)), f"{name}: no result")
        print(line)
        if " differ   0" not in line or "compared   0" in line:
            bad += 1
            print("   " + "\n   ".join(l for l in text.split("\n") if l.startswith("DIFF"))[:1500])
    print("all match" if not bad else f"{bad} function(s) to look at")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
