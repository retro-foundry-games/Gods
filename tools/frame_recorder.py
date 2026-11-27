#!/usr/bin/env python3
"""Frame/memory recorder and comparer, driven as an ami9000_host peer.

The host's Lua script sends one binary message per emulated frame on stdin:

    'F' <u32 frame> <u64 frame_hash> <u32 ram_len> <u32 rgb_len> <u16 w> <u16 h> ram[ram_len] rgb[rgb_len]

`rgb_len` is 0 for frames whose pixels are not sent. EOF ends the run.

record mode (--out DIR):
    index.bin          16 bytes per frame: <u32 frame, u64 frame_hash, u32 crc32(ram)>
    ram-NNNNNN.zst     per segment of --segment frames: the first frame's RAM, then each next frame XOR the
                       previous one, as one zstd stream; every item is <u32 frame><u32 length> + bytes
    screen-NNNNNN.zst  the same for the frames that arrive with pixels (first of a segment is a full image)
    meta.json          sizes and counts

extract mode (--extract DIR FRAME OUT): write one frame's chip RAM, decoded from the deltas.

compare mode (--compare DIR [--dump DIR2]): checks each incoming frame against DIR/index.bin and stops at the
first difference, reporting the frame, which of hash/RAM differs and, when DIR still holds the RAM segment, the
differing byte ranges.  The peer writes one line, `diff <frame> <what>`, to stdout and stderr and exits 1 at the first difference;
it stays silent while frames match and reports the count on stderr at EOF.

Stdlib only (Python 3.14 `compression.zstd`).
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

from compression import zstd

HEADER = struct.Struct("<IQIIHH")
INDEX = struct.Struct("<IQI")


def read_exact(stream, n: int) -> bytes | None:
    buf = bytearray()
    while len(buf) < n:
        chunk = stream.read(n - len(buf))
        if not chunk:
            return None if not buf else bytes(buf)
        buf += chunk
    return bytes(buf)


def xor(a: bytes, b: bytes) -> bytes:
    n = len(a)
    return (int.from_bytes(a, "little") ^ int.from_bytes(b, "little")).to_bytes(n, "little")


class Segments:
    """A sequence of zstd streams, a new one every `size` frames, each opening with a full item."""

    def __init__(self, out: Path, prefix: str, size: int, level: int) -> None:
        self.out, self.prefix, self.size, self.level = out, prefix, size, level
        self.file = None
        self.comp = None
        self.prev: bytes | None = None
        self.segment = -1
        self.bytes_out = 0
        self.items = 0

    def _open(self, segment: int) -> None:
        self.close()
        self.segment = segment
        self.file = open(self.out / f"{self.prefix}-{segment * self.size:06d}.zst", "wb")
        self.comp = zstd.ZstdCompressor(level=self.level)
        self.prev = None

    def add(self, frame: int, data: bytes) -> None:
        segment = frame // self.size
        if segment != self.segment or self.prev is None or len(self.prev) != len(data):
            self._open(segment)
        payload = data if self.prev is None else xor(self.prev, data)
        self.prev = data
        self.items += 1
        self._write(struct.pack("<II", frame, len(payload)) + payload)

    def _write(self, blob: bytes) -> None:
        assert self.comp is not None and self.file is not None
        chunk = self.comp.compress(blob)
        self.bytes_out += len(chunk)
        self.file.write(chunk)

    def close(self) -> None:
        if self.file and self.comp:
            tail = self.comp.flush()
            self.bytes_out += len(tail)
            self.file.write(tail)
            self.file.close()
            self.file = None


def load_ram_segment(ref: Path, frame: int, size: int) -> dict[int, bytes]:
    """Decode the reference RAM segment containing `frame` into {frame: ram} up to and including it."""
    start = frame // size * size
    path = ref / f"ram-{start:06d}.zst"
    if not path.exists():
        return {}
    data = zstd.decompress(path.read_bytes())
    frames: dict[int, bytes] = {}
    pos, prev = 0, None
    while pos + 8 <= len(data):
        f, n = struct.unpack_from("<II", data, pos)
        pos += 8
        payload = data[pos : pos + n]
        pos += n
        prev = payload if prev is None else xor(prev, payload)
        frames[f] = prev
        if f >= frame:
            break
    return frames


class RefRam:
    """The recorded chip RAM of consecutive frames, rebuilt from the XOR deltas as the frames are asked for."""

    def __init__(self, ref: Path, size: int) -> None:
        self.ref, self.size = ref, size
        self.segment = -1
        self.data = b""
        self.pos = 0
        self.cur: bytes | None = None
        self.frame = None

    def get(self, frame: int) -> bytes | None:
        seg = frame // self.size
        if seg != self.segment:
            path = self.ref / f"ram-{seg * self.size:06d}.zst"
            if not path.exists():
                return None
            self.data, self.pos, self.cur, self.segment = zstd.decompress(path.read_bytes()), 0, None, seg
        while self.pos + 8 <= len(self.data) and (self.frame is None or self.frame < frame or self.cur is None):
            f, n = struct.unpack_from("<II", self.data, self.pos)
            self.pos += 8
            payload = self.data[self.pos : self.pos + n]
            self.pos += n
            self.cur = payload if self.cur is None else xor(self.cur, payload)
            self.frame = f
            if f >= frame:
                break
        return self.cur if self.frame == frame else None


def ranges(a: bytes, b: bytes, limit: int = 12) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    i, n = 0, min(len(a), len(b))
    while i < n and len(out) < limit:
        if a[i] != b[i]:
            j = i
            while j < n and a[j] != b[j]:
                j += 1
            out.append((i, j - i))
            i = j
        else:
            i += 1
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    ap.add_argument("--out", help="record into this directory")
    ap.add_argument("--compare", help="compare against the recording in this directory")
    ap.add_argument("--segment", type=int, default=1000, help="frames per compressed segment (default 1000)")
    ap.add_argument("--level", type=int, default=1, help="zstd level (default 1)")
    ap.add_argument("--extract", nargs=3, metavar=("DIR", "FRAME", "OUT"),
                    help="write the chip RAM of FRAME from the recording in DIR to OUT and exit")
    ap.add_argument("--mask", help="compare mode: file of `START END` hex lines (END exclusive) of chip RAM to ignore")
    ap.add_argument("--survey", action="store_true",
                    help="compare mode: do not stop at the first difference; report every differing RAM range at the end")
    ap.add_argument("--no-ram", action="store_true", help="record index and screens but not RAM deltas")
    a = ap.parse_args()
    if a.extract:
        ref, frame, dest = Path(a.extract[0]), int(a.extract[1]), Path(a.extract[2])
        seg = load_ram_segment(ref, frame, json.loads((ref / "meta.json").read_text())["segment"])
        if frame not in seg:
            raise SystemExit(f"frame {frame} is not in the recording's RAM segments")
        dest.write_bytes(seg[frame])
        return 0
    if bool(a.out) == bool(a.compare):
        ap.error("choose exactly one of --out and --compare")
    stdin, stdout = sys.stdin.buffer, sys.stdout.buffer  # stdout carries only the diff line

    def messages():
        while True:
            kind = read_exact(stdin, 1)
            if not kind:
                return
            if kind != b"F":
                raise SystemExit(f"frame_recorder: bad message type {kind!r}")
            head = read_exact(stdin, HEADER.size)
            if head is None or len(head) < HEADER.size:
                return
            frame, fhash, ram_len, rgb_len, w, h = HEADER.unpack(head)
            ram = read_exact(stdin, ram_len) if ram_len else b""
            rgb = read_exact(stdin, rgb_len) if rgb_len else b""
            yield frame, fhash, ram, rgb, w, h

    if a.out:
        out = Path(a.out)
        out.mkdir(parents=True, exist_ok=True)
        index = open(out / "index.bin", "wb")
        ram_seg = Segments(out, "ram", a.segment, a.level)
        screen_seg = Segments(out, "screen", a.segment, a.level)
        frames = raw_ram = raw_rgb = 0
        size = (0, 0)
        for frame, fhash, ram, rgb, w, h in messages():
            index.write(INDEX.pack(frame, fhash, zlib.crc32(ram)))
            if ram and not a.no_ram:
                ram_seg.add(frame, ram)
                raw_ram += len(ram)
            if rgb:
                screen_seg.add(frame, rgb)
                raw_rgb += len(rgb)
                size = (w, h)
            frames += 1
        ram_seg.close()
        screen_seg.close()
        index.close()
        (out / "meta.json").write_text(json.dumps({
            "frames": frames, "segment": a.segment, "zstd_level": a.level,
            "ram_items": ram_seg.items, "ram_raw_bytes": raw_ram, "ram_compressed_bytes": ram_seg.bytes_out,
            "screen_items": screen_seg.items, "screen_raw_bytes": raw_rgb,
            "screen_compressed_bytes": screen_seg.bytes_out, "screen_size": size,
        }, indent=1))
        print(f"recorded {frames} frames: RAM {ram_seg.bytes_out} B, screens {screen_seg.bytes_out} B", file=sys.stderr)
        return 0

    ref = Path(a.compare)
    meta = json.loads((ref / "meta.json").read_text())
    ref_index = (ref / "index.bin").read_bytes()
    count = len(ref_index) // INDEX.size
    first = INDEX.unpack_from(ref_index, 0)[0] if count else 0  # a recording need not start at frame 0
    keep = None
    if a.mask:
        keep = bytearray(b"\xff" * 0x80000)
        for line in Path(a.mask).read_text().splitlines():
            line = line.split("#")[0].split()
            if len(line) == 2:
                lo, hi = int(line[0], 16), int(line[1], 16)
                keep[lo:hi] = bytes(hi - lo)
        keep_int = int.from_bytes(keep, "little")
    reader = RefRam(ref, meta["segment"]) if (keep is not None or a.survey) else None
    seen = 0
    differing: dict[int, list] = {}      # start of a differing range -> [length, first frame, frames]
    first_screen = None
    for frame, fhash, ram, rgb, w, h in messages():
        seen += 1
        if not 0 <= frame - first < count:
            continue
        rframe, rhash, rcrc = INDEX.unpack_from(ref_index, (frame - first) * INDEX.size)
        assert rframe == frame, f"index entry holds frame {rframe}, expected {frame}"
        what = []
        if fhash != rhash:
            what.append("screen")
        ref_ram = None
        if ram and reader is not None:
            ref_ram = reader.get(frame)
            if ref_ram is not None:
                live = int.from_bytes(ram, "little")
                same = ((live ^ int.from_bytes(ref_ram, "little")) & (keep_int if keep is not None else -1)) == 0
                if not same:
                    what.append("ram")
        elif ram and zlib.crc32(ram) != rcrc:
            what.append("ram")
        if what and a.survey:
            if "screen" in what and first_screen is None:
                first_screen = frame
            if "ram" in what and ref_ram is not None:
                for o, n in ranges(ref_ram, ram, 400):
                    if keep is not None and not any(keep[o:o + n]):
                        continue
                    e = differing.setdefault(o, [n, frame, 0])
                    e[0] = max(e[0], n)
                    e[2] += 1
            continue
        if what:
            line = f"diff {frame} {'+'.join(what)}"
            if "ram" in what:
                seg = ref_ram if ref_ram is not None else (load_ram_segment(ref, frame, meta["segment"]).get(frame))
                if seg is not None:
                    line += " ram@" + ",".join(f"{o:06x}+{n}" for o, n in ranges(seg, ram))
            stdout.write(line.encode() + b"\n")
            stdout.flush()
            print(line, file=sys.stderr)
            return 1
    if a.survey:
        merged: list[list[int]] = []
        for o in sorted(differing):
            n, f, c = differing[o]
            if merged and o <= merged[-1][1] + 16:
                merged[-1][1] = max(merged[-1][1], o + n)
                merged[-1][2] = min(merged[-1][2], f)
                merged[-1][3] += c
            else:
                merged.append([o, o + n, f, c])
        print(f"survey of {seen} frames: first screen difference at frame {first_screen}; {len(merged)} differing RAM ranges:", file=sys.stderr)
        for lo, hi, f, c in merged:
            print(f"  {lo:06x}-{hi:06x} ({hi - lo} bytes) first seen at frame {f}, {c} frame-differences", file=sys.stderr)
        return 0
    print(f"compared {seen} frames: all match", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
