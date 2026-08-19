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

/* Wait for the next game tick, which the VSYNC interrupt raises. */
static unsigned char last_frame;

static void wait_tick(void)
{
    while (last_frame == IRQ_FRAME)
        ;
    last_frame = IRQ_FRAME;
    frame_counter++;
    /* Counted from the start of play, not from boot, so the checkpoints mean
     * the same thing however long someone sat on the menu. */
    if (++frames_total == 60 || frames_total == 300)
        printf("AI%u\n", frames_total);
}

/* The intro screen: pick a level, a difficulty and a control scheme, then
 * start. Four options at character rows 2 to 5; the selected one is flashed by
 * cycling its colour, which is all the X16 does with it too. */
static void intro_screen(void)
{
    unsigned char key;

    plat_display_intro_screen();
    plat_display_map_name();
    plat_change_difficulty_level();
    MENUY = 0;
    printf("MENU\n");

    for (;;) {
        wait_tick();
        update_probe();

        /* Walk the flash colours, as the X16's interrupt does every 7 frames. */
        if (!SPRITECOLTIMER) {
            SPRITECOLTIMER = 7;
            SPRITECOLSTATE = (unsigned char)((SPRITECOLSTATE + 1) & 7);
            plat_flash_menu_option(SPRITECOLCHART[SPRITECOLSTATE]);
        }
        SPRITECOLTIMER--;

        key = plat_getin();
        if (!key)
            continue;

        if (key == 0x91 || key == KEY_MOVE_UP[0]) {             /* up */
            if (MENUY) {
                plat_flash_menu_option(5);                      /* deselect */
                MENUY--;
                plat_play_sound(SFX_BEEP2);
            }
        } else if (key == 0x11 || key == KEY_MOVE_UP[1]) {      /* down */
            if (MENUY < 3) {
                plat_flash_menu_option(5);
                MENUY++;
                plat_play_sound(SFX_BEEP2);
            }
        } else if (key == 32 || key == 13) {                    /* space, return */
            plat_play_sound(SFX_BEEP);
            switch (MENUY) {
            case 0:
                return;                                         /* start */
            case 1:
                if (++SELECTED_MAP == 14)
                    SELECTED_MAP = 0;
                plat_display_map_name();
                break;
            case 2:
                if (++DIFF_LEVEL == 3)
                    DIFF_LEVEL = 0;
                plat_change_difficulty_level();
                break;
            case 3:
                if (++CONTROL == 3)
                    CONTROL = 0;
                break;
            }
        }
    }
}

/* INIT_GAME. Nothing starts in inventory; everything is found by searching. */
static void init_game(void)
{
    char name[16];
    unsigned char i;

    KEYS = AMMO_PISTOL = AMMO_PLASMA = 0;
    INV_BOMBS = INV_EMP = INV_MEDKIT = INV_MAGNET = 0;
    SELECTED_WEAPON = SELECTED_ITEM = 0;
    MAGNET_ACT = PLASMA_ACT = BIG_EXP_ACT = 0;
    CYCLES = SECONDS = MINUTES = HOURS = 0;

    plat_display_game_screen();

    /* "ROM:level-a" .. "ROM:level-n". The X16 patches the same last byte of
     * its filename; here the name is short enough to just build. */
    for (i = 0; i < 10; i++)
        name[i] = "ROM:level-"[i];
    name[10] = (char)('a' + SELECTED_MAP);
    name[11] = 0;
    slurp(name, UNIT_TYPE, LEVEL_BYTES);

    RANDOM = 1;
    PLAYER_DIRECTION = FACE_DOWN;
    PLAYER_ANIMATE = 0;
    UNIT_TYPE[0] = 1;

    /* SET_INITIAL_TIMERS: stagger them so the AI does not run every robot on
     * the same tick. */
    CLOCK_ACTIVE = 1;
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

    /* PRINT_INTRO_MESSAGE. */
    SOURCE = INTRO_MESSAGE;
    plat_print_info();
}

int main(void)
{
    unsigned char i, key;

    load_tileset();

    video_init();
    plat_sprites_init();
    input_init();
    clear_chars();

    for (i = 0; i < 13; i++)
        KEY_MOVE_UP[i] = STANDARD_CONTROLS[i];
    CONTROL = 0;
    DIFF_LEVEL = 1;
    SELECTED_MAP = 0;
    last_frame = IRQ_FRAME;

    intro_screen();
    init_game();
    frames_total = 0;

    update_probe();
    printf("BRINGUP OK level-a player %u,%u\n", UNIT_LOC_X[0], UNIT_LOC_Y[0]);

    for (;;) {
        wait_tick();
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
