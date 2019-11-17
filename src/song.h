#ifndef SONG_H_
#define SONG_H_

#include "track.h"

#define MAX_TRACKS 256
#define MAX_INSTRUMENTS 256

typedef struct {
    Track tracks[MAX_TRACKS];
    Instrument instruments[MAX_INSTRUMENTS];
    int bpm;
} Song;


#endif /* SONG_H_ */
