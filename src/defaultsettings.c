#include <string.h>

#include "defaultsettings.h"
#include "instrument.h"

void defaultsettings_createInstruments(Instrument *instruments) {
    Instrument instr1 = {
            .attack = 0,
            .decay = 4,
            .sustain = 50,
            .release = 60,
            .waves = {
                    {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0,
                            .pwm = 5,
                            .dutyCycle = 108,
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr2 = {
            .attack = 0,
            .decay = 2,
            .sustain = 20,
            .release = 60,
            .waves = {
                    {
                            .waveform = LOWPASS_PULSE,
                            .note = 0,
                            .length = 0
                    },{
                            .waveform = LOWPASS_PULSE,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = LOWPASS_PULSE,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr3 = {
            .attack = 0,
            .decay = 20,
            .sustain = 100,
            .release = 20,
            .waves = {
                    {
                            .waveform = NOISE,
                            .note = 0,
                            .length = 10
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 30,
                            .pwm = 0,
                            .dutyCycle = 128
                    }, {
                            .waveform = NOISE,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr4 = {
            .attack = 0,
            .decay = 1,
            .sustain = 60,
            .release = 40,
            .waves = {
                    {
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    },{
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    }
            }
    };


    Instrument instr5 = {
            .attack = 0,
            .decay = 4,
            .sustain = 64,
            .release = 10,
            .waves = {
                    {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0,
                            .pwm = 20,
                            .dutyCycle = 18,
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr6 = {
            .attack = 120,
            .decay = 0,
            .sustain = 60,
            .release = 30,
            .waves = {
                    {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0,
                            .pwm = 2,
                            .dutyCycle = 28,
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }
            }
    };


    Instrument instr7 = {
            .attack = 0,
            .decay = 10,
            .sustain = 40,
            .release = 30,
            .waves = {
                    {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0,
                            .pwm = 2,
                            .dutyCycle = 28,
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr8 = {
            .attack = 0,
            .decay = 2,
            .sustain = 0,
            .release = 0,
            .waves = {
                    {
                            .waveform = NOISE,
                            .note = 0,
                            .length = 10,
                    },{
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = NOISE,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr9 = {
            .attack = 2,
            .decay = 4,
            .sustain = 74,
            .release = 10,
            .waves = {
                    {
                            .waveform = TRIANGLE,
                            .note = 0,
                            .length = 0,
                            .pwm = 0,
                            .dutyCycle = 18,
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }
            }
    };


    memcpy(&instruments[1], &instr1, sizeof(Instrument));
    memcpy(&instruments[2], &instr2, sizeof(Instrument));
    memcpy(&instruments[3], &instr3, sizeof(Instrument));
    memcpy(&instruments[4], &instr4, sizeof(Instrument));
    memcpy(&instruments[5], &instr5, sizeof(Instrument));
    memcpy(&instruments[6], &instr6, sizeof(Instrument));
    memcpy(&instruments[7], &instr7, sizeof(Instrument));
    memcpy(&instruments[8], &instr8, sizeof(Instrument));
    memcpy(&instruments[9], &instr9, sizeof(Instrument));
}
