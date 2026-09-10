/* screen.h - Pengu: palettes, playfield, HUD, front end */
#ifndef SCREEN_H
#define SCREEN_H
#include "game.h"

void setup_palettes(void);
void apply_theme(u8 lv);
void pulse_egg_palette(void);
void anim_title(void);
void draw_scores(void);
void show_banner(const u8 *codes);
void wipe_screen(void);
void draw_cell(u8 cx, u8 cy);
void draw_playfield(void);
void draw_canopy(void);
void set_canopy_front(u8 on);
void sway_canopy(void);
u16  frame_tile(u8 tx, u8 ty);
void init_hud(void);
void fill_field(void);
void wipe_house(void);
void init_bonus_hud(void);
void draw_bonus_hud(void);
void draw_hud(void);
void draw_title(void);
void clear_all_sprites(void);
void sprite_kana(u8 slot, u8 x, u8 y, const u8 *codes);
void clear_sprite_kana(void);
void show_pause(u8 on);

#endif
