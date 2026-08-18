/* PETSCII Robots for the RP6502 -- video bring-up.
 *
 * Puts the three planes up, loads a level, and draws the map window. This is
 * the milestone that proves the format decisions on real hardware: the font
 * transpose, mode 1's 4-bit cell order, the palette, and the tile plotter.
 */
#include <fcntl.h>
#include <stdbool.h>
#include <rp6502.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xram.h"

/* TILESET.GFX, reorganised by tools/convert/conv_tiles.py: each game tile is
 * three character rows of six bytes -- glyph, colour, glyph, colour, glyph,
 * colour -- so a row is one address store and six portal writes. Colours are
 * pre-masked to $0F, which is what makes the playfield transparent over the
 * bitmap plane and what deletes nine AND #$0F from the X16's inner loop. */
static unsigned char tile_cells[256 * 3 * 6];
static unsigned char destruct_path[256];
static unsigned char tile_attrib[256];

/* A level, exactly as conv_level.py lays it out: no padding, one read(). */
static struct {
    unsigned char type[64], x[64], y[64], a[64], b[64], c[64], d[64], health[64];
    unsigned char map[MAP_W * MAP_H];
} lvl;

static unsigned char map_window_x, map_window_y;

static void die(const char *what)
{
    printf("\nERROR: %s\n", what);
    exit(1);
}

/* read() moves data through the 512-byte XSTACK, so it loops. */
static void slurp_fd(int fd, void *dst, unsigned len)
{
    unsigned got = 0;
    int n;
    while (got < len) {
        n = read(fd, (unsigned char *)dst + got, len - got);
        if (n <= 0)
            break;
        got += n;
    }
    if (got != len)
        die("short read");
}

static void slurp(const char *name, void *dst, unsigned len)
{
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        die(name);
    slurp_fd(fd, dst, len);
    close(fd);
}

/* A 320x240 4bpp screen straight into the bitmap plane. read_xram() hands the
 * whole transfer to the RIA, so the 6502 does not touch the pixels; count is
 * capped at 0x7FFF per call, so 38400 bytes is two calls, not a loop of small
 * ones -- bulk transfer approaches 800 KB/s but every call pays a round trip. */
static void load_bitmap(const char *name)
{
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        die(name);
    /* read_xram caps count at 0x7FFF, so a 38400-byte screen is two calls. */
    if (read_xram(XR_BITMAP, 0x7000u, fd) != 0x7000)
        die("read_xram lo");
    if (read_xram(XR_BITMAP + 0x7000u, 0x2600u, fd) != 0x2600)
        die("read_xram hi");
    close(fd);
}

static void video_init(void)
{
    /* Selecting a canvas clears every plane's programming, and takes the
     * console away until something asks for it back. */
    xreg_vga_canvas(1);                       /* 320x240 */

    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, x_wrap, false);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, y_wrap, false);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, x_pos_px, 0);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, y_pos_px, 0);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, width_px, 320);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, height_px, 240);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, xram_data_ptr, XR_BITMAP);
    xram0_struct_set(XR_CFG_BITMAP, vga_mode3_config_t, xram_palette_ptr, XR_PAL_BITMAP);

    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, x_wrap, false);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, y_wrap, false);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, x_pos_px, 0);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, y_pos_px, 0);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, width_chars, SCR_COLS);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, height_chars, SCR_ROWS);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, xram_data_ptr, XR_CHARS);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, xram_palette_ptr, XR_PAL_CHARS);
    xram0_struct_set(XR_CFG_CHARS, vga_mode1_config_t, xram_font_ptr, XR_FONT);

    /* mode 3 options 0x02 = 4bpp, bit 3 clear so the high nibble is the left
     * pixel, which is the order the RLE decoder already produces.
     * mode 1 options 0x02 = 4-bit colour {glyph, bg_fg}, bit 3 clear = 8x8. */
    xreg_vga_mode(3, 0x02, XR_CFG_BITMAP, 0);
    xreg_vga_mode(1, 0x02, XR_CFG_CHARS, 1);
}

static void clear_chars(void)
{
    unsigned i;
    RIA.addr0 = XR_CHARS;
    RIA.step0 = 1;
    for (i = 0; i < SCR_COLS * SCR_ROWS; i++) {
        RIA.rw0 = 32;      /* PETSCII space */
        RIA.rw0 = 0;       /* background 0, foreground 0: fully transparent */
    }
}

/* One 24x24 game tile as three character rows of six bytes. */
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

static void draw_map_window(void)
{
    unsigned char tx, ty;
    unsigned addr;
    for (ty = 0; ty < MAP_WIN_TILES_H; ty++) {
        for (tx = 0; tx < MAP_WIN_TILES_W; tx++) {
            addr = CELL(MAP_WIN_COL + tx * 3, MAP_WIN_ROW + ty * 3);
            plot_tile(addr, lvl.map[((unsigned)(map_window_y + ty) << 7)
                                    + map_window_x + tx]);
        }
    }
}

static void put_string(unsigned char col, unsigned char row,
                       const char *s, unsigned char color)
{
    RIA.addr0 = CELL(col, row);
    RIA.step0 = 1;
    while (*s) {
        /* PETSCII screen codes: 'a'-'z' land at 1-26, matching the charset. */
        char ch = *s++;
        RIA.rw0 = (ch >= 'a' && ch <= 'z') ? (unsigned char)(ch - 0x60)
                                           : (unsigned char)ch;
        RIA.rw0 = color;
    }
}

int main(void)
{
    /* tiles.bin is cells, then destruct_path, then tile_attrib. */
    {
        int fd = open("ROM:tiles", O_RDONLY);
        if (fd < 0)
            die("tiles");
        slurp_fd(fd, tile_cells, sizeof tile_cells);
        slurp_fd(fd, destruct_path, sizeof destruct_path);
        slurp_fd(fd, tile_attrib, sizeof tile_attrib);
        close(fd);
    }
    slurp("ROM:level-a", &lvl, sizeof lvl);

    video_init();
    load_bitmap("ROM:gamepic");
    clear_chars();

    /* Centre the window on the player, exactly as CACULATE_AND_REDRAW does. */
    map_window_x = lvl.x[0] - 5;
    map_window_y = lvl.y[0] - 3;
    draw_map_window();

    put_string(0, 27, "petscii robots rp6502", 5);
    put_string(0, 28, "video bring-up ok", 1);

    /* Also to the console, which is where the emulator's script harness
     * watches: selecting a canvas removes the VGA console, but stdout still
     * reaches the terminal stream. */
    printf("BRINGUP OK level-a player %u,%u\n", lvl.x[0], lvl.y[0]);

    for (;;) {
        /* vsync is a counter, not a flag: reading the delta means a frame that
         * overruns catches up instead of losing a tick, which the X16's
         * IRQ-driven clock could not do. */
        unsigned char v = RIA.vsync;
        while (RIA.vsync == v)
            ;
    }
}
