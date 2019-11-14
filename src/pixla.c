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

typedef enum {
    STOP,
    EDIT,
    PLAY
} Mode;

typedef struct {
    Track tracks[MAX_TRACKS];
    int bpm;
} Song;

typedef struct _Tracker Tracker;

typedef void(*KeyHandler)(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod);

typedef struct _Tracker {
    KeyHandler keyHandler[256];
    Sint8 keyToNote[256];
    Uint8 stepping;
    Uint8 octave;

    Synth *synth;
    SDL_TimerID playbackTimerId;
    Sint8 rowOffset;
    Uint8 currentTrack;
    Song song;
    Mode mode;
} Tracker;

Tracker *tracker;


void clearSong(Song *song) {
    memset(song->tracks, NO_NOTE, sizeof(Track)*MAX_TRACKS);
}

void moveToFirstRow(Tracker *tracker) {
    tracker->rowOffset = 0;
    screen_setRowOffset(tracker->rowOffset);
}

void moveHome(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveToFirstRow(tracker);
}

void moveEnd(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    tracker->rowOffset = 63;
    screen_setRowOffset(tracker->rowOffset);
}

void moveUpSteps(Tracker *tracker, int steps) {
    tracker->rowOffset-=steps;
    if (tracker->rowOffset < 0) {
        tracker->rowOffset += 64;
    }
    screen_setRowOffset(tracker->rowOffset);
}

void moveUp(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveUpSteps(tracker, 1);
}

void moveUpMany(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveUpSteps(tracker, 16);
}

void moveDownSteps(Tracker *tracker, int steps) {
    tracker->rowOffset+=steps;
    if (tracker->rowOffset > 63) {
        tracker->rowOffset -= 64;
    }
    screen_setRowOffset(tracker->rowOffset);
}

void moveDownMany(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(tracker, 16);
}

bool isEditMode(Tracker *tracker) {
    return tracker->mode == EDIT;
}

void setMode(Tracker *tracker, Mode modeToSet) {
    tracker->mode = modeToSet;
    screen_setEditMode(isEditMode(tracker));
};

void moveDown(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(tracker, 1);
}

void increaseStepping(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keymod & KMOD_LSHIFT) {
        tracker->stepping--;
        if (tracker->stepping < 0) {
            tracker->stepping = 0;
        }
    } else {
        tracker->stepping++;
        if (tracker->stepping > 8) {
            tracker->stepping = 8;
        }
    }
    printf("%d\n",keymod);
}

void playNote(Synth *synth, Uint8 channel, Sint8 note) {
    synth_setPwm(synth, channel, 40, 10);
    synth_noteTrigger(synth, channel, note);
}

Sint8 *getCurrentNote(Tracker *tracker) {
    return &tracker->song.tracks[tracker->currentTrack].notes[tracker->rowOffset];
}

void playOrUpdateNote(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (tracker->keyToNote[scancode] > -1) {
        Sint8 note = tracker->keyToNote[scancode] + 12 * tracker->octave;
        if (note > 95) {
            return;
        }
        if (isEditMode(tracker)) {
            *getCurrentNote(tracker) = note;
            moveDownSteps(tracker, tracker->stepping);
        }
        playNote(tracker->synth, tracker->currentTrack, note);

    }
}

void skipRow(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (isEditMode(tracker)) {
        moveDownSteps(tracker, tracker->stepping);
    } else {
        synth_noteRelease(tracker->synth, tracker->currentTrack);
    }
}

void deleteNote(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode(tracker)) {
        *getCurrentNote(tracker) = NO_NOTE;
        moveDownSteps(tracker, tracker->stepping);
    }
}

void noteOff(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode(tracker)) {
        *getCurrentNote(tracker) = NOTE_OFF;
        moveDownSteps(tracker, tracker->stepping);
    }
}

void setOctave(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    tracker->octave = scancode - SDL_SCANCODE_F1;
    screen_setOctave(tracker->octave);
}

void previousColumn(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->currentTrack == 0) {
        return;
    }
    tracker->currentTrack--;
    screen_setSelectedColumn(tracker->currentTrack);
}

void nextColumn(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->currentTrack == CHANNELS-1) {
        return;
    }
    tracker->currentTrack++;
    screen_setSelectedColumn(tracker->currentTrack);
}

void registerNote(Tracker *tracker, SDL_Scancode scancode, Sint8 note) {
    tracker->keyToNote[scancode] = note;
    tracker->keyHandler[scancode] = playOrUpdateNote;
}

