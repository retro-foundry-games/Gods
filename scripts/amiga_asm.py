"""Shared by the generators of the programs' assembly (gen_intro_amiga_asm.py, gen_menu_amiga_asm.py): the original's
instructions from a memory image as vasm source, with data addresses as symbols and branch targets as labels, every
instruction checked by assembling it again at its original address.

The caller sets (module attributes) ENTRIES (address -> label of the routines called by name), LABELS (names for the labels
inside routines), SRC (the sources; its parent has the flake, for vasm), data_symbol(addr) and short_symbol(addr).
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from capstone import CS_ARCH_M68K, CS_MODE_M68K_000, Cs

ENTRIES: dict[int, str] = {}
LABELS: dict[int, str] = {}
SRC: Path = Path(".")
CODE_RANGES: list[tuple[int, int]] = []   # all the code that is emitted (branches may go from one piece to another)
PC_LABELS: dict[int, str] = {}   # data inside the emitted code that the original reaches pc-relative: address -> label


def data_symbol(addr: int) -> str | None:
    return None


def short_symbol(addr: int) -> str:
    return f"${addr:x}"


class Ins:
    def __init__(self, i, mem: bytes) -> None:
        self.address, self.size, self.mn, self.op = i.address, i.size, i.mnemonic, i.op_str
        self.raw = mem[i.address:i.address + i.size]


def disassemble(mem: bytes, lo: int, hi: int) -> list[Ins]:
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    ins = [Ins(i, mem) for i in md.disasm(mem[lo:hi], lo)]
    assert sum(i.size for i in ins) == hi - lo, f"{lo:x}-{hi:x}: the disassembly stops early"
    return ins


BRANCH = re.compile(r"^(b(?!tst|set|clr|chg)\w*|db\w+)(\.[bwl])?$")


def is_branch(i: Ins) -> bool:
    return bool(BRANCH.match(i.mn)) and "$" in i.op and "(" not in i.op


def branch_target(i: Ins) -> int:
    m = re.search(r"\$([0-9a-f]+)$", fix_operand(i))
    assert m
    return int(m.group(1), 16)


def jump_target(i: Ins) -> int | None:
    """The address of a `jsr`/`jmp` with an absolute long operand, or None."""
    m = re.fullmatch(r"\$([0-9a-f]+)\.l", fix_operand(i))
    return int(m.group(1), 16) if m and i.mn.split(".")[0] in ("jsr", "jmp") else None


def in_code(t: int) -> bool:
    return any(a <= t < b for a, b in CODE_RANGES)


def fix_operand(i: Ins) -> str:
    """capstone prints some absolute operands as $aaaaaaaa; read them from the instruction's bytes instead."""
    op = i.op
    m = re.search(r"\$([0-9a-f]+)\(pc\)", op)
    if m:   # capstone's pc-relative address is off by 2 for some forms: take the displacement word from the bytes
        t = int(m.group(1), 16)
        best = None
        for p in range(2, i.size, 2):
            w = int.from_bytes(i.raw[p:p + 2], "big")
            w -= 0x10000 if w & 0x8000 else 0
            cand = i.address + p + w
            if abs(cand - t) <= 4 and (best is None or abs(cand - t) < abs(best - t)):
                best = cand
        if best is not None and best != t:
            op = op.replace(m.group(0), f"${best:x}(pc)")
    if "$aaaa" not in op:
        return op
    if "$aaaaaaaa.l" in op:
        op = op.replace("$aaaaaaaa.l", f"${int.from_bytes(i.raw[-4:], 'big'):x}.l")
    size = i.mn.split(".")[-1]
    imm = {"b": i.raw[3:4], "w": i.raw[2:4], "l": i.raw[2:6]}.get(size)
    if imm is not None and re.search(r"#\$a{2,8}\b", op):
        op = re.sub(r"#\$a{2,8}\b", "#$" + format(int.from_bytes(imm, "big"), "x"), op, count=1)
    return op


