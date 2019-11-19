#ifndef PERSIST_H_
#define PERSIST_H_

#include <stdbool.h>
#include "song.h"

bool persist_loadSongWithName(Song *tracker, char *name);

bool persist_saveSongWithName(Song *song, char* name);

#endif /* PERSIST_H_ */
