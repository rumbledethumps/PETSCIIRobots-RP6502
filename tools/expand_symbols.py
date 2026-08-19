#!/usr/bin/env python3
"""Substitute RAM symbol addresses into an emulator test script.

The `poke` and `peek` commands take literal addresses, and RAM symbols move
whenever the code above them changes size -- four tests broke at once when the
gamepad code was added. Writing @UNIT_TYPE@ instead and resolving it from the
linker map at build time makes them say what they mean and stop rotting.

    @SYMBOL@      the symbol's address as $XXXX
    @SYMBOL+12@   that address plus a decimal offset

XRAM addresses are not symbols and stay literal: they are constants the program
chooses, listed in src/xram.h, and do not move.

Comment lines are left alone, so a script can explain this syntax without the
explanation being eaten by it.
"""
import pathlib
import re
import sys

WORD = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)(?:\+(\d+))?@")
# cc65 map lines pack several "NAME  ADDR FLAGS" columns onto one line.
ENTRY = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s+([0-9A-F]{6})\s+[A-Z]{3}")


def main():
    if len(sys.argv) != 4:
        sys.exit(f"usage: {sys.argv[0]} <robots.map> <in.txt> <out.txt>")
    mapfile, src, dst = (pathlib.Path(p) for p in sys.argv[1:])

    syms = {}
    for name, addr in ENTRY.findall(mapfile.read_text()):
        syms.setdefault(name, int(addr, 16))

    missing = []

    def sub(m):
        name, off = m.group(1), int(m.group(2) or 0)
        if name not in syms:
            missing.append(name)
            return m.group(0)
        return f"${syms[name] + off:04X}"

    out = "".join(line if line.lstrip().startswith("#") else WORD.sub(sub, line)
                  for line in src.read_text().splitlines(keepends=True))
    if missing:
        sys.exit(f"{src}: not in {mapfile.name}: {', '.join(sorted(set(missing)))}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
