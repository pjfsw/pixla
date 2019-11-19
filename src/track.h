#ifndef TRACK_H_
#define TRACK_H_

#define TRACK_LENGTH 64

#include <SDL2/SDL.h>

typedef struct {
    Sint8 note;
    Uint8 patch;
    Uint16 command;
} Note;

typedef struct {
    Note notes[TRACK_LENGTH];
} Track;

void track_clear(Track *track);


#endif /* TRACK_H_ */
