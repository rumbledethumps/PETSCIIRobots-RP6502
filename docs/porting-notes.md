# Porting notes

`reference/x16/` holds David Murray's Commander X16 sources exactly as
published, and `tests/reference/verify_conversion.py` proves the ACME→ca65
converter still rebuilds his binaries from them byte for byte. `src/` is the
port. Every difference between the two is deliberate, and this file records it.

## What came across unchanged

These are the original instructions, converted by `tools/convert/acme2ca65.py`
and then adapted only where the RP6502 forced it.

| `src/game/` | from `reference/x16/BACKGROUND_TASKS.ASM` | |
|---|---|---|
| `map.s` `GET_TILE_FROM_MAP`, `PLOT_TILE_TO_MAP` | 2176–2212 | map read/write |
| `move.s` `REQUEST_WALK_UP/DOWN/LEFT/RIGHT` | 2306–2417 | one square, bounds and tile attributes |
| `move.s` `CHECK_FOR_UNIT` | 2422–2440 | occupancy, slots 0–27 only |
| `rng.s` `GENERATE_RANDOM_NUMBER` | 231–237 | 8-bit LFSR, `EOR #$1D` |

### Changes forced by the target

- **The map pointer.** The X16 uses zero page `$04/$05` as scratch. cc65's
  runtime owns those, so the port uses a named `MAP_PTR` allocated in the
  `ZEROPAGE` segment. Same two bytes, same use.
- **Zero page in general.** The X16 assigns fixed addresses (`TILE` = `$23`,
  `MAP_X` = `$26`, …). Under cc65 the linker allocates them; `src/game/capi.s`
  exports the C-visible aliases and `game.h` declares them with `#pragma zpsym`
  so cc65 emits zero-page addressing rather than absolute.
- **`MAP` alignment.** `GET_TILE_FROM_MAP` composes the address arithmetically
  — `ORA MAP_X` into a zero low byte, `ADC #>MAP` into the high byte — so `MAP`
  must start on a page boundary and `#>MAP` must be a link-time constant.
  `src/rp6502-petscii.cfg` adds an aligned `LEVELDATA` segment, and
  `globals.s` carries `.assert (MAP .MOD 256) = 0` so a future rearrangement
  fails the build instead of quietly reading the wrong tiles.
  `LEVELDATA` is placed *after* the loaded segments: it is `bss`, so it is not
  in the output file, and `rp6502_executable()` loads that file at a fixed
  address — putting an unwritten segment first shifts the code out from under
  the load address.

### Behaviour kept on purpose

- **The walk bounds are equality tests.** `CMP #122 / BEQ` blocks a unit
  standing exactly on column 122 and lets one at 123 keep going. No shipped
  level starts a unit outside the bounds, so it never fires; changing it would
  be a behaviour change, not a fix.
- **The tile test is `(attrib & MOVE_TYPE) == MOVE_TYPE`**, not a bare `AND`. A
  unit that can both walk and hover needs a tile that permits both.
- **`CHECK_FOR_UNIT` stops at 28**, so weapons fire (28–31), doors (32–47) and
  hidden objects (48–63) are invisible to it. That is what lets the player walk
  onto a door tile or over a key.

## What was replaced rather than ported

The presentation and I/O are new code, because nothing about VERA, the KERNAL
or ZSOUND survives the move.

| X16 | RP6502 |
|---|---|
| VERA layer 1 text mode, 128-wide tilemap | VGA mode 1, 40×30 character plane, stride 80 |
| VERA layer 0 320×240 4bpp bitmap | VGA mode 3 on plane 0 |
| 4 VERA sprites, one 64×32 | 6 mode 5 sprites, all 32×32 — mode 5 is square-only, so each 64×32 HUD icon is two sprites |
| VERA palette offset 1 | each mode 5 sprite carries its own `palette_ptr` |
| `L1_HSCROLL_L` screen shake | the mode 1 config's `x_pos_px` |
| KERNAL `GETIN`, PETSCII codes | the RIA's 32-byte USB HID keycode bit array |
| `LOAD_PAYLOAD` and six uploader `.PRG`s | `rp6502_asset()` plus `read_xram()` |
| CINV IRQ chain plus a VERA line IRQ | polling `RIA.vsync` as a counter delta |
| ZSOUND, YM2151, VERA PCM | the RIA PSG (not yet written) |

Reading `RIA.vsync` as a delta rather than a change flag also fixes something:
the X16's IRQ-driven `UPDATE_GAME_CLOCK` simply lost a tick whenever a frame
overran.

## Bugs in the original, not yet reached

To fix when the routines that contain them are ported, each with a test
asserting the original behaviour first:

- `x16Robots.ASM:2091` — `STA $E3C9,X` inside `SEARCH_OBJECT`, a PET-era
  leftover with an unconstrained X. Ignored on the X16 because `$E3C9` is ROM;
  on the RP6502 it is RAM. **Delete it.**
- `SOBJ20`–`SOBJ23` compare a clobbered accumulator after `JSR PRINT_INFO` /
  `DISPLAY_WEAPON`.
- `EXEC05`/`EXEC06` fall through, so "cycle map" also runs the difficulty test.
- `IRQ20` has `STA KEYSOFF` commented out, so `KEYSOFF` free-runs.

## The whole of BACKGROUND_TASKS.ASM

`src/game/background_tasks.s` is the complete file — the dispatcher and all 24
unit types — converted by `tools/convert/acme2ca65.py` and then
`tools/convert/port_zeropage.py`. It is the only part of the port that came
across whole, because it is the part David Murray deliberately kept
machine-independent across the C64, PET, VIC-20 and X16.

