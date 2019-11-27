#include "instrument.h"

char *instrument_getWaveformName(Waveform waveform) {
    switch (waveform) {
    case PWM:
        return "PWM";
    case LOWPASS_PULSE:
        return "Pulse";
    case LOWPASS_SAW:
        return "Saw";
    case NOISE:
        return "Noise";
    case TRIANGLE:
        return "Tria";
    case RING_MOD:
        return "Ring";
    }
    return "";
}
