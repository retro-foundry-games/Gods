"""Generate native/menu_amiga.asm: the menu program's hardware routines as the original has them (the blitter's masked
blit), with C-callable wrappers; see gen_intro_amiga_asm.py, which does the same for the intro.

    uv run python projects/gods/scripts/gen_menu_amiga_asm.py
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import amiga_asm as A  # noqa: E402
import gen_menu_symbols as S  # noqa: E402

PROJECT = Path(__file__).resolve().parents[1]
NATIVE = PROJECT / "native"
OUT = NATIVE / "menu_amiga.asm"

MEM = (PROJECT / "build" / "menu-ram.bin").read_bytes()

ENTRIES = {
    0x171E4: "orig_blit_image", 0x171F6: "orig_blit_image_wide", 0x17208: "blit_image_body",
    # the sound system: the audio interrupt, the sequencer that the frame tick calls, starting a sound and what they use
    0x1F9EA: "_audio_irq", 0x1FA8C: "stop_sound_of_id", 0x1FACC: "random_in_range", 0x1FAF6: "play_sound_body",
    0x1FC58: "feed_voice", 0x1FCB8: "_sound_tick", 0x1FCDC: "voices_to_hardware", 0x1FCFA: "voice_to_hardware",
    0x1FD56: "voices_envelope", 0x1FD7A: "voice_envelope", 0x1FE3E: "start_voice", 0x1FF0E: "run_sequencer",
    0x1CFF8: "_vbl_irq", 0x1E39C: "_input_poll", 0x16C30: "_frame_tick",
    0x202DE: "select_period_table", 0x20316: "reset_volume_slides", 0x2032E: "reset_period_slides", 0x633E: "_random_word",
}
# the command table of the sequencer: the opcode $80 + 4 n runs the handler of entry n
COMMAND_TABLE = 0x1FF90
COMMAND_TARGETS = [int.from_bytes(MEM[COMMAND_TABLE + 4 * n:COMMAND_TABLE + 4 * n + 4], "big") for n in range(22)]
# the data the sound code reaches pc-relative, inside the code
DATA_LABELS = {0x1FACA: "last_random", 0x1FE2E: "voice_pointers", 0x1FEFE: "voice_stacks", COMMAND_TABLE: "command_table",
               0x2034A: "tempo"}
# the labels inside the blit: a mask is made of the image's planes (three ORed, then the fourth), then each plane is drawn
# through it over the screen; with a pixel shift the blitter's B channel does the shifting
LABELS = {
    0x17224: "wait_mask_or3", 0x17264: "mask_modulo_ready", 0x17274: "wait_mask_or4", 0x1729C: "wait_before_draw",
    0x172DE: "draw_planes", 0x172FC: "wait_second_plane", 0x17314: "wait_third_plane", 0x1732C: "wait_fourth_plane",
    0x17346: "wait_shifted_draw", 0x1736A: "shifted_modulo", 0x17372: "shifted_set_modulos",
}
LABELS.update({t: f"cmd_{0x80 + 4 * n:02x}" for n, t in enumerate(COMMAND_TARGETS)})
SIZES = {"uint8_t": 1, "int8_t": 1, "uint16_t": 2, "int16_t": 2, "uint32_t": 4, "int32_t": 4, "struct Rec6": 6, "struct Rec10": 10,
         "struct Rec80": 80, "struct Entry8": 8, "struct MemLists": 20, "struct Image": 8}


def size_of(decl: str) -> int:
    """The size in bytes of a declaration such as `uint16_t %s[22]` or `uint8_t *%s`."""
    base = decl.split("%s")[0].replace("const ", "").strip()
    count = re.search(r"\[(\d+)\]", decl)
    if "*" in base:
        return 4 * (int(count.group(1)) if count else 1)
    return SIZES[base] * (int(count.group(1)) if count else 1)


VARIABLES, TABLES = S.parse()
OBJECTS = sorted([(n, a, size_of(d)) for n, a, d, _ in VARIABLES] +
                 [(n, a, S.WIDTH[t] * c) for n, a, t, c, _ in TABLES], key=lambda o: o[1])


def data_symbol(addr: int) -> str | None:
    for name, a, size in OBJECTS:
        if a <= addr < a + size:
            return f"_{name}" if addr == a else f"_{name}+${addr - a:x}"
    return None


def short_symbol(addr: int) -> str:
    for name, a, _ in OBJECTS:
        if a == addr and a < 0x8000:
            return f"Addr_{name}"
    return f"${addr:x}"


A.ENTRIES, A.LABELS, A.NATIVE, A.PC_LABELS = ENTRIES, LABELS, NATIVE, DATA_LABELS
A.CODE_RANGES = [(0x171E4, 0x17380), (0x633E, 0x635A)]   # the sound pieces are added below
A.data_symbol, A.short_symbol = data_symbol, short_symbol


# the sound system's pieces: (first, end, kind); the data pieces are between the code pieces
SOUND = [(0x1F9EA, 0x1FA8C, "code"), (0x1FA8C, 0x1FACA, "code"), (0x1FACA, 0x1FACC, "bytes"), (0x1FACC, 0x1FAF6, "code"),
         (0x1FAF6, 0x1FE2E, "code"), (0x1FE2E, 0x1FE3E, "longs"), (0x1FE3E, 0x1FEFE, "code"), (0x1FEFE, 0x1FF0E, "longs"),
         (0x1FF0E, COMMAND_TABLE, "code"), (COMMAND_TABLE, COMMAND_TABLE + 88, "table"), (COMMAND_TABLE + 88, 0x20346, "code"),
         (0x20346, 0x2034A, "words"), (0x2034A, 0x2034C, "words")]


A.CODE_RANGES += [(lo, hi) for lo, hi, kind in SOUND if kind == "code"] + [(0x1CFF8, 0x1D058)]


def sound_system() -> list[str]:
    """The Paula sound driver: the audio interrupt, the sequencer (a command interpreter over four voices) and what
    they use, from the original, with the C-callable entries."""
    out = ["", "; ---- the sound system (orig $01F9EA-$02034B): four voices, a sequencer that interprets the command bytes of the",
           "; tracks, the audio interrupt that feeds each voice its next sample.  Entries for C: audio_irq (the interrupt),",
           "; sound_tick (once a frame), play_sound(id), random_word (orig $00633E).",
           "        xdef _audio_irq, _sound_tick, _play_sound, _random_word", "",
           "; void play_sound(uint16_t id, uint16_t priority): the sound the id names is started on a free voice (orig $01FAF6)",
           "_play_sound:", "        move.l 4(sp),d0", "        move.l 8(sp),d1", "        bra.w play_sound_body", ""]
    # every address that a branch of any piece goes to gets its label, in the piece that has the address
    force = set(COMMAND_TARGETS)
    for lo, hi, kind in SOUND:
        if kind == "code":
            force |= {A.branch_target(i) for i in A.disassemble(MEM, lo, hi) if A.is_branch(i)}
    for lo, hi, kind in SOUND:
        label = DATA_LABELS.get(lo)
        if kind == "code":
            out += A.emit(MEM, lo, hi, force)
        else:
            if label:
                out.append(f"{label}:")
            if kind == "bytes":
                out.append("        dc.b " + ",".join(f"${b:02x}" for b in MEM[lo:hi]))
            elif kind == "words":
                out.append("        dc.w " + ",".join(f"${int.from_bytes(MEM[a:a + 2], 'big'):04x}" for a in range(lo, hi, 2)))
            elif kind == "longs":
                out.append("        dc.l " + ",".join(f"${int.from_bytes(MEM[a:a + 4], 'big'):x}" for a in range(lo, hi, 4)))
            else:   # the command table: the addresses of code
                out += [f"        dc.l {LABELS[t]}" for t in COMMAND_TARGETS]
        out.append("")
    out += ["; uint16_t random_word(void), orig $00633E: the next word of a table, round and round; only d0 is changed"]
    out += A.emit(MEM, 0x633E, 0x635A)
    return out


def interrupt() -> list[str]:
    """The vertical blank interrupt (level 3): acknowledge, poll the input, copy the colour shadow table to the chip when
    it changed, count the frames and run the frame tick."""
    out = ["", "; ---- the vertical blank interrupt (orig $01CFF8-$01D056): the vector of level 3.  It calls input_poll and frame_tick (C).",
           "        xref _input_poll, _frame_tick", "        xdef _vbl_irq", ""]
    return out + [l.replace("jsr $16c30(pc)", "jsr _frame_tick") for l in A.emit(MEM, 0x1CFF8, 0x1D058)]


def main() -> None:
    lines = [
        "; Gods, the menu program: its hardware routines, generated by scripts/gen_menu_amiga_asm.py from the original's code.",
        "; The instruction sequences are the original's (each is checked against the original bytes); the C calling convention",
        "; (arguments on the stack, d2-d7/a2-a6 preserved) is added around them.",
        "        xref " + ", ".join(f"_{n}" for n, _, _ in OBJECTS),
        "        xdef _blit_image, _blit_image_wide",
        "        section .text,code",
        "",
        "; the variables that the original reaches with an absolute short address are equates (their symbols are absolute)",
    ]
    lines += [f"Addr_{n} = ${a:x}" for n, a, _ in OBJECTS if a < 0x8000]
    save = "        movem.l d2-d7/a2-a4,-(sp)"
    load = ["        movea.l 40(sp),a0          ; dst", "        movea.l 44(sp),a1          ; src",
            "        move.w 50(sp),d0           ; bytes between the planes", "        move.w 54(sp),d1           ; pixel shift",
            "        move.w 58(sp),d7           ; rows"]
    lines += ["", "; void blit_image(uint8_t *dst, const uint8_t *src, uint16_t plane, uint16_t shift, uint16_t rows), orig $0171E4:",
              "; draw an image one word wide (the planes are `plane` bytes apart) through a mask built from its planes",
              "_blit_image:", save] + load
    lines += A.entry_label(A.emit(MEM, 0x171E4, 0x171F6))
    lines += ["", "; void blit_image_wide(...), orig $0171F6: the same for an image two words wide", "_blit_image_wide:", save] + load
    lines += A.entry_label(A.emit(MEM, 0x171F6, 0x17208))
    rest = A.emit(MEM, 0x17208, 0x17380)
    idx = max(n for n, ln in enumerate(rest) if ln.strip() == "rts")
    rest[idx:idx + 1] = ["        movem.l (sp)+,d2-d7/a2-a4", "        rts"]
    lines += rest
    lines += sound_system()
    lines += interrupt()
    OUT.write_text("\n".join(A.localize(lines)) + "\n")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
