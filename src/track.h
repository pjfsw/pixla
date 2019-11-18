#ifndef TRACK_H_
#define TRACK_H_

#define TRACK_LENGTH 64

typedef struct {
    Sint8 note;
    Uint8 patch;
    Uint16 command;
} Note;

typedef struct {
    Note notes[TRACK_LENGTH];
} Track;


#endif /* TRACK_H_ */
