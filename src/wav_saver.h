#ifndef WAV_SAVER_H_
#define WAV_SAVER_H_

#include <SDL2/SDL.h>

typedef struct _WavSaver WavSaver;

WavSaver *wavSaver_init(char *filename, Uint32 sampleRate);

void wavSaver_consume(WavSaver *wavSaver, Sint16 *samples, int length);

void wavSaver_close(WavSaver *wavSaver);

#endif /* WAV_SAVER_H_ */
