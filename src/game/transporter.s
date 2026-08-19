; The dematerialize effect, from reference/x16/x16Robots.ASM lines 4817-4874.
;
; Unit type 23. It runs for 48 frames cycling a palette entry, then either ends
; the level or moves the player to the transporter's target coordinates. It
; lives outside background_tasks.s only because David Murray kept it in the
; machine-specific file, and it jumps back into the AI loop the same way every
; other unit routine does.
;
; The palette writes are the only part that had to change. On the X16 they poke
; VERA palette entry 40 -- index 4 of the sprites drawn with palette offset 1,
; i.e. the player and the cursor. On the RP6502 each mode 5 sprite carries its
; own palette pointer, so this cycles entry 4 of the player's palette alone and
; cannot bleed into the character plane.
;
; ONE DELIBERATE FIX. The original computes the cycled colour, writes it to
; VERA, and then does:
;
;       LDX     #40             ; ...as the VERA_L register value
;       STX     VERA_L
;       ...
;       LDA     #1
;       STA     UNIT_TIMER_A,X  ; X is still 40 here
;
; so it sets slot 40's timer rather than its own. Slot 40 is in the 32-47 band,
; which holds doors and elevators, so on the X16 a transporter in use nudges
; whatever door happens to occupy that slot. The surrounding code says plainly
; what was meant -- run this unit again next frame -- and the 40 is an artifact
; of the VERA register writes this port does not have. Ported as intended, with
; LDX UNIT.

        .include "petscii.inc"
        .import _plat_demat_palette
        .segment "CODE"

DEMATERIALIZE:
        inc     UNIT_TIMER_B
        lda     UNIT_TIMER_B
        cmp     #48
        bne     DEMA
        lda     #0
        sta     UNIT_TIMER_B
        jmp     DEMA1

DEMA:   ; Cycle the player palette's entry 4. The original builds the byte as
        ; ((A & 15) << 4) + UNIT_TIMER_B and writes it to both palette bytes.
        and     #15
        rol     a
        rol     a
        rol     a
        rol     a
        clc
        adc     UNIT_TIMER_B
        jsr     _plat_demat_palette
        ldx     UNIT                    ; see the note above; the original had 40
        lda     #1
        sta     UNIT_TIMER_A,x
        jmp     AILP

DEMA1:  ; Transport complete: restore the palette entry, then move the player.
        lda     #0
        jsr     _plat_demat_palette
        ldx     UNIT
        lda     UNIT_B,x
        cmp     #1                      ; 1 = send to coordinates
        beq     DEMA2
        lda     #2                      ; anything else ends the level
        sta     UNIT_TYPE
        lda     #7                      ; back to a normal transporter pad
        sta     UNIT_TYPE,x
        jmp     AILP
DEMA2:  lda     #97
        sta     UNIT_TILE
        lda     UNIT_C,x                ; target X
        sta     UNIT_LOC_X
        lda     UNIT_D,x                ; target Y
        sta     UNIT_LOC_Y
        lda     #7
        sta     UNIT_TYPE,x
        jsr     CACULATE_AND_REDRAW
        jmp     AILP
