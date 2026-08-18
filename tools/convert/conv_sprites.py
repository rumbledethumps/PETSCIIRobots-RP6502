#!/usr/bin/env python3
"""VERA sprite data -> RP6502 mode 5 sprites.

Mode 5 sprites are square only -- the firmware instantiates mode5_render for
8x8, 16x16, 32x32, 64x64, 128x128, 256x256, 512x512 -- so the two 64x32 HUD
icons become two 32x32 sprites each. Everything then has one size and one colour
depth, so all six live sprites are covered by a single
xreg_vga_mode(5, 0x12, cfg, 6, 2) call.

Pixel layout is identical to VERA's: 4bpp, 16 bytes per 32-pixel row, high
nibble the left pixel. The player and cursor sets copy verbatim.

    PSPRITE_DATA.BIN  6656 = 13 frames x 512   player, PLAYER_DIRECTION+ANIMATE
                                               (0 up, 3 down, 6 left, 9 right, 12 dead)
    CURSORS4BIT.BIN   1536 =  3 frames x 512   compass / magnifier / hand
    SPRITE_DATA.BIN   6144 =  6 frames x 1024  pistol, plasma, medkit, EMP,
                                               magnet, timebomb -- split L/R
"""
import argparse
import pathlib
import sys

SQUARE = 32 * 16          # 32 rows x 16 bytes = one 32x32 4bpp sprite
WIDE = 32 * 32            # a 64x32 icon
ICONS = ["pistol", "plasma", "medkit", "emp", "magnet", "timebomb"]


def split_wide(data: bytes) -> bytes:
    out = bytearray()
    for f in range(len(data) // WIDE):
        frame = data[f * WIDE:(f + 1) * WIDE]
        for half in (0, 16):
            for row in range(32):
                out += frame[row * 32 + half: row * 32 + half + 16]
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--player", type=pathlib.Path, required=True)
    ap.add_argument("--cursors", type=pathlib.Path, required=True)
    ap.add_argument("--hud", type=pathlib.Path, required=True)
    ap.add_argument("--out", type=pathlib.Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    player = args.player.read_bytes()
    cursors = args.cursors.read_bytes()
    hud = args.hud.read_bytes()

    if len(player) != 13 * SQUARE:
        sys.exit(f"{args.player}: expected {13 * SQUARE}, got {len(player)}")
    if len(cursors) != 3 * SQUARE:
        sys.exit(f"{args.cursors}: expected {3 * SQUARE}, got {len(cursors)}")
    if len(hud) != 6 * WIDE:
        sys.exit(f"{args.hud}: expected {6 * WIDE}, got {len(hud)}")

    icons = split_wide(hud)
    assert len(icons) == len(hud) == 12 * SQUARE

    for name, blob, frames in (("player", player, 13),
                               ("cursors", cursors, 3),
                               ("hud", icons, 12)):
        (args.out / f"spr_{name}.bin").write_bytes(blob)
        print(f"{args.out / f'spr_{name}.bin'}: {len(blob)} bytes, "
              f"{frames} frames x 32x32 4bpp")
    print("  HUD frame order: " +
          ", ".join(f"{n}L,{n}R" for n in ICONS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
