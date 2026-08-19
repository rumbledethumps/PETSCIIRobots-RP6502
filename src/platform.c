/* The presentation side of the ported game logic.
 *
 * src/game/platform_bridge.s forwards the AI's calls here. Each of these stands
 * in for a routine that lived in the X16's machine-specific file and talked to
 * VERA or ZSOUND directly.
 */
#include <rp6502.h>
#include <stdbool.h>

#include "game/game.h"
#include "platform.h"
#include "xram.h"

/* TILESET.GFX, reorganised by tools/convert/conv_tiles.py: each game tile is
 * three character rows of six bytes -- glyph, colour, glyph, colour, glyph,
 * colour -- so a row is one address store and six portal writes. Colours are
 * pre-masked to $0F, which is what makes the playfield transparent over the
 * bitmap plane and what deletes nine AND #$0F from the X16's inner loop. */
unsigned char tile_cells[256 * 3 * 6];

/* Character code $3A is the transparency marker when a unit tile is laid over
 * the terrain. It is also a real glyph -- a colon -- in the base tiles, which
 * is why the original keeps two plot routines rather than one with a flag. */
#define TILE_TRANSPARENT 0x3A

static void plot_tile(unsigned addr, unsigned char tile)
{
    const unsigned char *p = &tile_cells[(unsigned)tile * 18];
    unsigned char row;
    RIA.step0 = 1;
    for (row = 0; row < 3; row++) {
        RIA.addr0 = addr;
        RIA.rw0 = *p++; RIA.rw0 = *p++;
        RIA.rw0 = *p++; RIA.rw0 = *p++;
        RIA.rw0 = *p++; RIA.rw0 = *p++;
        addr += SCR_STRIDE;
    }
}

/* The unit overlay. Reading RIA.rw0 advances addr0 by step0 with a correct
 * 16-bit add, so skipping a cell costs two reads and cannot walk off a page
 * the way the X16's double INC of the address low byte would at this stride. */
static void plot_transparent_tile(unsigned addr, unsigned char tile)
{
    const unsigned char *p = &tile_cells[(unsigned)tile * 18];
    unsigned char row, col;
    RIA.step0 = 1;
    for (row = 0; row < 3; row++) {
        RIA.addr0 = addr;
        for (col = 0; col < 3; col++) {
            if (*p == TILE_TRANSPARENT) {
                p += 2;
                (void)RIA.rw0;
                (void)RIA.rw0;
            } else {
                RIA.rw0 = *p++;
                RIA.rw0 = *p++;
            }
        }
        addr += SCR_STRIDE;
    }
}

void plat_draw_map_window(void)
{
    unsigned char tx, ty, n = 0;
    unsigned addr;

    MAP_PRE_CALCULATE();
    REDRAW_WINDOW = 0;

    for (ty = 0; ty < MAP_WIN_TILES_H; ty++) {
        for (tx = 0; tx < MAP_WIN_TILES_W; tx++, n++) {
            addr = CELL(MAP_WIN_COL + tx * 3, MAP_WIN_ROW + ty * 3);
            plot_tile(addr, MAP[((unsigned)(MAP_WINDOW_Y + ty) << 7)
                                + MAP_WINDOW_X + tx]);
            /* A unit standing here draws over the terrain. */
            if (MAP_PRECALC[n])
                plot_transparent_tile(addr, MAP_PRECALC[n]);
        }
    }
}

/* Stubs for the milestones that have not landed yet. Each one is a routine the
 * AI legitimately calls, so they have to exist and return cleanly; what they do
 * not do is listed in docs/porting-notes.md. */

void __fastcall__ plat_play_sound(unsigned char effect)
{
    (void)effect;               /* M7: the PET music engine on the RIA PSG */
}

void plat_print_info(void)
{
    /* M5: the three-line message console at character rows 27-29. SOURCE
     * points at a screen-code string, 0 to end and 255 to force a newline. */
}

void plat_display_item(void) { }             /* M5: the HUD item icon    */
void plat_display_player_health(void) { }    /* M5: the six-cell bar     */
void plat_elevator_select(void) { }          /* M6: the elevator UI      */

/* Cycles entry 4 of the player's own palette, which is what the X16 achieved by
 * poking VERA palette entry 40 -- index 4 of the sprites drawn with palette
 * offset 1. Here it cannot reach the character plane. */
void __fastcall__ plat_demat_palette(unsigned char value)
{
    RIA.addr0 = XR_PAL_PLAYER + 4 * 2;
    RIA.step0 = 1;
    RIA.rw0 = value;
    RIA.rw0 = value;
}
