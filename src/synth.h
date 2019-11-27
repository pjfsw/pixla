#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "instrument.h"

#define MAX_INSTRUMENTS 256

typedef struct _Synth Synth;

Synth *synth_init(Uint8 channels);

/**
 * Load patch data into synth
 */
void synth_loadPatch(Synth *synth, Uint8 patch, Instrument *instrument);

/**
 *
 * \brief Set the Pulse Width Modulation setting of a channel
 *
 * hej
 *
 */
void synth_setPwm(Synth* synth, Uint8 channel, Sint8 dutyCycle, Sint8 pwm);

void synth_notePitch(Synth *synth, Uint8 channel, Uint8 patch, Sint8 note);

/** Create pitch modulation effects (arpeggio) */
void synth_pitchModulation(Synth *synth, Uint8 channel, Uint16 speed, Sint8 *notes, Uint8 notesLength);

void synth_noteOff(Synth *synth, Uint8 channel);

void synth_frequencyModulation(Synth *synth, Uint8 channel, Uint8 frequency, Uint8 amplitude);

/**
 * Start frequency gliding. Stop gliding by setting speed to 0 or call pitchGlideStop. Offset is reset with notePitch or noteTrigger
 */
void synth_pitchGlideUp(Synth *synth, Uint8 channel, Uint8 speed);

/**
 * Start frequency gliding. Stop gliding by setting speed to 0 or call pitchGlideStop. Offset is reset with notePitch or noteTrigger
 */
void synth_pitchGlideDown(Synth *synth, Uint8 channel, Uint8 speed);

/** Stop frequency gliding */
void synth_pitchGlideStop(Synth *synth, Uint8 channel);

void synth_pitchGlideReset(Synth *synth, Uint8 channel);

void synth_muteChannel(Synth *synth, Uint8 channel, bool mute);

bool synth_isChannelMuted(Synth *synth, Uint8 channel);

void synth_noteRelease(Synth *synth, Uint8 channel);

void synth_noteTrigger(Synth *synth, Uint8 channel, Uint8 patch, Sint8 note);

/* Sint8* synth_getTable(Synth *synth); */

void synth_close(Synth *synth);

void synth_play(Synth *synth);

/**
 * Perform tests of synth functions and print out responses
 */
void synth_test();

#endif /* SYNTH_H_ */
