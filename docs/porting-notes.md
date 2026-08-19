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

### Zero page belongs to the game, and the linker enforces it

The ported assembly names the addresses David Murray gave it -- `$02/$03` for
the message pointer, `$04/$05` for the map pointer, `$23-$3B` for everything
else. `src/rp6502-petscii.cfg` gives the game that range as its own memory area
and starts cc65's runtime zero page at `$3C`:

    GAMEZP:   file = "", define = yes, start = $0002, size = $003A;
    ZP:       file = "", define = yes, start = $003C, size = $00C4;

so `$04` in the ported code is `$04`, and `src/game/background_tasks.s` and
`src/game/items.s` are the unedited conversion apart from the platform calls.
cc65's runtime turns out to need only 26 bytes, so there is room to spare.

It was not always done this way, and the detour is worth recording. The first
attempt translated the game's zero page onto named variables instead, and the
substitution covered only the direct forms -- `LDA $04`, `STA $04` -- missing
the sixteen **indirect** ones, `LDA ($04),Y`. The result stored through the new
pointer and read through cc65's `$04`, so `GET_TILE_FROM_MAP` returned whatever
the C runtime had left lying there.

That does not present as a bad address. The player simply stops walking, because
the tile lookup returns a number whose attributes say the way is blocked -- and
the map, the unit arrays and `TILE_ATTRIB` all check out against their source
files while you look for it.

`globals.s` pins each address with `.assert`, so the layout is checked every
build, and `tests/reference/verify_conversion.py` checks the partition and the
assertions are still there. Both were verified by breaking them.

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

## The live map, and the end of a game

TAB draws the whole 128x64 map into the bitmap plane through
`MAP_TRANSLATION_TABLE`, one byte per tile with each row emitted twice so it
fills 128x128 pixels under the blanked playfield. Both copies of a row go out
together, portal 0 to the even scanline and portal 1 to the odd one, so each
tile is translated once. Leaving restores the backdrop by reading the slice back
out of the ROM rather than blanking it, which costs the 6502 nothing and puts
the real artwork back instead of assuming it was black.

The stats screen needs the backdrop out of the way. The X16 turns VERA's layer 0
off; mode 3 has no enable bit, but the plane's position is a signed pixel offset,
so `x_pos_px = -320` moves it a screenful left without disturbing the pixels.

### Anything an interrupt writes has to be volatile

`GAME_OVER` waits by spinning on `KEYTIMER`, which the VSYNC handler decrements.
Without `volatile` the compiler is entitled to hoist the load out of the loop,
and it does: the game reached the GAME OVER box and stayed there. Every variable
`src/game/irq.s` touches is declared volatile in `game.h` now.

Worth noting for anyone debugging the emulator scripts: `expect` consumes the
console buffer up to the point it checks, so two `expect`s in a row after a
single `run` will see the second string as missing even when it was printed.
`wait` is the one to use when checking that a sequence of things happened.

### Not yet covered by a test

Game over is verified by hand, not by a script. Dying takes a long walk to the
robots, and winning needs the transporter at 74,45 -- 59 tiles away, and it only
activates once every robot is dead. Both endings were checked with a throwaway
build that forced the transition; the screens are right, but until there is a
way to script a death cheaply this is the one path CI does not cover.

## The elevator panel

`plat_elevator_select` draws the floor numbers along the bottom row, highlights
the one the player is on, and moves him when left or right picks another. The
window offsets look inconsistent -- `x - 5` but `y - 4` -- and are not: the
original decrements the player's Y after reading it, so he ends up one square
above the elevator and the window four above that, which is the same viewport
row 3 every other move puts him on.

**This one is untested.** It compiles and reads correctly against the original,
but no level puts an elevator somewhere a script can reach cheaply. The nearest
is level-k's, eight tiles from the start, and the route runs around a bunker and
through a corridor where a robot stands in the way. Worth coming back to with a
scripted path, or a test hook that places the player.

## ANIMATE_WATER: rewritten rather than converted

