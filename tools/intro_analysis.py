"""Gods analysis: executed-code coverage + the recording -> resident instruction set and its function structure.

Authority is the recorded original: instruction starts come from the executed-code coverage and the bytes from the
recording's chip RAM.  Only code that stays resident and unchanged from the handoff frame to the end of the recording
is "stable"; loader code that is overwritten earlier is not.  Used by lift_gods.py.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from frame_recorder import load_ram_segment  # noqa: E402

REC = ROOT / "captures" / "ref100k"
COVERAGE = ROOT / "analysis" / "coverage-100k.txt"
SEGMENT = json.loads((REC / "meta.json").read_text())["segment"]
LAST_FRAME = json.loads((REC / "meta.json").read_text())["frames"]
CHECK_FRAMES = list(range(5000, LAST_FRAME + 1, 5000))  # frames sampled to prove the code never changes

END_UNCOND = ("rts", "rte", "rtr", "jmp", "bra", "illegal", "stop")


def ram_at(frame: int, cache: dict[int, bytes] = {}) -> bytes:
    if frame not in cache:
        seg = load_ram_segment(REC, frame, SEGMENT)
        if frame not in seg:
            raise SystemExit(f"frame {frame} is not in the recording")
        cache[frame] = seg[frame]
        if len(cache) > 6:
            cache.pop(next(iter(cache)))
    return cache[frame]


def read_coverage() -> dict[int, list[bytes]]:
    cov: dict[int, list[bytes]] = {}
    for line in COVERAGE.read_text().split("\n"):
        if line:
            pc, raw = line.split()
            cov.setdefault(int(pc, 16), []).append(bytes.fromhex(raw))
    return cov


def decode(cov: dict[int, list[bytes]]):
    from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs

    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000 | CS_MODE_BIG_ENDIAN)
    out: dict[int, list[tuple[bytes, str, str]]] = {}
    for pc, variants in cov.items():
        seen = []
        for v in variants:
            ins = next(md.disasm(v, pc), None)
            if ins is None:
                continue
            seen.append((bytes(v[: ins.size]), ins.mnemonic, ins.op_str))
        out[pc] = seen
    return out


def stable_set(decoded) -> tuple[int, dict[int, tuple[bytes, str, str]]]:
    """The instructions whose bytes equal a recorded variant at every sampled frame from the handoff on, and the
    earliest such handoff frame (binary-searched, the recording being deterministic)."""

    def holds(frame: int, pcs) -> bool:
        ram = ram_at(frame)
        return all(any(ram[pc : pc + len(v[0])] == v[0] for v in decoded[pc]) for pc in pcs)

    pcs = [pc for pc in decoded if decoded[pc]]
    for frame in CHECK_FRAMES:  # drop whatever changes later: those instructions are overlays, not resident code
        ram = ram_at(frame)
        pcs = [pc for pc in pcs if any(ram[pc : pc + len(v[0])] == v[0] for v in decoded[pc])]
    lo, hi = 1, CHECK_FRAMES[0]
    while lo < hi:
        mid = (lo + hi) // 2
        if holds(mid, pcs):
            hi = mid
        else:
            lo = mid + 1
    ram = ram_at(lo)
    stable = {}
    for pc in pcs:
        v = next(v for v in decoded[pc] if ram[pc : pc + len(v[0])] == v[0])
        stable[pc] = v
    return lo, stable


def branch_target(mn: str, ops: str) -> int | None:
    base = mn.split(".")[0]
    if base in ("bsr", "bra", "jsr", "jmp") or re.fullmatch(r"b(cc|cs|eq|ge|gt|hi|le|ls|lt|mi|ne|pl|vc|vs)", base) \
            or re.fullmatch(r"db\w+", base):
        m = re.fullmatch(r"(?:.*,\s*)?\$([0-9a-f]+)(?:\.[lw])?", ops.strip())
        if m:
            return int(m.group(1), 16)
    return None


def functions(stable: dict[int, tuple[bytes, str, str]]):
    """Partition the executed instructions into functions: connected components of the flow graph without calls.
    Returns (components, entry points) with entries = call targets, vector targets and flow-less starts."""
    order = sorted(stable)
    nxt = {pc: pc + len(stable[pc][0]) for pc in order}
    parent = {pc: pc for pc in order}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[max(ra, rb)] = min(ra, rb)

    calls, has_pred = set(), set()
    for pc in order:
        _, mn, ops = stable[pc]
        base = mn.split(".")[0]
        t = branch_target(mn, ops)
        if base in ("bsr", "jsr"):
            if t is not None and t in stable:
                calls.add(t)
        elif t is not None and t in stable:
            union(pc, t)
            has_pred.add(t)
        if base not in END_UNCOND and nxt[pc] in stable:
            union(pc, nxt[pc])
            has_pred.add(nxt[pc])
    comps: dict[int, list[int]] = {}
    for pc in order:
        comps.setdefault(find(pc), []).append(pc)
    entries = set(calls) | {pc for pc in order if pc not in has_pred}
    return comps, entries, calls


def static_function(ram: bytes, entry: int, known: set[int]):
    """Complete function body by recursive descent over `ram` from `entry` (calls are not followed).

    Returns {pc: (bytes, mnemonic, operands, executed)}; `executed` says whether the recording ran it.  Stops at the
    first undecodable instruction (reported as a gap) so that data is never mistaken for code."""
    from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs

    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000 | CS_MODE_BIG_ENDIAN)
    out: dict[int, tuple[bytes, str, str, bool]] = {}
    work = [entry]
    while work:
        pc = work.pop()
        if pc in out or pc & 1 or not 0x700 <= pc < 0x3a70:
            continue
        ins = next(md.disasm(ram[pc : pc + 12], pc), None)
        if ins is None:
            continue
        out[pc] = (bytes(ram[pc : pc + ins.size]), ins.mnemonic, ins.op_str, pc in known)
        base = ins.mnemonic.split(".")[0]
        t = branch_target(ins.mnemonic, ins.op_str)
        if base not in ("bsr", "jsr") and t is not None and base not in ("rts", "rte"):
            work.append(t)
        if base not in END_UNCOND:
            work.append(pc + ins.size)
    return out
