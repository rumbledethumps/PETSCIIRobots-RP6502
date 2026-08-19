; The presentation entry points the ported AI calls.
;
; BACKGROUND_TASKS.ASM ends every so often in a JSR to something that draws,
; prints or makes a noise. On the X16 those were VERA and ZSOUND routines in the
; machine-specific file; here they are C, and this is the only place that knows
; it. Keeping the forwarding in one file means every file under src/game/ stays
; a straight reading of the original.
;
; These are tail calls: the C function's rts returns to the AI routine's caller.
; cc65 functions clobber A, X, Y and the C runtime's zero page scratch, which is
; exactly what the X16 versions did, so no caller is relying on anything a C
; function would take away.

        .include "petscii.inc"
        .segment "CODE"

        .import _plat_draw_map_window, _plat_play_sound, _plat_print_info
        .import _plat_display_item, _plat_display_player_health
        .import _plat_elevator_select, _plat_display_weapon
        .import _plat_display_keys, _plat_display_player_sprite
        .import _plat_reverse_tile, _plat_gamepad_read
        .import _plat_getin, _plat_clear_key_buffer

; No arguments.
DRAW_MAP_WINDOW:        jmp _plat_draw_map_window
DISPLAY_ITEM:           jmp _plat_display_item
DISPLAY_PLAYER_HEALTH:  jmp _plat_display_player_health
ELEVATOR_SELECT:        jmp _plat_elevator_select

; A holds the effect number, which cc65's fastcall convention also passes in A.
PLAY_DIGI_SOUND:        jmp _plat_play_sound

DISPLAY_WEAPON:         jmp _plat_display_weapon
DISPLAY_KEYS:           jmp _plat_display_keys
DISPLAY_PLAYER_SPRITE:  jmp _plat_display_player_sprite
REVERSE_TILE:           jmp _plat_reverse_tile
SNES_CONTROLER_READ:    jmp _plat_gamepad_read
CLEAR_KEY_BUFFER:       jmp _plat_clear_key_buffer

; SOURCE points at the message, as it did on the X16 through $02/$03.
PRINT_INFO:             jmp _plat_print_info