The one routine of David Murray's that could not be converted mechanically.
Everything it does is machine independent -- it permutes bytes of the tileset
and sets `REDRAW_WINDOW` -- but it reaches them through the PET's nine
`TILE_DATA_xx` and nine `TILE_COLOR_xx` arrays, one 256-byte array per cell
position. This port stores the tileset the other way round, eighteen bytes per
tile with glyph and colour interleaved and the colour pre-masked, which is what
makes the blitter six portal writes per row. Structure of arrays into array of
structures is not something symbol arithmetic can bridge, so the routine is
re-expressed in `src/platform.c` against `tile_cells`, with the addressing in
two macros.

Five animations share one twenty-frame timer, as the original runs them: water
(tiles 204 and 221), the trash compactor (148), the HVAC fans (196, 197, 200,
201), the cinema marquee (20, 21, 22, scrolling the 197 bytes of
`CINEMA_MESSAGE`) and the server room light (143). Tile 221 gets five of its
nine cells updated rather than all nine; that is the original's doing and is
reproduced rather than tidied.

`tests/emu/animate.txt` walks to the water in level-a and watches one cell come
back round every third tick, which catches a rotation that runs backwards or
drops a cell as readily as one that does not run.

## The tile blitter had to be assembly

`PLOT_TILE` was C, and it cost most of a frame.

It surfaced when `ANIMATE_WATER` landed. The original sets `REDRAW_WINDOW` from
the interrupt every twenty frames so the animations reach the screen, and two
tests that had been passing started failing on timing. Measured by forcing a
full window redraw every frame and counting main loop passes against video
frames:

| | passes per 180 frames |
|---|---|
| C blitter, redraw every pass | 95 (1.9 frames per pass) |
| same with `plot_tile` skipped entirely | 180 (1.0) |
| same with `MAP_PRE_CALCULATE` skipped | 91 (no change) |

So the whole cost was the blitter and none of it was the precalculation: about
15 ms of a 16.7 ms frame for a 77-tile window, roughly 1,560 cycles a tile
against the X16's 219 for the same work. The window redraws on every step the
player takes, so the game was losing most of a frame per step -- and that was
true before the animations existed, which merely made it visible.

cc65 spends it on index arithmetic it cannot keep in registers: a 16-bit
multiply per tile and a pointer increment per byte, reloaded around every store
because `RIA` is volatile. `src/plot.s` writes the same work out by hand at
about 280 cycles a tile, which is what this document predicted for the loop
originally. The redraw now fits inside a frame with room to spare: **180 passes
per 180 frames even with a full redraw on every one of them**, against 95
before. It is also 209 bytes smaller than the C.

The interface is a zero-page pair rather than arguments: the caller sets
`plot_addr` and passes the tile number in A, where fastcall has it anyway, and
the blitter leaves `plot_addr` alone because the window plots terrain and then
the unit overlay at the same address.

Both blitters are verified against the tileset rather than by eye.
`plot_tile` was checked by computing the whole 11x7 window from `level-a` and
`tiles.bin` and comparing all 1,386 bytes of the character plane -- zero
mismatches. `plot_transparent_tile` was checked by forcing a known overlay tile
with four transparent cells over known terrain and confirming each of the nine
cells resolves the right way. The overlay path is covered by `tests/emu/overlay.txt`, which is the only test
that leaves level-a: nothing in level-a puts a mobile unit inside the window
anywhere the player can walk to, since doors and found objects are written into
the map rather than drawn over it. Level-g starts with a robot already in view,
so it reaches the path on the first frame -- and getting there drives the intro
menu's map option, which nothing else did either.

## Key repeat: the original has one, and I had not used it

Reported as "controls suck ass: press down, press right, release right and you
keep going right". Correct, and the cause was that `src/input.c` was invented
rather than ported.

