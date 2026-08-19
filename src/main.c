/* PETSCII Robots for the RP6502.
 *
 * The presentation and I/O are new; the game logic under src/game/ is David
 * Murray's assembly converted to ca65, so movement and collision behave
 * exactly as they do on the X16 rather than approximately.
 */
#include <fcntl.h>
#include <rp6502.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "game/game.h"
#include "probe.h"
#include "xram.h"

/* TILESET.GFX, reorganised by tools/convert/conv_tiles.py: each game tile is
 * three character rows of six bytes -- glyph, colour, glyph, colour, glyph,
 * colour -- so a row is one address store and six portal writes. Colours are
 * pre-masked to $0F, which is what makes the playfield transparent over the
 * bitmap plane and what deletes nine AND #$0F from the X16's inner loop. */
static unsigned char tile_cells[256 * 3 * 6];

static unsigned char keyboard[32];
static unsigned char map_window_x, map_window_y;
static unsigned char player_direction = FACE_DOWN;
static unsigned char player_animate;
static unsigned char frame_counter;

/* USB HID keycodes. The RIA publishes a bit array of these, not PS/2 codes. */
#define KEY_A 0x04
#define KEY_D 0x07
#define KEY_I 0x0C
#define KEY_J 0x0D
#define KEY_K 0x0E
#define KEY_L 0x0F
#define KEY_S 0x16
#define KEY_W 0x1A
#define KEY_RIGHT 0x4F
#define KEY_LEFT  0x50
#define KEY_DOWN  0x51
#define KEY_UP    0x52

/* Bits 0-3 of byte 0 are special: bit 0 is "no key pressed", bits 1-3 are the
 * lock LEDs. Real keys start at HID code 4. */
#define key(code) (keyboard[(code) >> 3] & (1 << ((code) & 7)))
#define KEY_NONE_DOWN 0x01

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

static void load_tileset(void)
{
    int fd = open("ROM:tiles", O_RDONLY);
    if (fd < 0)
        die("ROM:tiles");
    slurp_fd(fd, tile_cells, sizeof tile_cells);
    slurp_fd(fd, DESTRUCT_PATH, sizeof DESTRUCT_PATH);
    slurp_fd(fd, TILE_ATTRIB, sizeof TILE_ATTRIB);
    close(fd);
}

/* The unit arrays and the map are contiguous and in file order, so a level is
 * one read straight into place. */
static void load_level(const char *name)
{
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        die(name);
    slurp_fd(fd, UNIT_TYPE, LEVEL_BYTES);
    close(fd);
}

/* A 320x240 4bpp screen into the bitmap plane. read_xram() hands the whole
 * transfer to the RIA, so the 6502 never touches the pixels; count is capped at
 * 0x7FFF per call, so this is two calls rather than a loop of small ones --
 * bulk transfer approaches 800 KB/s but every call pays a round trip. */
static void load_bitmap(const char *name)
{
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        die(name);
    if (read_xram(XR_BITMAP, 0x7000u, fd) != 0x7000)
        die("read_xram lo");
    if (read_xram(XR_BITMAP + 0x7000u, 0x2600u, fd) != 0x2600)
        die("read_xram hi");
    close(fd);
}

static void video_init(void)
{
    /* Selecting a canvas clears every plane's programming and takes the
     * console away; it stays away until something asks for it back. */
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

    /* mode 3 options 0x02 = 4bpp with bit 3 clear, so the high nibble is the
     * left pixel -- the order conv_rle.py already produces.
     * mode 1 options 0x02 = 4-bit colour {glyph, bg_fg}, bit 3 clear = 8x8. */
    xreg_vga_mode(3, 0x02, XR_CFG_BITMAP, 0);
    xreg_vga_mode(1, 0x02, XR_CFG_CHARS, 1);

    xreg_ria_keyboard(XR_KEYBOARD);
}

/* Park every sprite off screen, then point the player at its first frame.
 * Mode 5 has no enable bit, so off screen is how a sprite is turned off. */
static void sprites_init(void)
{
    unsigned char i;
    RIA.addr0 = XR_SPRITES;
    RIA.step0 = 1;
    for (i = 0; i < SPR_COUNT; i++) {
        RIA.rw0 = 0;  RIA.rw0 = 0;                       /* x */
        RIA.rw0 = SPR_PARKED_Y; RIA.rw0 = 0;             /* y */
        RIA.rw0 = 0;  RIA.rw0 = 0;                       /* bitmap */
        RIA.rw0 = 0;  RIA.rw0 = 0;                       /* palette */
    }
    /* options 0x12 = 4bpp (2) | 32x32 (2 << 3); six sprites on plane 2. */
    xreg_vga_mode(5, 0x12, XR_SPRITES, SPR_COUNT, 2);
}

/* The player's frame is direction + animate, exactly as the X16 indexes
 * PLAYER_SPRITE_TABLE. He never moves on screen; the window moves under him. */
