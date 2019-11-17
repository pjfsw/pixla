#ifndef SCREEN_H_
#define SCREEN_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "track.h"
#include "trackermode.h"
#include "instrument.h"

/*
 * Initialize screen
 *
 * After screen_init() has been called, screen_close() must be called before
 * termination
 *
 * numerOfTracks The number of tracks to display on screen
 */
bool screen_init(Uint8 numberOfTracks);

/*
 * Close screen
 *
 */
void screen_close();

/*
 * Set pointers to column data to render
 *
 * track the track number, 0 to maximum number of columns-1
 * trackData a pointer to an array of tracks containing track data
 */
void screen_setTrackData(Uint8 track, Track *trackData);

void screen_setSelectedTrack(Uint8 track);

void screen_setSelectedColumn(Uint8 column);

void screen_setStepping(Uint8 stepping);

void screen_setOctave(Uint8 octave);

void screen_selectPatch(Uint8 patch, Instrument *instrument);

void screen_setRowOffset(Sint8 rowOffset);

void screen_setStatusMessage(char* msg);

void screen_setTrackermode(Trackermode trackermode);

void screen_setChannelMute(Uint8 track, bool mute);

void screen_setTableToShow(Sint8 *table, Uint8 elements);
/*
 * Update screen
 */
void screen_update();

#endif /* SCREEN_H_ */
