#!/usr/bin/env python3
"""Verify tools/convert/acme2ca65.py against the shipped X16 binaries.

reference/x16/ holds David Murray's X16 sources exactly as published, and
reference/x16/prg/ holds the binaries they were released as. Converting those
sources to ca65 and assembling them must reproduce those binaries byte for byte.

That is the whole safety net for the port: it proves the converter preserves
meaning, so the RP6502 sources in src/ start from provably-correct assembly and
every later difference is a change we made on purpose.

Run directly, or via ctest as `reference.conversion`.
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


def check_zeropage_is_partitioned() -> int:
    """The linker must keep reserving the game's zero page.

    The ported assembly names the addresses David Murray gave it -- $02-$05 for
    its two pointers, $23-$3B for the rest -- so those addresses have to belong
    to the game and not to cc65's runtime. src/rp6502-petscii.cfg does that with
    a separate memory area, and src/game/globals.s asserts at assembly time that
    each variable landed where the original put it.

    Both halves matter, and either can be deleted without anything obviously
    breaking until a routine starts reading the wrong byte, so check both are
    still there. The addresses themselves are checked by the build.
    """
    bad = []
    cfg = (ROOT / "src" / "rp6502-petscii.cfg").read_text()
    if "GAMEZP:" not in cfg or "GAMEZEROPAGE:" not in cfg:
        bad.append("  rp6502-petscii.cfg no longer reserves a zero page area "
                   "for the game")
    if not re.search(r'start\s*=\s*\$0002', cfg):
        bad.append("  the game's zero page area does not start at $02")

    globals_s = (ROOT / "src" / "game" / "globals.s").read_text()
    if '.segment "GAMEZEROPAGE"' not in globals_s:
        bad.append("  globals.s does not put the game's variables in GAMEZEROPAGE")
    asserts = len(re.findall(r'^\s*\.assert\s+\w+\s*=\s*\$[0-9A-Fa-f]{2},', 
                             globals_s, re.M))
    if asserts < 8:
        bad.append(f"  globals.s pins only {asserts} zero page addresses; "
                   "the layout is load-bearing")

    if bad:
        print("zero page is no longer partitioned:")
        print("\n".join(bad))
        return 1
    print(f"  zero page          partitioned, {asserts} addresses pinned")
    return 0


def main() -> int:
    failures = check_zeropage_is_partitioned()
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
