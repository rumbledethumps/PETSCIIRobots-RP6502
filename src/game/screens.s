; Screen layouts and intro artwork, from reference/x16/x16Robots.ASM 5327-5457.
;
; The four screens are run-length encoded 40x30 character grids: a literal byte
; is one cell, and byte 96 introduces a run of "next byte, count+1 times".
; Decoding costs about sixty bytes of code and saves four thousand -- SCR_TEXT
; is eighteen bytes encoded and twelve hundred expanded -- so they stay
; compressed and src/platform.c expands them on the way to the screen.
;
; CINEMA_MESSAGE is the marquee ANIMATE_WATER scrolls across the cinema tiles.
; THREE_FACES is the intro robot's expression, three 16x10 images at 2bpp, one
; per difficulty setting.

        .include "petscii.inc"
        .segment "RODATA"

INTRO_TEXT:
	.byte $60, $20, $53, $13, $14, $01, $12, $14, $20, $07, $01, $0D, $05, $60, $20
	.byte $1D, $13, $05, $0C, $05, $03, $14, $20, $0D, $01, $10, $60, $20, $1D, $04
	.byte $09, $06, $06, $09, $03, $15, $0C, $14, $19, $60, $20, $1D, $03, $0F, $0E
	.byte $14, $12, $0F, $0C, $13, $60, $20, $6E, $70, $60, $40, $02, $73, $0D, $01
	.byte $10, $6B, $60, $40, $02, $6E, $60, $20, $19, $0B, $09, $0C, $0C, $20, $01
	.byte $0C, $0C, $20, $08, $15, $0D, $01, $0E, $13, $60, $20, $FF, $60, $20, $FF
	.byte $60, $20, $71
	.byte $60,$20,$C8;extra 5 blank lines

SCR_TEXT:
	.byte $60,$20,$C8;extra 5 blank lines
	.byte $60,$20,$C8;extra 5 blank lines
	.byte $60,$20,$C8;extra 5 blank lines
	.byte $60,$20,$C8;extra 5 blank lines
	.byte $60,$20,$C8;extra 5 blank lines
	.byte $60,$20,$C8;extra 5 blank lines

SCR_ENDGAME:
	.byte $55, $60, $40, $03, $73, $01, $14, $14, $01, $03, $0B, $20, $0F, $06, $20
	.byte $14, $08, $05, $20, $10, $05, $14, $13, $03, $09, $09, $20, $12, $0F, $02
	.byte $0F, $14, $13, $6B, $60, $40, $03, $49, $5D, $60, $20, $25, $5D, $5D, $60
	.byte $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60
	.byte $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $0A, $13, $03, $05
	.byte $0E, $01, $12, $09, $0F, $3A, $60, $20, $11, $5D, $5D, $60, $20, $25, $5D
	.byte $5D, $60, $20, $06, $05, $0C, $01, $10, $13, $05, $04, $20, $14, $09, $0D
	.byte $05, $3A, $60, $20, $11, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $02
	.byte $12, $0F, $02, $0F, $14, $13, $20, $12, $05, $0D, $01, $09, $0E, $09, $0E
	.byte $07, $3A, $60, $20, $11, $5D, $5D, $60, $20, $25, $5D, $5D, $20, $20, $13
	.byte $05, $03, $12, $05, $14, $13, $20, $12, $05, $0D, $01, $09, $0E, $09, $0E
	.byte $07, $3A, $60, $20, $11, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $08
	.byte $04, $09, $06, $06, $09, $03, $15, $0C, $14, $19, $3A, $60, $20, $11, $5D
	.byte $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D
	.byte $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D
	.byte $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D
	.byte $5D, $60, $20, $25, $5D;ROW 24
	.byte $5D, $60, $20, $25, $5D;ROW 25
	.byte $5D, $60, $20, $25, $5D;ROW 26
	.byte $5D, $60, $20, $25, $5D;ROW 27
	.byte $5D, $60, $20, $25, $5D;ROW 28
	.byte $4A, $60, $40, $25, $4B;ROW 29

