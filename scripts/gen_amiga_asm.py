"""Generate native/amiga.asm: the Amiga-specific routines as the original has them.

The routines are the original's instructions, disassembled from the resident code in the reference recording (frame
1746), with data addresses turned into symbols and branch targets into labels, so they take as long as the original,
which the program's timing (frame flips, the interrupt, the music) depends on.  Every instruction is checked: it is
assembled again at its original address and must give the original bytes.  Where the C calling convention is needed
(arguments on the stack, d2-d7/a2-a6 preserved) a small wrapper is added around the original instructions.

    uv run python projects/gods/scripts/gen_amiga_asm.py
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from capstone import CS_ARCH_M68K, CS_MODE_M68K_000, Cs

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gods_offsets  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "gods"
NATIVE = PROJECT / "native"
OUT = NATIVE / "amiga.asm"
REF = PROJECT / "captures" / "ref100k"
FRAME = 1746

# entry points of the original's routines that are called by name: address -> label
ENTRIES = {
    0x2F1E: "_blitter_wait", 0x1B7C: "orig_blit_copy", 0x1BC6: "orig_blit_copy_right",
    0x30D4: "_screen_clear", 0x30EE: "_screen_restore", 0x1D54: "_plane5_set", 0x1D7E: "_plane5_clear",
    0x32B6: "orig_blit_image", 0x32CA: "orig_blit_image_wide", 0x32DE: "blit_image_body",
    0x2CDE: "_vbl_wait", 0x1DD6: "_irq_level3", 0x2242: "orig_music_init", 0x22E6: "music_step",
    0x2CF4: "irq_housekeeping", 0x22C4: "music_stop",
}
# names for the labels inside the routines, by the original's address (any other branch target is called L<address>)
LABELS = {
    0x2F26: "blitter_wait_loop", 0x30E2: "screen_clear_loop", 0x3102: "screen_restore_loop",
    0x1D64: "plane5_set_loop", 0x1D8E: "plane5_clear_loop",
    # blit_image: a mask is made of the image's planes (the first three ORed, then the fourth), then each plane is drawn
    # through it over the screen; with a pixel shift the blitter's B channel does the shifting
    0x32FC: "wait_mask_or3", 0x333E: "mask_modulo_ready", 0x334E: "wait_mask_or4", 0x3378: "wait_before_draw",
    0x33BC: "draw_planes", 0x33DC: "wait_second_plane", 0x33F4: "wait_third_plane", 0x340C: "wait_fourth_plane",
    0x3430: "wait_fifth_plane", 0x343E: "blit_image_done",
    0x344A: "wait_shifted_draw", 0x3470: "shifted_modulo", 0x3478: "shifted_set_modulos",
    0x2CE6: "vbl_wait_loop",
    0x1DF2: "irq_clear_request", 0x1E32: "irq_buttons_done", 0x1E36: "irq_to_housekeeping",
    0x2D2E: "irq_count_frames", 0x2D40: "irq_count_vblanks",
    # music_init
    0x2254: "find_last_pattern", 0x2258: "scan_order_table", 0x2278: "locate_samples",
    # music_step and what it calls: per row (every `speed` ticks) the four channels start their next cell
    0x230E: "tick_effects", 0x2598: "step_done", 0x256A: "next_order", 0x23A4: "new_row",
    0x2412: "channel_row", 0x2480: "sample_no_repeat", 0x2496: "row_effect", 0x24B6: "start_note",
    0x24F2: "start_dma", 0x24F6: "start_dma_wait1", 0x250E: "start_dma_wait2",
    0x25A6: "tone_portamento_target", 0x25C6: "portamento_target_reached", 0x25CA: "portamento_done",
    0x25CC: "tone_portamento", 0x25DA: "tone_portamento_step", 0x2604: "portamento_set_period",
    0x260C: "tone_portamento_down",
    0x262C: "vibrato", 0x2636: "vibrato_apply", 0x2664: "vibrato_subtract", 0x2666: "vibrato_set_period",
    0x267A: "set_period", 0x2682: "channel_tick",
    0x234A: "arpeggio", 0x236E: "arpeggio_low_nibble", 0x237A: "arpeggio_base", 0x2380: "arpeggio_offset",
    0x238E: "arpeggio_find_period", 0x239E: "arpeggio_set_period",
    0x26C6: "volume_slide", 0x26E4: "volume_slide_write", 0x26EC: "volume_slide_down", 0x2700: "volume_slide_down_write",
    0x2708: "portamento_up", 0x272C: "portamento_up_write", 0x273A: "portamento_down", 0x275E: "portamento_down_write",
    0x276C: "row_command", 0x2794: "command_extended", 0x2796: "command_pattern_break", 0x279E: "command_position_jump",
    0x27B2: "command_volume", 0x27C0: "command_volume_write", 0x27C8: "command_speed", 0x27DE: "command_done",
}
SKIP_OBJECTS = {"vectors"}


def ram() -> bytes:
    tmp = PROJECT / "build" / "ref-1746.bin"
    if not tmp.exists():
        tmp.parent.mkdir(exist_ok=True)
        subprocess.run([sys.executable, str(ROOT / "tools/amiga/frame_recorder.py"), "--extract", str(REF), str(FRAME), str(tmp)],
                       check=True, capture_output=True)
    return tmp.read_bytes()


def objects() -> list[tuple[str, int, int]]:
    """The objects gods.objects pins at the original addresses, with their sizes (from the sections of build/data.o)."""
    sizes: dict[str, int] = {}
    out = subprocess.run(["nix", "develop", str(NATIVE), "--command", "m68k-amigaos-objdump", "-h",
                          str(PROJECT / "build" / "data.o")], capture_output=True, text=True).stdout
    for m in re.finditer(r"\.(?:data|bss|rodata)\.(\w+)\s+([0-9a-f]{8})", out):
        sizes[m.group(1)] = int(m.group(2), 16)
    result = []
    for line in (NATIVE / "gods.objects").read_text().split("\n"):
        if line.strip():
            name, addr = line.split()
            if name not in SKIP_OBJECTS and name in sizes:
                result.append((name, int(addr, 16), sizes[name]))
    return sorted(result, key=lambda o: o[1])


OBJECTS = objects()


# which struct each pinned object is; the fields and their offsets come from the compiler (scripts/gods_offsets.py), so they
# follow gods.h and the assembly names them as `_flags+Flags_fire_pressed`
OBJECT_STRUCT = {"flags": "Flags", "game_clock": "GameClock", "vblank": "Vblank", "music": "MusicState",
                 "display": "DisplayData", "module": "Module"}
OFFSETS = gods_offsets.write()


def fields(struct: str) -> dict[int, str]:
    """offset -> field name of the constants `Struct_field` in gods_offsets.i (the first name for an offset)."""
    out: dict[int, str] = {}
    for name, value in OFFSETS.items():
        if name.startswith(struct + "_") and name != "Channel_size":
            out.setdefault(value, name[len(struct) + 1:])
    return out


CHANNEL_SIZE = OFFSETS["Channel_size"]


def data_symbol(addr: int) -> str | None:
    """`_name+Struct_field` for an address inside one of the pinned objects, preferring the object that starts there."""
    for name, a, size in OBJECTS:
        if a <= addr < a + size or a == addr:
            off = addr - a
            if name in OBJECT_STRUCT:
                struct = OBJECT_STRUCT[name]
                if off in fields(struct):
                    return f"_{name}+{struct}_{fields(struct)[off]}"
            return f"_{name}" if off == 0 else f"_{name}+${off:x}"
    return None


def short_symbol(addr: int) -> str:
    """The name of a pinned table's address for absolute short addressing (an equate, as the symbol is in another hunk)."""
    for name, a, size in OBJECTS:
        if name == "channel" and a <= addr < a + size:
            return f"Addr_channel+Channel_size*{(addr - a) // CHANNEL_SIZE}" if addr != a else "Addr_channel"
        if a == addr and name in ("sample_start", "period_table", "vibrato_table"):
            return f"Addr_{name}"
    return f"${addr:x}"


