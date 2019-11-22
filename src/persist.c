#include <SDL2/SDL.h>
#include "persist.h"
#include "note.h"
#include "instrument.h"

bool persist_loadSongWithName(Song *song, char *name) {
    char parameter[20];
    Uint32 address;
    Uint32 value;

    FILE *f = fopen(name, "r");
    if (f == NULL) {
        return false;
    }
    int pattern = 0;
    while (!feof(f)) {
        if (3 == fscanf(f, "%s %04x %04x\n", parameter, &address, &value)) {
            if (strcmp(parameter, "pattern") == 0) {
                if (address >= MAX_PATTERNS) {
                    address = MAX_PATTERNS;
                }
                pattern = address;
            }
            if (strcmp(parameter, "arr") == 0) {
                if (address >= MAX_PATTERNS) {
                    address = MAX_PATTERNS;
                }
                if (address < 0) {
                    address = 0;
                }
                if (value >= MAX_PATTERNS) {
                    value = MAX_PATTERNS;
                }
                song->arrangement[address].pattern = value;
            }
            if (strcmp(parameter, "note") == 0) {
                int track = address / 256;
                int note = address & 255;
                if (track < TRACKS_PER_PATTERN && note < TRACK_LENGTH) {
                    Note *target = &song->patterns[pattern].tracks[track].notes[note];
                    target->note = value;
                    if (target->patch == 0) {
                        target->patch = track+1;
                    }
                }
            }
            if (strcmp(parameter, "patch") == 0) {
                int track = address / 256;
                int note = address & 255;
                if (track < TRACKS_PER_PATTERN && note < TRACK_LENGTH) {
                    Note *target = &song->patterns[pattern].tracks[track].notes[note];
                    target->patch = value;
                }
            }
            if (strcmp(parameter, "cmd") == 0) {
                int track = address / 256;
                int note = address & 255;
                if (track < TRACKS_PER_PATTERN && note < TRACK_LENGTH) {
                    Note *target = &song->patterns[pattern].tracks[track].notes[note];
                    target->command = value;
                }
            }
            if (strcmp(parameter, "attack") == 0) {
                int instr = address / 256;
                printf("instrument address %d: %d attack = %d\n", address, instr, value);
                song->instruments[instr].attack = value;
            }
            if (strcmp(parameter, "decay") == 0) {
                int instr = address / 256;
                song->instruments[instr].decay = value;
            }
            if (strcmp(parameter, "sustain") == 0) {
                int instr = address / 256;
                song->instruments[instr].sustain = value;
            }
            if (strcmp(parameter, "release") == 0) {
                int instr = address / 256;
                song->instruments[instr].release = value;
            }
            if (strcmp(parameter, "wave") == 0) {
                int instr = address / 256;
                int wave = address & 255;
                if (wave < MAX_WAVESEGMENTS) {
                    song->instruments[instr].waves[wave].waveform = value;
                }
            }
            if (strcmp(parameter, "wlen") == 0) {
                int instr = address / 256;
                int wave = address & 255;
                if (wave < MAX_WAVESEGMENTS) {
                    song->instruments[instr].waves[wave].length = value;
                }
            }
            if (strcmp(parameter, "wnote") == 0) {
                int instr = address / 256;
                int wave = address & 255;
                if (wave < MAX_WAVESEGMENTS) {
                    song->instruments[instr].waves[wave].note = value;
                }
            }
            if (strcmp(parameter, "wdc") == 0) {
                int instr = address / 256;
                int wave = address & 255;
                if (wave < MAX_WAVESEGMENTS) {
                    song->instruments[instr].waves[wave].dutyCycle = value;
                }
            }
            if (strcmp(parameter, "wpwm") == 0) {
                int instr = address / 256;
                int wave = address & 255;
                if (wave < MAX_WAVESEGMENTS) {
                    song->instruments[instr].waves[wave].pwm = value;
                }
            }


        }

    }
    fclose(f);
    return true;
}


bool persist_saveSongWithName(Song *song, char* name) {
    FILE *f = fopen(name, "w");
    if (f  == NULL) {
        return false;
    }

    for (int pattern = 0; pattern < MAX_PATTERNS; pattern++) {
        bool patternStored = false;
        for (int track = 0; track < TRACKS_PER_PATTERN; track++) {
            for (int row = 0; row < TRACK_LENGTH; row++) {
                Note note = song->patterns[pattern].tracks[track].notes[row];
                Uint16 encodedNote = (track << 8) + row;
                if (note.note != NOTE_NONE) {
                    if (!patternStored) {
                        fprintf(f, "pattern %04x %04x\n", pattern, 0);
                        patternStored = true;
                    }
                    fprintf(f, "note %04x %02x\n", encodedNote, note.note);
                    fprintf(f, "patch %04x %02x\n", encodedNote, note.patch);
                }
                if (note.command != 0) {
                    if (!patternStored) {
                        fprintf(f, "pattern %04x %04x\n", pattern, 0);
                        patternStored = true;
                    }
                    fprintf(f, "cmd %04x %03x\n", encodedNote, note.command & 0xFFF);
                }

            }
        }
    }
    for (int pos = 0; pos < MAX_PATTERNS; pos++) {
        if (song->arrangement[pos].pattern >= 0) {
            fprintf(f, "arr %04x %04x\n", pos, song->arrangement[pos].pattern);
        }
    }
    for (int patch = 1; patch < MAX_INSTRUMENTS; patch++) {
        Instrument *instr = &song->instruments[patch];
        bool save = false;
        char *instrBytes = (char*)instr;
        for (int i = 0; i < sizeof(Instrument); i++) {
            if (instrBytes[i] != 0) {
                save = true;
                break;
            }
        }
        if (save) {
            int patchByte = patch << 8;
            if (instr->attack != 0) {
                fprintf(f, "attack %04x %04x\n", patchByte, instr->attack);
            }
            if (instr->decay != 0) {
                fprintf(f, "decay %04x %04x\n", patchByte, instr->decay);
            }
            if (instr->sustain != 0) {
                fprintf(f, "sustain %04x %04x\n", patchByte, instr->sustain);
            }
            if (instr->release != 0) {
                fprintf(f, "release %04x %04x\n", patchByte, instr->release);
            }
            for (int wav = 0; wav < MAX_WAVESEGMENTS; wav++) {
                Wavesegment *ws = &instr->waves[wav];
                if (ws->waveform != 0) {
                    fprintf(f, "wave %04x %04x\n", patchByte | wav, ws->waveform);
                }
                if (ws->length != 0) {
                    fprintf(f, "wlen %04x %04x\n", patchByte | wav, ws->length);
                }
                if (ws->note != 0) {
                    fprintf(f, "wnote %04x %04x\n", patchByte | wav, ws->note);
                }
                if (ws->dutyCycle != 0) {
                    fprintf(f, "wdc %04x %04x\n", patchByte | wav, ws->dutyCycle);
                }
                if (ws->pwm != 0) {
                    fprintf(f, "wpwm %04x %04x\n", patchByte | wav, ws->pwm);
                }
            }

        }
    }
    fclose(f);
    return true;
}
