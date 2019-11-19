#include "SDL2/SDL.h"
#include "pattern.h"
#include "track.h"

void pattern_clear(Pattern *pattern) {
    for (int track = 0; track < TRACKS_PER_PATTERN; track++) {
        track_clear(&pattern->tracks[track]);
    }
}