SCR_CUSTOM_KEYS:
	.byte $55, $60, $40, $03, $73, $01, $14, $14, $01, $03, $0B, $20, $0F, $06, $20
	.byte $14, $08, $05, $20, $10, $05, $14, $13, $03, $09, $09, $20, $12, $0F, $02
	.byte $0F, $14, $13, $6B, $60, $40, $03, $49, $5D, $60, $20, $25, $5D, $5D, $60
	.byte $20, $25, $5D, $5D, $60, $20, $03, $10, $12, $05, $13, $13, $20, $14, $08
	.byte $05, $20, $0B, $05, $19, $13, $20, $19, $0F, $15, $20, $17, $09, $13, $08
	.byte $20, $14, $0F, $20, $15, $13, $05, $60, $20, $03, $5D, $5D, $60, $20, $04
	.byte $06, $0F, $12, $20, $14, $08, $05, $20, $06, $0F, $0C, $0C, $0F, $17, $09
	.byte $0E, $07, $20, $06, $15, $0E, $03, $14, $09, $0F, $0E, $13, $60, $20, $05
	.byte $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $25
	.byte $5D, $5D, $60, $20, $06, $0D, $0F, $16, $05, $20, $15, $10, $3A, $60, $20
	.byte $16, $5D, $5D, $60, $20, $04, $0D, $0F, $16, $05, $20, $04, $0F, $17, $0E
	.byte $3A, $60, $20, $16, $5D, $5D, $60, $20, $04, $0D, $0F, $16, $05, $20, $0C
	.byte $05, $06, $14, $3A, $60, $20, $16, $5D, $5D, $60, $20, $03, $0D, $0F, $16
	.byte $05, $20, $12, $09, $07, $08, $14, $3A, $60, $20, $16, $5D, $5D, $60, $20
	.byte $06, $06, $09, $12, $05, $20, $15, $10, $3A, $60, $20, $16, $5D, $5D, $60
	.byte $20, $04, $06, $09, $12, $05, $20, $04, $0F, $17, $0E, $3A, $60, $20, $16
	.byte $5D, $5D, $60, $20, $04, $06, $09, $12, $05, $20, $0C, $05, $06, $14, $3A
	.byte $60, $20, $16, $5D, $5D, $60, $20, $03, $06, $09, $12, $05, $20, $12, $09
	.byte $07, $08, $14, $3A, $60, $20, $16, $5D, $5D, $20, $03, $19, $03, $0C, $05
	.byte $20, $17, $05, $01, $10, $0F, $0E, $13, $3A, $60, $20, $16, $5D, $5D, $60
	.byte $20, $02, $03, $19, $03, $0C, $05, $20, $09, $14, $05, $0D, $13, $3A, $60
	.byte $20, $16, $5D, $5D, $60, $20, $05, $15, $13, $05, $20, $09, $14, $05, $0D
	.byte $3A, $60, $20, $16, $5D, $5D, $20, $13, $05, $01, $12, $03, $08, $20, $0F
	.byte $02, $0A, $05, $03, $14, $3A, $60, $20, $16, $5D, $5D, $60, $20, $02, $0D
	.byte $0F, $16, $05, $20, $0F, $02, $0A, $05, $03, $14, $3A, $60, $20, $16, $5D
	.byte $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D, $5D, $60, $20, $25, $5D
	.byte $5D, $60, $20, $25, $5D;ROW 24
	.byte $5D, $60, $20, $25, $5D;ROW 25
	.byte $5D, $60, $20, $25, $5D;ROW 26
	.byte $5D, $60, $20, $25, $5D;ROW 27
	.byte $5D, $60, $20, $25, $5D;ROW 28
	.byte $4A, $60, $40, $25, $4B;ROW 29

