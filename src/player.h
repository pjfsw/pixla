#ifndef PLAYER_H_
#define PLAYER_H_

#include <SDL2/SDL.h>
#include "synth.h"
#include "song.h"

typedef struct _Player Player;

Player *player_init(Synth *synth, Uint8 channels);

void player_close(Player *player);

void player_start(Player *player, Song *song, Uint16 songPos);

bool player_isPlaying(Player *player);

Uint8 player_getCurrentRow(Player *player);

Uint8 player_getCurrentBpm(Player *player);

Uint16 player_getSongPos(Player *player);

void player_stop(Player *player);

#endif /* PLAYER_H_ */
