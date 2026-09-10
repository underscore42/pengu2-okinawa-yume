/* sound.h - Pengu sound effects */
#ifndef SOUND_H
#define SOUND_H
#include "game.h"

#define SND_PUSH   1
#define SND_CRUSH  2
#define SND_LAND   3
#define SND_SQUASH 4
#define SND_HATCH  5
#define SND_STUN   6
#define SND_THUD   7
#define SND_STOMP  8
#define SND_DIE    9
#define SND_CLEAR 10
#define SND_START 11
#define SND_KICK  12
#define SND_BLIP  13

void sound_init(void);

#endif
