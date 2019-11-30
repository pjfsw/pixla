#include <stdlib.h>
#include "audiorenderer.h"
#include "pattern.h"
#include "synth.h"
#include "player.h"
#include "wav_saver.h"

typedef struct _AudioRenderer {
    Synth *synth;
    Player *player;
    WavSaver *wavSaver;
} AudioRenderer;

typedef void (*AudioRendererConsumer)(void *userData, Uint8 *stream, int len);

AudioRenderer *audiorenderer_init(char *fileName) {
    AudioRenderer *renderer = calloc(1, sizeof(AudioRenderer));
    renderer->synth = synth_init(TRACKS_PER_PATTERN, false);
    if (renderer->synth == NULL) {
        audiorenderer_close(renderer);
        fprintf(stderr, "Audiorenderer: Failed to initialize synth\n");
        return NULL;
    }
    printf("Audiorenderer: Synth initialized\n");
    renderer->player = player_init(renderer->synth, TRACKS_PER_PATTERN);
    if (renderer->player == NULL) {
        audiorenderer_close(renderer);
        fprintf(stderr, "Audiorenderer: Failed to initialize player\n");
        return NULL;
    }
    printf("Audiorenderer: Player initialized\n");
    renderer->wavSaver = wavSaver_init(fileName, synth_getSampleRate(renderer->synth));
    if (renderer->wavSaver == NULL) {
        audiorenderer_close(renderer);
        fprintf(stderr, "Audiorenderer: Failed to initialize WAV saver\n");
        return NULL;
    }
    return renderer;
}

void audiorenderer_renderSong(AudioRenderer *renderer, Song *song, Uint32 timeLimitInMs) {
    Uint32 ms = 0;
    Player *player = renderer->player;
    Synth *synth = renderer->synth;
    int samplerate = synth_getSampleRate(synth);

    for (int i = 0; i < MAX_INSTRUMENTS; i++) {
        synth_loadPatch(synth, i, &song->instruments[i]);
    }

    player_reset(player, song, 0);
    fprintf(stderr, "Audiorenderer: Beginrender song\n");

    while (!player_isEndReached(player) && ms < timeLimitInMs) {
        Uint32 interval = player_processSong(0, player);
        Uint32 samples = samplerate * interval/1000;
        printf("%02d:%02d: Render %d ms = %d samples\n", ms/60000, (ms/1000)%60, interval, samples);

        Uint8 *stream = calloc(samples, sizeof(Uint16));
        synth_processBuffer(synth, stream, samples*2);

        Sint16 *buffer = (Sint16*)stream;
        wavSaver_consume(renderer->wavSaver, buffer, samples);

        free(stream);

        ms += interval;
    }
}

void audiorenderer_close(AudioRenderer *renderer) {
    if (renderer != NULL) {
        if (renderer->player != NULL) {
            player_close(renderer->player);
            renderer->player = NULL;
        }
        if (renderer->synth != NULL) {
            synth_close(renderer->synth);
            renderer->synth = NULL;
        }
        if (renderer->wavSaver != NULL) {
            wavSaver_close(renderer->wavSaver);
        }
        free(renderer);
    }
}
