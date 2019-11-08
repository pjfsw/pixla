#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "screen.h"

Uint8 pwm = 0;

Uint16 sampleFreq = 48000;

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

#define CHANNELS 2

Channel channelData[CHANNELS];

Sint8 *triangleWave;
Sint8 *squareWave;
Sint8 *squareWave2;


Uint16 frequencyTable[37] = {
         55,
         58,
         62,
         65,
         69,
         73,
         78,
         82,
         87,
         93,
         98,
         103,
        110,
        117,
        123,
        131,
        139,
        147,
        156,
        165,
        175,
        185,
        196,
        207,
        220,
        233,
        247,
        261,
        277,
        294,
        311,
        330,
        349,
        370,
        392,
        415,
        440
};

typedef struct {
    Uint8 note[CHANNELS];
} NoteData;


NoteData song[16] = {
        0, 24,
        255, 255,
        12, 255,
        254, 255,
        0, 254,
        255, 34,
        12, 255,
        0, 36,
        3, 255,
        255, 24,
        15, 27,
        3, 34,
        3, 255,
        255, 27,
        15, 255,
        3, 24

};

Sint16 adTable[128];
Sint16 releaseTable[128];



void beepCallback2(void*  userdata,
                       Uint8* stream,
                       int    len) {
    int amplitude = 23;

    pwm+=1; // Pulse width modulation - change the duty cycle of the pulse, once per pulse

    Sint8* buffer = (Sint8*)stream;

    int split = len / 256 * pwm;  // How much of the buffer is amplitude low , how much is amplitude high.

    for (int i = 0; i < len; i++) {
        int noise = rand()>>26;

        // A pulse takes up the whole buffer so
        // musical frequency is 24000/512 (playback frequency/buffer size)
        if (i < split) {
            buffer[i] = noise + -amplitude;
        } else {
            buffer[i] = noise + amplitude;
        }
    }
}


void beepCallback(void*  userdata,
        Uint8* stream,
        int    len) {

    Sint8* buffer = (Sint8*)stream;

    for (int i = 0; i < 256; i++) {
        buffer[i] = 0;


        for (int j = 0; j < CHANNELS; j++) {
            Channel *ch = &channelData[j];

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
                        ch->amplitude += adTable[ch->attack];  // (127-ch->attack);
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
                        ch->amplitude -= adTable[ch->decay]; //(127-ch->decay);
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
                        ch->amplitude -= releaseTable[ch->release];
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
                buffer[i] += sample >> (1+CHANNELS);
            }

            ch->wavePos += (65536 * ch->voiceFreq) / sampleFreq;
        }

    }
}

void beep() {
    squareWave = malloc(256);
    squareWave2 = malloc(256);

    triangleWave = malloc(256);
    for (int i = 0; i < 128; i++) {
        squareWave2[i] = -128;
        squareWave2[i+128] = 127;
    }

    for (int i = 0; i < 64; i++) {
        //wave[i]=i-128;
        squareWave[i] = -128;
        squareWave[i+64] = -128;
        squareWave[i+128] = -128;
        squareWave[i+192] = 127;
    }

    for (int i = 0; i < 63; i++) {
        triangleWave[i] = i * 2;
        triangleWave[i+64] = 127-i*2;
        triangleWave[i+128] = 127-i*2;
        triangleWave[i+192] = i * 2 - 128;
    }

    for (int i = 0; i < 128; i++) {
        adTable[i] =  1024/(i+1);
        releaseTable[i] = 256/(i+1);
    }
    memset(&channelData, 0, sizeof(Channel)*CHANNELS);

    channelData[0].attack = 0 ;
    channelData[0].decay = 20 ;
    channelData[0].sustain = 60 ;
    channelData[0].release = 30;
    channelData[0].wave = squareWave2;
    channelData[1].attack = 4 ;
    channelData[1].decay = 50 ;
    channelData[1].sustain = 70 ;
    channelData[1].release = 20;
    channelData[1].wave = squareWave;



    SDL_AudioSpec want;
    SDL_AudioSpec have;

    srand(time(NULL));

    SDL_memset(&want, 0, sizeof(want));


    want.freq = sampleFreq; // Playback frequency on Sound card. Each sample takes worth 1/24000 second
    want.format = AUDIO_S8; // 8-bit unsigned samples
    want.channels = 1; // Only play mono for simplicity = 1 byte = 1 sample
    want.samples = 256; // Buffer size.
    want.callback = beepCallback; // Called whenever the sound card needs more data

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);

    if (0 == audio) {
        fprintf(stderr, "Failed to open audio due to %s\n", SDL_GetError());
        return;
    } else {
        fprintf(stderr, "Freq %d channels %d format %04x\n", have.freq, have.channels, have.format);
    }

    if (have.format != want.format) { /* we let this one thing change. */
        SDL_Log("We didn't get out audio format.");
    }
    SDL_PauseAudioDevice(audio, 0); /* start audio playing. */
    for (int i = 0; i < 2; i++) {
        for (int pos = 0; pos < sizeof(song)/sizeof(NoteData); pos++) {
            for (int channel = 0; channel < CHANNELS; channel++) {
                if (song[pos].note[channel] == 0xff) {
                    continue;
                }
                if (song[pos].note[channel] == 0xfe) {
                    channelData[channel].adsr = RELEASE;
                    continue;
                }
                channelData[channel].adsr = ATTACK;
                channelData[channel].amplitude = 0;
                channelData[channel].voiceFreq = frequencyTable[song[pos].note[channel] % (sizeof(frequencyTable)/sizeof(Uint16))];
            }
            SDL_Delay(250); /* let the audio callback play some sound for 5 seconds. */
        }
    }
    SDL_CloseAudioDevice(audio);

    free(squareWave);
    free(squareWave2);
    free(triangleWave);
}

int main(int argc, char* args[]) {
    SDL_Window* window = NULL;
    SDL_Surface* screenSurface = NULL;

    SDL_Event event;


    beep();
    return 1;

    if (!screen_init()) {
        screen_close();
        return 1;
    }

    screenSurface = SDL_GetWindowSurface(window);
    SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0xFF, 0xFF));
    SDL_UpdateWindowSurface(window);



    bool quit = false;
    /* Loop until an SDL_QUIT event is found */
    while( !quit ){

        /* Poll for events */
        while( SDL_PollEvent( &event ) ){

            switch( event.type ){
            /* Keyboard event */
            /* Pass the event data onto PrintKeyInfo() */
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                break;

                /* SDL_QUIT event (window close) */
            case SDL_QUIT:
                quit = 1;
                break;

            default:
                break;
            }

        }
    }
    screen_close();
    return 0;
}
