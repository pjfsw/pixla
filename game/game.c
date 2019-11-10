#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "screen.h"
#include "synth.h"

#define CHANNELS 4
#define NOTE_OFF 126
#define NO_NOTE 127


typedef struct {
    Sint8 notes[64];
}  Track;

Track tracks[32];
Sint8 rowOffset = 0;
Uint8 currentTrack = 0;

typedef void(*KeyHandler)(SDL_Scancode scancode, SDL_Keymod keymod);

KeyHandler keyHandler[256];
Sint8 keyToNote[256];
Uint8 stepping = 1;
int bpm = 135;
int trackPos = 63;
Uint8 octave = 0;


void moveHome(SDL_Scancode scancode, SDL_Keymod keymod) {
    rowOffset = 0;
    screen_setRowOffset(rowOffset);
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

void playNote(SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keyToNote[scancode] > -1) {
        tracks[currentTrack].notes[rowOffset] = keyToNote[scancode] + 12 * octave;
    }
    moveDownSteps(stepping);
}

void skipRow(SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(stepping);
}

void deleteNote(SDL_Scancode scancode, SDL_Keymod keymod) {
    tracks[currentTrack].notes[rowOffset] = NO_NOTE;
    moveDownSteps(stepping);
}

void noteOff(SDL_Scancode scancode, SDL_Keymod keymod) {
    tracks[currentTrack].notes[rowOffset] = NOTE_OFF;
    moveDownSteps(stepping);
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
    keyHandler[scancode] = playNote;
}


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
}

Uint32 getDelayFromBpm(int bpm) {
    return 15000/bpm;
}

Uint32 playCallback(Uint32 interval, void *param) {
    trackPos = (trackPos + 1) % 64;
    for (int channel = 0; channel < CHANNELS; channel++) {
        Sint8 note = tracks[channel].notes[trackPos];

        if (note == NOTE_OFF) {
            synth_noteOff(channel);
        } else if (note >= 0 && note < 97) {
            synth_setPwm(channel, 20, 2);
            synth_noteTrigger(channel, note);
        }
    }
    return interval;
}

int main(int argc, char* args[]) {
    SDL_Event event;

    memset(tracks, NO_NOTE, sizeof(Track)*32);
    initKeyHandler();
    initNotes();

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

    SDL_TimerID my_timer_id = SDL_AddTimer(getDelayFromBpm(bpm), playCallback, NULL);

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
    SDL_RemoveTimer(my_timer_id);
    synth_close();
    screen_close();
    return 0;
}
