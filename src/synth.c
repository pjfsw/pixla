
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


typedef struct _Channel Channel;

typedef struct _Synth Synth;

/*
 * Function to create a byte in the waveform
 */
typedef Sint8 (*GenerateWaveformFunc)(Uint8 offset);

/*
 * Function to generate a sample for the current time period and channel
 */
typedef Sint8 (*GetSampleFunc)(Synth *synth, Channel *channel);


#define SWIPE_OFFSET_SCALE 480000
#define SWIPE_LIMIT 4

typedef struct {
    Sint32 offset; // Offset in 1/100 halfnotes
    Uint8 speed;
    Sint8 direction;
} Swipe;

typedef struct {
    Uint8 frequency;
    Uint8 amplitude;
} Modulation;

typedef struct {
    Uint8 speed;
    Sint8 notes[256];
    Uint8 notesLength;
} PitchModulation;

#define FREQ_SWIPE_NOTE_SCALER 32

typedef struct {
    Uint16 wavePos;
    Sint8 *wave;
    GetSampleFunc sampleFunc;
    Uint16 dutyCycle;
    Uint16 carrierFrequency;
    Sint8 pwm;
    Uint8 currentSegment;
    Sint8 filter;
    Sint8 volume;
    Swipe swipe;
    Swipe ringModSwipe;
    Modulation frequencyModulation;
    Sint8 noteModulation;
} WaveData;


typedef struct {
    Sint16 amplitude;
    Uint16 adsrTimer;
    Adsr adsr;
} AmpData;
/*
 * Definition of an oscillator channel
 */
typedef struct _Channel {
    Uint32 playtime;
    Uint8 patch;
    Sint8 note;
    WaveData waveData;
    AmpData ampData;
    bool mute;
    Sint8 mean;
    PitchModulation pitchModulation;
} Channel;

/*
 * Definition of the synth
 */
typedef struct _Synth {
    FrequencyTable *frequencyTable;
    Uint16 sampleFreq;
    Sint8 lowpassSaw[256];
    Sint8 lowpassPulse[256];
    Uint8 attackTable[512];
    Uint8 decayReleaseTable[512];
    SDL_AudioDeviceID audio;
    Channel *channelData;
    Instrument *instruments;
    Uint8 channels;
    /** Store sine values between -32768 and 32767 */
    Sint16 sineTable[65536];
    /** 8192 represents 1/2 and 32768 represents 2. Mid index 32768 means 16384 aka 1 */
    Uint16 halfToDoubleModulationTable[65536];
    Uint32 clock;
} Synth;

#define SAMPLE_RATE 48000
#define SAMPLE_RATE_MS 48
#define MODULATION_SCALER 12
#define ADSR_PWM_PRESCALER 16
#define ADSR_MAX_TIME_IN_SECS 5

//Uint16 adsrTimescaler = (255 * 127 * ADSR_PWM_PRESCALER)/(ADSR_MAX_TIME_IN_SECS * SAMPLE_RATE) = 2.67
/** Scaled up two times */
#define ADSR_TIMESCALER_2 (511 * 127 * ADSR_PWM_PRESCALER)/(ADSR_MAX_TIME_IN_SECS * SAMPLE_RATE)


