#ifndef PLAYER_H_
#define PLAYER_H_

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "synth.h"
#include "song.h"

typedef struct _Player Player;

Player *player_init(Synth *synth, Uint8 channels);

void player_close(Player *player);

/**
 *
 * Reset the player, preparing for playback. Will not start the song.
 */
void player_reset(Player *player, Song *song, Uint16 songPos);

/**
 * Start playing song using timer
 */
void player_play(Player *player);

bool player_isPlaying(Player *player);

Uint8 player_getCurrentRow(Player *player);

Uint8 player_getCurrentBpm(Player *player);

Uint16 player_getSongPos(Player *player);

void player_stop(Player *player);

/**
 * Process song data. Don't call manually if player_play() has been called
 */
Uint32 player_processSong(Uint32 interval, void *param);

/**
 * Returns true if an "end" marker has been reached, used for file rendering
 */
bool player_isEndReached();


#endif /* PLAYER_H_ */
