#!/usr/bin/env python3
"""Invariants the converted assets must satisfy before they reach the ROM.

These are the assumptions the platform layer is built on. Each one is cheap to
check and expensive to debug on hardware, because the failure modes are quiet:
a mis-transposed font makes the VGA silently substitute its built-in code page,
and a tile cell whose foreground is palette 0 turns transparent over the bitmap
plane instead of drawing.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
GEN = ROOT / "assets" / "gen"
SRC = ROOT / "assets" / "src"

failures = []


def check(name, cond, detail=""):
    if cond:
        print(f"  ok    {name}")
    else:
        print(f"  FAIL  {name}  {detail}")
        failures.append(name)


def main() -> int:
    # ---- font ---------------------------------------------------------------
    font = (GEN / "font.bin").read_bytes()
    orig = (SRC / "gfxfont.bin").read_bytes()[:2048]
    check("font is 2048 bytes", len(font) == 2048, len(font))
    check("font is row-major (font[row*256+glyph])",
          all(font[r * 256 + g] == orig[g * 8 + r] for g in range(256) for r in range(8)))
    check("glyph 32 is blank (PETSCII space)",
          all(font[r * 256 + 32] == 0 for r in range(8)))
    check("glyph 1 is 'A' and glyph 26 is 'Z' (PETSCII screen-code order)",
          any(font[r * 256 + 1] for r in range(8)) and any(font[r * 256 + 26] for r in range(8)))

    # ---- palettes -----------------------------------------------------------
    pal = (GEN / "palettes.bin").read_bytes()
    check("palettes are 3 x 16 x RGB555", len(pal) == 96, len(pal))
    val = lambda p, i: pal[p * 32 + i * 2] | (pal[p * 32 + i * 2 + 1] << 8)
    check("bitmap palette entry 0 is opaque black", val(0, 0) == 0x0020, hex(val(0, 0)))
    check("character palette entry 0 is transparent", val(1, 0) == 0x0000, hex(val(1, 0)))
    check("player palette entry 4 is opaque black (DEMATERIALIZE cycles it)",
          val(2, 4) == 0x0020, hex(val(2, 4)))
    check("all other entries are opaque",
          all(val(p, i) & 0x20 for p in range(3) for i in range(1, 16) if not (p == 2 and i == 4)))

    # ---- tiles --------------------------------------------------------------
    tiles = (GEN / "tiles.bin").read_bytes()
    check("tiles.bin is 4608 + 256 + 256", len(tiles) == 5120, len(tiles))
    cells = tiles[:4608]
    check("every tile colour byte has background nibble 0",
          all(cells[i] & 0xF0 == 0 for i in range(1, 4608, 2)))

    # ---- levels -------------------------------------------------------------
    levels = sorted(GEN.glob("level-*.bin"))
    check("14 levels converted", len(levels) == 14, len(levels))
    used = set()
    for p in levels:
        b = p.read_bytes()
        if len(b) != 8704:
            check(f"{p.name} is 8704 bytes", False, len(b))
            continue
        types, x, y = b[0:64], b[64:128], b[128:192]
        if types[0] != 1:
            check(f"{p.name} unit 0 is the player", False, types[0])
        # robots must sit inside the REQUEST_WALK_* bounds; hidden objects never
        # move, so they are exempt (level-k has a key at y=2).
        bad = [i for i in range(1, 28) if types[i] and not (5 <= x[i] <= 122 and 3 <= y[i] <= 60)]
        if bad:
            check(f"{p.name} robots within walk bounds", False, f"slots {bad}")
        if any(types[i] for i in range(28, 32)):
            check(f"{p.name} weapons band empty at load", False, "")
        used |= set(b[512:])
    check("all levels parse and their robots are in bounds", True)

    # The character palette's entry 0 is transparent, which is only safe if no
    # tile the levels actually place has a foreground of 0.
    fg0 = [(t, i) for t in sorted(used)
           for i in range(9) if cells[t * 18 + i * 2 + 1] == 0]
    check("no tile used by any level has a transparent foreground",
          not fg0, f"{len(fg0)} cells, e.g. {fg0[:3]}")

    # ---- images and sprites -------------------------------------------------
    for name in ("intropic.bin", "gamepic.bin"):
        b = (GEN / name).read_bytes()
        check(f"{name} is 320x240 at 4bpp", len(b) == 38400, len(b))
    for name, n in (("spr_player.bin", 13), ("spr_cursors.bin", 3), ("spr_hud.bin", 12)):
        b = (GEN / name).read_bytes()
        check(f"{name} is {n} x 32x32 4bpp", len(b) == n * 512, len(b))

    # ---- XRAM budget --------------------------------------------------------
    resident = (2048          # font
                + 96          # palettes
                + 6656 + 1536 + 6144   # sprites
                + 38400       # bitmap plane
                + 40 * 30 * 2)         # character plane
    check(f"XRAM working set fits in 64K ({resident} bytes, {65536 - resident} free)",
          resident < 65536, resident)

    print("assets:", "OK" if not failures else f"{len(failures)} FAILED")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