The X16 does have this logic. `KEY_REPEAT` (x16Robots.ASM 1988) runs at the top
of the main game loop, before `GETIN`. It reads `$C5` -- LSTX, the key the
KERNAL currently has down -- and when `KEYTIMER` reaches zero it writes 64 back
to it, which makes the KERNAL believe the key was pressed again and put another
copy in the buffer. The rates are the *game's*, not the input layer's:
`AFTER_MOVE` sets `KEYTIMER` to 15 before the first repeat of a held direction
and 7 after that, firing and cycling set 20, `INIT_GAME` sets 30, the game-over
wait sets 100.

Two things were wrong here. The first is that this port kept "the key being
repeated" as a single remembered code, set on a fresh press and cleared only
when *nothing* was down -- so releasing the newer of two held keys left the
released one repeating. The original cannot have that fault, because it reads
what is down now rather than remembering. The second is bigger: `KEYTIMER` was
written in twenty-odd places in the original and in **none** in this port, so
the game's own pacing was not connected at all and the input layer had invented
its own timings (20 and 7) in its place.

Now `plat_key_repeat` is `KEY_REPEAT` -- newest key still down in the RIA's bit
array standing in for LSTX, and pushing onto the queue `plat_getin` drains
standing in for clearing it -- and the game sets `KEYTIMER` where the original
sets it. `tests/emu/keys.txt` holds down, taps right, releases right and checks
he goes down; before the fix x ran on and y did not move.

Worth stating plainly: the menus call `GETIN` alone and only the game loop calls
`KEY_REPEAT` first, which is why menus act on presses and only walking repeats.
That is the original's arrangement, not a choice made here.

## Driving the game further than a fixed list of keys can reach

`tests/emu` scripts are open loop: press keys, check results, with no way to ask
where the player ended up and decide what to press next. That is fine for a few
steps and useless for crossing a level -- a door costs one press to open before
it costs one to walk through, and a robot in the way costs an unknown number, so
a fixed list desynchronises and every move after that is wrong. Two attempts at
hand-written routes into level-l and level-g died exactly there.

`rp6502-emu` takes `--script -`, so `tools/playthrough.py` drives it a line at a
time: read the player's position out of the probe block, plan a route from
*there* with a breadth-first search over the level's walkability, press one key,
look again. Walkability is `TILE_ATTRIB` bit 0, the same bit `REQUEST_WALK` tests
through `MOVE_TYPE`; door squares count as passable because walking into one
opens it, and the extra press that costs is what the loop absorbs.

What it prints is the script that worked. Replayed open loop that is
deterministic under a fixed seed, so a recorded route is a normal test until the
game's timing changes -- and when it does, regenerating is one command rather
than an afternoon. `tests/emu/flash.txt` is the first test built this way.

### What it still cannot reach, and why

**The win screens.** `TRANSPORTER_PAD` only makes itself active once every unit
in slots 1 to 27 is dead; until then standing on it does nothing. Winning is
therefore a full combat playthrough -- find weapons, find ammunition, clear
twenty-odd robots -- not a route. Surveying all fourteen levels for a pad
reachable without keys found exactly one, level-h's at 21,25, and driving to it
confirms the pad ignores a player who has not cleared the level. So
`plat_game_over`'s win path and the endgame statistics screen stay verified by
hand only.

**Game over.** The same in reverse: the player has to be killed, and the hazards
a route can reach stop hurting him well before zero. Level-e's rollerbot takes
twelve health down to one and then loses interest.

**Locked doors.** `UNIT_C` on a door is 0, or 1, 2, 3 for the spade, heart and
star keys, and `AI_DOOR` will not open a locked one without the key in `KEYS`.
The driver treats locked doors as walls because it starts from a cold boot with
no keys; teaching it to search for keys would make it a game player rather than
a test fixture.

## The border and background flashes

`BORDER` and `BGFLASH` are each one array whose index 0 is a countdown and whose
remaining ten bytes are a colour ramp; the interrupt walks the ramp backwards as
the counter falls. The X16 wrote them to the two halves of VERA palette entry 0
-- `BORDER` the byte holding red, `BGFLASH` the byte holding green and blue.
There is no border here, so both land on entry 0 of the bitmap palette, which is
the black the playfield sits on: the same thing the X16 was tinting.

