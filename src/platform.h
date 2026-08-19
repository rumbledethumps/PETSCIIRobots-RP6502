#ifndef PETSCII_PLATFORM_H
#define PETSCII_PLATFORM_H

/* Called from src/game/platform_bridge.s. */
void plat_draw_map_window(void);
void __fastcall__ plat_play_sound(unsigned char effect);
void plat_print_info(void);
void plat_display_item(void);
void plat_display_player_health(void);
void plat_elevator_select(void);
void __fastcall__ plat_demat_palette(unsigned char value);

extern unsigned char tile_cells[256 * 3 * 6];

#endif
