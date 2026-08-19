/* PETSCII Robots for the RP6502.
 *
 * The presentation and I/O are new; the game logic under src/game/ is David
 * Murray's assembly converted to ca65, so movement, collision, item use and
 * every robot behave exactly as they do on the X16 rather than approximately.
 */
#include <fcntl.h>
#include <rp6502.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "game/game.h"
#include "input.h"
#include "platform.h"
#include "probe.h"
#include "xram.h"

static unsigned char frame_counter;

/* Frames since the game loop started, as opposed to frame_counter which wraps
 * at 256 for the probe. Only used to announce two checkpoints on the console.
 *
 * Tests need a reference point that does not depend on how long booting took.
 * Loading the assets goes through the host filesystem, and that is not the same
 * number of frames on every machine. Announcing a frame number gives the
 * harness something to synchronise on that the game's own clock defines. */
static unsigned frames_total;

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

static void load_tileset(void)
{
    int fd = open("ROM:tiles", O_RDONLY);
    if (fd < 0)
        die("ROM:tiles");
    slurp_fd(fd, tile_cells, 256 * 3 * 6);
    slurp_fd(fd, DESTRUCT_PATH, 256);
    slurp_fd(fd, TILE_ATTRIB, 256);
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

    /* Tick the game once a frame. Writing the register sets the enable mask and
     * clears anything already triggered; src/game/irq.s reads it to acknowledge,
     * since reading returns the triggered bits and clears them. cc65's runtime
     * owns the $FFFE vector and walks the interruptor chain. */
    RIA.irq = 0x80;
}

/* The character plane starts fully transparent. The console rows carry a colour
 * from the outset because PRINT_INFO writes glyphs only, exactly as the X16's
 * GREEN_SCREEN left them. */
static void clear_chars(void)
{
    unsigned i;
    RIA.addr0 = XR_CHARS;
    RIA.step0 = 1;
    for (i = 0; i < SCR_COLS * SCR_ROWS; i++) {
        RIA.rw0 = 32;                                   /* PETSCII space */
        RIA.rw0 = (i >= SCR_COLS * 27) ? 5 : 0;         /* green console */
    }
}

void update_probe(void)
{
    unsigned char i, robots = 0, visible = 0;
    unsigned sum = 0;
    for (i = 1; i < 28; i++) {
        if (UNIT_TYPE[i])
            robots++;
        /* Weighted so a robot swapping places with another still shows up. */
        sum += (unsigned)UNIT_TYPE[i] * 7u
             + (unsigned)UNIT_LOC_X[i] * 31u
             + (unsigned)UNIT_LOC_Y[i] * 131u;
    }
    for (i = 0; i < 77; i++)
        if (MAP_PRECALC[i])
            visible++;

    RIA.addr0 = XR_PROBE;
    RIA.step0 = 1;
    RIA.rw0 = PROBE_MAGIC_0;
    RIA.rw0 = PROBE_MAGIC_1;
    RIA.rw0 = UNIT_TYPE[0] == 1 ? PROBE_STATE_PLAYING : PROBE_STATE_DEAD;
    RIA.rw0 = 0;                    /* level a */
    RIA.rw0 = UNIT_LOC_X[0];
    RIA.rw0 = UNIT_LOC_Y[0];
    RIA.rw0 = UNIT_HEALTH[0];
    RIA.rw0 = robots;
    RIA.rw0 = RANDOM;
    RIA.rw0 = frame_counter;
    RIA.rw0 = MAP_WINDOW_X;
    RIA.rw0 = MAP_WINDOW_Y;
    RIA.rw0 = sum & 0xFF;
    RIA.rw0 = sum >> 8;
    RIA.rw0 = visible;
    RIA.rw0 = (unsigned char)(SELECTED_WEAPON | (SELECTED_ITEM << 4));
}

/* Animate the player one step and redraw him, as ANIMATE_PLAYER does. */
static void animate_player(void)
{
    if (++PLAYER_ANIMATE == 3)
        PLAYER_ANIMATE = 0;
    plat_display_player_sprite();
}

static void walk(unsigned char facing, void (*request)(void))
{
    PLAYER_DIRECTION = facing;
    UNIT = 0;
    MOVE_TYPE = MOVE_WALK;
    request();
    if (MOVE_RESULT == 1) {
        animate_player();
        CACULATE_AND_REDRAW();
    } else {
        /* Facing changes even when the way is blocked, as it does on the X16. */
        plat_display_player_sprite();
    }
}

