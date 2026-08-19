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
void plat_green_screen(void);
void plat_decompress_screen(const unsigned char *src, unsigned char even_odd);
void plat_load_bitmap(const char *name);
void plat_display_intro_screen(void);
void plat_display_game_screen(void);
void plat_display_endgame_screen(void);
void __fastcall__ plat_flash_menu_option(unsigned char color);
void plat_display_map_name(void);
void plat_change_difficulty_level(void);
void plat_gamepad_read(void);

void update_probe(void);

extern unsigned char tile_cells[256 * 3 * 6];

#endif