bool loadSongWithName(Tracker *tracker, char *name) {
    char parameter[20];
    Uint32 address;
    Uint32 value;

    FILE *f = fopen(name, "r");
    if (f == NULL) {
        return false;
    }
    clearSong(&tracker->song);
    while (!feof(f)) {
        if (3 == fscanf(f, "%s %04x %02x\n", parameter, &address, &value)) {
            if (strcmp(parameter, "note") == 0) {
                int track = address >> 8;
                int note = address & 255;
                if (track < MAX_TRACKS && note < 64) {
                    tracker->song.tracks[track].notes[note] = value;
                }
            }
        }

    }
    fclose(f);
    return true;
}


bool saveSongWithName(Tracker *tracker, char* name) {
    FILE *f = fopen(name, "w");
    if (f  == NULL) {
        return false;
    }
    for (int track = 0; track < MAX_TRACKS; track++) {
        for (int row = 0; row < 64; row++) {
            if (tracker->song.tracks[track].notes[row] != NO_NOTE) {
                Uint16  encodedNote = (track << 8) + row;
                fprintf(f, "note %04x %02x\n", encodedNote, tracker->song.tracks[track].notes[row]);
            }

        }
    }
    fclose(f);
    return true;
}

void loadSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (!loadSongWithName(tracker, "song.pxm")) {
        screen_setStatusMessage("Could not open song.pxm");
    }

}

void saveSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (saveSongWithName(tracker, "song.pxm")) {
        screen_setStatusMessage("Successfully saved song.pxm");
    } else {
        screen_setStatusMessage("Could not save song.pxm");

    }
}

void stopPlayback(Tracker *tracker) {
    if (tracker->playbackTimerId != 0) {
        SDL_RemoveTimer(tracker->playbackTimerId);
        tracker->playbackTimerId = 0;
    }
    if (tracker->mode == STOP) {
        setMode(tracker, EDIT);
        for (int i = 0; i < CHANNELS; i++) {
            synth_noteOff(tracker->synth, i);
        }
    } else {
        setMode(tracker, STOP);
        for (int i = 0; i < CHANNELS; i++) {
            synth_noteRelease(tracker->synth, i);
        }
    }
}

void stopSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    stopPlayback(tracker);
}

Uint32 getDelayFromBpm(int bpm) {
    return 15000/bpm;
}

Uint32 playCallback(Uint32 interval, void *param) {
    Tracker *tracker = (Tracker*)param;

    for (int channel = 0; channel < CHANNELS; channel++) {
        Sint8 note = tracker->song.tracks[channel].notes[tracker->rowOffset];

        if (note == NOTE_OFF) {
            synth_noteRelease(tracker->synth, channel);
        } else if (note >= 0 && note < 97) {
            playNote(tracker->synth, channel, note);
        }
    }
    screen_setRowOffset(tracker->rowOffset);
    tracker->rowOffset = (tracker->rowOffset + 1) % 64;
    return interval;
}


void playPattern(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    stopPlayback(tracker);
    moveToFirstRow(tracker);
    tracker->playbackTimerId = SDL_AddTimer(getDelayFromBpm(tracker->song.bpm), playCallback, tracker);
    setMode(tracker, PLAY);
};


void initNotes() {
    memset(tracker->keyToNote, -1, sizeof(Sint8)*256);
    registerNote(tracker, SDL_SCANCODE_Z, 0);
    registerNote(tracker, SDL_SCANCODE_S, 1);
    registerNote(tracker, SDL_SCANCODE_X, 2);
    registerNote(tracker, SDL_SCANCODE_D, 3);
    registerNote(tracker, SDL_SCANCODE_C, 4);
    registerNote(tracker, SDL_SCANCODE_V, 5);
    registerNote(tracker, SDL_SCANCODE_G, 6);
    registerNote(tracker, SDL_SCANCODE_B, 7);
    registerNote(tracker, SDL_SCANCODE_H, 8);
    registerNote(tracker, SDL_SCANCODE_N, 9);
    registerNote(tracker, SDL_SCANCODE_J, 10);
    registerNote(tracker, SDL_SCANCODE_M, 11);
    registerNote(tracker, SDL_SCANCODE_COMMA, 12);
    registerNote(tracker, SDL_SCANCODE_L, 13);
    registerNote(tracker, SDL_SCANCODE_PERIOD, 14);
    registerNote(tracker, SDL_SCANCODE_SEMICOLON,15);
    registerNote(tracker, SDL_SCANCODE_SLASH,16);

    registerNote(tracker, SDL_SCANCODE_Q, 12);
    registerNote(tracker, SDL_SCANCODE_2, 13);
    registerNote(tracker, SDL_SCANCODE_W, 14);
    registerNote(tracker, SDL_SCANCODE_3, 15);
    registerNote(tracker, SDL_SCANCODE_E, 16);
    registerNote(tracker, SDL_SCANCODE_R, 17);
    registerNote(tracker, SDL_SCANCODE_5, 18);
    registerNote(tracker, SDL_SCANCODE_T, 19);
    registerNote(tracker, SDL_SCANCODE_6, 20);
    registerNote(tracker, SDL_SCANCODE_Y, 21);
    registerNote(tracker, SDL_SCANCODE_7, 22);
    registerNote(tracker, SDL_SCANCODE_U, 23);
    registerNote(tracker, SDL_SCANCODE_I, 24);
    registerNote(tracker, SDL_SCANCODE_9, 25);
    registerNote(tracker, SDL_SCANCODE_O, 26);
    registerNote(tracker, SDL_SCANCODE_0, 27);
    registerNote(tracker, SDL_SCANCODE_P, 28);
    registerNote(tracker, SDL_SCANCODE_LEFTBRACKET, 29);

}

