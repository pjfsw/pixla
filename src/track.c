#include "track.h"
#include "note.h"

void track_clear(Track *track) {
    for (int row = 0; row < TRACK_LENGTH; row++) {
        track->notes[row].note = NOTE_NONE;
        track->notes[row].patch = 0;
        track->notes[row].command = 0;
    }
}
