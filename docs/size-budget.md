# Size budget

RAM available to the program: `$0200–$FEFF` = **64,768 bytes**
(llvm-mos `link.ld`: `ram : ORIGIN = 0x0200, LENGTH = 0xfd00`, `__stack = 0xFF00`).
Zero page available to the compiler: `$20–$FF` = 224 bytes
(`zp : ORIGIN = __rc31 + 1, LENGTH = 0x100 - (__rc31 + 1)`).

## Reference: the X16 assembly build

| | bytes |
|---|---|
| Machine code (6,133 instructions, 2.37 avg) | 14,564 |
| Embedded tables + `!SCR` strings | 3,184 |
| **X16ROBOTS.PRG** | **17,748** |

## Spike 0a — the portable C++ port on llvm-mos

`zeropolis79/PETSCIIRobots-SDL` `petrobots.cpp` (5,892 lines) + `Platform.cpp`, against a
stub `PlatformRP6502` whose every method is a no-op. Character-based tile rendering
(`PLATFORM_SPRITE_SUPPORT` off — it swaps character tiles for image tiles, and mode 1 draws
characters). `PLATFORM_IMAGE_SUPPORT`, `PLATFORM_LIVE_MAP_SUPPORT`, `PLATFORM_COLOR_SUPPORT`,
`PLATFORM_CURSOR_SUPPORT` on; `PLATFORM_MODULE_BASED_AUDIO`, `OPTIMIZED_MAP_RENDERING` off.
Built `-Os -fno-lto -fno-exceptions -fno-rtti`.

| section | bytes |
|---|---|
| `.text` | 34,930 |
| `.rodata` | 282 |
| `.data` | 2,413 |
| `.bss` | 17,442 |
| `.noinit` | 14 |
| `.zp.data` + `.zp` | 48 |
| **RAM used, `$0200–$D94F`** | **55,375** |
| **free** | **9,393** |

Largest `.bss`: `MAP_DATA` 8,960, tileset 6,656, `SCREEN_MEMORY` 1,200 — all real, all
required by the design.

Largest `.text`: `MAIN_GAME_LOOP` 2,310, `main` 1,297, `PLOT_TRANSPARENT_TILE` 1,196,
`MOVE_OBJECT` 1,193, `ELEVATOR_SELECT` 1,141, `GAME_OVER` 1,067, `PLOT_TILE` 968.

### Verdict

**It fits, with 9.4 KB spare — but that spare is the entire budget for work not yet written.**
The stub platform is empty; the real one has to supply video, input, storage, the PSG driver
(~800 B) and the music/SFX data (~1,456 B). A 4–8 KB platform layer leaves 2–5 KB of margin
on a 64 KB machine. Viable, not comfortable.

### LTO is not available

`-flto` fails to link: `relocation R_MOS_ADDR8 out of range: 256 is not in [-128, 255];
references section '.zp.noinit'`. Whole-program LTO allocates more static stack frames into
zero page than the 224 available bytes hold. Swept `-mlto-zp` at 8/16/32/64 (8 came closest,
2 errors rather than 6) and devirtualised the platform class with `final`; neither fixes it.
So the usual llvm-mos size lever is off the table for this codebase, which matches the
warning that llvm-mos has no size-priority option that works.

`PLOT_TILE` at 968 bytes and `PLOT_TRANSPARENT_TILE` at 1,196 are both the hot path and
prime candidates for hand-written `.s` — that would buy size and speed together.

### cc65 is not an option for this path

`cc65-toolchain.cmake` fatals with "cc65 has no C++ compiler". The C path is llvm-mos only.

## Spike 0b — ACME → ca65 conversion

`tools/convert/acme2ca65.py`, **164 lines**, run over all 8,505 lines of ACME source
(`x16Robots.ASM`, `BACKGROUND_TASKS.ASM`, `sounds.asm`, `sounds.inc`, `zsound.inc`, and the
five payload loaders).

- **0 lines the converter could not handle.**
- **0 ca65 errors.**
- **All six programs assemble and link byte-identical to the shipped X16 builds:**

| program | bytes | |
|---|---|---|
| `X16ROBOTS.PRG` | 17,748 | identical |
| `PAYLOAD1.PRG` | 4,141 | identical |
| `PAYLOAD2.PRG` | 6,189 | identical |
| `PAYLOAD3.PRG` | 8,237 | identical |
| `PAYLOAD4.PRG` | 13,681 | identical |
| `PAYLOAD5.PRG` | 3,757 | identical |

PAYLOAD3 and PAYLOAD5 rebuilding bit-for-bit also **proves the carve offsets** for the two
assets that survive only inside them: `CURSORS4BIT.BIN = PAYLOAD3.PRG[47:1583]` (1,536 B) and
`gamepic.rle = PAYLOAD5.PRG[146:3759]` (3,613 B).

### The three things that were not mechanical

Worth recording, because they are the whole difference between "assembles" and "identical":

1. **`!SCR` lowercase.** ACME maps `a`–`z` to screen codes 1–26, i.e. `-0x60`, not the `-0x20`
   of the textbook PETSCII→screencode table. Verified against the binary: `"searching"`
   assembles to `13 05 01 12 03 08 09 0e 07`.
2. **`!ifndef SYM !eof`** means *stop assembling when SYM is undefined*, so everything below it
   is conditional on SYM being **defined** — `.ifdef`, not `.ifndef`. Getting this backwards
   added 3 bytes at the end of `sounds.asm`.
3. **ACME sizes an operand by how the literal is written.** `STA $00C6` is four hex digits, so
   ACME assembles it absolute; ca65 always shortens to zero page. Four sites (`$00C6` ×3,
   `$009E` ×1) need ca65's `a:` prefix. This was the missing 4 bytes.

## Decision: the assembly path

| | assembly (cc65/ca65) | C (llvm-mos) |
|---|---|---|
| Program | **17,748 B** | 37,625 B code+data |
| Resident data | ~14,500 B | 17,442 B |
| **RAM free** | **~47 KB** | **9.4 KB** |
| Behavioural risk | none — provably identical machine code | transliteration, shipped elsewhere |
| LTO | n/a | unusable (zero page overflows) |
| Conversion cost | paid: 164-line script, byte-exact | none |

Assembly wins on every axis that matters here. The 9.4 KB the C path leaves has to absorb the
entire platform layer, the PSG driver and the music data, none of which is written yet; the
assembly path starts with 47 KB free.

**The presentation layer still has to be rewritten** — roughly 2,900 lines of VERA/KERNAL/ZSOUND
code. That new code can be written in **cc65 C and linked against the ca65 objects**, which is
the arrangement cc65 is built for: proven game logic stays as assembly, new platform code is
readable C, and the size budget stays comfortable either way.
