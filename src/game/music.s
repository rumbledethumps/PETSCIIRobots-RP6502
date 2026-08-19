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
; and does nothing most of the time. A tempo command is the one row that does
; not reload the timer, so it costs a single tick and the next row follows.
;
; The arpeggio modes in bits 6-7 are dead weight, faithfully carried: the PET
; source has CYCLE_ARP commented out with the note "no music or sound effects
; were composed to use this feature", and no byte of the 1240 shipped here sets
; either bit. ARP_MODE and CHORD_ROOT are written and never read.
;
; TWO CHANNELS. The PET had one voice, so a sound effect had to take the
; music's: PLAY_SOUND parked the song's position and STOP_SONG put it back.
; That is a property of the machine, not of the game, and it costs the effect
; up to a full row of latency before it is heard -- it can only start when the
; music's row does. The RIA has eight oscillators, so the two run side by side
; on their own timers and an effect sounds on the next tick. Assembling with
; PETSCII_AUTHENTIC_AUDIO defined puts the PET's behaviour back for A/B against
; real hardware: both channels sound on oscillator 0 and the music holds still
; while an effect plays.
;
; The only other change is the output. The PET writes its VIA shift register at
; $E848 and $E84A; here the note number goes to the platform layer, which owns
; the PSG registers in XRAM.

        .include "petscii.inc"
        .import _plat_psg_note, _plat_psg_effect
        .segment "CODE"

CH_MUSIC  = 0
CH_EFFECT = 1

; A = sound effect number. Lower numbers win, so an explosion is not cut off by
; a menu beep.
PLAY_SOUND:
        ldy     SOUND_EFFECT
        cpy     #$FF
        beq     PSND2                   ; nothing playing
        cmp     SOUND_EFFECT
        beq     PSND2                   ; the same effect again: restart it
        bcs     PSND1                   ; a higher number: leave this one alone
PSND2:  tay
        ; MUSIC_ROUTINE reads this from the interrupt, and the pattern pointer
        ; is two bytes, so a tick landing between the halves would follow a
        ; pointer that is half one pattern and half another. The PET had the
        ; same shape and the same hazard; it costs two instructions to not have
        ; it.
        sei
        lda     SOUND_LIBRARY_L,y
        sta     CH_PATTERN_L+CH_EFFECT
        lda     SOUND_LIBRARY_H,y
        sta     CH_PATTERN_H+CH_EFFECT
        sty     SOUND_EFFECT
        lda     #0
        sta     CH_DATA_LINE+CH_EFFECT
        ; Zero rather than TEMPO: the first row plays on the next tick instead
        ; of waiting out a row belonging to whatever was playing before.
        sta     CH_TEMPO_TIMER+CH_EFFECT
        cli
PSND1:  rts

; Start a song: point the engine at a pattern and rewind it.
; A = pattern low byte, X = high byte.
START_MUSIC:
        sei                             ; as PLAY_SOUND, and for the same reason
        sta     CH_PATTERN_L+CH_MUSIC
        stx     CH_PATTERN_H+CH_MUSIC
        lda     #0
        sta     CH_DATA_LINE+CH_MUSIC
        sta     CH_TEMPO_TIMER+CH_MUSIC
        lda     #1
        sta     MUSIC_ON
        cli
        rts

STOP_MUSIC:
        lda     #0
        sta     MUSIC_ON
        ldx     #CH_MUSIC
        stx     CHANNEL
        lda     #0
        jmp     PSG_NOTE                ; release the voice

; One tick of the engine. Called from the VSYNC interrupt, sixty times a
; second, exactly where the PET calls it.
MUSIC_ROUTINE:
        ldx     #CH_EFFECT
        jsr     TICK_CHANNEL
        ldx     #CH_MUSIC
        ; fall through

; One tick of one channel. X = channel.
TICK_CHANNEL:
        stx     CHANNEL                 ; X survives to PS10; only A is used here
        cpx     #CH_MUSIC
        beq     PS02
        lda     SOUND_EFFECT            ; an effect plays whether or not music is on
        cmp     #$FF
        beq     PS08
        jmp     PS10
PS02:   lda     MUSIC_ON                ; is there a song?
        beq     PS08
.ifdef PETSCII_AUTHENTIC_AUDIO
        ; One voice, so the music holds where it is until the effect is done.
        ; The PET reached the same place by parking the song's position in
        ; PATTERN_*_TEMP and restoring it afterwards; simply not ticking is the
        ; same thing, and does not need the four variables.
        lda     SOUND_EFFECT
        cmp     #$FF
        bne     PS08
.endif
        jmp     PS10
PS08:   rts

PS10:   lda     CH_TEMPO_TIMER,x
        beq     PS15
        dec     CH_TEMPO_TIMER,x
        rts
PS15:   lda     CH_PATTERN_L,x          ; read this row through zero page
        sta     CUR_PATTERN
        lda     CH_PATTERN_H,x
        sta     CUR_PATTERN+1
        ldy     CH_DATA_LINE,x
        lda     (CUR_PATTERN),y
        beq     NEXT_ROW                ; a blank row holds the note
PS20:   cmp     #37                     ; end of pattern
        beq     END_PATTERN
        cmp     #38                     ; note off
        bne     PS22
        lda     #0
        sta     ARP_MODE
        jsr     PSG_NOTE                ; A = 0 releases the voice
        ldx     CHANNEL                 ; the call clobbered it
        jmp     NEXT_ROW                ; and costs a row, as a note does
PS22:   cmp     #39                     ; a tempo command, 39-48?
        bcc     PS23
        cmp     #49
        bcs     PS23
        sec
        sbc     #38
        sta     CH_TEMPO,x
        ; The one row that does not reload the timer: TEMPO_TIMER is already
        ; zero, so the next tick takes the row after this one.
        inc     CH_DATA_LINE,x
        rts
PS23:   ; Play a note. The top two bits are the arpeggio mode, the low six the
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
        jsr     PSG_NOTE
        ldx     CHANNEL
NEXT_ROW:
        lda     CH_TEMPO,x
        sta     CH_TEMPO_TIMER,x
        inc     CH_DATA_LINE,x
        rts

; Byte 37. The songs are 256 bytes and DATA_LINE wraps, so a song only reaches
; this if it was written to stop -- the win and lose jingles are.
END_PATTERN:
STOP_SONG:
        lda     #0
        jsr     PSG_NOTE                ; release the voice
        ldx     CHANNEL
        cpx     #CH_MUSIC
        bne     STSN1
        lda     #0
        sta     MUSIC_ON
        rts
STSN1:  lda     #$FF
        sta     SOUND_EFFECT
        rts

; A = note, played on the channel in CHANNEL. Note 0 releases the voice.
; Nothing here touches A.
PSG_NOTE:
.ifdef PETSCII_AUTHENTIC_AUDIO
        jmp     _plat_psg_note          ; one voice for both channels
.else
        ldx     CHANNEL
        beq     PSGN1
        jmp     _plat_psg_effect
PSGN1:  jmp     _plat_psg_note
.endif

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