def channel_fields(lines: list[str]) -> list[str]:
    """In the replayer a6 is the channel's state and a5 the chip's audio channel: name the offsets."""
    cf, af = fields("Channel"), fields("AudioChannel")

    def six(m):
        off = int(m.group(1), 16)
        if off in cf:
            return f"Channel_{cf[off]}(a6)"
        if 0 < off < 4:   # the bytes of the pattern cell
            return f"Channel_{cf[0]}+{off}(a6)"
        return m.group(0)

    def five(m):
        off = int(m.group(1), 16)
        return f"AudioChannel_{af[off]}(a5)" if off in af else m.group(0)
    out = []
    for ln in lines:
        head, sep, comment = ln.partition(";")
        head = re.sub(r"\$([0-9a-f]+)\(a6\)", six, head)
        if "$dff000" not in head:
            head = re.sub(r"\$([0-9a-f]+)\(a5\)", five, head)
        out.append(head + sep + comment)
    return out


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


def fix_operand(i: Ins) -> str:
    """capstone prints some absolute operands as $aaaaaaaa; read them from the instruction's bytes instead."""
    op = i.op
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
    if is_branch(i):
        tgt = branch_target(i)
        lab = targets.get(tgt) or ENTRIES.get(tgt) or f"${tgt:x}"
        return f"{mn} {op[:op.rindex('$')]}{lab}".replace(", ", ",")
    m = re.search(r"\$([0-9a-f]+)\.l", op)
    if m and final:
        sym = data_symbol(int(m.group(1), 16)) or ENTRIES.get(int(m.group(1), 16))
        if sym:
            op = op.replace(m.group(0), f"({sym}).l")
    m = re.search(r"#\$([0-9a-f]+)", op)
    if m and final and mn == "movea.l" and re.search(r", a\d$", op):
        sym = data_symbol(int(m.group(1), 16))
        if sym and "+" not in sym:
            op = op.replace(m.group(0), "#" + sym)
    m = re.match(r"\$([0-9a-f]+)\(pc\),(.*)$", op.replace(", ", ","))
    if m and final:
        # pc-relative to data: the data is in another hunk, so use absolute short (8 cycles as well, no relocation)
        return f"{mn} ({short_symbol(int(m.group(1), 16))}).w,{m.group(2)}"
    return f"{mn} {op}".replace(", ", ",")


