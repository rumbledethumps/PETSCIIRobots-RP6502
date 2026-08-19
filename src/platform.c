/* The presentation side of the ported game logic.
 *
 * src/game/platform_bridge.s forwards the AI's calls here. Each of these stands
 * in for a routine that lived in the X16's machine-specific file and talked to
 * VERA or ZSOUND directly.
 */
#include <fcntl.h>
#include <rp6502.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "game/game.h"
#include "input.h"
#include "platform.h"
#include "xram.h"

static void park_sprite(unsigned char slot);
static void decwrite(unsigned char col, unsigned char row, unsigned char v);
static void put_glyphs(unsigned char col, unsigned char row,
                       const unsigned char *g, unsigned char n);

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

/* The blitters live in src/plot.s. They were C; the profile moved them. A full
 * window is 77 tiles and the C version cost about 15 ms of a 16.7 ms frame,
 * which the window pays on every step the player takes. plot_addr is a zero
 * page pair the caller sets first; the tile number goes in A, where fastcall
 * puts it anyway. */
extern unsigned plot_addr;
#pragma zpsym("plot_addr")
void __fastcall__ plot_tile(unsigned char tile);
void __fastcall__ plot_transparent_tile(unsigned char tile);

/* ANIMATE_WATER, from reference/x16/x16Robots.ASM 4419-4560.
 *
 * The one piece of the original that could not be converted rather than
 * rewritten. Everything it does is machine independent -- it only permutes
 * bytes of the tileset and asks for a redraw -- but it does that through the
 * PET's nine TILE_DATA_xx and nine TILE_COLOR_xx arrays, each 256 bytes, one
 * per cell position. This port stores the tileset the other way round: eighteen
 * bytes per tile, glyph and colour interleaved and the colour pre-masked, which
 * is what makes plot_tile six portal writes per row. Structure of arrays into
 * array of structures is not something symbol arithmetic can bridge, so the
 * routine is re-expressed here against tile_cells and the arithmetic lives in
 * two macros.
 *
 * Five animations share the one twenty-frame timer, exactly as the original
 * runs them: water, the trash compactor, the HVAC fans, the cinema marquee and
 * the server room's blinking light.
 */

/* Cell (row, col) of a tile: row 0 top, col 0 left. TD is the glyph, TC the
 * colour, which is where the X16 wrote TILE_DATA_xx and TILE_COLOR_xx. */
#define TD(tile, row, col) tile_cells[(unsigned)(tile) * 18u + (row) * 6u + (col) * 2u]
#define TC(tile, row, col) tile_cells[(unsigned)(tile) * 18u + (row) * 6u + (col) * 2u + 1u]

/* The X16 declares this as a byte that is one and never assigns it again. Kept
 * because it is the switch the original offers for turning the animations off. */
unsigned char ANIMATE = 1;

static unsigned char water_timer, hvac_state, cinema_state;

