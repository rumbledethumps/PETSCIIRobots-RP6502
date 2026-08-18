#!/usr/bin/env python3
"""gfxfont.bin -> RP6502 VGA mode 1 font.

Two things matter and both are easy to get silently wrong:

  * Only the first 2048 bytes are the game's charset. PAYLOAD1 copies 8 pages
    (256 glyphs) to VRAM $F800; the rest of the 4096-byte file is unused.

  * RP6502 mode 1 stores fonts ROW-major -- font[row * 256 + glyph] -- while
    gfxfont.bin is glyph-major. The firmware reads
        font = mode1_get_font(config, height) + 256 * row;  glyph = font[code];
    so the data must be transposed. An out-of-range font pointer makes the
    renderer fall back to its built-in code page font *without an error*, so a
    mistake here shows up as plausible-looking garbage rather than a failure.

Bit order is unchanged: MSB is the leftmost pixel in both.
"""
import argparse
import pathlib
import sys

GLYPHS = 256
HEIGHT = 8


def transpose(src: bytes) -> bytes:
    return bytes(src[g * HEIGHT + r] for r in range(HEIGHT) for g in range(GLYPHS))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=pathlib.Path)
    ap.add_argument("dst", type=pathlib.Path)
    args = ap.parse_args()

    raw = args.src.read_bytes()
    if len(raw) < GLYPHS * HEIGHT:
        sys.exit(f"{args.src}: expected at least {GLYPHS * HEIGHT} bytes, got {len(raw)}")
    out = transpose(raw[:GLYPHS * HEIGHT])

    # The charset is PETSCII screen-code ordered: 0='@', 1='A', 26='Z', 32=blank.
    # Check a couple of glyphs survived the transpose in the right place, so a
    # transposition bug cannot reach the ROM.
    def glyph(code):
        return bytes(out[r * GLYPHS + code] for r in range(HEIGHT))

    assert glyph(32) == b"\0" * HEIGHT, "glyph 32 must be blank (PETSCII space)"
    assert glyph(1) == bytes(raw[1 * HEIGHT: 2 * HEIGHT]), "glyph 1 ('A') mismatch"
    assert glyph(0x3A) == bytes(raw[0x3A * HEIGHT: 0x3B * HEIGHT]), "glyph $3A mismatch"
    assert len(out) == GLYPHS * HEIGHT

    args.dst.parent.mkdir(parents=True, exist_ok=True)
    args.dst.write_bytes(out)
    print(f"{args.dst}: {len(out)} bytes, {GLYPHS} glyphs x {HEIGHT} rows, row-major")
    return 0


if __name__ == "__main__":
    sys.exit(main())
