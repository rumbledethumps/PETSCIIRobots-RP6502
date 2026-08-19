/* The presentation side of the ported game logic.
 *
 * src/game/platform_bridge.s forwards the AI's calls here. Each of these stands
 * in for a routine that lived in the X16's machine-specific file and talked to
 * VERA or ZSOUND directly.
 */
#include <rp6502.h>
#include <stdbool.h>

#include "game/game.h"
#include "input.h"
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

/* ---- the character plane -------------------------------------------- */

static void put_cell(unsigned char col, unsigned char row,
                     unsigned char glyph, unsigned char color)
{
    RIA.addr0 = CELL(col, row);
    RIA.step0 = 1;
    RIA.rw0 = glyph;
    RIA.rw0 = color;
}

/* Glyphs only, leaving the colour byte alone -- the X16 does the same by
 * setting the address increment to 2. */
static void put_glyphs(unsigned char col, unsigned char row,
                       const unsigned char *g, unsigned char n)
{
    RIA.addr0 = CELL(col, row);
    RIA.step0 = 2;
    while (n--)
        RIA.rw0 = *g++;
    RIA.step0 = 1;
}

static void fill_glyphs(unsigned char col, unsigned char row,
                        unsigned char glyph, unsigned char n)
{
    RIA.addr0 = CELL(col, row);
    RIA.step0 = 2;
    while (n--)
        RIA.rw0 = glyph;
    RIA.step0 = 1;
}

/* ---- the message console --------------------------------------------- */

/* Rows 27, 28 and 29, columns 0 to 32. */
#define CON_TOP    27
#define CON_BOTTOM 29
#define CON_WIDTH  33
#define CON_COLOR  5            /* green, as GREEN_SCREEN left it on the X16 */

static unsigned char print_x;

/* Copy row 28 to 27 and row 29 to 28, then blank row 29. Glyphs only: the
 * console colour is set once and never scrolls.
 *
 * The X16 reads the characters back out of video memory to do this. The RIA
 * portals read as well as write -- a read of RIA.rw0 returns the byte and then
 * advances addr0 by step0 -- so the routine ports directly instead of needing a
 * shadow copy in RAM. Portal 0 reads, portal 1 writes. */
void plat_scroll_info(void)
{
    unsigned char i;

    RIA.step0 = 2;
    RIA.step1 = 2;
    RIA.addr0 = CELL(0, CON_TOP + 1);
    RIA.addr1 = CELL(0, CON_TOP);
    for (i = 0; i < CON_WIDTH; i++)
        RIA.rw1 = RIA.rw0;

    RIA.addr0 = CELL(0, CON_BOTTOM);
    RIA.addr1 = CELL(0, CON_TOP + 1);
    for (i = 0; i < CON_WIDTH; i++)
        RIA.rw1 = RIA.rw0;

    RIA.addr0 = CELL(0, CON_BOTTOM);
    for (i = 0; i < CON_WIDTH; i++)
        RIA.rw0 = 32;                   /* PETSCII space */

    RIA.step0 = 1;
    RIA.step1 = 1;
}

/* SOURCE points at a screen-code string: 0 ends it, 255 forces a new line.
 * Text always arrives on the bottom row, so printing always scrolls first. */
void plat_print_info(void)
{
    const unsigned char *p = SOURCE;
    unsigned char ch;

    plat_scroll_info();
    print_x = 0;
    RIA.addr0 = CELL(0, CON_BOTTOM);
    RIA.step0 = 2;

    for (;;) {
        ch = *p++;
        if (ch == 0)
            break;
        if (ch == 255) {
            RIA.step0 = 1;
            plat_scroll_info();
            print_x = 0;
            RIA.addr0 = CELL(0, CON_BOTTOM);
            RIA.step0 = 2;
            continue;
        }
        RIA.rw0 = ch;
        if (++print_x == 34) {
            RIA.step0 = 1;
            plat_scroll_info();
            print_x = 0;
            RIA.addr0 = CELL(0, CON_BOTTOM);
            RIA.step0 = 2;
        }
    }
    RIA.step0 = 1;
}

/* The eight dots SEARCH_OBJECT lays down while it works, at row 29 from
 * column 9. A is the dot index. */
void __fastcall__ plat_search_dot(unsigned char n)
{
    put_cell((unsigned char)(9 + n), CON_BOTTOM, 46, CON_COLOR);
}

