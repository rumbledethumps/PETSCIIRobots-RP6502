; Game state shared between the ported assembly and the C platform layer.
;
; Layout is load-bearing. A level file is 8704 bytes -- eight 64-byte unit
; arrays followed by the 8192-byte map -- so the whole thing is one read() into
; UNIT_TYPE, and MAP lands exactly 512 bytes later.
;
; LEVELDATA is page-aligned by src/rp6502-petscii.cfg because
; GET_TILE_FROM_MAP composes the map address arithmetically rather than by
; indexing: the low byte is ORA MAP_X into a zero, the high byte is
; ADC #>MAP. Both only work if MAP starts on a page boundary, and #>MAP is a
; link-time constant so the alignment has to be static.

        .include "petscii.inc"

; ---- zero page ----------------------------------------------------------
;
; At the addresses David Murray gave them. src/rp6502-petscii.cfg reserves
; $02-$3B for the game and starts cc65's own zero page at $3C, so there is
; nothing to translate and nothing to collide with -- the ported assembly says
; $04 and means $04.
;
; The order below is the original's, gap and all, and the assertions at the end
; check it landed where it should. Getting this wrong used to be silent: a
; routine storing through one address and reading through another looks like
; corrupt map data rather than a bad pointer.

        .segment "GAMEZEROPAGE": zeropage

SOURCE:         .res 2          ; $02  message pointer, for PRINT_INFO
MAP_PTR:        .res 2          ; $04  map pointer, built by GET_TILE_FROM_MAP
                .res 29         ; $06-$22 the original leaves alone
TILE:           .res 1          ; $23  the tile being plotted or fetched
TEMP_X:         .res 1          ; $24  loop counters for the viewport draw
TEMP_Y:         .res 1          ; $25
MAP_X:          .res 1          ; $26  map coordinates, 0..127 and 0..63
MAP_Y:          .res 1          ; $27
MAP_WINDOW_X:   .res 1          ; $28  top-left of the visible map window
MAP_WINDOW_Y:   .res 1          ; $29
DECNUM:         .res 1          ; $2A  a value to print as three digits
ATTRIB:         .res 1          ; $2B
UNIT:           .res 1          ; $2C  the unit being processed
TEMP_A:         .res 1          ; $2D  scratch, shared by several routines
TEMP_B:         .res 1          ; $2E
TEMP_C:         .res 1          ; $2F
TEMP_D:         .res 1          ; $30
CURSOR_X:       .res 1          ; $31  selection cursor, in map window cells
CURSOR_Y:       .res 1          ; $32
CURSOR_ON:      .res 1          ; $33  0 off, 1 compass, 2 magnifier, 3 hand
REDRAW_WINDOW:  .res 1          ; $34  1 = the window needs redrawing
MOVE_RESULT:    .res 1          ; $35  1 = the move happened
UNIT_FIND:      .res 1          ; $36  255 = no unit present
MOVE_TYPE:      .res 1          ; $37  MOVE_WALK or MOVE_HOVER
SCREEN_SHAKE:   .res 1          ; $38  1 = shake the character plane
PRECALC_COUNT:  .res 1          ; $39  index into MAP_PRECALC while drawing
CUR_PATTERN:    .res 2          ; $3A  the music pattern being played

; The layout is load-bearing, so prove it rather than trusting the order above.
        .assert SOURCE        = $02, error, "SOURCE moved"
        .assert MAP_PTR       = $04, error, "MAP_PTR moved"
        .assert TILE          = $23, error, "TILE moved"
        .assert MAP_X         = $26, error, "MAP_X moved"
        .assert UNIT          = $2C, error, "UNIT moved"
        .assert CURSOR_X      = $31, error, "CURSOR_X moved"
        .assert MOVE_TYPE     = $37, error, "MOVE_TYPE moved"
        .assert CUR_PATTERN   = $3A, error, "CUR_PATTERN moved"

; ---- level data ---------------------------------------------------------
        .segment "LEVELDATA"

