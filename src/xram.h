#ifndef PETSCII_XRAM_H
#define PETSCII_XRAM_H

/* The XRAM working set.
 *
 * Addresses are as the 6502 sees them through the RIA portals. rp6502_asset()
 * in CMakeLists.txt uses the same numbers with 0x10000 added, which is how the
 * ROM loader addresses XRAM.
 *
 * The bitmap plane is shared: the intro screen, the game screen and the game
 * over screen are the same 38400 bytes, loaded when each is shown. That is the
 * X16's PAYLOAD4/PAYLOAD5 split without the loader programs.
 */

#define XR_BITMAP       0x0000u   /* 38400  mode 3, 320x240 4bpp, 160 B/row   */
#define XR_CHARS        0x9600u   /*  2400  mode 1, 40x30 cells, 80 B/row     */
#define XR_FONT         0xA000u   /*  2048  256 glyphs x 8 rows, ROW-major    */
#define XR_SPR_PLAYER   0xA800u   /*  6656  13 frames x 32x32 4bpp            */
#define XR_SPR_CURSOR   0xC200u   /*  1536   3 frames                         */
#define XR_SPR_HUD      0xC800u   /*  6144  12 frames (6 icons split L/R)     */
#define XR_PAL_BITMAP   0xE000u   /*    32  entry 0 opaque black              */
#define XR_PAL_CHARS    0xE020u   /*    32  entry 0 transparent               */
#define XR_PAL_PLAYER   0xE040u   /*    32  entry 4 cycled by DEMATERIALIZE   */
#define XR_SPRITES      0xE060u   /*    48  6 x vga_mode5_sprite_t            */
#define XR_CFG_BITMAP   0xE090u   /*    14  vga_mode3_config_t (even address) */
#define XR_CFG_CHARS    0xE0A0u   /*    16  vga_mode1_config_t (even address) */
#define XR_KEYBOARD     0xE0B0u   /*    32  HID keycode bit array             */
#define XR_GAMEPAD      0xE0D0u   /*    40  4 pads x 10 bytes                 */
#define XR_PROBE        0xFE00u   /*    16  test ABI; see docs/test-abi.md    */
#define XR_PSG          0xFF00u   /*    64  8 oscillators; even, no page cross*/

/* Screen geometry. The character plane is 40 wide rather than something wider
 * with a power-of-two stride: unlike VERA, the RIA takes a full 16-bit addr0,
 * so stepping a row costs about three cycles more than incrementing a high
 * byte. A 128-wide plane would save ~460 cycles on a full 77-tile redraw and
 * cost 5280 bytes of XRAM, which is a bad trade. */
#define SCR_COLS        40
#define SCR_ROWS        30
#define SCR_STRIDE      (SCR_COLS * 2)          /* 2 bytes per cell, 4-bit colour */
#define CELL(col, row)  (XR_CHARS + (row) * SCR_STRIDE + (col) * 2)

/* The playfield: 11x7 game tiles, each a 3x3 block of characters, at character
 * columns 0-32 and rows 2-22. The player is always at viewport cell (5,3). */
#define MAP_WIN_TILES_W 11
#define MAP_WIN_TILES_H 7
#define MAP_WIN_COL     0
#define MAP_WIN_ROW     2

#define MAP_W           128
#define MAP_H           64

/* Sprites. Mode 5 draws a later index on top of an earlier one, the reverse of
 * VERA's priority, so this order is the X16's sprite numbering reversed. All
 * six are 32x32 4bpp, which is why one xreg_vga_mode(5, ...) call covers them:
 * mode 5 has no non-square size, so the X16's two 64x32 HUD icons are four
 * 32x32 sprites here. */
#define SPR_PLAYER      0   /* was VERA sprite 3 */
#define SPR_CURSOR      1   /* was VERA sprite 2 */
#define SPR_ITEM_L      2   /* was VERA sprite 1, left half  */
#define SPR_ITEM_R      3   /*                    right half */
#define SPR_WEAPON_L    4   /* was VERA sprite 0, left half  */
#define SPR_WEAPON_R    5
#define SPR_COUNT       6
#define SPR_FRAME_BYTES 512 /* 32 rows x 16 bytes at 4bpp */

/* Off-screen parks a sprite; mode 5 has no enable bit. */
#define SPR_PARKED_Y    240

/* The player never moves on screen: the window follows him. These are the X16's
 * own coordinates for sprite 3, from SPRITE_ATTRIB. */
#define PLAYER_SPR_X    122
#define PLAYER_SPR_Y    89

/* PLAYER_DIRECTION values, as the X16 uses them: the frame index into
 * spr_player.bin is direction + animate, and animate cycles 0..2. */
#define FACE_UP     0
#define FACE_DOWN   3
#define FACE_LEFT   6
#define FACE_RIGHT  9
#define FACE_DEAD  12

#endif
