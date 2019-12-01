#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "instrument.h"

#define MAX_INSTRUMENTS 256

typedef struct _Synth Synth;

typedef void (*SoundOutputHook)(void *userData, int channel, Sint16 sample);
/**
 * Initialize synth device with specified number of channels
 *
 * Set enablePlayback to true for soundcard playback, or false for
 * offline processing such as audio file generation
 */
Synth *synth_init(Uint8 channels, bool enablePlayback, SoundOutputHook soundOutputHook, void *userData);

int synth_getSampleRate(Synth *synth);

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

void synth_amplitudeModulation(Synth *synth, Uint8 channel, Uint8 frequency, Uint8 amplitude);

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

void synth_setGlobalVolume(Synth *synth, Uint8 volume);

void synth_setChannelVolume(Synth *synth, Uint8 channel, Uint8 volume);


/* Sint8* synth_getTable(Synth *synth); */

void synth_close(Synth *synth);

void synth_play(Synth *synth);

/**
 * Perform tests of synth functions and print out responses
 */
void synth_test();

/**
 * Generate audio out to the provided stream of len bytes. Should not be called
 * manually when synth is initialized with sound card output as it would
 * interfere with the audio output
 */
void synth_processBuffer(void* userdata, Uint8* stream, int len);

#endif /* SYNTH_H_ */
