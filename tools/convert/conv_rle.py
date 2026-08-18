#!/usr/bin/env python3
"""intropic.rle / gamepic.rle -> raw 320x240 4bpp for VGA mode 3.

One byte per run: the high nibble is a repeat count, the low nibble a colour
index, and the run emits repeat+1 pixels. A repeat nibble of 15 means the next
byte is read and ADDED to the count, giving runs of 15..270. Pixels pack two per
byte with the first in the high nibble.

Decoding at build time removes PAYLOAD4/PAYLOAD5's 6502 decoders from the port
entirely: the ROM just read_xram()s 38400 bytes straight into the bitmap plane.

Nibble order: RP6502 mode 3 reads the high nibble for even (left) columns --
firmware mode3.c does `if (col & 1) *rgb++ = pal[*data & 0xF]` -- which is what
this format already produces. So mode 3 OPTIONS bit 3 (reverse bit order) must
be 0, i.e. options = 0x02 for 4bpp.
"""
import argparse
import pathlib
import sys

WIDTH, HEIGHT = 320, 240
EXPECT = WIDTH * HEIGHT // 2          # 38400 bytes, 4 bits per pixel


def decode(src: bytes) -> bytes:
    out = bytearray()
    hi, acc, i = True, 0, 0
    while i < len(src):
        b = src[i]; i += 1
        rep, col = b >> 4, b & 0x0F
        if rep == 15:
            if i >= len(src):
                sys.exit("truncated stream: escape byte with no continuation")
            rep = 15 + src[i]; i += 1
        for _ in range(rep + 1):
            if hi:
                acc = col << 4; hi = False
            else:
                out.append(acc | col); hi = True
    if not hi:
        sys.exit("stream ended mid-byte (odd pixel count)")
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=pathlib.Path)
    ap.add_argument("dst", type=pathlib.Path)
    ap.add_argument("--carve", metavar="OFF:LEN",
                    help="take LEN bytes at OFF from src first (for a payload PRG)")
    args = ap.parse_args()

    raw = args.src.read_bytes()
    if args.carve:
        off, length = (int(x) for x in args.carve.split(":"))
        raw = raw[off:off + length]

    out = decode(raw)
    if len(out) != EXPECT:
        sys.exit(f"{args.src}: decoded {len(out)} bytes, expected {EXPECT} "
                 f"({WIDTH}x{HEIGHT} at 4bpp)")

    args.dst.parent.mkdir(parents=True, exist_ok=True)
    args.dst.write_bytes(out)
    hist = {}
    for byte in out:
        hist[byte >> 4] = hist.get(byte >> 4, 0) + 1
        hist[byte & 15] = hist.get(byte & 15, 0) + 1
    top = sorted(hist.items(), key=lambda kv: -kv[1])[:4]
    print(f"{args.dst}: {len(out)} bytes from {len(raw)}, "
          f"{len(hist)}/16 colours, most common {[c for c, _ in top]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
