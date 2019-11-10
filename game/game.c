#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "screen.h"
#include "synth.h"

#define CHANNELS 4
#define NOTE_OFF 126
#define NO_NOTE 127
#define MAX_TRACKS 32

typedef struct {
    Sint8 notes[64];
}  Track;

Track tracks[MAX_TRACKS];
Sint8 rowOffset = 0;
Uint8 currentTrack = 0;

typedef void(*KeyHandler)(SDL_Scancode scancode, SDL_Keymod keymod);

KeyHandler keyHandler[256];
Sint8 keyToNote[256];
Uint8 stepping = 1;
int bpm = 135;
Uint8 octave = 0;

SDL_TimerID playbackTimerId = 0;

void clearSong() {
    memset(tracks, NO_NOTE, sizeof(Track)*MAX_TRACKS);
}

void moveToFirstRow() {
    rowOffset = 0;
    screen_setRowOffset(rowOffset);
}

void moveHome(SDL_Scancode scancode, SDL_Keymod keymod) {
    moveToFirstRow();
}

void moveEnd(SDL_Scancode scancode, SDL_Keymod keymod) {
    rowOffset = 63;
    screen_setRowOffset(rowOffset);
}

void moveUpSteps(int steps) {
    rowOffset-=steps;
    if (rowOffset < 0) {
        rowOffset += 64;
    }
    screen_setRowOffset(rowOffset);
}

void moveUp(SDL_Scancode scancode,SDL_Keymod keymod) {
    moveUpSteps(1);
}

void moveDownSteps(int steps) {
    rowOffset+=steps;
    if (rowOffset > 63) {
        rowOffset -= 64;
    }
    screen_setRowOffset(rowOffset);
}

bool isEditMode() {
    return playbackTimerId == 0;
}

void moveDown(SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(1);
}

void increaseStepping(SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keymod & KMOD_LSHIFT) {
        stepping--;
        if (stepping < 0) {
            stepping = 0;
        }
    } else {
        stepping++;
        if (stepping > 8) {
            stepping = 8;
        }
    }
    printf("%d\n",keymod);
}

void playNote(Uint8 channel, Sint8 note) {
    synth_setPwm(channel, 30, 5);
    synth_noteTrigger(channel, note);
}

void playOrUpdateNote(SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keyToNote[scancode] > -1) {
        Sint8 note = keyToNote[scancode] + 12 * octave;
        if (isEditMode()) {
            tracks[currentTrack].notes[rowOffset] = note;
            moveDownSteps(stepping);
        }
        playNote(currentTrack, note);

    }
}

void skipRow(SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(stepping);
}

void deleteNote(SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode()) {
        tracks[currentTrack].notes[rowOffset] = NO_NOTE;
        moveDownSteps(stepping);
    }
}

void noteOff(SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode()) {
        tracks[currentTrack].notes[rowOffset] = NOTE_OFF;
        moveDownSteps(stepping);
    }
}

void setOctave(SDL_Scancode scancode, SDL_Keymod keymod) {
    octave = scancode - SDL_SCANCODE_F1;
}

void previousColumn(SDL_Scancode scancode, SDL_Keymod keymod) {
    if (currentTrack == 0) {
        return;
    }
    currentTrack--;
    screen_setSelectedColumn(currentTrack);
}

void nextColumn(SDL_Scancode scancode, SDL_Keymod keymod) {
    if (currentTrack == CHANNELS-1) {
        return;
    }
    currentTrack++;
    screen_setSelectedColumn(currentTrack);
}

void registerNote(SDL_Scancode scancode, Sint8 note) {
    keyToNote[scancode] = note;
    keyHandler[scancode] = playOrUpdateNote;
}