`src/game/window.s` and `src/game/transporter.s` hold the routines the AI calls
that lived in the X16's machine-specific file but are pure logic:
`CACULATE_AND_REDRAW`, `MAP_PRE_CALCULATE`, `CHECK_FOR_WINDOW_REDRAW`, and
`DEMATERIALIZE`. `src/game/platform_bridge.s` forwards the rest to C.

### The zero page remap, and the form that is easy to miss

The file keeps two pointers in fixed low zero page, which cc65's runtime owns:

    $02/$03 -> SOURCE    the message pointer PRINT_INFO reads
    $04/$05 -> MAP_PTR   the map pointer GET_TILE_FROM_MAP builds

The first attempt substituted only the direct forms, `LDA $04` and `STA $04`,
and missed the sixteen **indirect** ones, `LDA ($04),Y`. The result stores
through `MAP_PTR` and reads through cc65's `$04`, so `GET_TILE_FROM_MAP` returns
whatever the C runtime happened to leave there. That does not present as a bad
address: the player simply stops walking, because the tile lookup returns a
number whose attributes say the way is blocked. Two hours of the map, the unit
arrays and `TILE_ATTRIB` all checking out against their source files.

`port_zeropage.py` now substitutes at the operand level, skips immediates
(`#$04` is the number four), and refuses to write a file that still names low
zero page in any addressing mode. `tests/reference/verify_conversion.py` runs
the same check over the committed sources, so a hand-edit cannot reintroduce it.

### A fixed bug

`DEMATERIALIZE` (`x16Robots.ASM:4817`) loads X with 40 as a VERA register value
and then, still holding 40, does `STA UNIT_TIMER_A,X` — so it sets slot 40's
timer instead of its own. Slot 40 is in the 32-47 band, which holds doors and
elevators, so on the X16 a transporter in use nudges whatever door occupies that
slot. The surrounding code says plainly what was meant, and the 40 is an
artifact of VERA writes this port does not have, so it is `LDX UNIT` here.

### What the platform layer still stubs

These are real entry points the AI calls; they return cleanly and do nothing
yet. Listed so nobody mistakes a silent one for a working one:

| stub | lands in |
|---|---|
| `plat_play_sound` | M7, the PET music engine on the RIA PSG |
| `plat_print_info` | M5, the three-line message console |
| `plat_display_item`, `plat_display_player_health` | M5, the HUD |
| `plat_elevator_select` | M6, the elevator UI |

## Items, searching, pushing and firing

`src/game/items.s` is `x16Robots.ASM` lines 1468-1986 and 2032-2544: `USE_ITEM`
and the four things it dispatches to, all eight weapons-fire spawns,
`SEARCH_OBJECT`, `MOVE_OBJECT` and `USER_SELECT_OBJECT`. This is the half of the
machine-specific file that is not actually machine-specific -- using an item,
spawning a shot, searching a crate and pushing an object are decisions about
game state.

Two things reached past that and had to change:

- **The search progress dots.** The original pokes a period straight into video
  memory at row 29, column 9 + `SEARCHBAR`. That is now `plat_search_dot` with
  the index in A.
- **`JSR $FFE4`**, KERNAL GETIN. On the RP6502 that address is the RIA's `RW0`
  portal, so the call would pull a byte out of XRAM and advance the portal
  address. `plat_getin` returns what GETIN returned -- the next key code, or
  zero -- and `src/input.c` keeps producing the codes `STANDARD_CONTROLS` names,
  so every comparison in the ported assembly is unchanged.

The `STA $E3C9,X` inside `SEARCH_OBJECT` is deleted. It is a PET-era leftover
with an unconstrained X; on the X16 the target is ROM so the write evaporates,
and here it is RAM.

## The tick, and what belongs where

The game runs on a sixty-times-a-second tick: timers count down, the game clock
advances, and `BACKGROUND_TASKS` gets its cue to run one pass of unit AI. That
is a display-rate heartbeat, so it lives on the RIA's VSYNC interrupt, which is
where the X16 has it too.

It has to be an interrupt rather than something the main loop does. The game
waits by spinning: `SEARCH_OBJECT` sits on `BGTIMER2` calling
`BACKGROUND_TASKS` until it reaches zero, and nothing inside that loop would
ever decrement it.

`src/game/irq.s` also carries the video register updates, because the start of
vblank is when a plane's scroll position or a palette entry can change without
tearing. The main loop decides what the registers should say and leaves it in
RAM; the handler pushes it out. That means touching XRAM from an interrupt, and
the two portals are global state with no save area, so the handler saves and
restores portal 1 around its writes -- six bytes at sixty hertz, and the main
loop never has to know.

The VIA is left alone. It is a free-running timer, which suits genuinely
asynchronous work; the game tick is not that.

## The intro screen and the menu

`src/game/screens.s` carries the four screen layouts and the intro artwork from
`x16Robots.ASM` 5327-5457. The layouts are run-length encoded 40x30 character
grids -- a literal byte is one cell, and byte 96 introduces "the next byte,
count+1 times". They stay compressed: `SCR_TEXT` is eighteen bytes encoded and
twelve hundred expanded, so decoding costs about sixty bytes of code and saves
four thousand.

The menu itself is presentation, so it is C rather than ported assembly, but it
keeps the original's shape: four options at character rows 2 to 5, the selected
one flashed by cycling its colour through `SPRITECOLCHART`, and the intro
robot's expression redrawn from `THREE_FACES` when the difficulty changes -- a
16x10 image at 2bpp, dropped into the bitmap plane at 234,95.

One thing worth knowing for the tests: the game reads the RIA's HID keycode bit
array, so the emulator's `type` command does not reach it. `type` goes through
the terminal; `press` and `release` drive the bitmap. Every test drives the game
with `press`.

`frames_total`, which the tests synchronise on, is reset when play starts rather
than counted from boot, so a checkpoint means the same thing however long
someone sat on the menu.
