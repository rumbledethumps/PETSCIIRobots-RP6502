#!/usr/bin/env python3
"""Remap the X16 sources' hardcoded low zero page onto named variables.

BACKGROUND_TASKS.ASM keeps two pointers in fixed zero page locations:

    $02/$03   the message pointer PRINT_INFO reads
    $04/$05   the map pointer GET_TILE_FROM_MAP builds

cc65's runtime owns those addresses, so the port has to name them instead. The
substitution has to see every addressing mode, not just the direct one --
`LDA ($04),Y` is the form that actually matters, and missing it produces code
that stores through the new pointer and loads through the old one, which fails
in a way that looks like corrupt map data rather than a bad address.

Immediates are left alone: `#$04` is the number four, not a location.
"""
import argparse
import pathlib
import re
import sys

REMAP = {"$02": "SOURCE", "$03": "SOURCE+1", "$04": "MAP_PTR", "$05": "MAP_PTR+1"}


def split_comment(line):
    in_str = False
    for i, c in enumerate(line):
        if c == '"':
            in_str = not in_str
        elif c == ';' and not in_str:
            return line[:i], line[i:]
    return line, ""


def remap(text):
    out, counts = [], {}
    for line in text.splitlines():
        code, comment = split_comment(line)
        # A low zero page literal, not preceded by '#' (an immediate) and not
        # part of a longer hex number.
        def sub(m):
            name = REMAP[m.group(0)]
            counts[name] = counts.get(name, 0) + 1
            return name
        code = re.sub(r'(?<![#$0-9A-Fa-f])\$0[2345](?![0-9A-Fa-f])', sub, code)
        out.append(code + comment)
    return "\n".join(out) + "\n", counts


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=pathlib.Path)
    ap.add_argument("dst", type=pathlib.Path)
    args = ap.parse_args()

    text, counts = remap(args.src.read_text())

    # Nothing may reference low zero page by number afterwards, in any
    # addressing mode. This is the check that would have caught the indirect
    # form the first time.
    leftover = []
    for n, line in enumerate(text.splitlines(), 1):
        code, _ = split_comment(line)
        if re.search(r'(?<![#$0-9A-Fa-f])\$0[0-9A-Fa-f](?![0-9A-Fa-f])', code):
            leftover.append(f"  {n}: {code.strip()}")
    if leftover:
        print("low zero page still referenced by number:", file=sys.stderr)
        print("\n".join(leftover), file=sys.stderr)
        return 1

    args.dst.write_text(text)
    print(f"{args.dst}: " + ", ".join(f"{k} x{v}" for k, v in sorted(counts.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
