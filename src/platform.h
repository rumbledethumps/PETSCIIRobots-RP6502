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
void __fastcall__ plat_search_dot(unsigned char n);
void plat_scroll_info(void);
void plat_display_weapon(void);
void plat_display_keys(void);
void plat_display_player_sprite(void);
void plat_reverse_tile(void);
void plat_sprites_init(void);
void plat_gamepad_read(void);

void update_probe(void);

extern unsigned char tile_cells[256 * 3 * 6];

#endif
