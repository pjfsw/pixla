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

typedef struct {
    Uint16 voiceFreq;
    Uint16 wavePos;
    Adsr adsr;
    Sint16 amplitude;
    Sint8 *wave;
    Sint8 attack;
    Sint8 decay;
    Sint8 sustain;
    Sint8 release;
} Channel;

typedef struct {
    Uint16 sampleFreq;
    Sint8 squareWave[256];
    Sint8 squareWave2[256];
    Sint8 lowpassWave[256];
    Sint8 lowpassSaw[256];
    Sint16 adTable[128];
    Sint16 releaseTable[128];
    SDL_AudioDeviceID audio;
    Channel* channelData;
    Uint8 channels;
} Synth;

Synth *synth = NULL;


void _synth_processBuffer(void* userdata, Uint8* stream, int len) {
    Synth *synth = (Synth*)userdata;
    Sint8 *buffer = (Sint8*)stream;

    // 256 = full scale, 128 = half scale etc
    Sint8 scaler =  (1 << (8 - synth->channels));

    for (int i = 0; i < 256; i++) {
        buffer[i] = 0;

        for (int j = 0; j < synth->channels; j++) {
            Channel *ch = &synth->channelData[j];

            if (0 == i % 8) {
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

            if (ch->adsr != OFF) {
                Uint16 sample = ch->wave[ch->wavePos >> 8] * (ch->amplitude)/32768;
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

typedef Sint8 (*GetSampleFunc)(Uint8 offset);

void createFilteredBuffer(GetSampleFunc sampleFunc, Sint8* output, int filter) {
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
    for (int i = 0; i < 128; i++) {
        synth->squareWave2[i] = -128;
        synth->squareWave2[i+128] = 127;
    }

    for (int i = 0; i < 64; i++) {
        //wave[i]=i-128;
        synth->squareWave[i] = -128;
        synth->squareWave[i+64] = -128;
        synth->squareWave[i+128] = -128;
        synth->squareWave[i+192] = 127;
    }


    createFilteredBuffer(getSquareAmplitude, synth->lowpassWave, 16);
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
        synth->channelData[i].wave = synth->squareWave;
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
    case SQUARE_1:
        ch->wave = synth->squareWave;
        break;
    case SQUARE_2:
        ch->wave = synth->squareWave2;
        break;
    case LOWPASS_SQUARE:
        ch->wave = synth->lowpassWave;
        break;
    case LOWPASS_SAW:
        ch->wave = synth->lowpassSaw;
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





void synth_play() {
    int channels  = 2;

    Note song[2][32] = {
            0,-2,12,-2,2,-2,14,-2,3,-2,15,-2,8,-2,20,-2,7,-2,19,-2,7,-2,19,-2,5,-2,17,-2,7,-2,19,-2,
            24,-2,27,-1,29,-1,27,29,-1,31,29,27,29,-1,27,-2,24,-2,27,-1,29,-1,27,29,-1,31,29,27,29,-1,27,-2,
    };
    Sint8 transpose[2] = {5,17};

    if (false == synth_init(channels)) {
        synth_close();
        return;
    }

    synth->channelData[0].attack = 0;
    synth->channelData[0].decay = 60;
    synth->channelData[0].sustain = 30;
    synth->channelData[0].release = 30;
    synth->channelData[0].wave = synth->lowpassWave;
    synth->channelData[1].attack = 3;
    synth->channelData[1].decay = 20;
    synth->channelData[1].sustain = 20;
    synth->channelData[1].release = 10;
    synth->channelData[1].wave = synth->lowpassSaw;

    for (int i = 0; i < 2; i++) {
        for (int pos = 0; pos < sizeof(song)/(channels * sizeof(Note)); pos++) {
            for (int channel = 0; channel < channels; channel++) {
                Note note = song[channel][pos];
                if (note.pitch == -1) {
                    continue;
                }
                if (note.pitch == -2) {
                    synth->channelData[channel].adsr = RELEASE;
                    continue;
                }
                synth->channelData[channel].adsr = ATTACK;
                synth->channelData[channel].amplitude = 0;
                synth->channelData[channel].voiceFreq = frequencyTable[(note.pitch+transpose[channel]) % (sizeof(frequencyTable)/sizeof(Uint16))];
            }
            SDL_Delay(125); /* let the audio callback play some sound for 5 seconds. */
        }
    }
    synth_close();
}