static void display_player_sprite(void)
{
    unsigned bitmap = XR_SPR_PLAYER
                    + (unsigned)(player_direction + player_animate) * SPR_FRAME_BYTES;
    RIA.addr0 = XR_SPRITES + SPR_PLAYER * 8;
    RIA.step0 = 1;
    RIA.rw0 = PLAYER_SPR_X & 0xFF;  RIA.rw0 = PLAYER_SPR_X >> 8;
    RIA.rw0 = PLAYER_SPR_Y & 0xFF;  RIA.rw0 = PLAYER_SPR_Y >> 8;
    RIA.rw0 = bitmap & 0xFF;        RIA.rw0 = bitmap >> 8;
    /* Its own palette, which is how the transporter effect can cycle the
     * player's colour 4 without touching the character plane. */
    RIA.rw0 = XR_PAL_PLAYER & 0xFF; RIA.rw0 = XR_PAL_PLAYER >> 8;
}

static void update_probe(void)
{
    unsigned char i, robots = 0;
    for (i = 1; i < 28; i++)
        if (UNIT_TYPE[i])
            robots++;
    RIA.addr0 = XR_PROBE;
    RIA.step0 = 1;
    RIA.rw0 = PROBE_MAGIC_0;
    RIA.rw0 = PROBE_MAGIC_1;
    RIA.rw0 = PROBE_STATE_PLAYING;
    RIA.rw0 = 0;                    /* level a */
    RIA.rw0 = UNIT_LOC_X[0];
    RIA.rw0 = UNIT_LOC_Y[0];
    RIA.rw0 = UNIT_HEALTH[0];
    RIA.rw0 = robots;
    RIA.rw0 = RANDOM;
    RIA.rw0 = frame_counter;
    RIA.rw0 = map_window_x;
    RIA.rw0 = map_window_y;
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
            plot_tile(addr, MAP[((unsigned)(map_window_y + ty) << 7)
                                + map_window_x + tx]);
        }
    }
}

/* The window follows the player, who stays at viewport cell (5,3). */
static void centre_window(void)
{
    map_window_x = UNIT_LOC_X[0] - 5;
    map_window_y = UNIT_LOC_Y[0] - 3;
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

static void read_keyboard(void)
{
    unsigned char i;
    RIA.addr0 = XR_KEYBOARD;
    RIA.step0 = 1;
    for (i = 0; i < sizeof keyboard; i++)
        keyboard[i] = RIA.rw0;
}

/* Ask the game logic to move the player one square. UNIT and MOVE_TYPE are how
 * these routines take their arguments, exactly as on the X16. */
static bool try_walk(void (*walk)(void))
{
    UNIT = 0;
    MOVE_TYPE = MOVE_WALK;
    walk();
    return MOVE_RESULT == 1;
}

int main(void)
{
    unsigned char last_vsync, repeat = 0;
    bool moved;

    load_tileset();
    load_level("ROM:level-a");

    video_init();
    sprites_init();
    load_bitmap("ROM:gamepic");
    clear_chars();

    RANDOM = 1;
    centre_window();
    draw_map_window();
    display_player_sprite();

    put_string(0, 27, "petscii robots rp6502", 5);
    put_string(0, 28, "ijkl or arrows to walk", 1);

    /* Publish state before announcing readiness, not after. A test that waits
     * for the console line then peeks the probe would otherwise be racing the
     * first pass through the loop -- which is exactly the flake CI found and a
     * local run did not. */
    update_probe();
    printf("BRINGUP OK level-a player %u,%u\n", UNIT_LOC_X[0], UNIT_LOC_Y[0]);

    last_vsync = RIA.vsync;
    for (;;) {
        /* vsync is a counter, not a flag: taking the delta means a frame that
         * overruns catches up instead of losing a tick, which the X16's
         * IRQ-driven clock could not do. */
        unsigned char v = RIA.vsync;
        if (v == last_vsync)
            continue;
        last_vsync = v;
        frame_counter++;
        update_probe();

        read_keyboard();
        if (keyboard[0] & KEY_NONE_DOWN) {
            repeat = 0;                 /* nothing held: the next press is instant */
            continue;
        }
        if (repeat) {
            repeat--;
            continue;
        }

        moved = false;
        if (key(KEY_L) || key(KEY_RIGHT)) {
            player_direction = FACE_RIGHT;
            moved = try_walk(REQUEST_WALK_RIGHT);
        } else if (key(KEY_J) || key(KEY_LEFT)) {
            player_direction = FACE_LEFT;
            moved = try_walk(REQUEST_WALK_LEFT);
        } else if (key(KEY_K) || key(KEY_DOWN)) {
            player_direction = FACE_DOWN;
            moved = try_walk(REQUEST_WALK_DOWN);
        } else if (key(KEY_I) || key(KEY_UP)) {
            player_direction = FACE_UP;
            moved = try_walk(REQUEST_WALK_UP);
        } else {
            continue;
        }

        repeat = 7;                     /* the X16's KEYTIMER repeat rate */
        if (moved) {
            if (++player_animate == 3)
                player_animate = 0;
            centre_window();
            draw_map_window();
        }
        /* Facing changes even when the way is blocked, as it does on the X16. */
        display_player_sprite();
    }
}