/* ---- the status panel ------------------------------------------------- */

/* Three digits, right where DECWRITE put them. */
static void decwrite(unsigned char col, unsigned char row, unsigned char v)
{
    unsigned char d[3];
    d[0] = (unsigned char)(48 + v / 100);
    d[1] = (unsigned char)(48 + (v / 10) % 10);
    d[2] = (unsigned char)(48 + v % 10);
    put_glyphs(col, row, d, 3);
}

/* Six cells at row 27, columns 34 to 39: a full block per two health, a half
 * block for the odd one, spaces after. */
void plat_display_player_health(void)
{
    unsigned char i, half = (unsigned char)(UNIT_HEALTH[0] >> 1);
    unsigned char cells[6];
    for (i = 0; i < 6; i++)
        cells[i] = (unsigned char)(i < half ? 0x66
                   : (i == half && (UNIT_HEALTH[0] & 1)) ? 0x5C : 32);
    put_glyphs(34, 27, cells, 6);
}

/* The three key cards, as 2x2 glyph blocks at rows 24-25, columns 34, 36 and
 * 38, in red, green and blue. The glyphs are PETSCII box drawing plus the suit:
 * spade, heart, and the star the game draws with $2A. */
void plat_display_keys(void)
{
    static const unsigned char suit[3] = { 0x41, 0x53, 0x2A };
    static const unsigned char color[3] = { 2, 5, 6 };
    unsigned char k, col;

    for (k = 0; k < 3; k++) {
        col = (unsigned char)(34 + k * 2);
        if (KEYS & (1 << k)) {
            put_cell(col,     24, 0x63, color[k]);
            put_cell(col + 1, 24, 0x4D, color[k]);
            put_cell(col,     25, suit[k], color[k]);
            put_cell(col + 1, 25, 0x67, color[k]);
        } else {
            put_cell(col,     24, 32, color[k]);
            put_cell(col + 1, 24, 32, color[k]);
            put_cell(col,     25, 32, color[k]);
            put_cell(col + 1, 25, 32, color[k]);
        }
    }
}

/* ---- sprites ---------------------------------------------------------- */

static void set_sprite(unsigned char slot, int x, int y,
                       unsigned bitmap, unsigned palette)
{
    RIA.addr0 = XR_SPRITES + slot * 8;
    RIA.step0 = 1;
    RIA.rw0 = (unsigned char)(x & 0xFF);  RIA.rw0 = (unsigned char)(x >> 8);
    RIA.rw0 = (unsigned char)(y & 0xFF);  RIA.rw0 = (unsigned char)(y >> 8);
    RIA.rw0 = (unsigned char)(bitmap & 0xFF);
    RIA.rw0 = (unsigned char)(bitmap >> 8);
    RIA.rw0 = (unsigned char)(palette & 0xFF);
    RIA.rw0 = (unsigned char)(palette >> 8);
}

static void park_sprite(unsigned char slot)
{
    set_sprite(slot, 0, SPR_PARKED_Y, 0, 0);
}

/* All six sprites are 32x32 4bpp, which is why one call covers them: mode 5 has
 * no non-square size, so the X16's two 64x32 HUD icons are four sprites here.
 * Options 0x12 is 4bpp (2) with the 32x32 size in bits 3-5. Plane 2.
 *
 * Mode 5 has no enable bit, so a sprite is turned off by moving it off screen;
 * everything starts parked. */
void plat_sprites_init(void)
{
    unsigned char i;
    for (i = 0; i < SPR_COUNT; i++)
        park_sprite(i);
    xreg_vga_mode(5, 0x12, XR_SPRITES, SPR_COUNT, 2);
}

/* A 64x32 icon is two 32x32 sprites side by side; conv_sprites.py emits them
 * in that order, so frame 2n is the left half and 2n+1 the right. */
static void set_icon(unsigned char left_slot, int x, int y, unsigned char frame)
{
    unsigned bitmap = XR_SPR_HUD + (unsigned)(frame * 2) * SPR_FRAME_BYTES;
    set_sprite(left_slot,     x,      y, bitmap, XR_PAL_CHARS);
    set_sprite(left_slot + 1, x + 32, y, bitmap + SPR_FRAME_BYTES, XR_PAL_CHARS);
}

