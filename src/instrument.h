#ifndef INSTRUMENT_H_
#define INSTRUMENT_H_

#include <SDL2/SDL.h>

typedef enum {
    /**
     * Some kind of ugly filtered saw
     */
    LOWPASS_SAW=0,

    /**
     * Some kind of ugly filtered pulse
     */
    LOWPASS_PULSE=1,

    /**
     * White noise (sadly)
     */
    NOISE=2,

    /**
     * Pulse width modular waveform
     */
    PWM=3,

    /** Triangle wave form */
    TRIANGLE=4,

    /** Ring modulated wave form */
    RING_MOD=5
} Waveform;
#define WAVEFORM_TYPES 6

typedef struct {
    Waveform waveform;
    /**
      * Alteration or override of played note
      * 0 = No change
      * < 0 = Subtract played note with half note value (i.e. adding the absolute value to the note)
      * > 0 = Set played note to the specified value
     */
    Sint8 note;

    /**
     * Length of this wave segment.
     *
     * A value of 0 means this is the last segment
     */
    Uint16 length;

    /**
     * Pulse width modulation in this segment. Only relevant for PWM waveform
     */
    Sint8 pwm;

    /**
     * Start duty cycle in this segment. Only relevant for PWM waveform
     */
    Uint8 dutyCycle;

    /**
     * Lowpass filter value, 0-127, 127 - no filter, 0 - 100% filter
     */
    Sint8 filter;

    /** Relative volume, 0-127 */
    Sint8 volume;

    /** Ring modulation carrier frequency */
    Uint16 carrierFrequency;

} Wavesegment;

#define MAX_WAVESEGMENTS 3

typedef struct {
    /**
     * Time in some unit from amplitude 0 to max amplitude
     */
    Sint8 attack;

    /**
     * Time in some unit from max amplitude to sustain level
     */
    Sint8 decay;

    /**
     * Sustain level at which note is held while playing
     */
    Sint8 sustain;

    /**
     * Time in some unit from sustain level to 0, started when note has been released
     */
    Sint8 release;
    Wavesegment waves[MAX_WAVESEGMENTS];
} Instrument;

char *instrument_getWaveformName(Waveform waveform);

#endif /* INSTRUMENT_H_ */