void plat_animate_water(void)
{
    unsigned char t;

    if (ANIMATE != 1)
        return;
    if (++water_timer != 20)
        return;
    water_timer = 0;

    /* Water, tile 204. Three diagonals each rotate one cell down-right, which
     * is what makes the surface appear to drift. Tile 221 is the same water
     * under something else and takes the five cells the original gives it --
     * not all nine, which is the original's business and not a slip to tidy. */
    t = TD(204, 2, 2);
    TD(204, 2, 2) = TD(204, 1, 1);  TD(221, 2, 2) = TD(204, 1, 1);
    TD(204, 1, 1) = TD(204, 0, 0);
    TD(204, 0, 0) = t;

    t = TD(204, 2, 0);
    TD(204, 2, 0) = TD(204, 1, 2);  TD(221, 2, 0) = TD(204, 1, 2);
    TD(204, 1, 2) = TD(204, 0, 1);
    TD(204, 0, 1) = t;              TD(221, 0, 1) = t;

    t = TD(204, 2, 1);
    TD(204, 2, 1) = TD(204, 1, 0);  TD(221, 2, 1) = TD(204, 1, 0);
    TD(204, 1, 0) = TD(204, 0, 2);
    TD(204, 0, 2) = t;              TD(221, 0, 2) = t;

    /* The trash compactor, tile 148: colours rotate right along each row while
     * the glyphs stay put, so the crusher looks like it is turning over. */
    t = TC(148, 0, 2);
    TC(148, 0, 2) = TC(148, 0, 1);
    TC(148, 0, 1) = TC(148, 0, 0);
    TC(148, 0, 0) = t;

    t = TC(148, 1, 2);
    TC(148, 1, 2) = TC(148, 1, 1);
    TC(148, 1, 1) = TC(148, 1, 0);
    TC(148, 1, 0) = t;

    t = TC(148, 2, 2);
    TC(148, 2, 2) = TC(148, 2, 1);
    TC(148, 2, 1) = TC(148, 2, 0);
    TC(148, 2, 0) = t;

    /* The HVAC fans, tiles 196, 197, 200 and 201: two frames, swapped. */
    if (hvac_state) {
        TD(196, 1, 1) = 0xCD;  TD(201, 0, 0) = 0xCD;
        TD(197, 1, 0) = 0xCE;  TD(200, 0, 1) = 0xCE;
        TD(196, 1, 2) = 0xA0;  TD(196, 2, 1) = 0xA0;
        TD(197, 2, 0) = 0xA0;  TD(200, 0, 2) = 0xA0;
        hvac_state = 0;
    } else {
        TD(196, 1, 1) = 0xA0;  TD(201, 0, 0) = 0xA0;
        TD(197, 1, 0) = 0xA0;  TD(200, 0, 1) = 0xA0;
        TD(196, 1, 2) = 0xC2;  TD(200, 0, 2) = 0xC2;
        TD(196, 2, 1) = 0xC0;  TD(197, 2, 0) = 0xC0;
        hvac_state = 1;
    }

    /* The cinema marquee: six cells across tiles 20, 21 and 22 shift one to the
     * left and the next character of CINEMA_MESSAGE arrives at the right. */
    TD(20, 1, 1) = TD(20, 1, 2);
    TD(20, 1, 2) = TD(21, 1, 0);
    TD(21, 1, 0) = TD(21, 1, 1);
    TD(21, 1, 1) = TD(21, 1, 2);
    TD(21, 1, 2) = TD(22, 1, 0);
    TD(22, 1, 0) = CINEMA_MESSAGE[cinema_state];

    if (++cinema_state == 197)
        cinema_state = 0;

    /* The server room light, tile 143, alternating between two glyphs. */
    TD(143, 1, 2) = (unsigned char)(TD(143, 1, 2) == 0xD7 ? 0xD1 : 0xD7);

    REDRAW_WINDOW = 1;
}