One difference, deliberately. VERA takes four bits of red and ignores the high
nibble of that byte, so the ramp 8, 15, 25, 31 reached the screen as 8, 15, 9,
15 -- a jump backwards in the middle of a fade the author wrote as monotonic and
symmetric. RP6502 palettes are RGB555, so the five-bit value goes through as
written. The green and blue nibbles are doubled to fill five bits.

Verified by forcing both counters at bring-up and reading the palette entry every
frame: red ramps 0, 8, 15, 25, 31, 31, 25, 15, 8, 0 with blue and green
alongside, the opacity bit stays set throughout, and entry 0 returns to opaque
black on the way out.

`tests/emu/flash.txt` then covers it in real play. The triggers are `USE_EMP`,
the trash compactor reaching the player, and the player taking damage, and
nothing in level-a's start area reaches any of them -- but level-e has a hazard
at 75,42 that hurts the player about once a second, which is a repeatable
trigger. Getting there is 25 squares through doors, which is what
`tools/playthrough.py` is for.

## Sound: the engine the X16 threw away

The RP6502 has a PSG, not a sample player, so the right engine is the original
one -- and it is not in this repo. When the X16 port adopted ZSOUND it replaced
`MUSIC_ROUTINE` and `SET_WAVEFORM` with bare `RTS` stubs and moved to recorded
samples, leaving only the dead state variables behind. `reference/pet/` holds
the PET sources, which still have all of it.

`src/game/music.s` is `PETROBOTS12.ASM` 4583-4790: `PLAY_SOUND`,
`MUSIC_ROUTINE`, `STOP_SONG`. `tools/convert/conv_music.py` extracts the
patterns -- fifteen effects and six songs, 1,240 bytes -- and rebuilds the note
table, because the PET's is VIA shift-register timer values rather than pitches.
Note 1 is B3 and note 2 is C4, so note n is n-2 semitones from C4; the check
that it came out right is note 11 landing on 1320, which is 440 Hz times the
three the RIA wants.

The pattern language: 0 rests, 1-36 play a note with an arpeggio mode in the top
two bits, 37 ends, 38 is note off, 39-48 set the tempo. Songs are 256 bytes and
`DATA_LINE` is a byte, so the wrap *is* the loop.

**The arpeggio modes are dead weight.** The bits are real and the port carries
them, but nothing uses them: `CYCLE_ARP` is commented out in the PET source with
Murray's note that "no music or sound effects were composed to use this feature,
so might as well disable it", and none of the 1,240 bytes shipped here sets bit
6 or bit 7. `ARP_MODE` and `CHORD_ROOT` are written and never read. Describing
this engine as having arpeggio overstates it -- there is nothing to hear.

Four things needed care:

- **The engine ticks from the VSYNC interrupt**, where the PET calls it. Its
  output goes through the PSG registers in XRAM, so it runs inside the portal
  save the handler already does, and `psg_init` happens before the interrupt is
  enabled -- otherwise the first tick would follow an uninitialised pattern
  pointer.
- **The gate is edge-triggered.** Writing 1 while the oscillator is already
  sounding does nothing, so a new note clears the gate and sets it again. Without
  that every pattern is one long slur.
- **`PLAY_SOUND` and `START_MUSIC` update a two-byte pointer the interrupt
  reads.** A tick landing between the halves follows a pointer that is half one
  pattern and half another. The PET has the same shape and the same hazard; it
  costs an `sei`/`cli` pair to not have it.

- **A `JSR` does not preserve A, and the original depends on it.** The note-off
  row silences the PET by storing the zero already in the accumulator, then
  falls through into the tempo test still holding it, so it plays note zero and
  advances the row. Routing the output through a subroutine breaks that: each of
  the 104 note-off rows was then read as whatever the call happened to leave
  behind, which usually landed in the 39-48 tempo range and rewrote `TEMPO`. The
  symptom was a tune that dragged and lurched. `TEMPO` measured 0 through the
  intro song with the bug and a steady 7 without it. An explicit `lda #0`
  restores the invariant the fallthrough assumes.

