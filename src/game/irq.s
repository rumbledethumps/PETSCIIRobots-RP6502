; The game tick.
;
; Sixty times a second, tied to the display: timers count down, the game clock
; advances, and BACKGROUND_TASKS gets its cue to run one pass of unit AI. The
; X16 does this from VERA's VBLANK interrupt; the RIA raises the same signal, so
; the structure ports directly.
;
; It has to be an interrupt rather than something the main loop does, because
; the game waits by spinning: SEARCH_OBJECT sits on BGTIMER2 calling
; BACKGROUND_TASKS until it reaches zero, and nothing inside that loop would
; ever decrement it.
;
; $FFF0 is an enable mask on write and a triggered-signal register on read, and
; reading clears what it reports -- so testing and acknowledging are the same
; instruction.
;
; Video register updates happen here too, and only here. This is the start of
; vblank, so a plane's scroll position or a palette entry changed now lands
; between frames instead of part way down one. The main loop decides what the
; registers should say and leaves it in RAM; this pushes it out.
;
; That means touching XRAM from an interrupt, and the two portals -- their
; addresses and step values -- are global state with no save area, so an
; interrupt that repointed one in the middle of PLOT_TILE would scribble the
; wrong cells. The handler saves and restores portal 1 around its writes. Six
; bytes at sixty hertz is nothing, and it keeps the main loop free to use both
; portals without knowing this exists.
;
; This handler reaches C -- MUSIC_ROUTINE calls the PSG driver -- and cc65
; compiles every C function against twenty bytes of shared zero-page scratch:
; the C stack pointer, sreg, regsave, ptr1-4 and tmp1-4. An interrupt that calls
; C without putting those back hands the interrupted function its own pointers
; rewritten, and PLOT_TILE is C, so a tile would occasionally draw its three
; rows into the bitmap plane instead of the character plane and sit there.
;
; cc65 already solves this. set_irq() installs a C level handler and the
; runtime's clevel_irq wrapper saves and restores that zero page, switches to a
; separate C stack and preserves jmpvec around the call. So this is registered
; through set_irq rather than as a bare .interruptor, and returns A = 1 for
; IRQ_HANDLED and 0 for IRQ_NOT_HANDLED, which is what a cc65 irq_handler is.

        .include "petscii.inc"
        .include "rp6502.inc"

; The character plane config, from src/xram.h. Kept in step by hand.
XR_CFG_CHARS = $E0A0

        .export _petscii_irq
        .import _plat_animate_water

        .segment "CODE"

_petscii_irq:
        lda     RIA_IRQ                 ; reading returns the triggered bits
        bmi     @vsync                  ; ...and clears them. bit 7 is VSYNC.
        lda     #0                      ; IRQ_NOT_HANDLED
        rts

@vsync:
        inc     IRQ_FRAME               ; the main loop's tick reference

        lda     #1
        sta     BGTIMER1                ; BACKGROUND_TASKS runs once per tick

        lda     BGTIMER2                ; counts down to zero and stays there
        beq     @keytimer
        dec     BGTIMER2
@keytimer:
        lda     KEYTIMER
        beq     @clock
        dec     KEYTIMER

@clock:
        jsr     UPDATE_GAME_CLOCK
        ; The tileset animations, where RUNIRQ calls them. They only permute
        ; bytes of tile_cells in RAM and set REDRAW_WINDOW, so they need no
        ; portal and sit outside VBLANK_VIDEO's save.
        jsr     _plat_animate_water
        jsr     VBLANK_VIDEO
        lda     #1                      ; IRQ_HANDLED
        rts

; Push the staged video register values out, inside vblank.
;
; Portal 1 is borrowed and handed back exactly as it was found, so nothing in
; the main loop has to know an interrupt happened.
VBLANK_VIDEO:
        lda     RIA_ADDR1
        pha
        lda     RIA_ADDR1+1
        pha
        lda     RIA_STEP1
        pha

        ; Screen shake. The X16 writes VERA's layer 1 horizontal scroll from
        ; its VBLANK handler, alternating 0 and 4 every few frames; the same
        ; effect here is the character plane's x_pos_px, which is a signed
        ; pixel offset, so the sign is the other way round.
        lda     SCREEN_SHAKE
        beq     @steady
        inc     SSCOUNT
        lda     SSCOUNT
        cmp     #5
        bne     @phase
        lda     #0
        sta     SSCOUNT
@phase: lda     SSCOUNT
        and     #1
        beq     @steady
        lda     #<-4
        ldx     #>-4
        bra     @write
@steady:
        lda     #0
        ldx     #0
@write:
        ldy     #1
        sty     RIA_STEP1
        ldy     #<(XR_CFG_CHARS + 2)    ; x_pos_px, after the two wrap flags
        sty     RIA_ADDR1
        ldy     #>(XR_CFG_CHARS + 2)
        sty     RIA_ADDR1+1
        sta     RIA_RW1
        stx     RIA_RW1

        ; One tick of the sound engine, where the PET calls it. Its only output
        ; is through the platform layer's PSG writes, which use this portal, so
        ; it belongs inside the save.
        jsr     MUSIC_ROUTINE

        pla
        sta     RIA_STEP1
        pla
        sta     RIA_ADDR1+1
        pla
        sta     RIA_ADDR1
        rts

; The game clock, from reference/x16/x16Robots.ASM. Sixty ticks to the second.
UPDATE_GAME_CLOCK:
        lda     CLOCK_ACTIVE
        beq     @out
        inc     CYCLES
        lda     CYCLES
        cmp     #60
        bne     @out
        lda     #0
        sta     CYCLES
        inc     SECONDS
        lda     SECONDS
        cmp     #60
        bne     @out
        lda     #0
        sta     SECONDS
        inc     MINUTES
        lda     MINUTES
        cmp     #60
        bne     @out
        lda     #0
        sta     MINUTES
        inc     HOURS
@out:   rts
