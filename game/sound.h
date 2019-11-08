#ifndef SOUND_H_
#define SOUND_H_

#include <stdbool.h>

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
    Sint8 attack;
    Sint8 decay;
    Sint8 sustain;
    Sint8 release;
    Sint16 amplitude;
    Sint8 *wave;
} Channel;

typedef struct {
    Uint16 sampleFreq;
    Sint8 squareWave[256];
    Sint8 squareWave2[256];
    Sint8 triangleWave[256];
    Sint16 adTable[128];
    Sint16 releaseTable[128];
    SDL_AudioDeviceID audio;
    Channel* channelData;
    Uint8 channels;
} Synth;

Synth *sound_init(int channels);

void sound_setChannel(Synth* synth, int channel);

void sound_close(Synth *synth);

void sound_play();

#endif /* SOUND_H_ */