/* The player's frame is direction + animate, exactly as the X16 indexes
 * PLAYER_SPRITE_TABLE. He never moves on screen; the window moves under him. */
void plat_display_player_sprite(void)
{
    set_sprite(SPR_PLAYER, PLAYER_SPR_X, PLAYER_SPR_Y,
               XR_SPR_PLAYER + (unsigned)(PLAYER_DIRECTION + PLAYER_ANIMATE)
                               * SPR_FRAME_BYTES,
               XR_PAL_PLAYER);
}

/* The selection cursor over a map window cell. CURSOR_ON picks the shape:
 * 1 compass for pushing, 2 magnifier for searching, 3 hand for using.
 *
 * Cell to pixel is the X16's own arithmetic, recovered from its cursor tables:
 * column 4, 5, 6 sit at x 98, 122, 146 and rows 2, 3, 4 at y 65, 89, 113. */
void plat_reverse_tile(void)
{
    if (!CURSOR_ON) {
        park_sprite(SPR_CURSOR);
        return;
    }
    set_sprite(SPR_CURSOR,
               CURSOR_X * 24 + 2, CURSOR_Y * 24 + 17,
               XR_SPR_CURSOR + (unsigned)(CURSOR_ON - 1) * SPR_FRAME_BYTES,
               XR_PAL_PLAYER);
}

/* Weapon and item panels. The selection rules are the X16's PRESELECT_*: if
 * nothing is selected, take the first thing in inventory; if the selected one
 * has run out, fall back the same way. */
void plat_display_weapon(void)
{
    if (SELECTED_WEAPON == 1 && !AMMO_PISTOL)
        SELECTED_WEAPON = 0;
    if (SELECTED_WEAPON == 2 && !AMMO_PLASMA)
        SELECTED_WEAPON = 0;
    if (!SELECTED_WEAPON)
        SELECTED_WEAPON = AMMO_PISTOL ? 1 : AMMO_PLASMA ? 2 : 0;

    if (!SELECTED_WEAPON) {
        park_sprite(SPR_WEAPON_L);
        park_sprite(SPR_WEAPON_R);
        fill_glyphs(37, 7, 32, 3);
        return;
    }
    set_icon(SPR_WEAPON_L, 263, 24, (unsigned char)(SELECTED_WEAPON - 1));
    decwrite(37, 7, SELECTED_WEAPON == 1 ? AMMO_PISTOL : AMMO_PLASMA);
}

void plat_display_item(void)
{
    unsigned char qty;

    /* 1 bomb, 2 EMP, 3 medkit, 4 magnet -- the order CYCLE_ITEM walks. */
    for (;;) {
        switch (SELECTED_ITEM) {
        case 1: qty = INV_BOMBS;  break;
        case 2: qty = INV_EMP;    break;
        case 3: qty = INV_MEDKIT; break;
        case 4: qty = INV_MAGNET; break;
        default: qty = 0;         break;
        }
        if (SELECTED_ITEM && qty)
            break;
        /* Nothing there: take the first thing that is. */
        SELECTED_ITEM = INV_BOMBS ? 1 : INV_EMP ? 2 : INV_MEDKIT ? 3
                      : INV_MAGNET ? 4 : 0;
        if (!SELECTED_ITEM)
            break;
    }

    if (!SELECTED_ITEM) {
        park_sprite(SPR_ITEM_L);
        park_sprite(SPR_ITEM_R);
        fill_glyphs(37, 18, 32, 3);
        return;
    }
    /* HUD frames run pistol, plasma, medkit, EMP, magnet, timebomb. */
    {
        static const unsigned char frame_for[5] = { 0, 5, 3, 2, 4 };
        set_icon(SPR_ITEM_L, 263, 114, frame_for[SELECTED_ITEM]);
    }
    decwrite(37, 18, qty);
}

/* Stubs for the milestones that have not landed yet. Each one is a routine the
 * AI legitimately calls, so they have to exist and return cleanly; what they do
 * not do is listed in docs/porting-notes.md. */

void __fastcall__ plat_play_sound(unsigned char effect)
{
    (void)effect;               /* M7: the PET music engine on the RIA PSG */
}

void plat_elevator_select(void) { }          /* M6: the elevator UI      */

/* M6: the gamepad path. CONTROL is 0 or 1 here, so the AI never reaches this. */
void plat_gamepad_read(void) { }

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
