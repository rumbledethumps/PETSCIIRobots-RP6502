; The music and sound engine, from reference/pet/PETROBOTS12.ASM 4583-4790.
;
; This is the engine the X16 port threw away. When ZSOUND arrived it replaced
; MUSIC_ROUTINE and SET_WAVEFORM with bare RTS stubs and moved to recorded
; samples, leaving only the dead state variables behind. The RP6502 has a PSG
; rather than a sample player, so the original engine is the right fit and it
; comes back from the PET sources.
;
; A pattern is one byte per row:
;
;     0        rest -- hold whatever is sounding for one row
;     1-36     play this note; the top two bits pick an arpeggio mode
;     37       end of pattern
;     38       note off
;     39-48    set the tempo to (byte - 38) rows
;
; TEMPO_TIMER counts ticks between rows, so the engine is called once per tick
; and does nothing most of the time.
;
; A sound effect interrupts the music: PLAY_SOUND saves the song's position and
; STOP_SONG puts it back, which is why both share one voice. That is the PET's
; constraint, not this machine's -- the RIA has eight oscillators -- but keeping
; it means the timing and the priority rules are the original's.
;
; The only change is the output. The PET writes its VIA shift register at $E848
; and $E84A; here the note number goes to the platform layer, which owns the
; PSG registers in XRAM.

        .include "petscii.inc"
        .import _plat_psg_note
        .segment "CODE"

; A = sound effect number. Lower numbers win, so an explosion is not cut off by
; a menu beep.
PLAY_SOUND:
        ldy     MUSIC_ON
        cpy     #0
        beq     PSND1
        ldy     SOUND_EFFECT
        cpy     #$FF                    ; nothing playing: remember the song
        bne     PSND1
        ldy     CUR_PATTERN
        sty     PATTERN_L_TEMP
        ldy     CUR_PATTERN+1
        sty     PATTERN_H_TEMP
        ldy     DATA_LINE
        sty     DATA_LINE_TEMP
        ldy     TEMPO
        sty     TEMPO_TEMP
PSND1:  ldy     SOUND_EFFECT
        cpy     #$FF
        beq     PSND2
        cmp     SOUND_EFFECT
        bcc     PSND2                   ; prioritise the lower number
        beq     PSND2
        rts
PSND2:  tay
        ; CUR_PATTERN is two bytes and MUSIC_ROUTINE reads it from the interrupt,
        ; so a tick landing between the halves would follow a pointer that is
        ; half one pattern and half another. The PET had the same shape and the
        ; same hazard; it costs two instructions to not have it.
        sei
        lda     SOUND_LIBRARY_L,y
        sta     CUR_PATTERN
        lda     SOUND_LIBRARY_H,y
        sta     CUR_PATTERN+1
        sty     SOUND_EFFECT
        lda     #0
        sta     DATA_LINE
        cli
        rts

; One tick of the engine. Called from the VSYNC interrupt, sixty times a second,
; exactly where the PET calls it.
MUSIC_ROUTINE:
        lda     SOUND_EFFECT
        cmp     #$FF
        bne     PS10
        lda     MUSIC_ON
        cmp     #1
        beq     PS10
        rts
PS10:   lda     TEMPO_TIMER
        cmp     #0
        beq     PS15
        dec     TEMPO_TIMER
        rts
PS15:   ldy     DATA_LINE
        lda     (CUR_PATTERN),y
        cmp     #0                      ; a blank row holds the note
        bne     PS20
        lda     TEMPO
        sta     TEMPO_TIMER
        inc     DATA_LINE
        rts
PS20:   cmp     #37                     ; end of pattern
        bne     PS21
        jmp     STOP_SONG
PS21:   cmp     #38                     ; note off
        bne     PS22
        lda     #0
        sta     ARP_MODE
        jsr     _plat_psg_note          ; note 0 silences the voice
PS22:   cmp     #38                     ; a tempo command?
        bcc     PS23
        cmp     #49
        bcs     PS23
        sec
        sbc     #38
        sta     TEMPO
        inc     DATA_LINE
        rts
PS23:   ; play a note. The top two bits are the arpeggio mode, the low six the
        ; note number.
        tay
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     ARP_MODE
        tya
        and     #%00111111
        sta     CHORD_ROOT
        jsr     _plat_psg_note
        lda     TEMPO
        sta     TEMPO_TIMER
        inc     DATA_LINE
        rts

; End of a sound effect: silence, then put the song back where it was.
STOP_SONG:
        lda     #0
        jsr     _plat_psg_note
        lda     #$FF
        sta     SOUND_EFFECT
        lda     TEMPO
        sta     TEMPO_TIMER
        lda     MUSIC_ON
        cmp     #1
        beq     STSN1
        rts
STSN1:  ldy     PATTERN_L_TEMP
        sty     CUR_PATTERN
        ldy     PATTERN_H_TEMP
        sty     CUR_PATTERN+1
        ldy     DATA_LINE_TEMP
        sty     DATA_LINE
        ldy     TEMPO_TEMP
        sty     TEMPO
        rts

; Start a song: point the engine at a pattern and rewind it.
; A = pattern low byte, X = high byte.
START_MUSIC:
        sei                             ; as PLAY_SOUND, and for the same reason
        sta     CUR_PATTERN
        stx     CUR_PATTERN+1
        lda     #0
        sta     DATA_LINE
        sta     TEMPO_TIMER
        lda     #1
        sta     MUSIC_ON
        cli
        rts

STOP_MUSIC:
        lda     #0
        sta     MUSIC_ON
        jsr     _plat_psg_note
        rts

; Sound effect number to pattern, in the X16's numbering because that is what
; every JSR PLAY_DIGI_SOUND site in the ported code names. Several of the X16's
; recorded effects are the same PET pattern -- it recorded two takes of the
; magnet -- so the table repeats.
SOUND_LIBRARY_L:
        .byte <SND_MENU_BEEP, <SND_SHORT_BEEP, <SND_CYCLE_ITEM, <SND_CYCLE_WEAPON
        .byte <SND_DOOR, <SND_EMP, <SND_ERROR, <SND_EXPLOSION
        .byte <SND_ITEM_FOUND, <SND_MAGNET, <SND_MAGNET, <SND_MEDKIT
        .byte <SND_MOVE_OBJ, <SND_PISTOL, <SND_PLASMA, <SND_SHOCK
SOUND_LIBRARY_H:
        .byte >SND_MENU_BEEP, >SND_SHORT_BEEP, >SND_CYCLE_ITEM, >SND_CYCLE_WEAPON
        .byte >SND_DOOR, >SND_EMP, >SND_ERROR, >SND_EXPLOSION
        .byte >SND_ITEM_FOUND, >SND_MAGNET, >SND_MAGNET, >SND_MEDKIT
        .byte >SND_MOVE_OBJ, >SND_PISTOL, >SND_PLASMA, >SND_SHOCK
