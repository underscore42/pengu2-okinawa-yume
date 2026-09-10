/* snobee.h - Pengu: the Sno-Bees */
#ifndef SNOBEE_H
#define SNOBEE_H
#include "game.h"

void bees_reset(void);
u8   spawn_bee(u8 cx, u8 cy);
void hatch_one(void);
void spawn_from_surf(void);
void update_bees(void);
void draw_bees(void);
u8   kill_bees_in_cell(u8 cx, u8 cy);
u8   bees_alive(void);
void wall_stun(u8 d);

#endif