void _synth_updateAdsr(Synth *synth, Channel *ch) {
    Instrument *instr = &synth->instruments[ch->patch];
    AmpData *amp = &ch->ampData;

    switch(amp->adsr) {
    case ATTACK:
        if (instr->attack == 0) {
            amp->amplitude = 32767;
            amp->adsr = DECAY;
        } else {
            Uint16 tableIndex = ADSR_TIMESCALER_2 * amp->adsrTimer / instr->attack;
            if (tableIndex < 511) { // 511 = scaled up twice
                amp->amplitude = synth->attackTable[tableIndex] << 7;
                amp->adsrTimer++;
            } else {
                amp->amplitude = 32767;
                amp->adsr = DECAY;
                amp->adsrTimer = 0;
            }
        }
        break;
    case DECAY:
        if (instr->decay == 0) {
            amp->amplitude = instr->sustain << 8;
            amp->adsr = SUSTAIN;
        } else {
            Uint16 tableIndex = ADSR_TIMESCALER_2 * amp->adsrTimer / instr->decay;
            if (tableIndex < 511) { // 511 = scaled up twice
                amp->amplitude = (instr->sustain << 8) + ((127-instr->sustain) * synth->decayReleaseTable[tableIndex]);
                amp->adsrTimer++;
            } else {
                amp->amplitude = instr->sustain << 8;
                amp->adsr = SUSTAIN;
            }
        }
        break;
    case RELEASE:
        if (instr->release == 0) {
            amp->amplitude = 0;
            amp->adsr = OFF;
        } else if (amp->adsrTimer < 0xffff) {
            // Normalized time in seconds:
            // t in s = adsrPos * ADSR_PWM_PRESCALER / SAMPLE_RATE, 16/48000 ger t = 3000  = 1 s
            // t in micros = 16000/48 = 300

            // max_length_in_s =
            // index: i = f(t)/release
            // index: 255 = f(4)/127
            // 255 = C * 4 / 127
            // C = 255*127/4
            // C= 8096,   f(t) = 8096 * t

            // release = 127, t = 0:  8096 * 0 / 127 = 0
            // release = 127, t = 2:  8096 * 2 / 127 = 127
            // release = 63, t = 2: 8096 * 2 / 63 = 257
            // release = 1, t = 2: 8096 * 2 / 1 = 16384

            // 255= C * t / r =>  t = 255 * r / 8096
            //
            // r = 127 => t = 255 * 127 / 8096 = 4 s
            // r = 63 => t = 255 * 63 / 8096 = 1.98s
            // r = 1 => t = 255 / 8096 =

            // i = 8096 * t_s / 127
            // i = (8096 * t_micros / 127) / 1000000
            // i = 80

            // i = 8096 * (pos * prescaler / samplerate) * (1/r)
            //
            //  i

            Uint16 tableIndex = ADSR_TIMESCALER_2 * amp->adsrTimer / instr->release;
            if (tableIndex < 511) { // 511 = scaled up twice
                amp->amplitude = instr->sustain * synth->decayReleaseTable[tableIndex];
                amp->adsrTimer++;
            } else {
                amp->amplitude = 0;
                amp->adsr = OFF;
            }
        } else {
            amp->amplitude = 0;
            amp->adsr = OFF;
        }
        break;
    case OFF:
    case SUSTAIN:
        break;
    }
}


Uint32 _synth_getWaveFactor(FrequencyTable *ft) {
    return  65536 / frequencyTable_getScaleFactor(ft);
}

Uint32 _synth_getSwipedFrequency(
        FrequencyTable *frequencyTable,
        Uint32 scaledFrequency,
        Uint32 millis,
        Swipe *swipe
) {
    if (swipe->direction > 0) {
        swipe->offset+=swipe->speed;
        if (swipe->offset > SWIPE_OFFSET_SCALE * SWIPE_LIMIT) {
            swipe->offset = SWIPE_OFFSET_SCALE * SWIPE_LIMIT;
        }
    }
    if (swipe->direction < 0) {
        swipe->offset-=swipe->speed;
        if (swipe->offset < -SWIPE_OFFSET_SCALE * SWIPE_LIMIT) {
            swipe->offset = -SWIPE_OFFSET_SCALE * SWIPE_LIMIT;
        }
    }
    if (swipe->offset == 0) {
        return scaledFrequency;
    }

    Uint32 resultFreq = scaledFrequency * pow(2, (double)swipe->offset/(double)SWIPE_OFFSET_SCALE);
    if (resultFreq > frequencyTable_getHighestScaledFrequency(frequencyTable)) {
        resultFreq = frequencyTable_getHighestScaledFrequency(frequencyTable);
    } else if (resultFreq < frequencyTable_getLowestScaledFrequency(frequencyTable)) {
        resultFreq = frequencyTable_getLowestScaledFrequency(frequencyTable);
    }
    return resultFreq;

}

Uint32 _synth_getModulatedFrequency(
        Synth *synth,
        Uint32 scaledFrequency,
        Uint32 playtime,
        Modulation *frequencyModulation
) {

    if (frequencyModulation->amplitude == 0) {
        return scaledFrequency;

    }
    Sint16 modulationIndex =  synth->sineTable[(playtime * frequencyModulation->frequency / MODULATION_SCALER) & 0xFFFF];
    Sint16 scaledModulationIndex = frequencyModulation->amplitude * modulationIndex / 255;

    return scaledFrequency * synth->halfToDoubleModulationTable[scaledModulationIndex+32768] / 16384;
}