CINEMA_MESSAGE:
	.byte 3,15,13,9,14,7,32,19,15,15,14,58,32,19,16,1,3,5,32,2,1,12,12,19,32,50,32,45,32,20,8,5,32,19,5,1,18,3,8,32,6,15,18,32,13,15,18,5,32,13,15,14,5,25,44,32
	.byte 1,20,20,1,3,11,32,15,6,32,20,8,5,32,16,1,16,5,18,3,12,9,16,19,58,32,3,12,9,16,16,25,39,19,32,18,5,22,5,14,7,5,44,32
	.byte 9,20,32,3,1,13,5,32,6,18,15,13,32,16,12,1,14,5,20,32,5,1,18,20,8,44,32
	.byte 18,15,3,11,25,32,53,48,48,48,44,32,1,12,12,32,13,25,32,3,9,18,3,21,9,20,19,32,20,8,5,32,13,15,22,9,5,44,32
	.byte 3,15,14,1,14,32,20,8,5,32,12,9,2,18,1,18,9,1,14,44,32,1,14,4,32,13,15,18,5,33,32

;This data represents the 3 different faces of the intro robot.
;80 bytes for each face.  the face size is 16x10 pixels (in 2bpp format)

THREE_FACES:
	.byte $6C, $CC, $C6, $66, $66, $6C, $CC, $C6, $C0, $00, $0C, $66, $66, $C0, $00
	.byte $0C, $06, $66, $60, $66, $66, $06, $66, $60, $33, $3C, $C6, $66, $66, $63
	.byte $33, $3C, $00, $00, $0C, $66, $66, $30, $00, $00, $0D, $DD, $00, $C6, $63
	.byte $00, $DD, $C0, $55, $00, $D0, $C6, $63, $0C, $50, $0C, $C5, $00, $D0, $66
	.byte $63, $05, $50, $0C, $C5, $55, $C0, $66, $63, $05, $55, $5C, $0A, $CC, $00
	.byte $66, $63, $00, $C8, $C0, $06, $CC, $60, $00, $00, $06, $CC, $60, $00, $00
	.byte $CC, $C6, $0C, $CC, $00, $00, $66, $66, $00, $00, $00, $00, $66, $66, $33
	.byte $3C, $C6, $66, $66, $63, $33, $3C, $00, $00, $0C, $66, $66, $30, $00, $00
	.byte $07, $77, $00, $C6, $63, $00, $77, $80, $A2, $22, $70, $C6, $63, $08, $22
	.byte $28, $22, $22, $70, $66, $63, $02, $22, $2A, $A2, $22, $80, $66, $63, $02
	.byte $22, $2A, $0A, $88, $00, $66, $63, $00, $28, $A0, $C0, $66, $66, $66, $66
	.byte $66, $66, $0C, $0C, $06, $66, $66, $66, $66, $60, $C0, $66, $C0, $66, $66
	.byte $66, $66, $0C, $06, $33, $3C, $06, $66, $66, $60, $C3, $3C, $00, $00, $C0
	.byte $66, $66, $0C, $00, $00, $07, $70, $0C, $06, $60, $C0, $07, $80, $A2, $27
	.byte $00, $C6, $63, $00, $72, $28, $22, $22, $70, $66, $63, $02, $22, $2A, $A2
	.byte $22, $80, $66, $63, $02, $22, $2A, $0A, $88, $00, $66, $63, $00, $28, $A0



;; Symbols from legacy sound engine pending deletion
;NOTE_FREQ_L:
;NOTE_FREQ_H:
;SND_EXPLOSION:
;SND_MEDKIT:
;SND_EMP:
;SND_MAGNET:
;SND_SHOCK:
;SND_MOVE_OBJ:
;SND_PLASMA:
;SND_PISTOL:
;SND_ITEM_FOUND:
;SND_ERROR:
;SND_CYCLE_WEAPON:
;SND_CYCLE_ITEM:
;SND_DOOR:
;SND_MENU_BEEP:
;SND_SHORT_BEEP:
;INTRO_MUSIC:
;WIN_MUSIC:
;LOSE_MUSIC:
;IN_GAME_MUSIC1:
;IN_GAME_MUSIC2:
;IN_GAME_MUSIC3:


