#include <SDL2/SDL.h>
#include <stdio.h>

#include "synth.h"
#include "frequency_table.h"

typedef enum {
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    OFF
} Adsr;

Uint16 additiveFilter;

typedef struct _Channel Channel;

/*
 * Function to create a byte in the waveform
 */
typedef Sint8 (*GenerateWaveformFunc)(Uint8 offset);

/*
 * Function to generate a sample for the current time period and channel
 */
typedef Sint8 (*GetSampleFunc)(Channel *channel);

typedef struct _Channel {
    Uint16 voiceFreq;
    Uint16 wavePos;
    Adsr adsr;
    Sint16 amplitude;
    Sint8 *wave;
    GetSampleFunc sampleFunc;
    Sint8 param1;
    Sint8 pwm;
    Uint16 dutyCycle;
    Sint8 attack;
    Sint8 decay;
    Sint8 sustain;
    Sint8 release;
} Channel;

typedef struct {
    Uint16 sampleFreq;
    Sint8 lowpassSaw[256];
    Sint8 lowpassPulse[256];
    Sint16 adTable[128];
    Sint16 releaseTable[128];
    SDL_AudioDeviceID audio;
    Channel* channelData;
    Uint8 channels;
} Synth;

Synth *synth = NULL;

#define SINE_TABLE_BITSIZE 12
#define SINE_TABLE_SIZE (1 << SINE_TABLE_BITSIZE)
#define ADDITIVE_SHIFT (16-SINE_TABLE_BITSIZE)

Sint8 sineTable[SINE_TABLE_SIZE];

void _synth_updateAdsr(Channel *ch) {
    switch(ch->adsr) {
    case ATTACK:
        if (ch->attack == 0) {
            ch->amplitude = 32767;
            ch->adsr = DECAY;
        } else {
            if (ch->amplitude == 0) {
                ch->amplitude = 1;
            }
            ch->amplitude += synth->adTable[ch->attack];
            if (ch->amplitude < 0) {
                ch->amplitude = 32767;
                ch->adsr = DECAY;
            }
        }
        break;
    case DECAY:
        if (ch->decay == 0) {
            ch->amplitude = ch->sustain << 8;
            ch->adsr = SUSTAIN;
        } else {
            ch->amplitude -= synth->adTable[ch->decay];
            if (ch->amplitude < (ch->sustain << 8)) {
                ch->amplitude = ch->sustain << 8;
                ch->adsr = SUSTAIN;
            }
        }
        break;
    case RELEASE:
        if (ch->release == 0) {
            ch->amplitude = 0;
        } else if (ch->amplitude > 0) {
            ch->amplitude -= synth->releaseTable[ch->release];
            if (ch->amplitude < 0) {
                ch->adsr = OFF;
            }
        }
        break;
    case OFF:
    case SUSTAIN:
        break;
    }
}

Sint8 _synth_getSampleFromArray(Channel *ch) {
    Sint8 sample = ch->wave[ch->wavePos >> 8];
    return sample;
}

Sint8 _synth_getPulseAtPos(Uint16 dutyCycle, Uint16 wavePos) {
    return wavePos > dutyCycle ? 127 : -128;
}

Sint8 _synth_getPulse(Channel *ch) {
    Uint16 dutyCycle = ch->dutyCycle;
    Uint16 wavePos = ch->wavePos;
    Sint16 sample = 0;
    /*for (int i = 0; i < 2; i++) {
        sample += _synth_getPulseAtPos(dutyCycle, wavePos+i);
    }
    return sample/2;*/
    return _synth_getPulseAtPos(dutyCycle, wavePos);
}

Sint8 _synth_getNoise(Channel *ch) {
    return rand() >> 24;
}

Sint8 _synth_getAdditivePulse(Channel *ch) {
    Sint16 sample = 0;


    for (int i = 0; i < ch->param1; i++) {
        int harmonic = i*2+1;
        int amplitude = 256/harmonic;

        Uint16 harmonicWavePos = harmonic * ch->wavePos;

        sample +=  amplitude * sineTable[(harmonicWavePos >> ADDITIVE_SHIFT) % SINE_TABLE_SIZE] / 256;
    }
    return sample;
}

void _synth_processBuffer(void* userdata, Uint8* stream, int len) {
    Synth *synth = (Synth*)userdata;
    Sint8 *buffer = (Sint8*)stream;

    additiveFilter++;

    // 256 = full scale, 128 = half scale etc
    Sint8 scaler =  (1 << (8 - synth->channels));

    for (int i = 0; i < 256; i++) {
        buffer[i] = 0;

        for (int j = 0; j < synth->channels; j++) {
            Channel *ch = &synth->channelData[j];

            if (0 == i % 8) {
                _synth_updateAdsr(ch);
                if (ch->pwm > 0) {
                    ch->dutyCycle+=ch->pwm;
                }
            }


            if (ch->adsr != OFF) {
                Sint16 sample = ch->sampleFunc(ch) * (ch->amplitude)/32768;
                buffer[i] += sample * scaler / 256;
            }

            ch->wavePos += (65536 * ch->voiceFreq) / synth->sampleFreq;
        }

    }
}

