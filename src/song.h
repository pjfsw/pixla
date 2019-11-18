#ifndef SONG_H_
#define SONG_H_

#include "pattern.h"

#define MAX_PATTERNS 999
#define MAX_INSTRUMENTS 256

typedef struct {
    Pattern patterns[MAX_PATTERNS];
    Instrument instruments[MAX_INSTRUMENTS];
    int bpm;
} Song;

#endif /* SONG_H_ */