def emit(mem: bytes, lo: int, hi: int) -> list[str]:
    """The original instructions in [lo, hi) as vasm source, verified against the original bytes."""
    ins = disassemble(mem, lo, hi)
    targets = {branch_target(i): LABELS.get(branch_target(i), f"L{branch_target(i):x}") for i in ins
               if is_branch(i) and lo <= branch_target(i) < hi and branch_target(i) not in ENTRIES}
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
        r = subprocess.run(["nix", "develop", str(NATIVE), "--command", "vasmm68k_mot", "-Fbin", "-no-opt", "-quiet", "-o", str(out), str(src)],
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
            lines.append("        " + text(i, targets, True))
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


SPACE_FIRE = True   # patch the interrupt so that the space bar sets fire_pressed as the fire buttons do


def space_fire(irq: list[str]) -> list[str]:
    """The original polls the two fire buttons in every interrupt until one is down.  Patched: when no button is down, a
    keyboard byte that has arrived is read, and the space bar (key down, raw code $40) counts as a fire button.  The keyboard
    sends one byte at a time and waits for the computer to acknowledge it with a pulse of the CIA's serial line (at least
    85 microseconds, set in control register A); without that it stops sending."""
    out = []
    for ln in irq:
        s = ln.strip()
        if s == "bne.b irq_buttons_done":
            out.append("        bne.b space_check   ; no button down: look at the keyboard (patch)")
        elif s == "move.w #$1,(_flags+Flags_fire_pressed).l":
            out.append("button_down:")
            out.append(ln)
        elif s.startswith("movem.l (a7)+,d0-d1"):
            out.append("irq_buttons_done:")
            out.append(ln)
        elif s == "irq_buttons_done:":
            continue
        else:
            out.append(ln)
    # the keyboard code goes after the button code, before the label the button code ends at
    at = next(n for n, ln in enumerate(out) if ln.strip() == "irq_buttons_done:")
    patch = [
        "        bra.b irq_buttons_done",
        "; patch: the space bar counts as a fire button",
        "space_check:",
        "        move.b $bfed01.l,d0         ; CIA-A interrupt flags (reading clears them); bit 3: a keyboard byte has arrived",
        "        btst #$3,d0",
        "        beq.b irq_buttons_done",
        "        move.b $bfec01.l,d0         ; the byte: the key code rotated left, inverted",
        "        ori.b #$40,$bfee01.l        ; acknowledge it: pull the serial line low (control register A, SPMODE) ...",
        "        moveq #$3c,d1",
        "keyboard_acknowledge:",
        "        dbra d1,keyboard_acknowledge   ; ... for 61 rounds, 85 microseconds or more",
        "        andi.b #$bf,$bfee01.l",
        "        not.b d0",
        "        ror.b #$1,d0                ; the key code, bit 7 set for a key that goes up",
        "        cmpi.b #$40,d0              ; space down",
        "        beq.b button_down",
    ]
    return out[:at] + patch + out[at:]


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


def main() -> None:
    mem = ram()
    a: list[str] = [
        "; Gods: the Amiga-specific routines, generated by scripts/gen_amiga_asm.py from the original's resident code.",
        "; The instruction sequences are the original's (the program's timing depends on how long they take, and each",
        "; instruction is checked against the original bytes); the C calling convention (arguments on the stack,",
        "; d2-d7/a2-a6 preserved) is added around them where C calls them.",
        "        xref " + ", ".join(f"_{n}" for n, _, _ in OBJECTS),
        "        xdef _blitter_wait, _blit_copy, _blit_copy_right, _screen_clear, _screen_restore, _plane5_set, _plane5_clear",
        "        xdef _blit_image, _blit_image_wide, _delay, _startup_delay, _interrupts_on, _load_next_stage, _vbl_wait, _irq_level3, _music_init",
        "        section .text,code",
        "",
        '        include "gods_offsets.i"',
        "",
        "; ---- blitter and screens ----",
        "; void blitter_wait(void), orig $002F1E: wait until the blitter has finished (and give it priority while waiting)",
    ]
    a += emit(mem, 0x2F1E, 0x2F3A)
    a += ["", "; void blit_copy(const void *src, void *dst), orig $001B7C: copy the 20 x 800 words at src to dst, A -> D",
          "_blit_copy:", "        move.l a6,-(sp)", "        movea.l 8(sp),a0", "        movea.l 12(sp),a1"]
    a += entry_label(emit(mem, 0x1B7C, 0x1BC6))[:-1] + ["        movea.l (sp)+,a6", "        rts", ""]
    a += ["; void blit_copy_right(const void *src, void *dst), orig $001BC6: only the right-hand 9 words of each row",
          "_blit_copy_right:", "        move.l a6,-(sp)", "        movea.l 8(sp),a0", "        movea.l 12(sp),a1"]
    a += entry_label(emit(mem, 0x1BC6, 0x1C18))[:-1] + ["        movea.l (sp)+,a6", "        rts", ""]
    a += ["; void screen_clear(void), orig $0030D4: clear the front screen"]
    a += emit(mem, 0x30D4, 0x30EE)
    a += ["", "; void screen_restore(void), orig $0030EE: copy the back screen over the front screen"]
    a += emit(mem, 0x30EE, 0x310E)
    a += ["", "; void plane5_set(void), orig $001D54: set the whole fifth bitplane of the front screen (d0/d1 for the original's d6/d7)"]
    a += scratch(emit(mem, 0x1D54, 0x1D7E))
    a += ["", "; void plane5_clear(void), orig $001D7E: clear the fifth bitplane of the front screen"]
    a += scratch(emit(mem, 0x1D7E, 0x1DA8))
    a += [""]

    save = "        movem.l d2-d7/a2-a4,-(sp)"
    load = ["        movea.l 40(sp),a0          ; dst", "        movea.l 44(sp),a1          ; src",
            "        move.w 50(sp),d0           ; bytes between the planes", "        move.w 54(sp),d1           ; pixel shift",
            "        move.w 58(sp),d7           ; rows"]
    a += ["; void blit_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows), orig $0032B6:",
          "; draw an image one word wide (the planes are `plane` bytes apart) through a mask built from its planes",
          "_blit_image:", save] + load
    a += entry_label(emit(mem, 0x32B6, 0x32CA))
    a += ["", "; void blit_image_wide(...), orig $0032CA: the same for an image two words wide", "_blit_image_wide:", save] + load
    a += entry_label(emit(mem, 0x32CA, 0x32DE))
    rest = emit(mem, 0x32DE, 0x3486)
    idx = max(n for n, ln in enumerate(rest) if ln.strip() == "rts")
    rest[idx:idx + 1] = ["        movem.l (sp)+,d2-d7/a2-a4", "        rts"]
    a += rest

    a += ["", "; ---- timing loops ----",
          "; void delay(void), the music's busy wait (inline at orig $0024F2 and $00250A): 301 rounds of a lone dbra",
          "_delay:", "        move.w #$12c,d0", "delay_loop:", "        dbra d0,delay_loop", "        rts",
          "", "; void startup_delay(void), orig $000C26: a long wait before the program starts (a register loop, 18 cycles a round)",
          "_startup_delay:", "        move.l #$f4240,d0", "startup_delay_loop:", "        subq.l #1,d0", "        bne.b startup_delay_loop", "        rts",
          "", "; void interrupts_on(void), the original's `move #$2000,sr` at orig $000C4A: until here the program runs with all",
          "; interrupts masked, so the vertical blank that came in the meantime is still pending and is taken at once",
          "_interrupts_on:", "        move.w #$2000,sr", "        rts",
          "", "; void load_next_stage(void), orig $000DC4: interrupts masked, then the loader that is still in memory (its entry at",
          "; $0150, reached in the original through `jmp $88`, the nops and `jmp $150` at $A2) loads the next part of the game,",
          "; which runs from $0084 as the first part did.  Does not return.",
          "_load_next_stage:", "        move.w #$2700,sr", "        jmp ($150).l"]

    if os.environ.get("GODS_ASM_NO_MUSIC"):
        OUT.write_text("\n".join(a) + "\n")
        return
    a += ["", "; ---- the vertical blank interrupt and the music ----",
          "; void vbl_wait(void), orig $002CDE: wait for the next vertical blank (the interrupt increments the frame count)"]
    a += emit(mem, 0x2CDE, 0x2CF4)
    a += ["", "; orig $001DD6: the level 3 interrupt (vertical blank): the music, the clock, the fire buttons, the colours"]
    irq = emit(mem, 0x1DD6, 0x1E3C)
    # the original saves d0/d1 with movem.w, which sign-extends them on the way back: harmless for its own code, which
    # only uses them as words, but not for C.  Save the whole registers (16 cycles more).
    irq = [ln.replace("movem.w d0-d1,-(a7)", "movem.l d0-d1,-(a7)   ; the original: movem.w")
             .replace("movem.w (a7)+,d0-d1", "movem.l (a7)+,d0-d1   ; the original: movem.w") for ln in irq]
    if SPACE_FIRE:
        irq = space_fire(irq)
    a += irq
    a += emit(mem, 0x2CF4, 0x2D4C)
    a += ["", "; void music_init(void), orig $002242: find the sample data, record each sample's address, silence the channels",
          "_music_init:", "        movem.l d2-d7/a2-a6,-(sp)"]
    music = emit(mem, 0x2242, 0x27E0)
    split = next(n for n, ln in enumerate(music) if ln.strip() == "music_step:")
    init = entry_label(music[:split])
    init = [x for ln in init for x in (["        movem.l (sp)+,d2-d7/a2-a6", "        rts"] if ln.strip() == "rts" else [ln])]   # also the channel silencing that follows
    a += init + ["", "; orig $0022E6: advance the music by one tick (called by the interrupt)"] + channel_fields(music[split:])
    a = localize(a)
    OUT.write_text("\n".join(a) + "\n")
    print(f"wrote {OUT} ({len(a)} lines)")


if __name__ == "__main__":
    main()
