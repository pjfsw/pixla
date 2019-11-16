#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

#define MAX_INSTRUMENTS 256

typedef struct _Synth Synth;

typedef enum {
    LOWPASS_SAW,
    LOWPASS_PULSE,
    NOISE,
    PWM
} Waveform;

typedef struct {
    Waveform waveform;
    Sint8 note;
    Uint16 length;
    Uint8 pwm;
    Uint8 dutyCycle;
} Wavesegment;

typedef struct {
    Sint8 attack;
    Sint8 decay;
    Sint8 sustain;
    Sint8 release;
    Wavesegment waves[3];
} Instrument;

typedef struct {
    Uint8 frequency;
    Uint8 amplitude;
} Modulation;

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

void synth_pitchOffset(Synth *synth, Uint8 channel, Sint8 offset);

void synth_noteOff(Synth *synth, Uint8 channel);

void synth_frequencyModulation(Synth *synth, Uint8 channel, Uint8 frequency, Uint8 amplitude);

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
