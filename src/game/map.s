; Map access. From reference/x16/BACKGROUND_TASKS.ASM lines 2176-2212.
;
; The address is built, not indexed: for a 128-wide map,
;     high = (MAP_Y >> 1) + >MAP        carry out of the shift is bit 0 of Y
;     low  = (carry << 7) | MAP_X
; which is $6000 + MAP_Y * 128 + MAP_X on the X16. Because MAP_X is OR-ed in
; rather than added, an X of 128 or more silently aliases into the next row --
; the original has the same property, and every caller is bounds-checked before
; it gets here.
;
; Only change from the original: the pointer lives in MAP_PTR rather than the
; hardcoded $04/$05, which cc65's runtime owns.

        .include "petscii.inc"
        .segment "CODE"

; TILE -> map[MAP_X, MAP_Y]
PLOT_TILE_TO_MAP:
        ldy     #0
        lda     MAP_Y
        clc
        ror     a
        php                     ; carry = bit 0 of MAP_Y, i.e. the odd row
        clc
        adc     #>MAP
        sta     MAP_PTR+1
        lda     #$00
        plp
        ror     a               ; $80 for an odd row, $00 for an even one
        ora     MAP_X
        sta     MAP_PTR
        lda     TILE
        sta     (MAP_PTR),y
        rts

; map[MAP_X, MAP_Y] -> TILE
GET_TILE_FROM_MAP:
        ldy     #0
        lda     MAP_Y
        clc
        ror     a
        php
        clc
        adc     #>MAP
        sta     MAP_PTR+1
        lda     #$00
        plp
        ror     a
        ora     MAP_X
        sta     MAP_PTR
        lda     (MAP_PTR),y
        sta     TILE
        rts