bool loadSongWithName(char *name) {
    char parameter[20];
    Uint32 address;
    Uint32 value;

    FILE *f = fopen(name, "r");
    if (f == NULL) {
        return false;
    }
    clearSong();
    while (!feof(f)) {
        if (3 == fscanf(f, "%s %04x %02x\n", parameter, &address, &value)) {
            if (strcmp(parameter, "note") == 0) {
                int track = address >> 8;
                printf("Track %d\n", track);
                int note = address & 255;
                if (track < MAX_TRACKS && note < 64) {
                    tracks[track].notes[note] = value;
                }
            }
        }

    }
    fclose(f);
    return true;
}


bool saveSongWithName(char* name) {
    FILE *f = fopen(name, "w");
    if (f  == NULL) {
        return false;
    }
    for (int track = 0; track < MAX_TRACKS; track++) {
        for (int row = 0; row < 64; row++) {
            if (tracks[track].notes[row] != NO_NOTE) {
                Uint16  encodedNote = (track << 8) + row;
                fprintf(f, "note %04x %02x\n", encodedNote, tracks[track].notes[row]);
            }

        }
    }
    fclose(f);
    return true;
}

void loadSong(SDL_Scancode scancode, SDL_Keymod keymod) {
    if (!loadSongWithName("song.pxm")) {
        screen_setStatusMessage("Could not open song.pxm");
    }

}

void saveSong(SDL_Scancode scancode, SDL_Keymod keymod) {
    if (saveSongWithName("song.pxm")) {
        screen_setStatusMessage("Successfully saved song.pxm");
    } else {
        screen_setStatusMessage("Could not save song.pxm");

    }
}

void stopPlayback() {
    if (playbackTimerId != 0) {
        SDL_RemoveTimer(playbackTimerId);
        playbackTimerId = 0;
    }
}

void stopSong(SDL_Scancode scancode, SDL_Keymod keymod) {
    stopPlayback();
    for (int i = 0; i < CHANNELS; i++) {
        synth_noteOff(i);
    }
}

Uint32 getDelayFromBpm(int bpm) {
    return 15000/bpm;
}

Uint32 playCallback(Uint32 interval, void *param) {
    for (int channel = 0; channel < CHANNELS; channel++) {
        Sint8 note = tracks[channel].notes[rowOffset];

        if (note == NOTE_OFF) {
            synth_noteOff(channel);
        } else if (note >= 0 && note < 97) {
            playNote(channel, note);
        }
    }
    screen_setRowOffset(rowOffset);
    rowOffset = (rowOffset + 1) % 64;
    return interval;
}


void playPattern(SDL_Scancode scancode, SDL_Keymod keymod) {
    stopPlayback();
    moveToFirstRow();
    playbackTimerId = SDL_AddTimer(getDelayFromBpm(bpm), playCallback, NULL);
};


void initNotes() {
    memset(keyToNote, -1, sizeof(Sint8)*256);
    registerNote(SDL_SCANCODE_Z, 0);
    registerNote(SDL_SCANCODE_S, 1);
    registerNote(SDL_SCANCODE_X, 2);
    registerNote(SDL_SCANCODE_D, 3);
    registerNote(SDL_SCANCODE_C, 4);
    registerNote(SDL_SCANCODE_V, 5);
    registerNote(SDL_SCANCODE_G, 6);
    registerNote(SDL_SCANCODE_B, 7);
    registerNote(SDL_SCANCODE_H, 8);
    registerNote(SDL_SCANCODE_N, 9);
    registerNote(SDL_SCANCODE_J, 10);
    registerNote(SDL_SCANCODE_M, 11);
    registerNote(SDL_SCANCODE_COMMA, 12);
    registerNote(SDL_SCANCODE_L, 13);
    registerNote(SDL_SCANCODE_PERIOD, 14);
    registerNote(SDL_SCANCODE_SEMICOLON,15);
    registerNote(SDL_SCANCODE_SLASH,16);

    registerNote(SDL_SCANCODE_Q, 12);
    registerNote(SDL_SCANCODE_2, 13);
    registerNote(SDL_SCANCODE_W, 14);
    registerNote(SDL_SCANCODE_3, 15);
    registerNote(SDL_SCANCODE_E, 16);
    registerNote(SDL_SCANCODE_R, 17);
    registerNote(SDL_SCANCODE_5, 18);
    registerNote(SDL_SCANCODE_T, 19);
    registerNote(SDL_SCANCODE_6, 20);
    registerNote(SDL_SCANCODE_Y, 21);
    registerNote(SDL_SCANCODE_7, 22);
    registerNote(SDL_SCANCODE_U, 23);
    registerNote(SDL_SCANCODE_I, 24);
    registerNote(SDL_SCANCODE_9, 25);
    registerNote(SDL_SCANCODE_O, 26);
    registerNote(SDL_SCANCODE_0, 27);
    registerNote(SDL_SCANCODE_P, 28);
    registerNote(SDL_SCANCODE_LEFTBRACKET, 29);

}

