#include "song.h"
#include "pattern.h"

void song_clear(Song *song) {
    for (int pattern = 0; pattern < MAX_PATTERNS; pattern++) {
        pattern_clear(&song->patterns[pattern]);
        for (int i = 0; i < MAX_PATTERNS; i++) {
            song->arrangement[i].pattern = -1;
        }
    }
    song->arrangement[0].pattern = 0;
}
