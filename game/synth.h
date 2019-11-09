#ifndef SYNTH_H_
#define SYNTH_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef enum {
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    OFF
} Adsr;


typedef struct {
    Sint8 attack;
    Sint8 decay;
    Sint8 sustain;
    Sint8 release;
    Uint16 voiceFreq;
    Uint16 wavePos;
    Adsr adsr;
    Sint16 amplitude;
    Sint8 *wave;
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

Synth *synth_init(int channels);

void synth_setChannel(Synth* synth, int channel);

void synth_close(Synth *synth);

void synth_play();

#endif /* SYNTH_H_ */
