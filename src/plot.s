; The tile blitter.
;
; A game tile is a 3x3 block of character cells: eighteen bytes in tile_cells,
; glyph and colour interleaved, the colour already masked to $0F by the asset
; converter. Drawing one is three rows of six portal writes, stepping 80 bytes
; -- one character row -- between them.
;
; This was C, and it is assembly now because the profile said so. Measured by
; forcing a full window redraw every frame and counting main loop passes against
; video frames: with the C blitter the loop ran at 95 passes per 180 frames,
; with the blitter skipped entirely it ran at 180. So the redraw was costing
; about 15 ms of a 16.7 ms frame, all of it here -- roughly 1,560 cycles a tile
; against the X16's 219 for the same work. The window redraws on every step the
; player takes, so that was most of a frame lost per step.
;
; cc65 spends it on index arithmetic it cannot hold in registers: a 16-bit
; multiply per tile and a pointer increment per byte, reloaded around every
; store because RIA is volatile. Written out by hand the same work is about 280
; cycles, which is the figure docs/porting-notes.md predicted for this loop.
;
; The caller puts the destination in plot_addr and passes the tile number in A,
; which is where cc65's fastcall convention already has it. plot_addr is left
; alone, because DRAW_MAP_WINDOW plots the terrain and then the unit overlay at
; the same address.

        .include "rp6502.inc"

        .export _plot_tile, _plot_transparent_tile
        .exportzp _plot_addr
        .import _tile_cells

; The glyph that means "leave what is underneath" when a unit is drawn over the
; terrain. It is also a real colon in the base tiles, which is why there are two
; routines here rather than one with a flag -- exactly as the original has.
TILE_TRANSPARENT = $3A

; One character row of the 40-column plane, two bytes per cell.
STRIDE = 80

        .segment "ZEROPAGE"
_plot_addr: .res 2              ; XRAM address of the tile's top left cell
ptr:        .res 2              ; into tile_cells
cur:        .res 2              ; plot_addr as it walks down the three rows
rows:       .res 1

        .segment "CODE"

; A = tile number. Leaves ptr = tile_cells + tile * 18, cur = plot_addr, the
; portal stepping by one, and Y at zero.
setup:
        ldx     #0
        stx     ptr+1
        asl     a                       ; tile * 2, carry is bit 7
        rol     ptr+1
        sta     ptr
        ldx     ptr+1                   ; hold tile * 2 in A and X
        asl     ptr
        rol     ptr+1
        asl     ptr
        rol     ptr+1
        asl     ptr
        rol     ptr+1                   ; ptr = tile * 16
        clc
        adc     ptr                     ; + tile * 2 = tile * 18
        sta     ptr
        txa
        adc     ptr+1
        sta     ptr+1
        clc
        lda     ptr
        adc     #<_tile_cells
        sta     ptr
        lda     ptr+1
        adc     #>_tile_cells
        sta     ptr+1

        lda     _plot_addr
        sta     cur
        lda     _plot_addr+1
        sta     cur+1
        lda     #1
        sta     RIA_STEP0
        ldy     #0
        rts

; cur += STRIDE.
nextrow:
        clc
        lda     cur
        adc     #STRIDE
        sta     cur
        bcc     :+
        inc     cur+1
:       rts

; The terrain. Every cell is written, so the six bytes go out flat.
_plot_tile:
        jsr     setup
        lda     #3
        sta     rows
@row:   lda     cur
        sta     RIA_ADDR0
        lda     cur+1
        sta     RIA_ADDR0+1
.repeat 6
        lda     (ptr),y
        sta     RIA_RW0
        iny
.endrepeat
        jsr     nextrow
        dec     rows
        bne     @row
        rts

; The unit overlay. A cell whose glyph is the transparency marker leaves the
; terrain showing, and the way past it is two reads of the data register: each
; advances addr0 by step0 with a correct sixteen bit add, so a cell that spans a
; page boundary is skipped as safely as one that does not. BIT reads without
; touching A.
_plot_transparent_tile:
        jsr     setup
        lda     #3
        sta     rows
@row:   lda     cur
        sta     RIA_ADDR0
        lda     cur+1
        sta     RIA_ADDR0+1
        ldx     #3
@col:   lda     (ptr),y
        cmp     #TILE_TRANSPARENT
        beq     @skip
        sta     RIA_RW0                 ; glyph
        iny
        lda     (ptr),y
        sta     RIA_RW0                 ; colour
        iny
        bra     @next
@skip:  bit     RIA_RW0
        bit     RIA_RW0
        iny
        iny
@next:  dex
        bne     @col
        jsr     nextrow
        dec     rows
        bne     @row
        rts
