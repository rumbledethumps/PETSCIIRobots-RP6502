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
