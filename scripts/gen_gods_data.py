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
add("palette_ptr", 0x1af6, "u16 *palette_ptr = palette;   /* the original holds the address of the colour table */")
scalar("palette_dirty", 0x2cc2, "volatile u16", 2)
scalar("vbl_saturating", 0x2cc4, "volatile u16", 2)
scalar("vbl_count", 0x2cc6, "volatile u16", 2)
add("screen_front", 0x2940, "struct Screen *screen_front;")
add("screen_back", 0x2944, "struct Screen *screen_back;")
scalar("fade_done", 0x2948, "volatile u8", 1)
pal = [u16(0x294a + 2 * i) for i in range(32)]
add("palette", 0x294a, "u16 palette[32] = {\n" + lines(pal, 8, "%#06x") + "\n};")
scalar("music_enabled", 0x1dd2, "volatile u16", 2)
scalar("fire_pressed", 0x1dd4, "volatile u16", 2)
scalar("input_seen", 0x1e3c, "volatile u16", 2)
scalar("ticks", 0x1e3e, "volatile u32", 4)
scalar("draw_flag", 0x2cb0, "u8", 1)
scalar("font_offset", 0x394a, "u16", 2)
scalar("scroll_y", 0x169a, "u16", 2)
scalar("scroll_step", 0x169e, "u16", 2)
scalar("music_speed", 0x284c, "u8", 1)
scalar("music_order", 0x284d, "u8", 1)
scalar("music_row", 0x284e, "u16", 2)
scalar("music_tick", 0x2850, "u8", 1)
scalar("music_jump", 0x2851, "u8", 1)
scalar("music_dma", 0x2852, "u16", 2)

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

# ---- pictures ------------------------------------------------------------------------------------------------------
def picture(name, addr):
    hdr = u16(addr)
    pal_ = [u16(addr + 2 + 2 * i) for i in range(16)]
    words = [u16(addr + 34 + 2 * i) for i in range(16000)]
    add(name, addr, f"struct Picture {name} = {{\n    .header = {hdr:#x},\n    .palette = {{\n" + lines(pal_, 8, "%#06x", "        ")
        + "\n    },\n    .data = {\n" + lines(words, 12, "%#06x", "        ") + "\n    },\n};")

picture("title_picture", 0x3a62)
picture("background_picture", 0x134a6)

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
add("images", 0x1b1c8, "struct Image images[165];   /* filled in at the start of the program */")

# ---- write data.c and gods.ld -----------------------------------------------------------------------------------------
objects.sort(key=lambda o: o[1])
(NATIVE / "data.c").write_text(
    "/* Gods: the game's data, copied from the original (the chip RAM of frame %d of the recording, just before the\n"
    " * program starts) and written as C initializers.  Generated by scripts/gen_gods_data.py; declared in gods.h; placed\n"
    " * at the original addresses by gods.ld.  Variables that are zero at that point have no initializer. */\n"
    "#include \"gods.h\"\n\n" % FRAME + "\n\n".join(t for _, _, t in objects) + "\n")
ld = ["/* Gods: the data at the addresses the original keeps it at.  Generated by scripts/gen_gods_data.py.",
      " * The code is placed by the loader; only data is pinned here. */", "SECTIONS {"]
for name, addr, _ in objects:
    ld.append(f"    .{name} 0x{addr:x} : {{ *(.data.{name}) *(.rodata.{name}) *(.bss.{name}) }}")
ld.append("}")
(NATIVE / "gods.ld").write_text("\n".join(ld) + "\n")
print(f"data.c: {len(objects)} objects, {(NATIVE / 'data.c').stat().st_size} bytes of source")
