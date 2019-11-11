#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct _Synth Synth;

typedef enum {
    LOWPASS_SAW,
    LOWPASS_PULSE,
    ADDITIVE_PULSE,
    NOISE,
    PWM
} Waveform;


Synth *synth_init(Uint8 channels);

void synth_setChannel(Synth *synth, Uint8 channel, Sint8 attack, Sint8 decay, Sint8 sustain, Sint8 release, Waveform waveform);

/*
 * Set the Pulse Width Modulation setting of a channel
 */
void synth_setPwm(Synth* synth, Uint8 channel, Sint8 dutyCycle, Sint8 pwm);

void synth_notePitch(Synth *synth, Uint8 channel, Sint8 note);

void synth_noteOff(Synth *synth, Uint8 channel);

void synth_noteTrigger(Synth *synth, Uint8 channel, Sint8 note);

Sint8* synth_getTable(Synth *synth);

void synth_close(Synth *synth);

void synth_play(Synth *synth);

#endif /* SYNTH_H_ */