void initKeyHandler() {
    memset(tracker->keyHandler, 0, sizeof(KeyHandler*)*256);
    tracker->keyHandler[SDL_SCANCODE_UP] = moveUp;
    tracker->keyHandler[SDL_SCANCODE_DOWN] = moveDown;
    tracker->keyHandler[SDL_SCANCODE_PAGEUP] = moveUpMany;
    tracker->keyHandler[SDL_SCANCODE_PAGEDOWN] = moveDownMany;
    tracker->keyHandler[SDL_SCANCODE_BACKSPACE] = deleteNote;
    tracker->keyHandler[SDL_SCANCODE_DELETE] = deleteNote;
    tracker->keyHandler[SDL_SCANCODE_HOME] = moveHome;
    tracker->keyHandler[SDL_SCANCODE_END] = moveEnd;
    tracker->keyHandler[SDL_SCANCODE_GRAVE] = increaseStepping;
    tracker->keyHandler[SDL_SCANCODE_NONUSBACKSLASH] = noteOff;
    tracker->keyHandler[SDL_SCANCODE_F1] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_F2] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_F3] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_F4] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_F5] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_F6] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_F7] = setOctave;
    tracker->keyHandler[SDL_SCANCODE_LEFT] = previousColumn;
    tracker->keyHandler[SDL_SCANCODE_RIGHT] = nextColumn;
    tracker->keyHandler[SDL_SCANCODE_RETURN] = skipRow;
    tracker->keyHandler[SDL_SCANCODE_F12] = saveSong;
    tracker->keyHandler[SDL_SCANCODE_RCTRL] = playPattern;
    tracker->keyHandler[SDL_SCANCODE_SPACE] = stopSong;
}


void tracker_close(Tracker *tracker) {
    if (NULL != tracker) {
        if (tracker->synth != NULL) {
            synth_close(tracker->synth);
            tracker->synth = NULL;
        }
        free(tracker);
        tracker = NULL;
    }
}

Tracker *tracker_init() {
    tracker = calloc(1, sizeof(Tracker));
    tracker->song.bpm = 140;
    tracker->stepping = 1;

    if (NULL == (tracker->synth = synth_init(CHANNELS))) {
        tracker_close(tracker);
        return NULL;
    }

    return tracker;
}


int main(int argc, char* args[]) {
    SDL_Event event;

    Tracker *tracker = tracker_init();

    if (!screen_init()) {
        screen_close();
        tracker_close(tracker);
        return 1;
    }

    clearSong(&tracker->song);
    initKeyHandler();
    initNotes();

    loadSongWithName(tracker, "song.pxm");

    synth_test();
    //return 0;


    screen_setColumn(0, 64, tracker->song.tracks[0].notes);
    screen_setColumn(1, 64, tracker->song.tracks[1].notes);
    screen_setColumn(2, 64, tracker->song.tracks[2].notes);
    screen_setColumn(3, 64, tracker->song.tracks[3].notes);

    synth_setChannel(tracker->synth, 0, 6, 70, 30, 60, PWM);
    synth_setChannel(tracker->synth, 1, 10, 30, 70, 60, LOWPASS_PULSE);
    synth_setChannel(tracker->synth, 2, 0, 20, 40, 50, NOISE);
    synth_setChannel(tracker->synth, 3, 0, 9, 40, 120, LOWPASS_SAW);

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
                if (tracker->keyHandler[event.key.keysym.scancode] != NULL) {
                    tracker->keyHandler[event.key.keysym.scancode](tracker, event.key.keysym.scancode, keymod);
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
        screen_setStepping(tracker->stepping);
        screen_update();
        SDL_Delay(5);
    }
    stopPlayback(tracker);
    screen_close();
    tracker_close(tracker);
    return 0;
}
