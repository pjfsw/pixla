#ifndef AUDIORENDERER_H_
#define AUDIORENDERER_H_

#include "song.h"

typedef struct _AudioRenderer AudioRenderer;

/**
 * Initialize audio renderer
 */
AudioRenderer *audiorenderer_init(char *fileName);

/**
 * Render song to audio file
 */
void audiorenderer_renderSong(AudioRenderer *renderer, Song *song, Uint32 timeLimitInMs);

/**
 * Close renderer
 */
void audiorenderer_close(AudioRenderer *renderer);

#endif /* AUDIORENDERER_H_ */