; Each tile as a single colour for the live map, from
; reference/x16/x16Robots.ASM 5203-5267. Both nibbles are the same value,
; because one map tile is drawn as two pixels of a 4bpp bitmap.
MAP_TRANSLATION_TABLE:
	.byte $00,$00,$11,$11
	.byte $11,$11,$11,$11
	.byte $11,$66,$11,$11
	.byte $11,$11,$00,$11
	.byte $11,$11,$11,$11
	.byte $11,$11,$11,$11
	.byte $55,$11,$11,$11
	.byte $11,$EE,$88,$88
	.byte $EE,$77,$22,$EE
	.byte $EE,$77,$66,$EE
	.byte $FF,$77,$88,$FF
	.byte $FF,$77,$88,$FF
	.byte $11,$11,$11,$FF
	.byte $11,$55,$55,$FF
	.byte $11,$55,$FF,$FF
	.byte $66,$66,$11,$11
	.byte $11,$11,$FF,$77
	.byte $11,$11,$11,$11
	.byte $FF,$FF,$FF,$FF
	.byte $11,$11,$11,$11
	.byte $11,$FF,$11,$FF
	.byte $11,$FF,$11,$FF
	.byte $11,$FF,$11,$11
	.byte $11,$11,$11,$FF
	.byte $88,$88,$00,$00
	.byte $00,$00,$00,$00
	.byte $11,$11,$11,$FF
	.byte $FF,$FF,$66,$22
	.byte $11,$11,$FF,$00
	.byte $FF,$66,$FF,$11
	.byte $11,$11,$88,$88
	.byte $88,$88,$88,$88
	.byte $11,$11,$00,$55
	.byte $11,$FF,$00,$55
	.byte $FF,$FF,$FF,$66
	.byte $00,$00,$00,$FF
	.byte $11,$11,$11,$11
	.byte $88,$66,$88,$88
	.byte $11,$11,$FF,$FF
	.byte $88,$88,$FF,$66
	.byte $88,$88,$88,$66
	.byte $00,$00,$11,$FF
	.byte $66,$BB,$11,$FF
	.byte $11,$11,$11,$00
	.byte $11,$11,$11,$11
	.byte $11,$11,$11,$88
	.byte $11,$11,$11,$11
	.byte $11,$BB,$11,$11
	.byte $11,$11,$11,$11
	.byte $FF,$FF,$55,$DD
	.byte $FF,$FF,$FF,$FF
	.byte $66,$88,$99,$99
	.byte $55,$55,$55,$DD
	.byte $FF,$FF,$BB,$DD
	.byte $11,$11,$11,$DD
	.byte $11,$11,$11,$DD
	.byte $11,$11,$11,$77
	.byte $BB,$BB,$77,$77
	.byte $BB,$BB,$77,$77
	.byte $11,$11,$00,$00
	.byte $00,$00,$CC,$00
	.byte $00,$00,$00,$00
	.byte $00,$00,$00,$00
	.byte $00,$00,$00,$00


; The game over box and the two endings, from reference/x16/x16Robots.ASM
; 3926-3928 and 3977-3978. The box is PETSCII line drawing -- corners and
; horizontal rules around the words -- eleven characters to a row.
GAMEOVER1:	.byte $70,$40,$40,$40,$40,$40,$40,$40,$40,$40,$6e
GAMEOVER2:	.byte $5d,$07,$01,$0d,$05,$20,$0f,$16,$05,$12,$5d
GAMEOVER3:	.byte $6d,$40,$40,$40,$40,$40,$40,$40,$40,$40,$7d
WIN_MSG:	.byte 25,15,21,32,23,9,14,33
LOS_MSG:	.byte 25,15,21,32,12,15,19,5,33

