"""Generate native/src/amiga/amiga.asm: the menu program's hardware routines as the original has them (the blitter's masked
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
SRC = PROJECT / "native" / "src"
OUT = SRC / "amiga" / "amiga.asm"

# the program as the loader leaves it (host/menu_image.lua): the initial values of the data in the code (frame 8000 has them used)
MEM = (PROJECT / "native" / "build" / "menu-image.bin").read_bytes()

ENTRIES = {
    0x171E4: "orig_blit_image", 0x171F6: "orig_blit_image_wide", 0x17208: "blit_image_body",
    # the sound system: the audio interrupt, the sequencer that the frame tick calls, starting a sound and what they use
    0x1F9EA: "_audio_irq", 0x1FA8C: "stop_sound_of_id", 0x1FACC: "random_in_range", 0x1FAF6: "play_sound_body",
    0x1FC58: "feed_voice", 0x1FCB8: "_sound_tick", 0x1FCDC: "voices_to_hardware", 0x1FCFA: "voice_to_hardware",
    0x1FD56: "voices_envelope", 0x1FD7A: "voice_envelope", 0x1FE3E: "start_voice", 0x1FF0E: "run_sequencer",
    0x1E2B8: "_keyboard_irq", 0x1E50C: "key_translate",
    0x1CFF8: "_vbl_irq", 0x1E39C: "_input_poll", 0x16C30: "_frame_tick",
    # the file layer: the entries the C code and the stubs use (the rest get labels from their addresses)
    0x1D536: "load_body", 0x1D80C: "scores_save_body", 0x1D966: "drives_idle_body", 0x1E254: "drive_bits",
    0xF518: "stub_clear_work_area", 0x1D05C: "stub_wait_vblank", 0x12D5C: "stub_text_draw_centered_other",
    0x1CF64: "stub_wait_frames_or_button", 0x16D76: "stub_message_request", 0x16E42: "stub_message_tick",
    0x202DE: "select_period_table", 0x20316: "reset_volume_slides", 0x2032E: "reset_period_slides", 0x633E: "_random_word",
}
# the command table of the sequencer: the opcode $80 + 4 n runs the handler of entry n
COMMAND_TABLE = 0x1FF90
COMMAND_TARGETS = [int.from_bytes(MEM[COMMAND_TABLE + 4 * n:COMMAND_TABLE + 4 * n + 4], "big") for n in range(22)]
# the data the sound code reaches pc-relative, inside the code
DATA_LABELS = {0x1E254: "drive_bits", 0x1FACA: "last_random", 0x1FE2E: "voice_pointers", 0x1FEFE: "voice_stacks", COMMAND_TABLE: "command_table",
               0x2034A: "tempo"}
# the labels inside the blit: a mask is made of the image's planes (three ORed, then the fourth), then each plane is drawn
# through it over the screen; with a pixel shift the blitter's B channel does the shifting
LABELS = {
    0x17224: "wait_mask_or3", 0x17264: "mask_modulo_ready", 0x17274: "wait_mask_or4", 0x1729C: "wait_before_draw",
    0x172DE: "draw_planes", 0x172FC: "wait_second_plane", 0x17314: "wait_third_plane", 0x1732C: "wait_fourth_plane",
    0x17346: "wait_shifted_draw", 0x1736A: "shifted_modulo", 0x17372: "shifted_set_modulos",
}
LABELS.update({
    0x1FC02: "store_variation_byte", 0x1FC22: "sync_busy_flags", 0x1FC52: "play_sound_exit", 0x1FD9C: "envelope_step",
    0x1FDC2: "envelope_store_level", 0x1FDC8: "envelope_not_attack", 0x1FDCE: "envelope_sustain", 0x1FDE8: "envelope_done",
    0x1FDEA: "envelope_decay", 0x1FE0C: "envelope_release", 0x1FF26: "sequencer_voice_loop", 0x1FF7E: "sequencer_next_byte",
    0x20066: "sequencer_wait", 0x20102: "sequencer_finish_voice", 0x20124: "sequencer_next_voice", 0x1D4CA: "cache_add_block",
    0x1D4E4: "copy_block_512", 0x1D4F0: "find_file_entry", 0x1D586: "load_with_message", 0x1D794: "screen_save_planes",
    0x1D7AC: "screen_restore_planes", 0x1D8BC: "draw_loading_text", 0x1D8F6: "drives_detect", 0x1D97E: "drive_select",
    0x1D9B4: "drive_deselect", 0x1D9E2: "disk_transfer", 0x1DADC: "disk_read_track", 0x1DBCA: "disk_write_track",
    0x1DC58: "track_build_mfm", 0x1DC9E: "disk_read_sectors", 0x1DCEA: "error_18", 0x1DCEE: "error_1b",
    0x1DCF2: "error_19", 0x1DCF6: "disk_process_track", 0x1DE22: "sector_offset", 0x1DE32: "sector_mark_done",
    0x1DE42: "sector_clear_headers", 0x1DE54: "sector_decode_header", 0x1DE76: "mfm_decode_long", 0x1DE8C: "sector_verify_header",
    0x1DE9C: "sector_header_checksum", 0x1DEA2: "checksum_words", 0x1DEBC: "mfm_fill_track", 0x1DEEE: "blitter_decode_start",
    0x1DF24: "blitter_encode_start", 0x1DFD8: "wait_blitter", 0x1DFE4: "blitter_init", 0x1E00E: "mfm_encode_long",
    0x1E01C: "mfm_fix_clock_bits", 0x1E03A: "mfm_spread_bits", 0x1E066: "dma_read_start", 0x1E0A4: "dma_read_wait",
    0x1E0C8: "dma_stop", 0x1E0DC: "drive_motor_off", 0x1E0EC: "drive_motor_pulse", 0x1E108: "drive_motor_off_end",
    0x1E10A: "drive_motor_on", 0x1E124: "drive_seek", 0x1E178: "drive_wait_ready", 0x1E1AE: "drive_side_direction",
    0x1E1D8: "drive_write_select", 0x1E1E4: "drive_select_bits", 0x1E218: "delay_ticks", 0x1E22C: "timer_poll",
    0x1E23A: "timer_start", 0x1E252: "timer_done", 0x1E300: "keyboard_irq_exit",
})
LABELS.update({t: f"cmd_{0x80 + 4 * n:02x}" for n, t in enumerate(COMMAND_TARGETS)})
SIZES = {"char": 1, "uint8_t": 1, "int8_t": 1, "uint16_t": 2, "int16_t": 2, "uint32_t": 4, "int32_t": 4, "struct Rec6": 6, "struct Rec10": 10,
         "struct Rec80": 80, "struct Entry8": 8, "struct MemLists": 20, "struct Image": 8, "struct AssetEntry": 8, "struct Voice": 148, "void": 2, "struct Slot8": 8, "struct Slot12": 12, "struct SavedByte": 6, "struct PathDescription": 28, "struct LeverFlag": 6, "struct LevelEnd": 34, "struct LevelEnds": 580, "struct Unit": 24, "struct Monster": 428, "struct Bolt": 22, "struct Planner": 34, "struct PlanItem": 8, "struct WalkGroup": 18, "struct WalkMode": 8}


def size_of(decl: str) -> int:
    """The size in bytes of a declaration such as `uint16_t %s[22]` or `uint8_t *%s`."""
    base = decl.split("%s")[0].replace("const ", "").replace("volatile ", "").strip()
    count = re.search(r"\[(\d+)\]", decl)
    if "*" in base:
        return 4 * (int(count.group(1)) if count else 1)
    return SIZES[base] * (int(count.group(1)) if count else 1)


VARIABLES, TABLES = S.parse()
OBJECTS = sorted([(n, a, size_of(d)) for n, a, d, _ in VARIABLES] +
                 [(n, a, (sum(f[0] for f in S.STRUCT_FIELDS[t.split()[1]]) if t.startswith('struct ') else S.WIDTH[t]) * c) for n, a, t, c, _ in TABLES], key=lambda o: o[1])


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


A.ENTRIES, A.LABELS, A.SRC, A.PC_LABELS = ENTRIES, LABELS, SRC, DATA_LABELS
A.CODE_RANGES = [(0x171E4, 0x17380), (0x633E, 0x635A)]   # the sound pieces are added below
A.data_symbol, A.short_symbol = data_symbol, short_symbol


# the sound system's pieces: (first, end, kind); the data pieces are between the code pieces
SOUND = [(0x1F9EA, 0x1FA8C, "code"), (0x1FA8C, 0x1FACA, "code"), (0x1FACA, 0x1FACC, "bytes"), (0x1FACC, 0x1FAF6, "code"),
         (0x1FAF6, 0x1FE2E, "code"), (0x1FE2E, 0x1FE3E, "longs"), (0x1FE3E, 0x1FEFE, "code"), (0x1FEFE, 0x1FF0E, "longs"),
         (0x1FF0E, COMMAND_TABLE, "code"), (COMMAND_TABLE, COMMAND_TABLE + 88, "table"), (COMMAND_TABLE + 88, 0x20346, "code"),
         (0x20346, 0x2034A, "words"), (0x2034A, 0x2034C, "words")]


FILE_LAYER = (0x1D4CA, 0x1E254)   # the disk drives and the file reading, as the original has them (a few of the C functions twice)
A.CODE_RANGES += [(lo, hi) for lo, hi, kind in SOUND if kind == "code"] + [(0x1CFF8, 0x1D058), FILE_LAYER, (0x1E2B8, 0x1E31C), (0x1E50C, 0x1E5A8)]


def sound_system() -> list[str]:
    """The Paula sound driver: the audio interrupt, the sequencer (a command interpreter over four voices) and what
    they use, from the original, with the C-callable entries."""
    out = ["", "; ---- the sound system (orig $01F9EA-$02034B): four voices, a sequencer that interprets the command bytes of the",
           "; tracks, the audio interrupt that feeds each voice its next sample.  Entries for C: audio_irq (the interrupt),",
           "; sound_tick (once a frame), play_sound(id), random_word (orig $00633E).",
           "        xdef _audio_irq, _sound_tick, _play_sound, _random_word, _voice_start", "        xdef last_random, voice_pointers, voice_stacks, tempo   ; the sound code's own state (the takeover test copies it)", "",
           "; void play_sound(uint16_t id, uint16_t priority): the sound the id names is started on a free voice (orig $01FAF6)",
           "_play_sound:", "        move.l 4(sp),d0", "        move.l 8(sp),d1", "        bra.w play_sound_body", "",
           "; void voice_start(uint16_t sound, uint16_t voice): the routine at orig $01FE3E (d0 the sound, d1 the voice) that stop_sound calls with 22", "_voice_start:",
           "        move.l 4(sp),d0", "        move.l 8(sp),d1", "        jmp (start_voice).l", ""]
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
    return out + [l.replace("jsr $16c30(pc)", "jsr _frame_tick").replace("jsr (_shot_deltas+$7c).l", "jsr _frame_tick") for l in A.emit(MEM, 0x1CFF8, 0x1D058)]


STUBS = [   # the original's routines that are C now, called from the file layer with their registers: (address, C name, arguments)
    (0xF518, "clear_work_area", []), (0x1D05C, "wait_vblank", []), (0x12D5C, "text_draw_centered_other", ["a0"]),
    (0x1CF64, "wait_frames_or_button", ["d0"]), (0x16D76, "message_request", ["d7"]), (0x16E42, "message_tick", []),
]


def file_layer() -> list[str]:
    """The disk layer: the drive control, track reading and writing with the MFM coding and the directory search, from the
    original (the Amiga's disk hardware), with the stubs it calls C through and the entries C calls it by."""
    out = ["", "; ---- the file layer (orig $01D4CA-$01E253): drive selection and motor, track reads and writes through the disk DMA,",
           "; the MFM decoding, the directory and the loading of a file.  It keeps its registers' meaning (a5 the track buffer,",
           "; a6 the drive's frame); only these entries are for C:",
           "        xref " + ", ".join(f"_{n}" for _, n, _ in STUBS),
           "        xdef _file_read, _drives_idle, _scores_save", ""]
    out += ["; uint32_t file_read(const char *name, void *dest): load the file; the result is -1 when it failed (orig $01D536 with",
            "; the name in d0 and the destination in a0; the original reports by the negative flag)",
            "_file_read:", "        movem.l d2-d7/a2-a6,-(sp)", "        move.l 48(sp),d0", "        movea.l 52(sp),a0",
            "        jsr load_body", "        smi d0", "        ext.w d0", "        ext.l d0", "        movem.l (sp)+,d2-d7/a2-a6", "        rts", "",
            "; void scores_save(void), orig $01CEEA (a jump to $01D80C): the 216 bytes at $3C2 are written to the file \\hiscores.bin",
            "_scores_save:", "        movem.l d2-d7/a2-a6,-(sp)", "        jsr scores_save_body", "        movem.l (sp)+,d2-d7/a2-a6", "        rts", "",
            "; void drives_idle(void), orig $01D966: the drives that are present are deselected",
            "_drives_idle:", "        movem.l d2-d7/a2-a6,-(sp)", "        jsr drives_idle_body", "        movem.l (sp)+,d2-d7/a2-a6", "        rts", ""]
    for addr, name, regs in STUBS:
        out += [f"; the original's routine at ${addr:x} is C: {name}", f"{ENTRIES[addr]}:", "        movem.l d0-d7/a0-a6,-(sp)"]
        for r in reversed(regs):
            out.append(f"        move.l {r},-(sp)")
        out += [f"        jsr _{name}", f"        lea {4 * len(regs)}(sp),sp" if regs else "", "        movem.l (sp)+,d0-d7/a0-a6", "        rts", ""]
    out = [ln for ln in out if ln != "" or True]
    # the code at $1D608 pushes the address $1D61C (the rest of the routine) to return to: that is a label of this code, not the original's
    out += [l.replace("move.l #$1d61c,-(a7)", "move.l #L1d61c,-(a7)") for l in A.emit(MEM, FILE_LAYER[0], FILE_LAYER[1], {0x1D61C})]
    out += ["drive_bits:", "        dc.w " + ",".join(f"${int.from_bytes(MEM[a:a + 2], 'big'):04x}" for a in range(0x1E254, 0x1E260, 2)), ""]
    return out


def keyboard() -> list[str]:
    """The keyboard interrupt (level 2, the CIA's serial register): reads the key, acknowledges it with the handshake pulse and
    keeps the keys that are down (key_down) and the last key as a request (pending_request)."""
    out = ["", "; ---- the keyboard interrupt (orig $01E2B8-$01E31B) and the key translation it calls (orig $01E50C-$01E5A7)",
           "        xdef _keyboard_irq", ""]
    handler = A.emit(MEM, 0x1E2B8, 0x1E31C)
    # not the original's: it reads the serial flag from the flags it read when it started and skips the key when it is not set;
    # the flag of a key that came while the handler for the one before was running is cleared by that handler's last read of the
    # flags, so with two key events close together the second was lost (a key whose release is lost stays down).  Here the key is
    # always taken (a spurious call reads the byte it wrote itself, 0, a release of an unused key)
    at = handler.index("        btst.l #$3,d2")
    assert handler[at + 1].strip().startswith("beq.w")
    handler[at:at + 2] = ["        ; (the original tests the serial flag here and skips the key without it: it lost the second of two close key events)"]
    out += handler
    out += [""] + A.emit(MEM, 0x1E50C, 0x1E5A8)
    out += ["", "; void sr_set(uint16_t value): the status register is set (not in the original; the start-up needs it in C)",
            "        xdef _sr_set", "_sr_set:", "        move.w 6(sp),d0", "        move.w d0,sr", "        rts", "",
            "; void jump_with_stack(void (*address)(void), uint8_t *stack): the stack is changed and the code entered (not in the original)",
            "        xdef _jump_with_stack", "_jump_with_stack:", "        movea.l 4(sp),a0", "        movea.l 8(sp),a1", "        movea.l a1,sp", "        jmp (a0)", "",
            "; game_loop: where the original's game loop at $2C20 is sent (game_loop_install patches a jmp there): the loop's body is the C game_frame,",
            "; run on a stack of its own (the original's is in memory that files are loaded to) with every register kept; then the loop goes on at its top",
            "        xref _game_frame", "        xdef _game_loop", "_game_loop:", "        movem.l d0-d7/a0-a6,-(sp)",
            "        lea.l game_loop_sp(pc),a0", "        move.l sp,(a0)", "        lea.l $eb000.l,sp", "        jsr _game_frame", "        movea.l game_loop_sp(pc),sp",
            "        movem.l (sp)+,d0-d7/a0-a6", "        jmp $2c20.l", "game_loop_sp: dc.l 0", ""]
    return out


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
    lines += file_layer()
    lines += keyboard()
    OUT.write_text("\n".join(A.localize(lines, calls_global=False)) + "\n")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
