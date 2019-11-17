#ifndef INSTRUMENT_H_
#define INSTRUMENT_H_

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


#endif /* INSTRUMENT_H_ */