Uint32 _synth_getChannelFrequency(Synth *synth, Channel *ch) {
    FrequencyTable *ft = synth->frequencyTable;
    WaveData *wav = &ch->waveData;
    Sint8 note = ch->note;

    if (wav->noteModulation < 0) {
        note -= wav->noteModulation;
    } else if (wav->noteModulation > 0) {
        note = wav->noteModulation;
    }
    if (ch->pitchModulation.notesLength > 0 && ch->pitchModulation.speed > 0) {
        note += ch->pitchModulation.notes[(ch->playtime/(SAMPLE_RATE_MS * ch->pitchModulation.speed) )% ch->pitchModulation.notesLength];
    }

    Uint32 scaledFrequency = frequencyTable_getScaledValue(ft, note);

    scaledFrequency = _synth_getSwipedFrequency(
            ft,
            scaledFrequency,
            ch->playtime / SAMPLE_RATE_MS,
            &wav->swipe);

    scaledFrequency = _synth_getModulatedFrequency(
            synth,
            scaledFrequency,
            ch->playtime,
            &wav->frequencyModulation
            );
    return scaledFrequency;
}


Sint8 _synth_getSampleFromArray(Synth *synth, Channel *ch) {
    Sint8 sample = ch->waveData.wave[ch->waveData.wavePos >> 8];
    return sample;
}

Sint8 _synth_getPulseAtPos(Uint16 dutyCycle, Uint16 wavePos) {
    return wavePos > dutyCycle ? 127 : -128;
}

Sint8 _synth_getMean(Channel *ch, Sint8 real) {
    return (127-ch->waveData.filter) * real / 127 + ch->waveData.filter * ch->mean / 127;
}

Sint8 _synth_getPulse(Synth *synth, Channel *ch) {
    Uint16 dutyCycle = ch->waveData.dutyCycle;
    return  _synth_getPulseAtPos(dutyCycle, ch->waveData.wavePos);
}

Sint8 _synth_getNoise(Synth *synth, Channel *ch) {
    return rand() >> 24;
}

Sint8 _synth_getTriangle(Synth *synth, Channel *ch) {
    Uint16 offset = ch->waveData.wavePos - 16384;
    if (offset < 32768) {
        return (offset-16384)/128;
    } else {
        return (49151-offset)/128;
    }
}

Sint8 _synth_getRingModulation(Synth *synth, Channel *ch) {
    Sint32 modulation = 0;
    for (int i = 1; i < 12; i+=2) {
        modulation += synth->sineTable[(i * ch->waveData.wavePos) % 65536] / i;
    }
    Uint32 carrierOffset = 0;

    if (ch->waveData.carrierFrequency == 0) {
        carrierOffset = _synth_getWaveFactor(synth->frequencyTable) * ch->playtime * _synth_getChannelFrequency(synth, &synth->channelData[0]);
    } else {
        carrierOffset = 65536 * ch->playtime * ch->waveData.carrierFrequency;
    }
    carrierOffset = (carrierOffset/SAMPLE_RATE) % 65536;

    Sint16 carrierValue = synth->sineTable[carrierOffset];


    return  modulation * carrierValue / 10000000;
}

void _synth_updateWaveform(Synth *synth, Uint8 channel) {
    Channel *ch =  &synth->channelData[channel];
    Instrument *instr = &synth->instruments[ch->patch];
    WaveData *wav = &ch->waveData;

    Uint32 length = 0;
    Sint8 segment = 0;

    int ms = ch->playtime/SAMPLE_RATE_MS;
    for (int i = 0; i < MAX_WAVESEGMENTS; i++) {
        if (instr->waves[i].length == 0 || (ms < length + instr->waves[i].length)) {
            segment = i;
            break;
        }
        length += instr->waves[i].length;
    }
    if (segment == wav->currentSegment) {
        return;
    }
    wav->currentSegment = segment;
    Wavesegment *waveData = &instr->waves[segment];
    Waveform waveform = waveData->waveform;
    ch->waveData.pwm = waveData->pwm;
    if (waveData->dutyCycle > 0) {
        ch->waveData.dutyCycle = waveData->dutyCycle << 8;
    }
    ch->waveData.noteModulation = waveData->note;
    ch->waveData.filter = waveData->filter;
    ch->waveData.volume = waveData->volume == 0 ? 127 : waveData->volume;
    ch->waveData.carrierFrequency = waveData->carrierFrequency;

    switch (waveform) {
    case LOWPASS_SAW:
        wav->wave = synth->lowpassSaw;
        wav->sampleFunc =  _synth_getSampleFromArray;
        break;
    case LOWPASS_PULSE:
        wav->wave = synth->lowpassPulse;
        wav->sampleFunc =  _synth_getSampleFromArray;
        break;
    case PWM:
        wav->sampleFunc = _synth_getPulse;
        break;
    case NOISE:
        wav->sampleFunc = _synth_getNoise;
        break;
    case TRIANGLE:
        wav->sampleFunc = _synth_getTriangle;
        break;
    case RING_MOD:
        wav->sampleFunc = _synth_getRingModulation;
        break;

    }
}


