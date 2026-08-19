; The game's random number generator, from
; reference/x16/BACKGROUND_TASKS.ASM lines 231-237.
;
; An 8-bit LFSR over x^8+x^4+x^3+x^2+1, which is primitive, so seeding with
; anything non-zero walks all 255 values before repeating. Zero is a fixed
; point of a bare shift-and-EOR, so the original special-cases it into the EOR
; branch; that is what makes the sequence 255 long rather than stalling.

        .include "petscii.inc"
        .segment "CODE"

GENERATE_RANDOM_NUMBER:
        lda     RANDOM
        beq     DOEOR
        asl     a
        bcc     NOEOR
DOEOR:  eor     #$1D
NOEOR:  sta     RANDOM
        rts