/* CYCLE_WEAPON and CYCLE_ITEM: step to the next one and let the display
 * routine's preselect rules sort out an empty slot. */
static void cycle_weapon(void)
{
    plat_play_sound(SFX_CYCLEWEAPON);
    SELECTED_WEAPON = (unsigned char)(SELECTED_WEAPON == 1 ? 2 : 1);
    plat_display_weapon();
}

static void cycle_item(void)
{
    plat_play_sound(SFX_CYCLEITEM);
    SELECTED_ITEM = (unsigned char)(SELECTED_ITEM >= 4 ? 1 : SELECTED_ITEM + 1);
    plat_display_item();
}

int main(void)
{
    unsigned char i, key, last_frame;

    load_tileset();
    slurp("ROM:level-a", UNIT_TYPE, LEVEL_BYTES);

    video_init();
    plat_sprites_init();
    input_init();
    load_bitmap("ROM:gamepic");
    clear_chars();

    /* INIT_GAME: nothing in inventory, everything is found by searching. */
    for (i = 0; i < 13; i++)
        KEY_MOVE_UP[i] = STANDARD_CONTROLS[i];
    CONTROL = 0;
    RANDOM = 1;
    PLAYER_DIRECTION = FACE_DOWN;
    PLAYER_ANIMATE = 0;
    UNIT_TYPE[0] = 1;

    /* Stagger the unit timers so the AI does not run every robot on the same
     * frame, exactly as SET_INITIAL_TIMERS does. */
    for (i = 1; i < 48; i++) {
        UNIT_TIMER_A[i] = i;
        UNIT_TIMER_B[i] = 0;
    }

    CACULATE_AND_REDRAW();
    plat_draw_map_window();
    plat_display_player_sprite();
    plat_display_player_health();
    plat_display_keys();
    plat_display_weapon();
    plat_display_item();

    /* PRINT_INTRO_MESSAGE: the welcome line, on the console at start. */
    SOURCE = INTRO_MESSAGE;
    plat_print_info();
    update_probe();
    last_frame = IRQ_FRAME;
    printf("BRINGUP OK level-a player %u,%u\n", UNIT_LOC_X[0], UNIT_LOC_Y[0]);

    CLOCK_ACTIVE = 1;
    for (;;) {
        /* Once a frame, off the interrupt's counter. Everything that needs a
         * portal lives out here rather than in the handler. */
        while (last_frame == IRQ_FRAME)
            ;
        last_frame = IRQ_FRAME;
        frame_counter++;
        if (++frames_total == 60 || frames_total == 300)
            printf("AI%u\n", frames_total);
        update_probe();

        BACKGROUND_TASKS();

        if (UNIT_TYPE[0] != 1)
            continue;                   /* dead; M6 brings the game over screen */

        key = plat_getin();
        if (!key)
            continue;

        if (key == 0x1D || key == KEY_MOVE_UP[3])
            walk(FACE_RIGHT, REQUEST_WALK_RIGHT);
        else if (key == 0x9D || key == KEY_MOVE_UP[2])
            walk(FACE_LEFT, REQUEST_WALK_LEFT);
        else if (key == 0x11 || key == KEY_MOVE_UP[1])
            walk(FACE_DOWN, REQUEST_WALK_DOWN);
        else if (key == 0x91 || key == KEY_MOVE_UP[0])
            walk(FACE_UP, REQUEST_WALK_UP);
        else if (key == KEY_MOVE_UP[4])
            FIRE_UP();
        else if (key == KEY_MOVE_UP[5])
            FIRE_DOWN();
        else if (key == KEY_MOVE_UP[6])
            FIRE_LEFT();
        else if (key == KEY_MOVE_UP[7])
            FIRE_RIGHT();
        else if (key == KEY_MOVE_UP[8])
            cycle_weapon();
        else if (key == KEY_MOVE_UP[9])
            cycle_item();
        else if (key == KEY_MOVE_UP[10])
            USE_ITEM();
        else if (key == KEY_MOVE_UP[11])
            SEARCH_OBJECT();
        else if (key == KEY_MOVE_UP[12])
            MOVE_OBJECT();
    }
}