void plat_draw_map_window(void)
{
    unsigned char tx, ty, n = 0;

    MAP_PRE_CALCULATE();
    REDRAW_WINDOW = 0;

    for (ty = 0; ty < MAP_WIN_TILES_H; ty++) {
        for (tx = 0; tx < MAP_WIN_TILES_W; tx++, n++) {
            plot_addr = CELL(MAP_WIN_COL + tx * 3, MAP_WIN_ROW + ty * 3);
            plot_tile(MAP[((unsigned)(MAP_WINDOW_Y + ty) << 7)
                          + MAP_WINDOW_X + tx]);
            /* A unit standing here draws over the terrain, at the same
             * address -- which is why the blitter leaves plot_addr alone. */
            if (MAP_PRECALC[n])
                plot_transparent_tile(MAP_PRECALC[n]);
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

/* ---- whole screens ---------------------------------------------------- */

/* Every colour byte green, which is what the X16 leaves behind before it
 * decodes a screen layout over the top. The layouts carry glyphs only. */
void plat_green_screen(void)
{
    unsigned i;
    RIA.addr0 = XR_CHARS + 1;           /* the colour byte of cell 0 */
    RIA.step0 = 2;
    for (i = 0; i < SCR_COLS * SCR_ROWS; i++)
        RIA.rw0 = 5;
    RIA.step0 = 1;
}

/* A 40x30 layout, run-length encoded: a literal byte is one cell, and byte 96
 * introduces "the next byte, count+1 times". even_odd picks the plane -- 0 for
 * glyphs, 1 for colours -- exactly as the X16 selects it by starting on an even
 * or odd address with the increment set to two. */
void plat_decompress_screen(const unsigned char *src, unsigned char even_odd)
{
    unsigned char x = 0, y = 0, ch, n;

    RIA.addr0 = XR_CHARS + even_odd;
    RIA.step0 = 2;
    while (y < SCR_ROWS) {
        ch = *src++;
        n = 1;
        if (ch == 96) {                 /* the repeat flag */
            ch = *src++;
            n = (unsigned char)(*src++ + 1);
        }
        while (n-- && y < SCR_ROWS) {
            RIA.rw0 = ch;
            if (++x == SCR_COLS) {
                x = 0;
                if (++y < SCR_ROWS)
                    RIA.addr0 = CELL(0, y) + even_odd;
            }
        }
    }
    RIA.step0 = 1;
}

/* A 320x240 4bpp screen into the bitmap plane. read_xram() hands the whole
 * transfer to the RIA, so the 6502 never touches the pixels; count is capped at
 * 0x7FFF per call, so this is two calls rather than a loop of small ones --
 * bulk transfer approaches 800 KB/s but every call pays a round trip. */
void plat_load_bitmap(const char *name)
{
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return;
    read_xram(XR_BITMAP, 0x7000u, fd);
    read_xram(XR_BITMAP + 0x7000u, 0x2600u, fd);
    close(fd);
}

void plat_display_intro_screen(void)
{
    plat_backdrop_visible(1);
    plat_green_screen();
    plat_decompress_screen(INTRO_TEXT, 0);
    plat_load_bitmap("ROM:intropic");
}

void plat_display_game_screen(void)
{
    plat_backdrop_visible(1);
    plat_green_screen();
    plat_decompress_screen(SCR_TEXT, 0);
    plat_load_bitmap("ROM:gamepic");
}

/* Slide the backdrop off the canvas. The X16 turns layer 0 off for the stats
 * screen; mode 3 has no enable bit, but the plane's position is a signed pixel
 * offset, so putting it a screenful to the left has the same effect and does
 * not disturb the pixels underneath. */
void __fastcall__ plat_backdrop_visible(unsigned char on)
{
    int x = on ? 0 : -320;
    RIA.addr0 = XR_CFG_BITMAP + 2;      /* x_pos_px, after the two wrap flags */
    RIA.step0 = 1;
    RIA.rw0 = (unsigned char)(x & 0xFF);
    RIA.rw0 = (unsigned char)(x >> 8);
}

void plat_display_endgame_screen(void)
{
    plat_backdrop_visible(0);
    plat_green_screen();
    plat_decompress_screen(SCR_ENDGAME, 0);
}

/* ---- the intro menu --------------------------------------------------- */

/* The four options sit at character rows 2 to 5, columns 4 to 13. Selecting one
 * is a colour change, so only the colour bytes move. */
void __fastcall__ plat_flash_menu_option(unsigned char color)
{
    unsigned char i;
    RIA.addr0 = CELL(4, MENUY + 2) + 1;
    RIA.step0 = 2;
    for (i = 0; i < 10; i++)
        RIA.rw0 = color;
    RIA.step0 = 1;
}

/* The level name, sixteen characters at row 9 column 2. MAP_NAMES is one
 * sixteen-byte name per level, so the offset is the level number times
 * sixteen -- which is what CALC_MAP_NAME computes the long way. */
void plat_display_map_name(void)
{
    const unsigned char *p = MAP_NAMES + (unsigned)SELECTED_MAP * 16;
    unsigned char i;
    RIA.addr0 = CELL(2, 9);
    RIA.step0 = 2;
    for (i = 0; i < 16; i++)
        RIA.rw0 = *p++;
    RIA.step0 = 1;
}

/* The intro robot's expression, three 16x10 images at 2bpp, one per difficulty.
 * It goes into the bitmap plane at 234,95 -- so 160 bytes a row, and 234/2 = 117
 * bytes in. */
void plat_change_difficulty_level(void)
{
    const unsigned char *p = THREE_FACES + (unsigned)DIFF_LEVEL * 80;
    unsigned char row, i;
    RIA.step0 = 1;
    for (row = 0; row < 10; row++) {
        RIA.addr0 = (unsigned)(95 + row) * 160u + 117u;
        for (i = 0; i < 8; i++)
            RIA.rw0 = *p++;
    }
}

/* ---- the end of a game ------------------------------------------------ */

/* The box, eleven characters wide at column 11, rows 9 to 11, in white. */
static void game_over_box(void)
{
    static const unsigned char *const rows[3] = { GAMEOVER1, GAMEOVER2, GAMEOVER3 };
    unsigned char r, i;
    for (r = 0; r < 3; r++) {
        RIA.addr0 = CELL(11, 9 + r);
        RIA.step0 = 1;
        for (i = 0; i < 11; i++) {
            RIA.rw0 = rows[r][i];
            RIA.rw0 = 1;                /* white */
        }
    }
}

/* The stats screen: which level, how long it took, and what was left. */
static void endgame_stats(void)
{
    const unsigned char *name = MAP_NAMES + (unsigned)SELECTED_MAP * 16;
    unsigned char i, robots = 0, secrets = 0;

    RIA.addr0 = CELL(22, 7);
    RIA.step0 = 2;
    for (i = 0; i < 16; i++)
        RIA.rw0 = *name++;
    RIA.step0 = 1;

    decwrite(21, 9, HOURS);
    decwrite(24, 9, MINUTES);
    decwrite(27, 9, SECONDS);
    /* Blank the leading digit of the hours and put colons between the fields,
     * exactly where the original pokes them. */
    put_glyphs(21, 9, (const unsigned char *)"\x20", 1);
    put_glyphs(24, 9, (const unsigned char *)"\x3A", 1);
    put_glyphs(27, 9, (const unsigned char *)"\x3A", 1);

    for (i = 1; i < 28; i++)
        if (UNIT_TYPE[i])
            robots++;
    for (i = 48; i < 64; i++)
        if (UNIT_TYPE[i])
            secrets++;
    decwrite(28, 11, robots);
    decwrite(28, 13, secrets);

    {   /* easy / normal / hard, in screen codes */
        static const unsigned char words[3][6] = {
            {  5,  1, 19, 25, 32, 32 },
            { 14, 15, 18, 13,  1, 12 },
            {  8,  1, 18,  4, 32, 32 },
        };
        put_glyphs(28, 15, words[DIFF_LEVEL < 3 ? DIFF_LEVEL : 1], 6);
    }
}

/* The player is dead, or has reached the transporter. Show the box, wait, then
 * the stats screen, then back to the title. */
void plat_game_over(void)
{
    const unsigned char *msg;
    unsigned char i, n, key;

    CLOCK_ACTIVE = 0;

    if (!UNIT_TYPE[0]) {                /* died rather than won */
        PLAYER_DIRECTION = FACE_DEAD;
        PLAYER_ANIMATE = 0;
        plat_display_player_sprite();
        KEYTIMER = 100;
        while (KEYTIMER)
            BACKGROUND_TASKS();
    }
    SCREEN_SHAKE = 0;
    game_over_box();

    KEYTIMER = 100;                     /* ignore keys for a moment */
    while (KEYTIMER)
        BACKGROUND_TASKS();
    plat_clear_key_buffer();
    do {
        BACKGROUND_TASKS();
        key = plat_getin();
    } while (!key);

    /* The stats. Sprites go away with the playfield. */
    for (i = 0; i < SPR_COUNT; i++)
        park_sprite(i);
    plat_display_endgame_screen();
    endgame_stats();

    msg = UNIT_TYPE[0] ? WIN_MSG : LOS_MSG;
    n = (unsigned char)(UNIT_TYPE[0] ? 8 : 9);
    put_glyphs(16, 3, msg, n);
    plat_play_sound(UNIT_TYPE[0] ? SFX_FOUNDITEM : SFX_ERROR);

    plat_clear_key_buffer();
    do {
        BACKGROUND_TASKS();
        key = plat_getin();
    } while (!key);
}

/* ---- the live map ----------------------------------------------------- */

/* TAB shows the whole 128x64 map drawn into the bitmap plane, one byte per
 * tile, each row emitted twice so it fills 128x128 pixels. It starts at pixel
 * row 36, and the bitmap is 160 bytes a row.
 *
 * The playfield characters are blanked first so the map underneath shows
 * through -- the character plane sits over the bitmap, and its transparent
 * cells are what let the backdrop be seen at all. */
#define LIVEMAP_TOP 36

static void clear_playfield(void)
{
    unsigned char row, i;
    RIA.step0 = 1;
    for (row = 2; row < 27; row++) {
        RIA.addr0 = CELL(0, row);
        for (i = 0; i < 33; i++) {
            RIA.rw0 = 32;
            RIA.rw0 = 0;                /* transparent */
        }
    }
}

static void draw_live_map(void)
{
    const unsigned char *m = MAP;
    unsigned char row, col;
    unsigned addr;

    /* Both copies of a row go out together, portal 0 to the even scanline and
     * portal 1 to the odd one, so the map byte is only translated once. */
    RIA.step0 = 1;
    RIA.step1 = 1;
    for (row = 0; row < MAP_H; row++) {
        addr = (unsigned)(LIVEMAP_TOP + row * 2) * 160u;
        RIA.addr0 = addr;
        RIA.addr1 = addr + 160u;
        for (col = 0; col < MAP_W; col++) {
            unsigned char c = MAP_TRANSLATION_TABLE[*m++];
            RIA.rw0 = c;
            RIA.rw1 = c;
        }
    }
}

/* A unit's position as two pixels on the live map, doubled the same way. */
static void plot_map_dot(unsigned char slot, unsigned char color)
{
    /* One map byte is one bitmap byte -- two pixels of the same colour -- so
     * the column is the map X, not half of it. The row is doubled to match the
     * doubled draw. */
    unsigned addr = (unsigned)(LIVEMAP_TOP + UNIT_LOC_Y[slot] * 2) * 160u
                  + UNIT_LOC_X[slot];
    RIA.step0 = 1;
    RIA.addr0 = addr;
    RIA.rw0 = color;
    RIA.addr0 = addr + 160u;
    RIA.rw0 = color;
}

static void blink_dots(unsigned char color, unsigned char robots)
{
    unsigned char i;
    if (!robots) {
        plot_map_dot(0, color);
        return;
    }
    for (i = 1; i < 28; i++)
        if (UNIT_TYPE[i] && UNIT_TYPE[i] != 8)   /* 8 is a dead robot */
            plot_map_dot(i, color);
}

/* Restore the backdrop under the map rather than blanking it: the bitmap is a
 * file, so reading the slice back costs the 6502 nothing. */
static void restore_backdrop(void)
{
    int fd = open("ROM:gamepic", O_RDONLY);
    if (fd < 0)
        return;
    lseek(fd, (long)LIVEMAP_TOP * 160L, SEEK_SET);
    read_xram((unsigned)LIVEMAP_TOP * 160u, 128u * 160u, fd);
    close(fd);
}

/* The TAB screen. Holds until a key, blinking the player or the robots; TAB
 * itself switches between the two, as it does on the X16. */
void plat_display_map(void)
{
    unsigned char robots = 0, color = 0, key;

    SCREEN_SHAKE = 0;
    plat_play_sound(SFX_BEEP2);
    clear_playfield();
    park_sprite(SPR_PLAYER);
    draw_live_map();

    for (;;) {
        if (!BGTIMER2) {
            color = (unsigned char)(color ? 0x00 : 0x11);
            blink_dots(color, robots);
            BGTIMER2 = 30;
        }
        BACKGROUND_TASKS();
        key = plat_getin();
        if (!key)
            continue;
        if (key == 9) {                 /* TAB toggles player and robots */
            plat_play_sound(SFX_BEEP2);
            robots = (unsigned char)!robots;
            continue;
        }
        break;
    }

    restore_backdrop();
    plat_display_player_sprite();
    plat_play_sound(SFX_BEEP2);
    REDRAW_WINDOW = 1;
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
    PLAY_SOUND(effect);
}

/* The elevator panel. The AI has already printed the two message lines; this
 * draws the floor numbers along the bottom row, highlights the one the player
 * is on, and lets left and right pick another.
 *
 * Choosing a floor moves the player immediately -- there is no travel -- by
 * finding the elevator unit whose UNIT_C matches, which is what
 * ELEVATOR_FIND_XY does. The window offsets look inconsistent at a glance:
 * x - 5 but y - 4. They are not. The original decrements the player's Y after
 * reading it, so the player ends up one square above the elevator and the
 * window four above that, which is the same viewport row 3 every other move
 * puts him on.
 */
#define ELEV_ROW      29
#define ELEV_COL      7
#define ELEV_NORMAL   0x05      /* green on transparent */
#define ELEV_SELECTED 0x50      /* inverted */

static unsigned char elevator_max, elevator_floor;

static void elevator_invert(unsigned char color)
{
    RIA.addr0 = CELL(ELEV_COL + elevator_floor - 1, ELEV_ROW) + 1;
    RIA.step0 = 1;
    RIA.rw0 = color;
}

/* Put the player on the chosen floor. */
static void elevator_find_xy(void)
{
    unsigned char i;
    for (i = 32; i < 48; i++) {
        if (UNIT_TYPE[i] != 19 || UNIT_C[i] != elevator_floor)
            continue;
        UNIT_LOC_X[0] = UNIT_LOC_X[i];
        MAP_WINDOW_X = (unsigned char)(UNIT_LOC_X[i] - 5);
        UNIT_LOC_Y[0] = (unsigned char)(UNIT_LOC_Y[i] - 1);
        MAP_WINDOW_Y = (unsigned char)(UNIT_LOC_Y[i] - 4);
        plat_draw_map_window();
        plat_play_sound(SFX_BEEP2);
        return;
    }
}

void plat_elevator_select(void)
{
    unsigned char i, key;

    plat_draw_map_window();
    elevator_max = UNIT_D[UNIT];
    elevator_floor = UNIT_C[UNIT];

    /* The floors, as digits from 1. */
    RIA.addr0 = CELL(ELEV_COL, ELEV_ROW);
    RIA.step0 = 2;
    for (i = 0; i < elevator_max; i++)
        RIA.rw0 = (unsigned char)(0x31 + i);    /* screen code for '1' */
    RIA.step0 = 1;
    elevator_invert(ELEV_SELECTED);

    /* ELS5 is a tight poll -- no BACKGROUND_TASKS -- so the world holds still
     * while a floor is being chosen. This used to run the AI round this loop,
     * which let robots move and shoot while the panel was up. The keyboard is
     * scanned by the interrupt now, so waiting here costs nothing. */
    for (;;) {
        key = plat_getin();
        if (!key)
            continue;

        if (key == 0x9D || key == KEY_MOVE_UP[2]) {             /* left */
            if (elevator_floor > 1) {
                elevator_invert(ELEV_NORMAL);
                elevator_floor--;
                elevator_invert(ELEV_SELECTED);
                elevator_find_xy();
            }
        } else if (key == 0x1D || key == KEY_MOVE_UP[3]) {      /* right */
            if (elevator_floor < elevator_max) {
                elevator_invert(ELEV_NORMAL);
                elevator_floor++;
                elevator_invert(ELEV_SELECTED);
                elevator_find_xy();
            }
        } else if (key == 0x11 || key == KEY_MOVE_UP[1]) {      /* down: leave */
            elevator_invert(ELEV_NORMAL);
            plat_scroll_info();
            plat_scroll_info();
            plat_scroll_info();
            plat_clear_key_buffer();
            return;
        }
    }
}

/* SNES_CONTROLER_READ, from x16Robots.ASM 778.
 *
 * The X16 asks the KERNAL for a joystick word and unpacks it into twelve bytes
 * -- B, Y, SELECT, START, UP, DOWN, LEFT, RIGHT, A, X, L, R -- keeping the
 * previous read alongside so it can tell a press from a hold. A press sets the
 * matching NEW_ latch and leaves it set; whichever piece of code acts on it
 * clears it. That is the contract the ported assembly expects: items.s calls
 * this from MOVE_OBJECT and USER_SELECT_OBJECT and then reads NEW_ directly.
 *
 * The RIA keeps the same twelve facts in XRAM, continuously updated, so this
 * reads four bytes of the player-one block instead of calling anything:
 * DPAD holds the directions in bits 0-3 and a connected flag in bit 7, BTN0 the
 * face and shoulder buttons, BTN1 select and start.
 *
 * FACE BUTTONS ARE MAPPED BY POSITION, NOT BY NAME. The original fires in the
 * direction of the button under your thumb -- SNES Y is the left button and
 * fires left, A is the right one and fires right, X is the top, B is the
 * bottom. The RIA reports buttons by label, and a label sits in a different
 * place depending on the pad: DPAD bits 4-5 say which convention this one uses,
 * and an "Eastern BA" pad has A and B the other way round from a "Western AB"
 * one. Mapping by name would fire left when you press the top button on half
 * the pads in the world, so the layout byte picks the positions.
 *
 * BOTH STICKS ARE MERGED IN. The RIA reduces each to four direction bits in
 * STICKS, so the left one joins the d-pad and the right one joins the face
 * buttons where each points. The pad becomes twin-stick and nothing above this
 * function knows: the twelve latches are still the twelve the X16 unpacked. */

#define PAD_CONNECTED 0x80      /* DPAD bit 7                       */
#define PAD_TYPE_MASK 0x30      /* DPAD bits 4-5                    */
#define PAD_TYPE_BA   0x20      /* 2 = Eastern BA, A and B swapped  */

static unsigned char pad_now[12], pad_last[12];

void plat_gamepad_read(void)
{
    unsigned char i, dpad, sticks, btn0, btn1, bottom, right, left, top;

    for (i = 0; i < 12; i++)
        pad_last[i] = pad_now[i];

    RIA.addr0 = XR_GAMEPAD;             /* player one */
    RIA.step0 = 1;
    dpad = RIA.rw0;
    sticks = RIA.rw0;
    btn0 = RIA.rw0;
    btn1 = RIA.rw0;

    if (!(dpad & PAD_CONNECTED)) {
        for (i = 0; i < 12; i++)
            pad_now[i] = 0;
        return;                         /* no pad: nothing held, nothing new */
    }

    if ((dpad & PAD_TYPE_MASK) == PAD_TYPE_BA) {
        bottom = 0x02; right = 0x01;    /* B, A */
        left   = 0x10; top   = 0x08;    /* Y, X */
    } else {
        bottom = 0x01; right = 0x02;    /* A/Cross, B/Circle */
        left   = 0x08; top   = 0x10;    /* X/Square, Y/Triangle */
    }

    /* The left stick walks, alongside the d-pad: STICKS bits 0-3 are the same
     * four directions already reduced to bits, so this is an OR rather than a
     * threshold to pick. */
    pad_now[PAD_UP]     = (dpad & 0x01) || (sticks & 0x01);
    pad_now[PAD_DOWN]   = (dpad & 0x02) || (sticks & 0x02);
    pad_now[PAD_LEFT]   = (dpad & 0x04) || (sticks & 0x04);
    pad_now[PAD_RIGHT]  = (dpad & 0x08) || (sticks & 0x08);

    /* The right stick fires, alongside the face buttons -- bits 4-7, the same
     * four directions again. The face buttons already fire in the direction
     * they sit in, so the stick joins each one where it points and the pad ends
     * up twin-stick without the game knowing anything changed. */
    pad_now[PAD_X]      = (btn0 & top)    || (sticks & 0x10);   /* fire up    */
    pad_now[PAD_B]      = (btn0 & bottom) || (sticks & 0x20);   /* fire down  */
    pad_now[PAD_Y]      = (btn0 & left)   || (sticks & 0x40);   /* fire left  */
    pad_now[PAD_A]      = (btn0 & right)  || (sticks & 0x80);   /* fire right */

    pad_now[PAD_L]      = (btn0 & 0x40) != 0;   /* L1 */
    pad_now[PAD_R]      = (btn0 & 0x80) != 0;   /* R1 */
    pad_now[PAD_SELECT] = (btn1 & 0x04) != 0;
    pad_now[PAD_START]  = (btn1 & 0x08) != 0;

    for (i = 0; i < 12; i++)
        if (!NEW_BUTTONS[i] && pad_now[i] && !pad_last[i])
            NEW_BUTTONS[i] = 1;
}

/* SNES_SELECT and friends: whether a button is down now, as opposed to whether
 * it has just been pressed. SELECT is used as a shift key rather than an
 * action, so the code that reads it wants the hold, not the edge. */
unsigned char __fastcall__ pad_held(unsigned char button)
{
    return pad_now[button];
}

/* SC02 zeroes SNES_UP..SNES_RIGHT before reading again, which makes a direction
 * that is still held look like a fresh press and is how a held d-pad repeats.
 * Those four live here rather than in the game's variables, so the main loop
 * asks for them to be forgotten instead of writing them. */
void plat_gamepad_forget_dirs(void)
{
    pad_now[PAD_UP] = pad_now[PAD_DOWN] = 0;
    pad_now[PAD_LEFT] = pad_now[PAD_RIGHT] = 0;
}

/* The border and background flashers, from x16Robots.ASM 652-677.
 *
 * Both are one array whose index 0 is a countdown and whose remaining ten bytes
 * are a colour ramp, and the interrupt walks the ramp backwards as the counter
 * falls. On the X16 they wrote the two halves of VERA palette entry 0: BORDER
 * the byte holding red, BGFLASH the byte holding green and blue. There is no
 * border here, so both land on entry 0 of the bitmap palette, which is the
 * black the playfield sits on -- the same thing the X16 was tinting.
 *
 * One difference worth stating. VERA takes four bits of red and ignores the
 * high nibble of that byte, so its ramp -- 8, 15, 25, 31 -- actually reached
 * the screen as 8, 15, 9, 15: a jump backwards in the middle of a fade the
 * author clearly wrote as monotonic and symmetric. RP6502 palettes are RGB555,
 * so the five-bit value goes through as written and the fade is the one the
 * table describes. The green and blue nibbles are doubled to fill five bits.
 */
#define PAL_ALPHA (1u << 5)             /* the opacity bit, per conv_palette.py */

static unsigned char flashing;

void plat_flash_step(void)
{
    unsigned char r = 0, gb = 0;
    unsigned c;

    if (BORDER[0]) {
        r = BORDER[BORDER[0]];
        BORDER[0]--;
    }
    if (BGFLASH[0]) {
        gb = BGFLASH[BGFLASH[0]];
        BGFLASH[0]--;
    }

    if (!r && !gb) {
        /* Put the backdrop's opaque black back, once, on the way out. */
        if (!flashing)
            return;
        flashing = 0;
    } else {
        flashing = 1;
    }

    c = PAL_ALPHA
      | (r & 31u)
      | ((unsigned)((gb >> 4) * 2u) << 6)
      | ((unsigned)((gb & 15u) * 2u) << 11);

    RIA.addr1 = XR_PAL_BITMAP;
    RIA.step1 = 1;
    RIA.rw1 = (unsigned char)(c & 0xFF);
    RIA.rw1 = (unsigned char)(c >> 8);
}

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
