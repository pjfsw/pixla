#ifndef PERSIST_H_
#define PERSIST_H_

#include <stdbool.h>
#include "song.h"

bool persist_loadSongWithName(Song *song, char *name);

bool persist_saveSongWithName(Song *song, char* name);

#endif /* PERSIST_H_ */