void _synth_processBuffer(void* userdata, Uint8* stream, int len) {
    Synth *synth = (Synth*)userdata;
    Sint16 *buffer = (Sint16*)stream;
    FrequencyTable *ft = synth->frequencyTable;

    Sint16 scaler = 30000/synth->channels;

    Uint32 waveFactor = 65536 / frequencyTable_getScaleFactor(ft);

    for (int i = 0; i < len/2; i++) {
        Sint32 output = 0;

        for (int j = 0; j < synth->channels; j++) {
            Channel *ch = &synth->channelData[j];
            WaveData *wav = &ch->waveData;
            AmpData *amp = &ch->ampData;

            Uint32 scaledFrequency = _synth_getChannelFrequency(synth, ch);


            if (0 == i % ADSR_PWM_PRESCALER) {
                _synth_updateWaveform(synth, j);
                _synth_updateAdsr(synth, ch);

                if (ch->waveData.pwm > 0) {
                    ch->waveData.dutyCycle+=ch->waveData.pwm;
                }
            }

            if (!ch->mute && amp->adsr != OFF) {
                /** Sample func 0-127 */
                /** Amplitude 0-32767 */
                Sint8 real = wav->sampleFunc(synth, ch);
                ch->mean = _synth_getMean(ch, real);
                Sint32 sample = ch->mean * wav->volume * amp->amplitude/16384;
                output += sample;
            }

            wav->wavePos += waveFactor * scaledFrequency / synth->sampleFreq;
            ch->playtime++;

        }
        buffer[i] = output * scaler / 32768;
        synth->clock++;
    }
}

Sint8 getSquareAmplitude(Uint8 offset) {
    return (offset > 128) ? 127 : -128;
}

Sint8 getSawAmplitude(Uint8 offset) {
    return 127-offset;
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
    createFilteredBuffer(getSquareAmplitude, synth->lowpassPulse, 8);
    createFilteredBuffer(getSawAmplitude, synth->lowpassSaw, 4);


    for (int i = 0; i < 512; i++) {
        synth->attackTable[i] = 11.2*sqrt(i);
    }
    for (int i = 0; i < 512; i++) {
        synth->decayReleaseTable[i] = 13950/(i+50)-24;
    }

    for (int i = 0; i < 65536; i++) {
        synth->sineTable[i] = 32767 * sin((double)i/10430.3);
    }
    for (int i = 0; i < 65536; i++) {
        synth->halfToDoubleModulationTable[i] = (double)16384 * pow(2, (double)(i-32768)/(double)32768);
    }
}

void _synth_initChannels(Synth *synth) {
    for (int i = 0; i < synth->channels; i++) {
        synth->channelData[i].ampData.amplitude = 0;
        synth->channelData[i].ampData.adsr = OFF;
        synth->channelData[i].waveData.wave = synth->lowpassPulse;
        synth->channelData[i].waveData.sampleFunc = _synth_getSampleFromArray;
        synth->channelData[i].patch  = 0;
    }
}

Synth* synth_init(Uint8 channels) {
    if (channels < 1) {
        fprintf(stderr, "Cannot set 0 channels\n");
        return NULL;
    }
    Synth *synth = calloc(1, sizeof(Synth));
    synth->sampleFreq = SAMPLE_RATE;
    synth->channels = channels;
    synth->channelData = calloc(channels, sizeof(Channel));
    synth->instruments = calloc(MAX_INSTRUMENTS, sizeof(Instrument));
    synth->frequencyTable = frequencyTable_init(96, 128, -45);

    SDL_AudioSpec want;
    SDL_AudioSpec have;

    srand(time(NULL));

    SDL_memset(&want, 0, sizeof(want));
    want.freq = synth->sampleFreq; // Playback frequency on Sound card. Each sample takes worth 1/24000 second
    want.format = AUDIO_S16SYS; // 8-bit unsigned samples
    want.channels = 1; // Only play mono for simplicity = 1 byte = 1 sample
    want.samples = 256; // Buffer size.
    want.callback = _synth_processBuffer; // Called whenever the sound card needs more data
    want.userdata = synth;

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    synth->audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (synth->audio == 0) {
        fprintf(stderr, "Failed to open audio due to %s\n", SDL_GetError());
        synth_close(synth);
        return NULL;
    }
    if (have.format != want.format) { /* we let this one thing change. */
        SDL_Log("We didn't get our audio format.");
    }
    _synth_initAudioTables(synth);
    _synth_initChannels(synth);

    SDL_PauseAudioDevice(synth->audio, 0); /* start audio playing. */

    return synth;
}

