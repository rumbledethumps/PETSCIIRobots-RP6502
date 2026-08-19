#!/usr/bin/env python3
"""Verify tools/convert/acme2ca65.py against the shipped X16 binaries.

reference/x16/ holds David Murray's X16 sources exactly as published, and
reference/x16/prg/ holds the binaries they were released as. Converting those
sources to ca65 and assembling them must reproduce those binaries byte for byte.

That is the whole safety net for the port: it proves the converter preserves
meaning, so the RP6502 sources in src/ start from provably-correct assembly and
every later difference is a change we made on purpose.

Run directly, or via ctest as `reference-conversion`.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
REF = ROOT / "reference" / "x16"
PRG = REF / "prg"
ASSETS = ROOT / "assets" / "src"
CONV = ROOT / "tools" / "convert" / "acme2ca65.py"

# program -> (top-level source, load address, extra sources it .includes)
PROGRAMS = {
    "X16ROBOTS": ("x16Robots.ASM", 0x0801,
                  ["BACKGROUND_TASKS.ASM", "sounds.asm", "sounds.inc", "zsound.inc"]),
    "PAYLOAD1":  ("payload1-font.asm", 0x5D00, []),
    "PAYLOAD2":  ("payload2-sprites.asm", 0x5D00, []),
    "PAYLOAD3":  ("payload3-sprites.asm", 0x5D00, []),
    "PAYLOAD4":  ("payload4-intro-graphics.asm", 0x5D00, []),
    "PAYLOAD5":  ("payload5-game-graphics.asm", 0x5D00, []),
}

# The reference PRGs for the payloads live beside the assets they embed.
PRG_DIR = {"X16ROBOTS": PRG}


def convert(src: pathlib.Path, dst: pathlib.Path) -> None:
    with dst.open("wb") as out:
        r = subprocess.run([sys.executable, str(CONV), str(src)],
                           stdout=out, stderr=subprocess.PIPE)
    if r.returncode != 0:
        raise SystemExit(f"convert failed for {src}:\n{r.stderr.decode()}")
    tail = r.stderr.decode().strip().splitlines()[-1]
    if "0 unhandled" not in tail:
        raise SystemExit(f"converter reported unhandled lines: {tail}")


def build(name: str, work: pathlib.Path) -> bytes:
    top, addr, extra = PROGRAMS[name]

    def out_name(f):
        return f.lower().replace(".asm", ".s").replace(".inc", ".inc")

    convert(REF / top, work / out_name(top))
    for f in extra:
        convert(REF / f, work / out_name(f))

    main = work / out_name(top)
    text = main.read_text()
    for f in extra:
        text = text.replace(f'.include "{f}"', f'.include "{out_name(f)}"')
    # .incbin paths are relative to the original flat layout
    text = text.replace('.incbin "', f'.incbin "{ASSETS}/')
    main.write_text(text)

    cfg = work / "link.cfg"
    cfg.write_text(
        f"MEMORY {{ RAM: start = ${addr:04X}, size = ${0x10000 - addr:04X}, file = %O, fill = no; }}\n"
        f"SEGMENTS {{ CODE: load = RAM, type = ro; }}\n")

    obj, binf = work / f"{name}.o", work / f"{name}.bin"
    subprocess.run(["ca65", "-o", str(obj), str(main)], check=True)
    subprocess.run(["ld65", "-C", str(cfg), "-o", str(binf), str(obj)], check=True)
    return binf.read_bytes()


def check_no_raw_zeropage() -> int:
    """No file under src/game/ may name low zero page by number.

    cc65's runtime owns $00-$1F, so the ported sources use MAP_PTR and SOURCE
    instead. Getting this wrong is quiet: a routine that stores through the new
    pointer and reads through the old one looks like corrupt map data, not a bad
    address, and the indirect form -- LDA ($04),Y -- is the one that is easy to
    miss because it does not look like the direct form.
    """
    bad = []
    pat = re.compile(r'(?<![#$0-9A-Fa-f])\$0[0-9A-Fa-f](?![0-9A-Fa-f])')
    for path in sorted((ROOT / "src" / "game").glob("*.s")):
        for n, line in enumerate(path.read_text().splitlines(), 1):
            code = line.split(";", 1)[0]
            if pat.search(code):
                bad.append(f"  {path.name}:{n}: {code.strip()}")
    if bad:
        print("src/game references low zero page by number:")
        print("\n".join(bad))
        return 1
    print("  src/game            no raw low zero page")
    return 0


def main() -> int:
    failures = check_no_raw_zeropage()
    for name in PROGRAMS:
        ref_prg = PRG_DIR.get(name, ASSETS) / f"{name}.PRG"
        if not ref_prg.exists():
            print(f"  {name:12} SKIP (no reference binary)")
            continue
        expected = ref_prg.read_bytes()[2:]      # drop the CBM load address
        with tempfile.TemporaryDirectory() as td:
            got = build(name, pathlib.Path(td))
        if got == expected:
            print(f"  {name:12} {len(got):6} bytes  identical")
        else:
            print(f"  {name:12} MISMATCH: got {len(got)}, expected {len(expected)}")
            failures += 1
    print("reference conversion:", "OK" if not failures else f"{failures} FAILED")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
