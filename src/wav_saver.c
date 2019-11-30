#include <stdlib.h>
#include <SDL2/SDL.h>

#include "wav_saver.h"

#define WAV_NUMBER_OF_CHANNELS 1
#define WAV_BYTES_PER_SAMPLE 2

typedef struct _WavSaver {
    char *rawName;
    char *wavName;
    FILE *rawFile;
    Uint32 dataLength;
    Uint32 sampleRate;
} WavSaver;

typedef struct {
    char id[4];
    Uint32 size;
} WavChunkHeader;

typedef struct {
    WavChunkHeader header;
    Uint16 audioFormat;
    Uint16 numChannels;
    Uint32 sampleRate;
    Uint32 byteRate;
    Uint16 blockAlign;
    Uint16 bitsPerSample;
} FmtChunk;

typedef struct {
    WavChunkHeader header;
    char format[4];
    FmtChunk fmt;
    WavChunkHeader data;
} WavHeader;


void _wavSaver_writeHeader(WavSaver *wavSaver, FILE *file) {
    WavHeader wavHeader;

    memset(&wavHeader, 0, sizeof(WavHeader));

    memcpy(wavHeader.header.id, "RIFF", 4);
    wavHeader.header.size = wavSaver->dataLength + sizeof(WavHeader) - 8;
    memcpy(wavHeader.format, "WAVE", 4);

    /* fmt subchunk */
    memcpy(wavHeader.fmt.header.id, "fmt ", 4);
    wavHeader.fmt.header.size = 16; /* 16 for PCM */
    wavHeader.fmt.audioFormat = 1; /* Uncompressed PCM */

    wavHeader.fmt.numChannels = WAV_NUMBER_OF_CHANNELS;
    wavHeader.fmt.sampleRate = wavSaver->sampleRate;
    wavHeader.fmt.byteRate = wavSaver->sampleRate * WAV_NUMBER_OF_CHANNELS * WAV_BYTES_PER_SAMPLE;
    wavHeader.fmt.blockAlign = WAV_NUMBER_OF_CHANNELS * WAV_BYTES_PER_SAMPLE;
    wavHeader.fmt.bitsPerSample = WAV_BYTES_PER_SAMPLE * 8;
    memcpy(wavHeader.data.id, "data", 4);
    wavHeader.data.size = wavSaver->dataLength;

    fwrite(&wavHeader, 1, sizeof(WavHeader), file);
}

WavSaver *wavSaver_init(char *fileName, Uint32 sampleRate) {
    WavSaver *wavSaver = calloc(1, sizeof(WavSaver));
    wavSaver->sampleRate = sampleRate;

    wavSaver->wavName = calloc(strlen(fileName)+1, sizeof(char));
    strcpy(wavSaver->wavName, fileName);

    wavSaver->rawName = calloc(strlen(fileName)+3, sizeof(char));
    strcpy(wavSaver->rawName, fileName);
    strcat(wavSaver->rawName, ".r");

    wavSaver->rawFile = fopen(wavSaver->rawName, "wb");
    if (wavSaver->rawFile == NULL) {
        wavSaver_close(wavSaver);
        return NULL;
    }

    return wavSaver;

}

void wavSaver_consume(WavSaver *wavSaver, Sint16 *samples, int length) {
    wavSaver->dataLength += length * sizeof(Sint16);
    fwrite(samples, sizeof(Sint16), length, wavSaver->rawFile);
}

void _wavSaver_free(void **ptr) {
    if (*ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

void _wavSaver_closeFile(FILE **file) {
    if (*file != NULL) {
        fclose(*file);
        *file = NULL;
    }
}

void _wavSaver_copyFile(FILE *target, FILE *src) {
    int bufSize = 16384;
    char buf[bufSize];

    while (!feof(src)) {
        size_t bytesRead = fread(buf, 1, bufSize, src);
        if (bytesRead > 0) {
            fwrite(buf, 1, bytesRead, target);
        }
    }

}

void wavSaver_close(WavSaver *wavSaver) {
    if (wavSaver != NULL) {
        _wavSaver_closeFile(&wavSaver->rawFile);
        FILE *wavFile = fopen(wavSaver->wavName, "wb");
        FILE *rawFile = fopen(wavSaver->rawName, "rb");
        if (wavFile != NULL && rawFile != NULL) {
            _wavSaver_writeHeader(wavSaver, wavFile);
            _wavSaver_copyFile(wavFile, rawFile);
        }
        remove(wavSaver->rawName);
        _wavSaver_closeFile(&wavFile);
        _wavSaver_closeFile(&rawFile);
        _wavSaver_free((void**)&wavSaver->rawName);
        _wavSaver_free((void**)&wavSaver->wavName);
        free(wavSaver);
    }
}


