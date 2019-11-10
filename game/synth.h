#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>


typedef enum {
    LOWPASS_SAW,
    LOWPASS_PULSE,
    ADDITIVE_PULSE,
    NOISE,
    PWM
} Waveform;


bool synth_init(Uint8 channels);

void synth_setChannel(Uint8 channel, Sint8 attack, Sint8 decay, Sint8 sustain, Sint8 release, Waveform waveform);

void synth_setPwm(Uint8 channel, Sint8 dutyCycle, Sint8 pwm);

void synth_notePitch(Uint8 channel, Sint8 note);

void synth_noteOff(Uint8 channel);

void synth_noteTrigger(Uint8 channel, Sint8 note);

void synth_close();

void synth_play();

#endif /* SYNTH_H_ */