def text(i: Ins, targets: dict[int, str], final: bool) -> str:
    """vasm source for one instruction.  final: symbols and the C build's addressing; otherwise the original's numbers."""
    op = fix_operand(i)
    mn = i.mn
    if mn.split(".")[0] in ("btst", "bset", "bclr", "bchg") and re.search(r", d\d$", op):
        mn = mn.split(".")[0] + ".l"   # capstone prints a bit operation on a data register as a byte one; it is a long
    elif mn.split(".")[0] in ("btst", "bset", "bclr", "bchg") and not re.search(r", d\d$", op):
        mn = mn.split(".")[0] + ".b"   # ... and on memory a byte one (capstone may call it a long)
    if is_branch(i):
        tgt = branch_target(i)
        lab = targets.get(tgt) or ENTRIES.get(tgt) or f"${tgt:x}"
        return f"{mn} {op[:op.rindex('$')]}{lab}".replace(", ", ",")
    jt = jump_target(i)
    if jt is not None and final and jt not in ENTRIES and in_code(jt):
        return f"{mn} {targets.get(jt) or LABELS.get(jt, f'L{jt:x}')}"
    m = re.search(r"\$([0-9a-f]+)\.l", op)
    if m and final:
        address = int(m.group(1), 16)
        sym = (ENTRIES.get(address) if mn.split(".")[0] in ("jsr", "jmp") else None) or data_symbol(address) or ENTRIES.get(address)
        if sym:
            op = op.replace(m.group(0), f"({sym}).l")
    m = re.search(r"#\$([0-9a-f]+)", op)
    if m and final and mn == "movea.l" and re.search(r", a\d$", op):
        sym = data_symbol(int(m.group(1), 16))
        if sym and "+" not in sym:
            op = op.replace(m.group(0), "#" + sym)
    m = re.search(r"\$([0-9a-f]+)\(pc(?:, *([ad]\d\.[wl]))?\)", op)
    if m and final and int(m.group(1), 16) in PC_LABELS:   # pc-relative to data in the code: keep it, by label
        op = op.replace(m.group(0), f"{PC_LABELS[int(m.group(1), 16)]}(pc" + (f",{m.group(2)})" if m.group(2) else ")"))
    m = re.search(r"\$([0-9a-f]+)\.w\b", op)
    if m and final and mn.split(".")[0] in ("jsr", "jmp") and int(m.group(1), 16) in ENTRIES:   # a call by absolute short address to a routine that is here (or C)
        return f"{mn} ({ENTRIES[int(m.group(1), 16)]}).l"
    if m and final:   # absolute short: a symbol is an equate (the variable is in another hunk)
        sym = short_symbol(int(m.group(1), 16))
        if not sym.startswith("$"):
            op = op.replace(m.group(0), f"({sym}).w")
    m = re.search(r"\$([0-9a-f]+)\(pc\)", op)
    if m and final and int(m.group(1), 16) >= 0x8000 and not in_code(int(m.group(1), 16)):
        a = int(m.group(1), 16)
        return f"{mn} {op.replace(m.group(0), '(' + (data_symbol(a) or f'${a:x}') + ').l')}".replace(", ", ",")
    m = re.match(r"\$([0-9a-f]+)\(pc\),(.*)$", op.replace(", ", ","))
    if m and final:
        # pc-relative to data: the data is in another hunk, so use absolute short (8 cycles as well, no relocation)
        return f"{mn} ({short_symbol(int(m.group(1), 16))}).w,{m.group(2)}"
    return f"{mn} {op}".replace(", ", ",")


def emit(mem: bytes, lo: int, hi: int, force: set[int] = frozenset()) -> list[str]:
    """The original instructions in [lo, hi) as vasm source, verified against the original bytes.  `force`: addresses that
    get a label although no branch in the piece goes there (a jump table's targets)."""
    ins = disassemble(mem, lo, hi)

    def in_code(t: int) -> bool:
        return lo <= t < hi or any(a <= t < b for a, b in CODE_RANGES)
    all_targets = {branch_target(i): LABELS.get(branch_target(i), f"L{branch_target(i):x}") for i in ins
                   if is_branch(i) and in_code(branch_target(i)) and branch_target(i) not in ENTRIES}
    for i in ins:
        jt = jump_target(i)
        if jt is not None and in_code(jt) and jt not in ENTRIES:
            all_targets[jt] = LABELS.get(jt, f"L{jt:x}")
    for a in force:
        if in_code(a) and a not in ENTRIES:
            all_targets[a] = LABELS.get(a, f"L{a:x}")
    targets = {t: n for t, n in all_targets.items() if lo <= t < hi}   # defined here
    check = [f"{name} = ${addr:x}" for addr, name in ENTRIES.items() if not lo <= addr < hi]
    check.append(f"        org ${lo:x}")
    for i in ins:
        if i.address in ENTRIES:
            check.append(f"{ENTRIES[i.address]}:")
        if i.address in targets:
            check.append(f"{targets[i.address]}:")
        check.append(f"        org ${i.address:x}")
        check.append("        " + text(i, targets, False))
    with tempfile.TemporaryDirectory() as d:
        src, out = Path(d) / "c.asm", Path(d) / "c.bin"
        src.write_text("\n".join(check) + "\n")
        r = subprocess.run(["nix", "develop", str(SRC.parent), "--command", "vasmm68k_mot", "-Fbin", "-no-opt", "-quiet", "-o", str(out), str(src)],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"{lo:x}: {r.stdout}{r.stderr}")
        built = out.read_bytes()
    lines = []
    for i in ins:
        off = i.address - lo
        if i.address in ENTRIES:
            lines.append(f"{ENTRIES[i.address]}:")
        if i.address in targets:
            lines.append(f"{targets[i.address]}:")
        if built[off:off + i.size] == i.raw:
            lines.append("        " + text(i, all_targets, True))
        else:
            assert not is_branch(i) and "(pc)" not in i.op, \
                f"{i.address:x} {i.mn} {i.op}: assembled as {built[off:off + i.size].hex()}, the original is {i.raw.hex()}"
            words = ",".join(f"${int.from_bytes(i.raw[k:k + 2], 'big'):04x}" for k in range(0, i.size, 2))
            lines.append(f"        dc.w {words}   ; {i.mn} {i.op}, the original's bytes")
    return lines