void synth_close(Synth *synth) {
    if (NULL != synth) {
        if (synth->audio != 0) {
            SDL_CloseAudioDevice(synth->audio);
        }
        if (NULL != synth->channelData) {
            free(synth->channelData);
            synth->channelData = NULL;
        }
        if (NULL != synth->instruments) {
            free(synth->instruments);
            synth->instruments = NULL;
        }
        if (NULL != synth->frequencyTable) {
            frequencyTable_close(synth->frequencyTable);
            synth->frequencyTable = NULL;
        }
        free(synth);
        synth = NULL;
    }
}

void synth_loadPatch(Synth *synth, Uint8 patch, Instrument *instrument) {
    if (synth == NULL || instrument == NULL) {
        return;
    }
    memcpy(&synth->instruments[patch], instrument, sizeof(Instrument));
}

void _synth_updateAmpData(AmpData *amp) {
    amp->adsr = ATTACK;
    amp->amplitude = 0;
    amp->adsrTimer = 0;
}

void synth_pitchModulation(Synth *synth, Uint8 channel, Uint16 speed, Sint8 *relativeNotes, Uint8 notesLength) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].pitchModulation.speed = speed;
    synth->channelData[channel].pitchModulation.notesLength = notesLength;
    memcpy(synth->channelData[channel].pitchModulation.notes, relativeNotes, notesLength);

}


void synth_notePitch(Synth *synth, Uint8 channel, Uint8 patch, Sint8 note) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }

    Channel *ch = &synth->channelData[channel];
    ch->note = note;
    ch->waveData.swipe.speed = 0;
    ch->waveData.swipe.direction = 0;
    ch->waveData.swipe.offset = 0;

}

void synth_noteTrigger(Synth *synth, Uint8 channel, Uint8 patch, Sint8 note) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    Channel *ch = &synth->channelData[channel];
    ch->ampData.adsr = OFF;
    ch->playtime = 0;
    ch->patch = patch;
    ch->waveData.wavePos = 0;
    ch->waveData.currentSegment = -1;
    _synth_updateWaveform(synth, channel);
    synth_notePitch(synth, channel, patch, note);
    _synth_updateAmpData(&ch->ampData);
}

void synth_noteRelease(Synth *synth, Uint8 channel) {
    if (synth == NULL || channel >= synth->channels || synth->channelData[channel].ampData.adsr == OFF) {
        return;
    }
    synth->channelData[channel].ampData.adsr = RELEASE;
    synth->channelData[channel].ampData.adsrTimer = 0;
}

void synth_noteOff(Synth *synth, Uint8 channel) {
    if (synth == NULL || channel >= synth->channels || synth->channelData[channel].ampData.adsr == OFF) {
        return;
    }
    synth->channelData[channel].ampData.adsr = OFF;
    synth->channelData[channel].ampData.amplitude = 0;
    synth->channelData[channel].ampData.adsrTimer = 0;
}

void synth_frequencyModulation(Synth *synth, Uint8 channel, Uint8 frequency, Uint8 amplitude) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].waveData.frequencyModulation.frequency = frequency;
    synth->channelData[channel].waveData.frequencyModulation.amplitude = amplitude;
}

void synth_pitchGlideUp(Synth *synth, Uint8 channel, Uint8 speed) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].waveData.swipe.speed = speed;
    synth->channelData[channel].waveData.swipe.direction = 1;
}

void synth_pitchGlideDown(Synth *synth, Uint8 channel, Uint8 speed) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].waveData.swipe.speed = speed;
    synth->channelData[channel].waveData.swipe.direction = -1;
}

void synth_pitchGlideStop(Synth *synth, Uint8 channel) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].waveData.swipe.speed = 0;
    synth->channelData[channel].waveData.swipe.direction = 0;
}

