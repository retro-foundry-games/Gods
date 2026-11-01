"""Check the functions of menu.c against the original, one oracle run each (host/menu_oracle.lua), samples spread over the run.

    uv run python projects/gods/scripts/check_menu.py [NAME ...]     # all the functions in host/menu_cases.lua, or the named ones
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "gods"
HOST = ROOT / "tools" / "engine9000" / "ami9000_host"
SAMPLES = 24


def calls(name: str) -> int:
    """How many times the original calls it in the run, from a first oracle run with no comparisons (samples=0)."""
    out = PROJECT / "build" / "menu_oracle_calls.txt"
    if not out.exists():
        subprocess.run([str(HOST), str(PROJECT / "host" / "menu_oracle.lua"), "--headless", "--script-args",
                        f"until=9000 samples=0 out={out}"], capture_output=True)
    m = re.search(rf"^{name}\s+calls\s+(\d+)", out.read_text(), re.M)
    return int(m.group(1)) if m else 0


def main() -> int:
    cases = re.findall(r'name = "(\w+)"', (PROJECT / "host" / "menu_cases.lua").read_text())
    names = sys.argv[1:] or cases
    bad = 0
    for name in names:
        n = calls(name)
        stride = max(1, n // SAMPLES)
        out = PROJECT / "build" / f"menu_oracle_{name}.txt"
        subprocess.run([str(HOST), str(PROJECT / "host" / "menu_oracle.lua"), "--headless", "--script-args",
                        f"until=9000 only={name} samples={SAMPLES} stride={stride} out={out}"], capture_output=True)
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
