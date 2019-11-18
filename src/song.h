#ifndef SONG_H_
#define SONG_H_

#include "pattern.h"
#include "instrument.h"

#define MAX_PATTERNS 1000
#define MAX_INSTRUMENTS 256

typedef struct {
    Sint16 pattern;
} PatternPtr;

typedef struct {
    Pattern patterns[MAX_PATTERNS];
    Instrument instruments[MAX_INSTRUMENTS];
    PatternPtr arrangement[MAX_PATTERNS];
    int bpm;
} Song;

#endif /* SONG_H_ */