; One read() fills all of this. Slots: 0 player, 1-27 robots, 28-31 weapons
; fire, 32-47 doors and other unsprited units, 48-63 hidden findable objects.
UNIT_TYPE:      .res 64
UNIT_LOC_X:     .res 64
UNIT_LOC_Y:     .res 64
UNIT_A:         .res 64
UNIT_B:         .res 64
UNIT_C:         .res 64
UNIT_D:         .res 64
UNIT_HEALTH:    .res 64
MAP:            .res 128 * 64

; GET_TILE_FROM_MAP ORs MAP_X into a zero low byte and adds #>MAP to the high
; byte, so a MAP that is not page-aligned reads the wrong tiles quietly rather
; than failing. src/rp6502-petscii.cfg aligns LEVELDATA; this makes sure.
        .assert (MAP .MOD 256) = 0, error, "MAP must start on a page boundary"

; ---- tileset ------------------------------------------------------------
        .segment "BSS"

DESTRUCT_PATH:  .res 256        ; tile -> the tile it becomes when destroyed
TILE_ATTRIB:    .res 256        ; ATTR_* bits

; ---- per-unit working state ---------------------------------------------
; Not part of a level file: rebuilt when a level starts.
UNIT_TIMER_A:   .res 64         ; primary AI timer, counts down to an action
UNIT_TIMER_B:   .res 64         ; secondary timer
UNIT_TILE:      .res 32         ; the tile each visible unit currently draws as
EXP_BUFFER:     .res 16         ; tiles an explosion covered, to restore after

; MAP_PRE_CALCULATE writes the units visible in the 11x7 window here, so the
; draw loop never has to search for them.
MAP_PRECALC:    .res 77

; ---- game flags ---------------------------------------------------------
; Maintained by the VSYNC interrupt in irq.s, exactly as the X16's VBLANK
; handler maintained them.
IRQ_FRAME:      .res 1          ; ticks elapsed, wraps at 256
KEY_FAST:       .res 1          ; 0 until the first repeat of a held move key
KEYTIMER:       .res 1          ; counts down; gates the key repeat
CLOCK_ACTIVE:   .res 1          ; 1 once the level starts
CYCLES:         .res 1          ; the game clock, 60 cycles to the second
SECONDS:        .res 1
MINUTES:        .res 1
HOURS:          .res 1

; ---- the sound engine ---------------------------------------------------
; The PET engine's state, from reference/pet/PETROBOTS12.ASM 242-246 and
; 4619-4621.
;
; The PET had one voice, so an effect had to borrow the music's: PLAY_SOUND
; parked the song's position in a set of _TEMP variables and STOP_SONG put it
; back. The RIA has eight oscillators, so the state is two channels wide
; instead -- index 0 the music, index 1 the effects -- and they run side by
; side. The PETSCII_AUTHENTIC_AUDIO build collapses them back onto one voice
; to A/B against real hardware; see music.s.
;
; Index 0 keeps the original names so that the code reading "the tempo" is
; reading the music's tempo, as it always was.
CH_TEMPO_TIMER:
TEMPO_TIMER:    .res 2          ; ticks left before this channel's next row
CH_TEMPO:
TEMPO:          .res 2          ; ticks between rows
CH_DATA_LINE:
DATA_LINE:      .res 2          ; the row being played; wraps at 256, which is
                                ; how the songs loop -- each is 256 bytes
CH_PATTERN_L:   .res 2          ; the pattern this channel is walking
CH_PATTERN_H:   .res 2
CHANNEL:        .res 1          ; the channel being ticked, for the PSG call

MUSIC_ON:       .res 1          ; 1 = a song is playing
SOUND_EFFECT:   .res 1          ; $FF = none; otherwise the effect number
ARP_MODE:       .res 1          ; 0 none, 1 major, 2 minor, 3 sus4
CHORD_ROOT:     .res 1          ; the note the arpeggio is built on

RANDOM:         .res 1          ; 8-bit LFSR state; seed it non-zero
SSCOUNT:        .res 1          ; screen shake phase, 0..4
BGTIMER1:       .res 1          ; set once a tick, cleared by BACKGROUND_TASKS
BGTIMER2:       .res 1          ; counts down to zero and stays there
BIG_EXP_ACT:    .res 1          ; only one big explosion may run at a time
MAGNET_ACT:     .res 1
PLASMA_ACT:     .res 1
KEYS:           .res 1          ; bit 0 spade, bit 1 heart, bit 2 star