### The interrupt calls C, so it goes through set_irq

Symptom: after walking a while, small runs of coloured pixels appeared over the
playfield and stayed at a fixed screen position while the map scrolled under
them.

They were tile data in the wrong plane. Each run was six bytes, and every second
byte had a zero high nibble -- which is `tile_cells`' `{glyph, colour & 0x0F}`
pairs -- and the runs sat exactly eighty bytes apart, which is `SCR_STRIDE`. So
one `PLOT_TILE` call had written its three character rows into the bitmap plane
instead of the character plane. The bitmap does not scroll, hence "locked to the
screen", and nothing redraws it during play, hence "and stays there".

The cause is that the VSYNC handler reaches C: `MUSIC_ROUTINE` calls the PSG
driver. cc65 compiles every C function against twenty bytes of shared zero-page
scratch -- the C stack pointer, `sreg`, `regsave`, `ptr1-4`, `tmp1-4` -- so an
interrupt that calls C and does not put them back returns to whatever C function
was running with its pointers and temporaries overwritten. `plot_tile` is C, and
its `addr` is one of those temporaries. `zeropage.inc` names the amount for
exactly this case: `zpsavespace`, "the amount of space that needs to be saved by
an interrupt handler that calls C code".

The fix is not to write that save loop. cc65 already has it: `set_irq()` installs
a C level handler and the runtime's `clevel_irq` wrapper saves and restores that
zero page, switches to a separate C stack and preserves `jmpvec` around the call.
So `src/game/irq.s` exports `_petscii_irq` returning `IRQ_HANDLED` /
`IRQ_NOT_HANDLED` and `main()` installs it with `set_irq`, instead of declaring
a bare `.interruptor`. A bare interruptor is only safe if it never reaches C.

Measured: 18 bytes of the bitmap corrupted after ten steps as an `.interruptor`,
zero after 160 steps through `set_irq`. `tests/emu/irq_zp.txt` walks twenty steps
and checks the three addresses it used to land on.

### Two voices, not one

The PET had a single voice, so a sound effect had to take the music's:
`PLAY_SOUND` parked the song's position in four `_TEMP` variables and
`STOP_SONG` put it back. Two costs come with that. The music stops for the
duration of every beep, and -- because `PLAY_SOUND` does not reset
`TEMPO_TIMER` -- the effect cannot begin until the music's current row ends, up
to seven frames later. On a machine with eight oscillators neither is worth
reproducing by default.

So the engine state is two channels wide, index 0 the music and index 1 the
effects, each with its own tempo, timer, row and pattern pointer.
`MUSIC_ROUTINE` ticks both. `PLAY_SOUND` writes the effect channel and zeroes
its timer, so an effect sounds on the next tick rather than waiting out someone
else's row. The four `_TEMP` variables are gone; there is no longer a position
to save.

The effect voice also gets its own timbre -- a narrower pulse, a faster release
-- since it no longer announces itself by the music stopping. That is the one
piece of design the PET data cannot supply: it only ever named notes.

Configuring with `-DPETSCII_AUTHENTIC_AUDIO=ON` puts the PET's arrangement back
for A/B against real hardware: both channels sound on oscillator 0 and the music
channel simply does not tick while an effect plays, which is what saving and
restoring the position amounted to. It is an assembler symbol, so it goes to
cl65 as `--asm-define` -- `target_compile_definitions` becomes `-D`, which
defines a C preprocessor symbol that ca65 never sees, and the flag silently does
nothing.

`tests/emu/audio_channels.txt` covers both halves: that an effect sounds on
oscillator 1 while the music holds oscillator 0 and keeps advancing its own row,
and that the effect ends and releases its own voice.
