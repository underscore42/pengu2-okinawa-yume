/* orca.h - Pengu: the Konami whack-a-orca bonus round */
#ifndef ORCA_H
#define ORCA_H
#include "game.h"

void orca_reset(void);
void update_orca(void);
void draw_orca(void);
void hide_orca(void);
u8   resolve_hole(u8 cx, u8 cy);

#endif