void initKeyHandler() {
    memset(keyHandler, 0, sizeof(KeyHandler*)*256);
    keyHandler[SDL_SCANCODE_UP] = moveUp;
    keyHandler[SDL_SCANCODE_DOWN] = moveDown;
    keyHandler[SDL_SCANCODE_BACKSPACE] = deleteNote;
    keyHandler[SDL_SCANCODE_DELETE] = deleteNote;
    keyHandler[SDL_SCANCODE_HOME] = moveHome;
    keyHandler[SDL_SCANCODE_END] = moveEnd;
    keyHandler[SDL_SCANCODE_GRAVE] = increaseStepping;
    keyHandler[SDL_SCANCODE_NONUSBACKSLASH] = noteOff;
    keyHandler[SDL_SCANCODE_F1] = setOctave;
    keyHandler[SDL_SCANCODE_F2] = setOctave;
    keyHandler[SDL_SCANCODE_F3] = setOctave;
    keyHandler[SDL_SCANCODE_F4] = setOctave;
    keyHandler[SDL_SCANCODE_F5] = setOctave;
    keyHandler[SDL_SCANCODE_F6] = setOctave;
    keyHandler[SDL_SCANCODE_LEFT] = previousColumn;
    keyHandler[SDL_SCANCODE_RIGHT] = nextColumn;
    keyHandler[SDL_SCANCODE_RETURN] = skipRow;
    keyHandler[SDL_SCANCODE_F12] = saveSong;
    keyHandler[SDL_SCANCODE_RCTRL] = playPattern;
    keyHandler[SDL_SCANCODE_SPACE] = stopSong;
}

int main(int argc, char* args[]) {
    SDL_Event event;

    clearSong();
    initKeyHandler();
    initNotes();

    loadSongWithName("song.pxm");

    if (!synth_init(CHANNELS)) {
        synth_close();
        return 1;
    }

    if (!screen_init()) {
        synth_close();
        screen_close();
        return 1;
    }



    screen_setColumn(0, 64, tracks[0].notes);
    screen_setColumn(1, 64, tracks[1].notes);
    screen_setColumn(2, 64, tracks[2].notes);
    screen_setColumn(3, 64, tracks[3].notes);

    synth_setChannel(0, 0, 30, 70, 20, PWM);
    synth_setChannel(1, 0, 30, 70, 20, LOWPASS_PULSE);
    synth_setChannel(2, 0, 30, 70, 20, NOISE);
    synth_setChannel(3, 0, 30, 70, 20, LOWPASS_SAW);

    SDL_Keymod keymod;
    bool quit = false;
    /* Loop until an SDL_QUIT event is found */


    while( !quit ){
        /* Poll for events */
        while( SDL_PollEvent( &event ) ){

            switch( event.type ){
            /* Keyboard event */
            /* Pass the event data onto PrintKeyInfo() */
            case SDL_KEYDOWN:
                keymod = SDL_GetModState();
                if (keyHandler[event.key.keysym.scancode] != NULL) {
                    keyHandler[event.key.keysym.scancode](event.key.keysym.scancode, keymod);
                }
                printf("Key %d\n", event.key.keysym.scancode);

                break;
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
        screen_setStepping(stepping);
        screen_update();
    }
    stopPlayback();
    synth_close();
    screen_close();
    return 0;
}