Sint8 getSquareAmplitude(Uint8 offset) {
    return (offset > 50) ? 127 : -128;
}

Sint8 getSawAmplitude(Uint8 offset) {
    return offset-128;
}

void createFilteredBuffer(GenerateWaveformFunc sampleFunc, Sint8* output, int filter) {
    for (int i = 0; i < 256; i++) {
        Sint16 value = 0;
        if (filter == 0) {
            value = sampleFunc(i);
        } else {
            for (int j = 0; j < filter; j++) {
                value += sampleFunc((i+j)%256);
            }
            value /= filter;
        }
        output[i] = value;
    }
}


void _synth_initAudioTables(Synth *synth) {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        sineTable[i] = 127 * sin(i * 6.283185 / SINE_TABLE_SIZE);
    }

    createFilteredBuffer(getSquareAmplitude, synth->lowpassPulse, 16);
    createFilteredBuffer(getSawAmplitude, synth->lowpassSaw, 4);

    for (int i = 0; i < 128; i++) {
        synth->adTable[i] =  4096/(2*i+1);
        synth->releaseTable[i] = 256/(i+1);
    }
}

void _synth_initChannels(Synth *synth) {
    for (int i = 0; i < synth->channels; i++) {
        synth->channelData[i].amplitude = 0;
        synth->channelData[i].adsr = OFF;
        synth->channelData[i].wave = synth->lowpassPulse;
        synth->channelData[i].sampleFunc = _synth_getSampleFromArray;
    }
}

bool synth_init(Uint8 channels) {
    if (channels < 1) {
        fprintf(stderr, "Cannot set 0 channels\n");
        return false;
    }
    synth = calloc(1, sizeof(Synth));
    synth->sampleFreq = 48000;
    synth->channels = channels;
    synth->channelData = calloc(channels, sizeof(Channel));

    SDL_AudioSpec want;
    SDL_AudioSpec have;

    srand(time(NULL));

    SDL_memset(&want, 0, sizeof(want));
    want.freq = synth->sampleFreq; // Playback frequency on Sound card. Each sample takes worth 1/24000 second
    want.format = AUDIO_S8; // 8-bit unsigned samples
    want.channels = 1; // Only play mono for simplicity = 1 byte = 1 sample
    want.samples = 256; // Buffer size.
    want.callback = _synth_processBuffer; // Called whenever the sound card needs more data
    want.userdata = synth;

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    synth->audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (synth->audio == 0) {
        fprintf(stderr, "Failed to open audio due to %s\n", SDL_GetError());
        return false;
    } else {
        fprintf(stderr, "Freq %d channels %d format %04x\n", have.freq, have.channels, have.format);
    }
    if (have.format != want.format) { /* we let this one thing change. */
        SDL_Log("We didn't get our audio format.");
    }
    _synth_initAudioTables(synth);
    _synth_initChannels(synth);

    SDL_PauseAudioDevice(synth->audio, 0); /* start audio playing. */

    return true;
}

void synth_close() {
    if (NULL != synth) {
        if (synth->audio != 0) {
            SDL_CloseAudioDevice(synth->audio);
        }
        if (NULL != synth->channelData) {
            free(synth->channelData);
            synth->channelData = NULL;
        }
        free(synth);
        synth = NULL;
    }
}

typedef struct {
    Sint8 pitch;
} Note;

void synth_setPwm(Uint8 channel, Sint8 dutyCycle, Sint8 pwm) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    Channel *ch = &synth->channelData[channel];
    if (dutyCycle >= 0) {
        ch->dutyCycle = dutyCycle << 9;
    }
    ch->pwm = pwm;
}

void synth_setChannel(Uint8 channel, Sint8 attack, Sint8 decay, Sint8 sustain, Sint8 release, Waveform waveform) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }

    Channel *ch = &synth->channelData[channel];
    ch->attack = attack;
    ch->decay = decay;
    ch->sustain = sustain;
    ch->release = release;
    switch (waveform) {
    case LOWPASS_SAW:
        ch->wave = synth->lowpassSaw;
        ch->sampleFunc =  _synth_getSampleFromArray;
        break;
    case LOWPASS_PULSE:
        ch->wave = synth->lowpassPulse;
        ch->sampleFunc =  _synth_getSampleFromArray;
        break;

    case ADDITIVE_PULSE:
        ch->param1 = 20;
        ch->sampleFunc = _synth_getAdditivePulse;
        break;
    case PWM:
        ch->dutyCycle = 64 << 9;
        ch->pwm = 2;
        ch->sampleFunc = _synth_getPulse;
        break;
    case NOISE:
        ch->sampleFunc = _synth_getNoise;
        break;

    }
}

void synth_notePitch(Uint8 channel, Sint8 note) {
    synth->channelData[channel].voiceFreq = frequencyTable[(note) % (sizeof(frequencyTable)/sizeof(Uint16))];
}

void synth_noteTrigger(Uint8 channel, Sint8 note) {
    synth->channelData[channel].adsr = ATTACK;
    synth->channelData[channel].amplitude = 0;
    synth_notePitch(channel, note);
}

void synth_noteOff(Uint8 channel) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].adsr = RELEASE;
}

