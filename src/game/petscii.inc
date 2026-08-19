; Shared declarations for the ported game logic.
;
; The routines below are David Murray's, converted from ACME to ca65 by
; tools/convert/acme2ca65.py and then adapted for the RP6502. Every deliberate
; difference from reference/x16/BACKGROUND_TASKS.ASM is commented at the site.

; ---- zero page ----------------------------------------------------------
; The X16 build assigns fixed zero page addresses. Under cc65 the linker owns
; zero page, so these are allocated in the ZEROPAGE segment instead. The one
; that matters is MAP_PTR: the original hardcodes $04/$05 as its map pointer,
; which belongs to the C runtime here.
        .globalzp TILE, MAP_X, MAP_Y, UNIT, MOVE_TYPE, MOVE_RESULT
        .globalzp UNIT_FIND, MAP_PTR, RANDOM

; ---- level data ---------------------------------------------------------
        .global UNIT_TYPE, UNIT_LOC_X, UNIT_LOC_Y
        .global UNIT_A, UNIT_B, UNIT_C, UNIT_D, UNIT_HEALTH
        .global MAP

; ---- tileset ------------------------------------------------------------
        .global DESTRUCT_PATH, TILE_ATTRIB

; ---- routines -----------------------------------------------------------
        .global GET_TILE_FROM_MAP, PLOT_TILE_TO_MAP, CHECK_FOR_UNIT
        .global REQUEST_WALK_UP, REQUEST_WALK_DOWN
        .global REQUEST_WALK_LEFT, REQUEST_WALK_RIGHT
        .global GENERATE_RANDOM_NUMBER

; MOVE_TYPE bits, tested as (TILE_ATTRIB & MOVE_TYPE) == MOVE_TYPE
MOVE_WALK       = %00000001
MOVE_HOVER      = %00000010

; TILE_ATTRIB bits
ATTR_WALKABLE     = %00000001
ATTR_HOVERABLE    = %00000010
ATTR_PUSHABLE     = %00000100
ATTR_DESTRUCTIBLE = %00001000
ATTR_SEETHROUGH   = %00010000
ATTR_DESTINATION  = %00100000
ATTR_SEARCHABLE   = %01000000
