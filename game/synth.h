#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct _Synth Synth;

typedef enum {
    LOWPASS_SAW,
    LOWPASS_PULSE,
    NOISE,
    PWM
} Waveform;


Synth *synth_init(Uint8 channels);

/**
 * Initialise channel with ADSR and waveform parameters
 */
void synth_setChannel(Synth *synth, Uint8 channel, Sint8 attack, Sint8 decay, Sint8 sustain, Sint8 release, Waveform waveform);

/**
 *
 * \brief Set the Pulse Width Modulation setting of a channel
 *
 * hej
 *
 */
void synth_setPwm(Synth* synth, Uint8 channel, Sint8 dutyCycle, Sint8 pwm);

void synth_notePitch(Synth *synth, Uint8 channel, Sint8 note);

void synth_noteOff(Synth *synth, Uint8 channel);

void synth_noteRelease(Synth *synth, Uint8 channel);

void synth_noteTrigger(Synth *synth, Uint8 channel, Sint8 note);

/* Sint8* synth_getTable(Synth *synth); */

void synth_close(Synth *synth);

void synth_play(Synth *synth);

/**
 * Perform tests of synth functions and print out responses
 */
void synth_test();

#endif /* SYNTH_H_ */
