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
        .segment "ZEROPAGE"

TILE:           .res 1          ; the tile number being plotted or fetched
MAP_X:          .res 1          ; map coordinates, 0..127 and 0..63
MAP_Y:          .res 1
UNIT:           .res 1          ; the unit being processed
MOVE_TYPE:      .res 1          ; MOVE_WALK or MOVE_HOVER
MOVE_RESULT:    .res 1          ; 1 = the move happened
UNIT_FIND:      .res 1          ; 255 = no unit present
RANDOM:         .res 1          ; 8-bit LFSR state
MAP_PTR:        .res 2          ; was the X16's hardcoded $04/$05

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