void synth_pitchGlideReset(Synth *synth, Uint8 channel) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    synth->channelData[channel].waveData.swipe.speed = 0;
    synth->channelData[channel].waveData.swipe.direction = 0;
    synth->channelData[channel].waveData.swipe.offset = 0;
}

bool synth_isChannelMuted(Synth *synth, Uint8 channel) {
    if (synth == NULL || channel >= synth->channels) {
        return false;
    }
    return synth->channelData[channel].mute;
}

void synth_muteChannel(Synth *synth, Uint8 channel, bool mute) {
    if (synth == NULL || channel >= synth->channels) {
        return;
    }
    printf("mute\n");
    synth->channelData[channel].mute = mute;
}


void _synth_printChannel(AmpData *amp) {
    char *adsrText;
    switch (amp->adsr) {
    case ATTACK:
        adsrText = "Attack";
        break;
    case DECAY:
        adsrText = "Decay";
        break;
    case SUSTAIN:
        adsrText = "Sustain";
        break;
    case RELEASE:
        adsrText = "Release";
        break;
    case OFF:
        adsrText = "Off";
        break;
    }
    printf("? Amplitude: %d, ADSR: %s, ADSR Pos: %d\n", amp->amplitude, adsrText, amp->amplitude);
}

Uint32 testSamples=0;
int testNumberOfChannels=1;

void _synth_testRunBuffer(Synth *testSynth) {
    int bufSize = 64;
    Uint8 buf[bufSize];
    _synth_processBuffer(testSynth, buf, bufSize);

    Sint8 *sbuf = (Sint8*)buf;

    Sint8 high = -128;
    Sint8 low = 127;

    for (int i = 0; i < bufSize; i++) {
        if (sbuf[i] < low) {
            low = sbuf[i];
        }
        if (sbuf[i] > high) {
            high = sbuf[i];
        }
    }

    double t = testSamples / (double)SAMPLE_RATE;
    printf("%.2fs ", t);
    testSamples += bufSize;

    for (int i = -64; i < 64; i++) {
        if (i < (low/2) || i > (high/2)) {
            if (i == 0) {
                printf("|");
            } else {
                printf(".");
            }
        } else {
            printf("X");
        }
    }
    printf("[% 3d..% 3d]\n", low,high);
}

void _synth_testInitChannel(Synth *testSynth,int channel,  Sint8 attack, Sint8 decay, Sint8 sustain, Sint8 release) {
    printf("+ Ch: %d A: %d D: %d S: %d R: %d\n", channel, attack, decay, sustain, release);
    //synth_setChannel(testSynth, channel, attack, decay, sustain, release, PWM);
}

void _synth_testNoteOn(Synth *testSynth) {
    printf("+ NOTE TRIGGER (24)\n");
    for (int i = 0; i < testNumberOfChannels; i++) {
        synth_noteTrigger(testSynth, i, 0, 24);
    }
}

void _synth_testNoteOff(Synth *testSynth) {
    printf("+ NOTE OFF \n");
    for (int i = 0; i < testNumberOfChannels; i++) {
        synth_noteRelease(testSynth, i);
    }
}


void _synth_runTests(Synth *testSynth) {
    printf("\n");
    for (int i = 0; i < testNumberOfChannels; i++) {
        _synth_testInitChannel(testSynth, i, 10, 20, 100, 120);
        _synth_printChannel(&testSynth->channelData[i].ampData);
    }
    _synth_testRunBuffer(testSynth);
    _synth_testNoteOn(testSynth);
    for (int i = 0; i < testNumberOfChannels; i++) {
        _synth_printChannel(&testSynth->channelData[i].ampData);
    }
    testSamples = 0;
    while (testSamples < SAMPLE_RATE/2) {
        _synth_testRunBuffer(testSynth);
    }
    _synth_testNoteOff(testSynth);
    testSamples = 0;
    while (testSamples < SAMPLE_RATE) {
        _synth_testRunBuffer(testSynth);
    }

}

void synth_test() {
    Synth *testSynth = synth_init(testNumberOfChannels);
    if (testSynth == NULL) {
        fprintf(stderr, "Synth test failed to start\n");
        return;
    }
    printf("======================TEST OF %d CHANNELS!========================\n", testNumberOfChannels);
    SDL_PauseAudioDevice(testSynth->audio, 1); /* sop audio playing. */
    _synth_runTests(testSynth);
    printf("\n");
    SDL_PauseAudioDevice(testSynth->audio, 0); /* start audio playing. */
    synth_close(testSynth);
}
