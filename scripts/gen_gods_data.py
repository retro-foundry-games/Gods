#!/usr/bin/env python3
"""Copy the game's initial data from the recording into native/data.c (C initializers) and native/gods.ld.

    uv run python projects/gods/scripts/gen_gods_data.py

The initial image is the chip RAM of frame 1746 of captures/ref100k: the last frame before the program's `main` runs,
with the music module, the pictures and every variable as the loader left them.  Each object below is defined in C with
its initial value; the linker script puts it back at the address the original keeps it at, so memory matches.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gods_analysis as G  # noqa: E402

FRAME = 1746
NATIVE = G.ROOT / "native"
ram = G.ram_at(FRAME)

objects: list[tuple[str, int, str]] = []   # (name, address, C definition)


def u8(a): return ram[a]
def u16(a): return struct.unpack(">H", ram[a:a + 2])[0]
def u32(a): return struct.unpack(">I", ram[a:a + 4])[0]


def lines(items, per: int, fmt: str, indent: str = "    ") -> str:
    out = []
    for i in range(0, len(items), per):
        out.append(indent + ", ".join(fmt % v for v in items[i:i + per]) + ",")
    return "\n".join(out)


def add(name: str, addr: int, text: str) -> None:
    objects.append((name, addr, text))


def scalar(name, addr, ctype, size):
    v = {1: u8, 2: u16, 4: u32}[size](addr)
    add(name, addr, f"{ctype} {name} = {v:#x};" if v else f"{ctype} {name};")


# ---- variables -------------------------------------------------------------------------------------------------------
add("palette_ptr", 0x1af6, "u16 *palette_ptr = display.palette.colour;   /* the original holds the address of the colour table */")
add("screen_front", 0x2940, "struct Screen *screen_front;")
add("screen_back", 0x2944, "struct Screen *screen_back;")
scalar("draw_flag", 0x2cb0, "u8", 1)
scalar("font_offset", 0x394a, "u16", 2)
scalar("scroll_y", 0x169a, "u16", 2)
scalar("scroll_step", 0x169e, "u16", 2)
scalar("glyph_plane_size", 0xe96, "u16", 2)
assert not any(ram[0x3486:0x3888])
add("blank_plane", 0x3486, "const u16 blank_plane[513] = { 0 };   /* zeros, up to the table at $3888 */")

def struct_def(ctype, name, addr, fields, extra=""):
    """A struct from the original's bytes; fields = (name, offset, size); zero fields are left out."""
    init = []
    for fname, off, size in fields:
        v = {1: u8, 2: u16, 4: u32}[size](addr + off)
        if v:
            init.append(f".{fname} = {v:#x}")
    body = ", ".join(init + ([extra] if extra else []))
    add(name, addr, f"{ctype} {name} = {{ {body} }};" if body else f"{ctype} {name};")


struct_def("struct Vblank", "vblank", 0x2cc2, [("palette_dirty", 0, 2), ("saturating", 2, 2), ("count", 4, 2)])
struct_def("struct Flags", "flags", 0x1dd2, [("music_enabled", 0, 2), ("fire_pressed", 2, 2)])
struct_def("struct GameClock", "game_clock", 0x1e3c, [("input_seen", 0, 2), ("ticks", 2, 4)])
struct_def("struct MusicState", "music", 0x284c, [("speed", 0, 1), ("order", 1, 1), ("row", 2, 2), ("tick", 4, 1),
                                                  ("jump", 5, 1), ("dma", 6, 2)])
colours = [u16(0x294a + 2 * i) for i in range(32)]
colours = [u16(0x294a + 2 * i) for i in range(32)]
assert not any(ram[0x2990:0x2990 + 792]) and u16(0x298e) == 0
add("display", 0x2948, "struct DisplayData display = {\n    .palette = { .fade_done = %d, .unused = %d, .colour = {%s} },\n    .blit = { .plane_stride = %#x },\n};"
    % (u8(0x2948), u8(0x2949), ", ".join(f"{c:#05x}" for c in colours), u32(0x298a)))

# ---- the music replayer's tables and channels ---------------------------------------------------------------------------
add("vibrato_table", 0x27e0, "const u8 vibrato_table[32] = {\n" + lines(list(ram[0x27e0:0x2800]), 16, "%d") + "\n};")
add("period_table", 0x2800, "const u16 period_table[38] = {\n" + lines([u16(0x2800 + 2 * i) for i in range(38)], 8, "%#06x") + "\n};")
add("sample_start", 0x2854, "u32 sample_start[31];   /* filled in by music_init */")
fields = [("cell", 0, 4), ("sample", 4, 4), ("length", 8, 2), ("loop", 10, 4), ("loop_length", 14, 2), ("period", 16, 2),
          ("volume", 18, 2), ("dma", 20, 2), ("porta_down", 22, 1), ("porta_speed", 23, 1), ("porta_target", 24, 2),
          ("vibrato", 26, 1), ("vibrato_pos", 27, 1)]
ch = []
for i in range(4):
    base = 0x28d0 + 28 * i
    init = []
    for name, off, size in fields:
        raw = ram[base + off:base + off + size]
        if any(raw):
            if name == "cell":
                init.append(".cell = {" + ", ".join(f"{b:#04x}" for b in raw) + "}")
            else:
                init.append(f".{name} = {int.from_bytes(raw, 'big'):#x}")
    ch.append("    { " + ", ".join(init) + " }," if init else "    { 0 },")