def entry_label(lines: list[str]) -> list[str]:
    """Drop the label line of the original's entry (the wrapper has its own)."""
    return lines[1:] if lines and lines[0].endswith(":") and lines[0].split(":")[0] in ENTRIES.values() else lines


def scratch(lines: list[str]) -> list[str]:
    """The original keeps a counter and a value in d7 and d6; in C those are callee-saved, d1 and d0 are not (same cost)."""
    return [re.sub(r"\bd6\b", "d0", re.sub(r"\bd7\b", "d1", ln)) for ln in lines]



LABEL_DEF = re.compile(r"^([A-Za-z_]\w*):\s*$")
CALL = re.compile(r"^\s+(bsr|jsr|jmp)\b")


def localize(lines: list[str]) -> list[str]:
    """Labels that are only branched to from inside their own routine become local (`.name`, scoped to the preceding
    global label); a routine's single `*_loop` label is called `.loop`.  Labels that are called, jumped to from another
    routine or exported stay global."""
    defs = {ln.split(":")[0]: n for n, ln in enumerate(lines) if LABEL_DEF.match(ln)}
    exported = set()
    for ln in lines:
        if ln.strip().startswith("xdef"):
            exported |= {x.strip() for x in ln.split("xdef")[1].split(",")}
    refs: dict[str, list[tuple[int, bool]]] = {name: [] for name in defs}
    for n, ln in enumerate(lines):
        code = ln.split(";")[0]
        if LABEL_DEF.match(ln) or not code.strip() or code.strip().startswith(("xref", "xdef", "section")):
            continue
        for name in defs:
            if re.search(r"(?<![\w.])" + re.escape(name) + r"(?!\w)", code):
                refs[name].append((n, bool(CALL.match(code))))
    glob = {name for name in defs if name in exported or name.startswith("_") or not refs[name]
            or any(call for _, call in refs[name])}
    while True:
        starts = sorted(defs[g] for g in glob)

        def scope(n: int) -> int:
            k = [x for x in starts if x <= n]
            return k[-1] if k else -1
        promote = {name for name in defs if name not in glob and any(scope(r) != scope(defs[name]) for r, _ in refs[name])}
        if not promote:
            break
        glob |= promote
    local = {name for name in defs if name not in glob}
    # name the single loop of a routine `.loop`
    by_scope: dict[int, list[str]] = {}
    for name in local:
        by_scope.setdefault(scope(defs[name]), []).append(name)
    new = {name: "." + name for name in local}
    for names in by_scope.values():
        loops = [x for x in names if x.endswith("_loop")]
        if len(loops) == 1:
            new[loops[0]] = ".loop"
    out = []
    for ln in lines:
        m = LABEL_DEF.match(ln)
        if m and m.group(1) in new:
            out.append(new[m.group(1)] + ":")
            continue
        head, sep, comment = ln.partition(";")
        if head.strip() and not head.strip().startswith(("xref", "xdef", "section")):
            for name in local:
                head = re.sub(r"(?<![\w.])" + re.escape(name) + r"(?!\w)", new[name], head)
        out.append(head + sep + comment)
    return out


