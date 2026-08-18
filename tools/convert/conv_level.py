#!/usr/bin/env python3
"""level-a .. level-n -> RP6502 level blobs.

Each file is a CBM PRG that the X16 loads whole to $5D00:

    +0     2 bytes   load address $5D00
    +2     8 x 64    UNIT_TYPE, LOC_X, LOC_Y, A, B, C, D, HEALTH
    +514   256       filler ($00, or $AA in level-a and level-m)
    +770   8192      MAP, 128 wide x 64 tall, one byte per tile

Nothing in x16Robots.ASM or BACKGROUND_TASKS.ASM reads $5F00-$5FFF, so the
filler is dropped and the result is 8704 bytes: 512 of unit arrays followed by
the map, which is exactly the layout the port declares, so loading a level is
one read() into one struct.

Unit slots: 0 player, 1-27 robots, 28-31 weapons fire, 32-47 doors and other
unsprited units, 48-63 hidden findable objects.
"""
import argparse
import pathlib
import sys

UNITS, FILLER, MAP = 512, 256, 8192


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=pathlib.Path)
    ap.add_argument("dst", type=pathlib.Path)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    raw = args.src.read_bytes()
    if len(raw) != 2 + UNITS + FILLER + MAP:
        sys.exit(f"{args.src}: expected {2 + UNITS + FILLER + MAP} bytes, got {len(raw)}")
    load = raw[0] | (raw[1] << 8)
    if load != 0x5D00:
        sys.exit(f"{args.src}: load address ${load:04X}, expected $5D00")

    body = raw[2:]
    units, filler, mapdata = body[:UNITS], body[UNITS:UNITS + FILLER], body[UNITS + FILLER:]
    if set(filler) - {0x00, 0xAA}:
        sys.exit(f"{args.src}: filler is not inert: {sorted(set(filler))[:8]}")

    types = units[0:64]
    if types[0] != 1:
        sys.exit(f"{args.src}: unit 0 is type {types[0]}, expected 1 (the player)")

    out = units + mapdata
    assert len(out) == UNITS + MAP == 8704
    args.dst.parent.mkdir(parents=True, exist_ok=True)
    args.dst.write_bytes(out)

    if args.report:
        x, y = units[64:128], units[128:192]
        robots = sum(1 for i in range(1, 28) if types[i])
        doors = sum(1 for i in range(32, 48) if types[i])
        hidden = sum(1 for i in range(48, 64) if types[i])
        fire = sum(1 for i in range(28, 32) if types[i])
        print(f"{args.dst.name}: player ({x[0]},{y[0]})  robots {robots:2}  "
              f"doors {doors:2}  hidden {hidden:2}  fire {fire}  "
              f"tiles {len(set(mapdata)):3}")
    else:
        print(f"{args.dst}: {len(out)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