; ---- inventory ----------------------------------------------------------
AMMO_PISTOL:    .res 1
AMMO_PLASMA:    .res 1
INV_BOMBS:      .res 1
INV_EMP:        .res 1
INV_MEDKIT:     .res 1
INV_MAGNET:     .res 1
SELECTED_WEAPON: .res 1         ; 0 none, 1 pistol, 2 plasma
SELECTED_ITEM:  .res 1          ; 0 none, 1 bomb, 2 EMP, 3 medkit, 4 magnet

; ---- player presentation state ------------------------------------------
PLAYER_DIRECTION: .res 1        ; 0 up, 3 down, 6 left, 9 right, 12 dead
PLAYER_ANIMATE:   .res 1        ; 0..2, added to PLAYER_DIRECTION for the frame

; ---- control scheme -----------------------------------------------------
; 0 keyboard, 1 custom keys, 2 gamepad. The X16 called the third one SNES.
CONTROL:        .res 1

; ---- the intro menu -----------------------------------------------------
MENUY:          .res 1          ; which of the four options is selected
MENUCOL:        .res 1          ; the colour it is being flashed in
; SELECTED_MAP lives in messages.s: it came across with the level names it
; indexes, which is where the original keeps it too.
DIFF_LEVEL:     .res 1          ; 0 easy, 1 normal, 2 hard
SPRITECOLSTATE: .res 1          ; walks SPRITECOLCHART for the flash
SPRITECOLTIMER: .res 1

; The thirteen bindings, in the order STANDARD_CONTROLS lists them. Declared as
; one array because SET_CUSTOM_KEYS fills it with STA KEY_MOVE_UP,Y.
KEY_MOVE_UP:      .res 1
KEY_MOVE_DOWN:    .res 1
KEY_MOVE_LEFT:    .res 1
KEY_MOVE_RIGHT:   .res 1
KEY_FIRE_UP:      .res 1
KEY_FIRE_DOWN:    .res 1
KEY_FIRE_LEFT:    .res 1
KEY_FIRE_RIGHT:   .res 1
KEY_CYCLE_WEAPONS: .res 1
KEY_CYCLE_ITEMS:  .res 1
KEY_USE:          .res 1
KEY_SEARCH:       .res 1
KEY_MOVE:         .res 1

; New gamepad presses, latched until the game consumes them. Same order the X16
; unpacked them in, and contiguous because SNES_CONTROLER_READ walks all twelve
; with one index.
NEW_BUTTONS:
NEW_B:      .res 1
NEW_Y:      .res 1
NEW_SELECT: .res 1
NEW_START:  .res 1
NEW_UP:     .res 1
NEW_DOWN:   .res 1
NEW_LEFT:   .res 1
NEW_RIGHT:  .res 1
NEW_A:      .res 1
NEW_X:      .res 1
NEW_BACK_L: .res 1
NEW_BACK_R: .res 1

        .segment "DATA"

; The border flash. Index 0 is the countdown; the rest is the colour ramp the
; IRQ walked on the X16. Kept as one array because the original decrements
; BORDER and then indexes BORDER,X with it.
BORDER:         .byte 0, 0, 8, 15, 25, 31, 31, 25, 15, 8, 0

; The background flash, same shape: index 0 counts down, the rest is the ramp.
BGFLASH:        .byte 0, 0, 8, 15, 79, 159, 159, 79, 15, 8, 0

; The colours the selected menu option cycles through.
SPRITECOLCHART: .byte 0, 11, 12, 15, 1, 15, 12, 11

; The thirteen defaults: I K J L to walk, W S A D to fire, F1 and F3 to cycle,
; space to use, Z to search, M to push. Copied into KEY_MOVE_UP at level start.
STANDARD_CONTROLS:
        .byte 73, 75, 74, 76           ; move    up down left right
        .byte 87, 83, 65, 68           ; fire    up down left right
        .byte 133, 134                 ; cycle   weapons, items
        .byte 32, 90, 77               ; use, search, push