add("channel", 0x28d0, "struct Channel channel[4] = {\n" + "\n".join(ch) + "\n};")

# ---- the module ----------------------------------------------------------------------------------------------------
M = 0x1b6f2
order = list(ram[M + 952:M + 1080])
lens = [u16(M + 20 + 30 * i + 22) for i in range(31)]
total = 0x43c + (max(order) + 1) * 1024 + sum(lens) * 2
samples = []
for i in range(31):
    b = M + 20 + 30 * i
    name_bytes = ", ".join(f"{x:#04x}" for x in ram[b:b + 22])
    samples.append("        { .name = {%s}, .length = %d, .volume = %#x, .loop_start = %d, .loop_length = %d },"
                   % (name_bytes, u16(b + 22), u16(b + 24), u16(b + 26), u16(b + 28)))
body = list(ram[M + 0x43c:M + total])
add("module", M, "struct Module module = {\n    .title = {" + ", ".join(f"{x:#04x}" for x in ram[M:M + 20]) + "},\n    .sample = {\n"
    + "\n".join(samples) + f"\n    }},\n    .song_length = {ram[M + 950]},\n    .restart = {ram[M + 951]},\n    .order = {{\n"
    + lines(order, 16, "%d", "        ") + "\n    },\n    .tag = {" + ", ".join(f"'{chr(x)}'" for x in ram[M + 1080:M + 1084])
    + "},\n    /* the patterns, then the sample data */\n    .data = {\n" + lines(body, 24, "%d", "        ") + "\n    },\n};")

# ---- pictures and the image table: one block ------------------------------------------------------------------------------
def picture_init(addr):
    pal_ = [u16(addr + 2 + 2 * i) for i in range(16)]
    words = [u16(addr + 34 + 2 * i) for i in range(16000)]
    return ("{\n        .header = %#x,\n        .palette = {\n" % u16(addr) + lines(pal_, 8, "%#06x", "            ")
            + "\n        },\n        .data = {\n" + lines(words, 12, "%#06x", "            ") + "\n        },\n    }")

add("graphics", 0x3a62, "struct Graphics graphics = {\n    .title = " + picture_init(0x3a62) + ",\n    .glyph_sheet = "
    + picture_init(0xb784) + ",\n    .background = " + picture_init(0x134a6) + ",\n    /* the table: how to find each image on the glyph sheet; the program turns it into the images' sizes and addresses */\n    .images = { .source = { .count = %d, .entry = {\n%s\n    } } },\n};" % (u16(0x1b1c8), "\n".join(
        "        { %d, %d, %d, %d, %d }," % tuple(u16(0x1b1ca + 10 * i + 2 * k) for k in range(5)) for i in range(u16(0x1b1c8)))))

# ---- copper lists, the vectors, the screens and the image table -------------------------------------------------------
def copper(name, addr):
    w = [u16(addr + 2 * i) for i in range(42)]
    add(name, addr, f"struct CopperList {name} = {{\n    .words = {{\n" + lines(w[:40], 8, "%#06x", "        ")
        + f"\n    }},\n    .end = {{ {w[40]:#06x}, {w[41]:#06x} }},\n}};")

copper("copper_a", 0xa8)
copper("copper_b", 0xfc)
add("vectors", 0x0, "volatile u32 vectors[42] = {\n" + lines([u32(4 * i) for i in range(42)], 4, "%#010x") + "\n};")
add("screen_a", 0x763c0, "struct Screen screen_a;   /* the original's bitplanes; drawn to by the program */")
add("screen_b", 0x6c780, "struct Screen screen_b;")

# ---- write data.c and gods.ld -----------------------------------------------------------------------------------------
objects.sort(key=lambda o: o[1])
(NATIVE / "data.c").write_text(
    "/* Gods: the game's data, copied from the original (the chip RAM of frame %d of the recording, just before the\n"
    " * program starts) and written as C initializers.  Generated by scripts/gen_gods_data.py; declared in gods.h; placed\n"
    " * at the original addresses by gods.ld.  Variables that are zero at that point have no initializer. */\n"
    "#include \"gods.h\"\n\n" % FRAME + "\n\n".join(t for _, _, t in objects) + "\n")
ld = ["/* Gods: link script for m68k-amigaos-ld -m amiga.  Generated by scripts/gen_gods_data.py.",
      " *",
      " * The data hunk is laid out so that an object's offset in it is the address the original keeps it at; it is",
      " * loaded at address 0, and only the bytes of the listed objects are ever written to memory.  SUBALIGN(1) because",
      " * the original keeps byte variables at odd addresses.  The code, and the data that only the code uses, follow in",
      " * a hunk of their own, which the loader places in memory the original does not use. */",
      "SECTIONS {", "    .data : SUBALIGN(1) {"]
for name, addr, _ in objects:
    ld.append(f"        . = 0x{addr:x}; *(.data.{name}) *(.rodata.{name}) *(.bss.{name})")
ld += ["    }", "    .text : { *(.text .text.*) *(.rodata .rodata.*) *(.data .data.*) *(.bss .bss.*) *(COMMON) }", "}"]
(NATIVE / "gods.ld").write_text("\n".join(ld) + "\n")
(NATIVE / "gods.objects").write_text("".join(f"{name} {addr:x}\n" for name, addr, _ in objects))
print(f"data.c: {len(objects)} objects, {(NATIVE / 'data.c').stat().st_size} bytes of source")
