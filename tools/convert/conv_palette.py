#!/usr/bin/env python3
"""The X16 PALETTE table -> RP6502 RGB555 palettes.

VERA stores a colour as two bytes: byte0 = GGGGBBBB, byte1 = ----RRRR (the top
nibble of the red byte is ignored by the hardware). RP6502 wants RGB555 little
endian, 16-bit aligned, with bit 5 as a binary opacity flag:

    COLOR_FROM_RGB8(r,g,b) = ((b>>3)<<11) | ((g>>3)<<6) | (r>>3)
    alpha                  = 1 << 5

Three palettes come out of the one table, because RP6502 gives each mode-5
sprite its own palette pointer instead of VERA's single "palette offset":

  bitmap   plane 0 is the base and must be opaque everywhere, so entry 0 is
           opaque black.
  chars    entry 0 transparent, which is what lets the character plane sit over
           the backdrop. Safe because PLOT_TILE masks the colour byte with $0F,
           so every playfield cell has background 0 and no tile cell uses
           foreground 0.
  player   the character palette with entry 4 replaced. VERA's DEMATERIALIZE
           cycles palette entry 20 = offset-1 index 4, which is the player and
           cursor sprites; here that is a palette of their own and the effect
           cannot bleed into the character plane.
"""
import argparse
import pathlib
import re
import sys

NAMES = ["black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
         "orange", "brown", "light red", "dark grey", "grey", "light green",
         "light blue", "light grey"]
ALPHA = 1 << 5


def parse(asm_text: str):
    m = re.search(r'^PALETTE:?\s*$', asm_text, re.M)
    if not m:
        sys.exit("PALETTE label not found")
    vals = []
    for line in asm_text[m.end():].splitlines():
        b = re.match(r'\s*!BYTE\s+(\d+|\$[0-9A-Fa-f]+)', line)
        if not b:
            if line.strip() and not line.strip().startswith(';'):
                break
            continue
        t = b.group(1)
        vals.append(int(t[1:], 16) if t.startswith('$') else int(t))
        if len(vals) == 32:
            break
    if len(vals) != 32:
        sys.exit(f"expected 32 palette bytes, got {len(vals)}")
    out = []
    for i in range(16):
        gb, r = vals[i * 2], vals[i * 2 + 1]
        g, b, r4 = gb >> 4, gb & 0x0F, r & 0x0F   # VERA ignores the top nibble of red
        out.append((r4, g, b))
    return out


def rgb555(r4, g4, b4, opaque=True):
    r8, g8, b8 = r4 * 17, g4 * 17, b4 * 17
    v = ((b8 >> 3) << 11) | ((g8 >> 3) << 6) | (r8 >> 3)
    return v | ALPHA if opaque else v


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("asm", type=pathlib.Path)
    ap.add_argument("dst", type=pathlib.Path)
    ap.add_argument("--print", action="store_true")
    args = ap.parse_args()

    cols = parse(args.asm.read_text(encoding="latin-1"))
    bitmap = [rgb555(*c) for c in cols]
    chars = [0x0000] + bitmap[1:]
    player = list(chars)
    player[4] = 0x0020                       # opaque black; DEMATERIALIZE cycles this

    if args.print:
        print(f"{'#':>3}  {'name':<12} {'R G B':>8}  {'RGB8':>8}  RGB555  LE")
        for i, (r, g, b) in enumerate(cols):
            v = bitmap[i]
            print(f"{i:3}  {NAMES[i]:<12} {r:2} {g:2} {b:2}  "
                  f"#{r*17:02X}{g*17:02X}{b*17:02X}  0x{v:04X}  "
                  f"{v & 0xFF:02X} {v >> 8:02X}")

    blob = b"".join(v.to_bytes(2, "little") for pal in (bitmap, chars, player) for v in pal)
    assert len(blob) == 3 * 32
    args.dst.parent.mkdir(parents=True, exist_ok=True)
    args.dst.write_bytes(blob)
    print(f"{args.dst}: {len(blob)} bytes (bitmap, chars, player) x 16 x RGB555 LE")
    return 0


if __name__ == "__main__":
    sys.exit(main())
