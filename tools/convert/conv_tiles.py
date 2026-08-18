#!/usr/bin/env python3
"""TILESET.GFX -> RP6502 tile tables.

The file is a CBM PRG: a 2-byte load address ($4900, though the loader forces
$8000 with SETLFS SA=0) then 5119 bytes laid out as twenty 256-byte arrays --
except the last one is 255 bytes, one short. The highest tile id used by any
shipped level is 242, so the missing TILE_COLOR_BR[255] is padded with 0.

    $000  DESTRUCT_PATH   tile -> the tile it becomes when destroyed
    $100  TILE_ATTRIB     b0 walkable   b1 hoverable  b2 pushable  b3 destructible
                          b4 see-through b5 valid destination b6 searchable
    $200  TILE_DATA_TL..BR   9 x 256 character codes: a tile is a 3x3 block of
                             8x8 characters, so 24x24 pixels
    $B00  TILE_COLOR_TL..BR  9 x 256 colour nibbles

The X16 draws a tile as nine (character, colour) pairs and masks every colour
byte with $0F on the way out, which is what makes the playfield background
transparent over the bitmap plane. Two changes for RP6502, both free:

  * apply that mask here instead of in the inner loop, deleting nine AND #$0F
    per tile from the hot path;
  * store the pairs tile-major and row-major so each character row of a tile is
    six sequential bytes, which is one RIA.addr0 store and six RIA.rw0 writes.

RP6502 mode 1's 4-bit cell is {glyph_code, bg_fg_index} with the background in
the high nibble and the foreground in the low nibble -- byte-identical to VERA's
text mode, so the masked colour byte transfers unchanged.
"""
import argparse
import pathlib
import sys

CELLS = ["TL", "TM", "TR", "ML", "MM", "MR", "BL", "BM", "BR"]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=pathlib.Path)
    ap.add_argument("dst", type=pathlib.Path)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    raw = args.src.read_bytes()
    if len(raw) != 5121:
        sys.exit(f"{args.src}: expected 5121 bytes, got {len(raw)}")
    load = raw[0] | (raw[1] << 8)
    body = raw[2:] + b"\x00"          # pad the one missing TILE_COLOR_BR byte
    assert len(body) == 5120

    def arr(i):
        return body[i * 256:(i + 1) * 256]

    destruct, attrib = arr(0), arr(1)
    glyphs = {c: arr(2 + i) for i, c in enumerate(CELLS)}
    colors = {c: arr(11 + i) for i, c in enumerate(CELLS)}

    cells = bytearray()
    for t in range(256):
        for row in (("TL", "TM", "TR"), ("ML", "MM", "MR"), ("BL", "BM", "BR")):
            for c in row:
                cells.append(glyphs[c][t])
                cells.append(colors[c][t] & 0x0F)
    assert len(cells) == 256 * 3 * 6 == 4608

    blob = bytes(cells) + destruct + attrib
    args.dst.parent.mkdir(parents=True, exist_ok=True)
    args.dst.write_bytes(blob)
    print(f"{args.dst}: {len(blob)} bytes "
          f"(cells 4608 + destruct 256 + attrib 256), PRG load was ${load:04X}")

    if args.report:
        used = {g for c in CELLS for g in glyphs[c]}
        hi = {c: len({v >> 4 for v in colors[c]}) for c in CELLS}
        print(f"  distinct glyph codes referenced by tiles: {len(used)}")
        print(f"  distinct TILE_ATTRIB values: {len(set(attrib))}")
        print(f"  colour high nibbles present before masking: "
              f"{sorted({v >> 4 for c in CELLS for v in colors[c]})}")
        fg0 = sum(1 for c in CELLS for v in colors[c] if (v & 0x0F) == 0)
        print(f"  cells whose foreground would be palette 0 (transparent): {fg0}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
