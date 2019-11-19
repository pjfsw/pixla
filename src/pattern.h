#ifndef PATTERN_H_
#define PATTERN_H_

#include "track.h"

#define TRACKS_PER_PATTERN 4

typedef struct {
    Track tracks[TRACKS_PER_PATTERN];
} Pattern;

void pattern_clear(Pattern *pattern);


#endif /* PATTERN_H_ */
