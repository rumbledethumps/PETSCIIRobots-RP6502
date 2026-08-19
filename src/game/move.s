; Movement and occupancy, from reference/x16/BACKGROUND_TASKS.ASM lines
; 2306-2440. Shared by the player and every walking or hovering robot: the
; caller sets UNIT and MOVE_TYPE, calls one of these, and reads MOVE_RESULT.
;
; Two behaviours here are quirks of the original that the port keeps on
; purpose, both pinned by tests:
;
;   * The bounds are equality tests, not ranges. CMP #122 / BEQ blocks a unit
;     standing exactly on column 122 and lets one at 123 walk further right.
;     Nothing in a shipped level starts outside the bounds, so it never
;     triggers, but "fixing" it would change behaviour rather than preserve it.
;
;   * The tile test is (TILE_ATTRIB & MOVE_TYPE) == MOVE_TYPE, not a bare AND.
;     A unit that could both walk and hover needs a tile that permits both.

        .include "petscii.inc"
        .segment "CODE"

REQUEST_WALK_RIGHT:
        ldy     UNIT
        lda     UNIT_LOC_X,y
        cmp     #122
        beq     MGR01
        sta     MAP_X
        inc     MAP_X
        lda     UNIT_LOC_Y,y
        sta     MAP_Y
        jsr     GET_TILE_FROM_MAP
        ldy     TILE
        lda     TILE_ATTRIB,y
        and     MOVE_TYPE
        cmp     MOVE_TYPE
        bne     MGR01
        jsr     CHECK_FOR_UNIT
        lda     UNIT_FIND
        cmp     #255
        bne     MGR01
        ldx     UNIT
        inc     UNIT_LOC_X,x
        lda     #1
        sta     MOVE_RESULT
        rts
MGR01:  lda     #0
        sta     MOVE_RESULT
        rts

REQUEST_WALK_LEFT:
        ldy     UNIT
        lda     UNIT_LOC_X,y
        cmp     #5
        beq     MGL01
        sta     MAP_X
        dec     MAP_X
        lda     UNIT_LOC_Y,y
        sta     MAP_Y
        jsr     GET_TILE_FROM_MAP
        ldy     TILE
        lda     TILE_ATTRIB,y
        and     MOVE_TYPE
        cmp     MOVE_TYPE
        bne     MGL01
        jsr     CHECK_FOR_UNIT
        lda     UNIT_FIND
        cmp     #255
        bne     MGL01
        ldx     UNIT
        dec     UNIT_LOC_X,x
        lda     #1
        sta     MOVE_RESULT
        rts
MGL01:  lda     #0
        sta     MOVE_RESULT
        rts

REQUEST_WALK_DOWN:
        ldy     UNIT
        lda     UNIT_LOC_Y,y
        cmp     #60
        beq     MGD01
        sta     MAP_Y
        inc     MAP_Y
        lda     UNIT_LOC_X,y
        sta     MAP_X
        jsr     GET_TILE_FROM_MAP
        ldy     TILE
        lda     TILE_ATTRIB,y
        and     MOVE_TYPE
        cmp     MOVE_TYPE
        bne     MGD01
        jsr     CHECK_FOR_UNIT
        lda     UNIT_FIND
        cmp     #255
        bne     MGD01
        ldx     UNIT
        inc     UNIT_LOC_Y,x
        lda     #1
        sta     MOVE_RESULT
        rts
MGD01:  lda     #0
        sta     MOVE_RESULT
        rts

REQUEST_WALK_UP:
        ldy     UNIT
        lda     UNIT_LOC_Y,y
        cmp     #3
        beq     MGU01
        sta     MAP_Y
        dec     MAP_Y
        lda     UNIT_LOC_X,y
        sta     MAP_X
        jsr     GET_TILE_FROM_MAP
        ldy     TILE
        lda     TILE_ATTRIB,y
        and     MOVE_TYPE
        cmp     MOVE_TYPE
        bne     MGU01
        jsr     CHECK_FOR_UNIT
        lda     UNIT_FIND
        cmp     #255
        bne     MGU01
        ldx     UNIT
        dec     UNIT_LOC_Y,x
        lda     #1
        sta     MOVE_RESULT
        rts
MGU01:  lda     #0
        sta     MOVE_RESULT
        rts

; Is a unit standing on MAP_X, MAP_Y? Result in UNIT_FIND, 255 for none.
;
; The scan stops at 28, so weapons fire (28-31), doors (32-47) and hidden
; objects (48-63) are invisible to it -- which is what lets the player walk
; over a key and onto a door tile.
CHECK_FOR_UNIT:
        ldx     #0
CFU00:  lda     UNIT_TYPE,x
        cmp     #0
        bne     CFU02
CFU01:  inx
        cpx     #28
        bne     CFU00
        lda     #255
        sta     UNIT_FIND
        rts
CFU02:  lda     UNIT_LOC_X,x
        cmp     MAP_X
        bne     CFU01
        lda     UNIT_LOC_Y,x
        cmp     MAP_Y
        bne     CFU01
        stx     UNIT_FIND
        rts
