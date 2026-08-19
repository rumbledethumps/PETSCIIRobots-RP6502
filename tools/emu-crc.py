#!/usr/bin/env python3
"""Re-bless the expect-crc hashes in an emulator script.

A frame hash is the cheapest way to catch a rendering regression and the most
annoying thing to maintain, because any palette or art change invalidates every
one of them. This turns the update into a reviewable diff: it swaps each
expect-crc for a bare crc, runs the script once, reads the hashes the emulator
prints back in order, and writes them into the file in place.

Then read `git diff tests/emu/` before committing. A re-bless that moves four
hashes when you expected one is the bug you just caught.
"""
import argparse
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("script", type=pathlib.Path, nargs="+")
    ap.add_argument("--emu", type=pathlib.Path,
                    default=ROOT / "tools" / "rp6502-emu")
    ap.add_argument("--rom", type=pathlib.Path, required=True)
    args = ap.parse_args()
    # The emulator runs in a temp working directory so its screenshots and MSC0:
    # writes land there, which means every path handed to it has to be absolute.
    args.emu = args.emu.resolve()
    args.rom = args.rom.resolve()

    for script in args.script:
        text = script.read_text()
        lines = text.splitlines()
        slots = [i for i, l in enumerate(lines)
                 if re.match(r"\s*expect-crc\b", l)]
        if not slots:
            print(f"{script}: no expect-crc lines")
            continue

        probe = [re.sub(r"(\s*)expect-crc\s+\S+", r"\1crc", l) for l in lines]
        with tempfile.TemporaryDirectory() as td:
            tmp = pathlib.Path(td) / script.name
            tmp.write_text("\n".join(probe) + "\n")
            r = subprocess.run(
                [str(args.emu), "--mute", "--seed", "1", "--tmpdrive",
                 "--script", str(tmp), str(args.rom)],
                capture_output=True, text=True, cwd=td)
        got = re.findall(r"^([0-9A-Fa-f]{8})$", r.stdout, re.M)
        if len(got) != len(slots):
            print(f"{script}: emulator printed {len(got)} hashes for "
                  f"{len(slots)} expect-crc lines", file=sys.stderr)
            if r.stderr.strip():
                print(r.stderr.strip(), file=sys.stderr)
            return 1
        for i, h in zip(slots, got):
            lines[i] = re.sub(r"(expect-crc\s+)\S+", r"\g<1>" + h.upper(), lines[i])
        script.write_text("\n".join(lines) + "\n")
        print(f"{script}: {len(got)} hash(es) updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
