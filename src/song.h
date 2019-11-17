#ifndef SONG_H_
#define SONG_H_

#include "track.h"

#define MAX_TRACKS 256

typedef struct {
    Track tracks[MAX_TRACKS];
    int bpm;
} Song;


#endif /* SONG_H_ */
