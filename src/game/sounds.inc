        .ifndef SOUNDS_INC
SOUNDS_INC = 1

; Sound effect and song indices, from reference/x16/sounds.inc.
;
; The X16 played these as digitised samples through ZSOUND; the RP6502 plays the
; original PET/C64 engine's patterns on the RIA PSG instead. The indices are
; unchanged, so every JSR PLAY_DIGI_SOUND site in the AI still names the right
; effect.
SFX_BEEP2 = 0
SFX_BEEP = 1
SFX_CYCLEITEM = 2
SFX_CYCLEWEAPON = 3
SFX_DOOR = 4
SFX_EMP = 5
SFX_ERROR = 6
SFX_EXPLOSION = 7
SFX_FOUNDITEM = 8
SFX_MAGNET2 = 9
SFX_MAGNET = 10
SFX_MEDKIT = 11
SFX_MOVE = 12
SFX_PISTOL = 13
SFX_PLASMA = 14
SFX_SHOCK = 15

SONG_WIN = 0
SONG_LOSE = 1
SONG_MAIN = 2
SONG_LEVELA = 3
SONG_LEVELB = 4
SONG_LEVELC = 5
SONG_LEVELD = 6
SONG_LEVELE = 7
SONG_LEVELF = 8

SONG_DYNAMIC = 2

        .endif
