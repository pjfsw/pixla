#ifndef TRACK_H_
#define TRACK_H_

#define MAX_TRACK_LENGTH 256

typedef struct {
    Sint8 note;
    Uint8 patch;
    Uint16 command;
} Note;

typedef struct {
    Note notes[MAX_TRACK_LENGTH];
    Uint8 length;
} Track;


#endif /* TRACK_H_ */
