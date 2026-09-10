/* save.h - Pengu flash high score table */
#ifndef SAVE_H
#define SAVE_H
#include "game.h"

void load_high_scores(void);
void save_high_scores(void);
void insert_high_score(u16 s);
u8   is_high_score(u16 s);

#endif
