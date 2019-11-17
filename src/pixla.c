#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#include "screen.h"
#include "synth.h"
#include "track.h"
#include "trackermode.h"
#include "song.h"
#include "player.h"
#include "note.h"

#define CHANNELS 4
#define DEFAULT_TRACK_LENGTH 64
#define SUBCOLUMNS 4

typedef struct _Tracker Tracker;

typedef void(*KeyHandler)(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod);

typedef struct _Tracker {
    Synth *synth;
    Player *player;
    KeyHandler keyHandler[256];
    Sint8 keyToNote[256];
    Uint8 keyToCommandCode[256];
    Uint8 stepping;
    Uint8 octave;
    Uint8 patch;

    Sint8 rowOffset;
    Uint8 currentTrack;
    Song song;
    Trackermode mode;
    Uint8 currentColumn;
} Tracker;

Tracker *tracker;


void clearSong(Song *song) {
    for (int track = 0; track < MAX_TRACKS; track++) {
        song->tracks[track].length = DEFAULT_TRACK_LENGTH;;
        for (int row = 0; row < MAX_TRACK_LENGTH; row++) {
            song->tracks[track].notes[row].note = NOTE_NONE;
            song->tracks[track].notes[row].patch = 0;
        }
    }
}

void moveToFirstRow(Tracker *tracker) {
    tracker->rowOffset = 0;
}

void moveHome(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveToFirstRow(tracker);
}

void moveEnd(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    tracker->rowOffset = 63;
}

void moveUpSteps(Tracker *tracker, int steps) {
    tracker->rowOffset-=steps;
    if (tracker->rowOffset < 0) {
        tracker->rowOffset += 64;
    }
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
}

void moveDownMany(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(tracker, 16);
}

bool isEditMode(Tracker *tracker) {
    return tracker->mode == EDIT;
}

void setMode(Tracker *tracker, Trackermode modeToSet) {
    tracker->mode = modeToSet;
    screen_setTrackermode(tracker->mode);
};

void moveDown(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(tracker, 1);
}

void increaseStepping(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keymod & KMOD_LSHIFT) {
        if (tracker->stepping == 0) {
            return;
        }
        tracker->stepping--;
    } else {
        if (tracker->stepping > 7) {
            return;
        }
        tracker->stepping++;
    }
}

void playNote(Synth *synth, Uint8 channel, Uint8 patch, Sint8 note) {
    synth_noteTrigger(synth, channel, patch, note);
}

Note *getCurrentNote(Tracker *tracker) {
    return &tracker->song.tracks[tracker->currentTrack].notes[tracker->rowOffset];
}


void editCommand(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (!isEditMode(tracker) || tracker->currentColumn < 1) {
        return;
    }
    int nibblePos = (3-tracker->currentColumn) * 4;
    Uint16 mask = 0XFFF - (0xF << nibblePos);

    Note *note = &tracker->song.tracks[tracker->currentTrack].notes[tracker->rowOffset];
    note->command &= mask;
    note->command |= (tracker->keyToCommandCode[scancode] << nibblePos);

    moveDownSteps(tracker, tracker->stepping);
}

void handleAltkeys(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (scancode == SDL_SCANCODE_Z) {
        bool mute = !synth_isChannelMuted(tracker->synth, 0);
        synth_muteChannel(tracker->synth, 0, mute);
        screen_setChannelMute(0, mute);
    } else if (scancode == SDL_SCANCODE_X) {
        bool mute = !synth_isChannelMuted(tracker->synth, 1);
        synth_muteChannel(tracker->synth, 1, mute);
        screen_setChannelMute(1, mute);
    } else if (scancode == SDL_SCANCODE_C) {
        bool mute = !synth_isChannelMuted(tracker->synth, 2);
        synth_muteChannel(tracker->synth, 2, mute);
        screen_setChannelMute(2, mute);
    } else if (scancode == SDL_SCANCODE_V) {
        bool mute = !synth_isChannelMuted(tracker->synth, 3);
        synth_muteChannel(tracker->synth, 3, mute);
        screen_setChannelMute(3, mute);
    }
}
void playOrUpdateNote(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (isEditMode(tracker) && tracker->currentColumn > 0) {
        editCommand(tracker, scancode, keymod);
        return;
    }
    if (keymod & KMOD_LALT) {
        printf("herpderp\n");
        handleAltkeys(tracker, scancode, keymod);
        return;
    }
    if (tracker->keyToNote[scancode] > -1) {
        Sint8 note = tracker->keyToNote[scancode] + 12 * tracker->octave;
        if (note > 95) {
            return;
        }
        if (isEditMode(tracker)) {
            getCurrentNote(tracker)->note = note;
            getCurrentNote(tracker)->patch = tracker->patch;
            moveDownSteps(tracker, tracker->stepping);
        }
        playNote(tracker->synth, tracker->currentTrack, tracker->patch, note);
        //SDL_AddTimer(100, stopJamming, tracker);
        synth_noteRelease(tracker->synth, tracker->currentTrack);

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
        getCurrentNote(tracker)->note = NOTE_NONE;
        getCurrentNote(tracker)->patch = 0;
        moveDownSteps(tracker, tracker->stepping);
    }
}

void noteOff(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode(tracker)) {
        getCurrentNote(tracker)->note = NOTE_OFF;
        moveDownSteps(tracker, tracker->stepping);
    }
}

void setOctave(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    tracker->octave = scancode - SDL_SCANCODE_F1;
    screen_setOctave(tracker->octave);
}

void gotoNextTrack(Tracker *tracker) {
    if (tracker->currentTrack < CHANNELS-1) {
        tracker->currentTrack++;
        tracker->currentColumn = 0;
        screen_setSelectedTrack(tracker->currentTrack);
    }
}

void gotoPreviousTrack(Tracker *tracker) {
    if (tracker->currentTrack > 0) {
        tracker->currentTrack--;
        tracker->currentColumn = 0;
        screen_setSelectedTrack(tracker->currentTrack);
    }
}

void previousOrNextColumn(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_LSHIFT) {
        gotoPreviousTrack(tracker);
    } else {
        gotoNextTrack(tracker);
    }
}

void previousColumn(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->currentColumn > 0) {
        tracker->currentColumn--;
    } else if (tracker->currentTrack > 0) {
        gotoPreviousTrack(tracker);
        tracker->currentColumn = SUBCOLUMNS-1;
    }
    screen_setSelectedColumn(tracker->currentColumn);
}

void nextColumn(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->currentColumn < SUBCOLUMNS-1) {
        tracker->currentColumn++;
        screen_setSelectedColumn(tracker->currentColumn);
    } else {
        gotoNextTrack(tracker);
    }
}

void previousPatch(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->patch <= 1) {
        tracker->patch = 255;
    } else {
        tracker->patch--;
    }
    screen_selectPatch(tracker->patch);
}

void nextPatch(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->patch == 255) {
        tracker->patch = 1;
    } else {
        tracker->patch++;
    }
    screen_selectPatch(tracker->patch);
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
        if (3 == fscanf(f, "%s %04x %04x\n", parameter, &address, &value)) {
            if (strcmp(parameter, "note") == 0) {
                int track = address / 256;
                int note = address & 255;
                if (track < MAX_TRACKS && note < MAX_TRACK_LENGTH) {
                    Note *target = &tracker->song.tracks[track].notes[note];
                    target->note = value;
                    if (target->patch == 0) {
                        target->patch = track+1;
                    }
                }
            }
            if (strcmp(parameter, "patch") == 0) {
                int track = address / 256;
                int note = address & 255;
                if (track < MAX_TRACKS && note < MAX_TRACK_LENGTH) {
                    Note *target = &tracker->song.tracks[track].notes[note];
                    target->patch = value;
                }
            }
            if (strcmp(parameter, "cmd") == 0) {
                int track = address / 256;
                int note = address & 255;
                if (track < MAX_TRACKS && note < MAX_TRACK_LENGTH) {
                    Note *target = &tracker->song.tracks[track].notes[note];
                    target->command = value;
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
        for (int row = 0; row < tracker->song.tracks[track].length; row++) {
            Note note = tracker->song.tracks[track].notes[row];
            Uint16  encodedNote = (track << 8) + row;
            if (note.note != NOTE_NONE) {
                fprintf(f, "note %04x %02x\n", encodedNote, note.note);
                fprintf(f, "patch %04x %02x\n", encodedNote, note.patch);
            }
            if (note.command != 0) {
                fprintf(f, "cmd %04x %03x\n", encodedNote, note.command & 0xFFF);
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

void resetChannelParams(Synth *synth, Uint8 channel) {
    synth_pitchOffset(synth, channel, 0);
    synth_frequencyModulation(synth, channel, 0,0);
}

void stopPlayback(Tracker *tracker) {
    if (player_isPlaying(tracker->player)) {
        player_stop(tracker->player);
        for (int i = 0; i < CHANNELS; i++) {
            synth_pitchGlideReset(tracker->synth, i);
        }

        tracker->rowOffset = player_getCurrentRow(tracker->player);
    }
    if (tracker->mode == STOP) {
        setMode(tracker, EDIT);
        for (int i = 0; i < CHANNELS; i++) {
            synth_noteOff(tracker->synth, i);
            resetChannelParams(tracker->synth, i);
        }
    } else {
        setMode(tracker, STOP);
        for (int i = 0; i < CHANNELS; i++) {
            synth_noteRelease(tracker->synth, i);
            resetChannelParams(tracker->synth, i);
        }
    }
}

void stopSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    stopPlayback(tracker);
}

void playPattern(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    stopPlayback(tracker);
    moveToFirstRow(tracker);
    player_start(tracker->player, &tracker->song);
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

void initCommandKeys() {
    tracker->keyToCommandCode[SDL_SCANCODE_0] = 0;
    tracker->keyToCommandCode[SDL_SCANCODE_1] = 1;
    tracker->keyToCommandCode[SDL_SCANCODE_2] = 2;
    tracker->keyToCommandCode[SDL_SCANCODE_3] = 3;
    tracker->keyToCommandCode[SDL_SCANCODE_4] = 4;
    tracker->keyToCommandCode[SDL_SCANCODE_5] = 5;
    tracker->keyToCommandCode[SDL_SCANCODE_6] = 6;
    tracker->keyToCommandCode[SDL_SCANCODE_7] = 7;
    tracker->keyToCommandCode[SDL_SCANCODE_8] = 8;
    tracker->keyToCommandCode[SDL_SCANCODE_9] = 9;
    tracker->keyToCommandCode[SDL_SCANCODE_A] = 10;
    tracker->keyToCommandCode[SDL_SCANCODE_B] = 11;
    tracker->keyToCommandCode[SDL_SCANCODE_C] = 12;
    tracker->keyToCommandCode[SDL_SCANCODE_D] = 13;
    tracker->keyToCommandCode[SDL_SCANCODE_E] = 14;
    tracker->keyToCommandCode[SDL_SCANCODE_F] = 15;
}

void initKeyHandler() {
    memset(tracker->keyHandler, 0, sizeof(KeyHandler*)*256);
    tracker->keyHandler[SDL_SCANCODE_1] = editCommand;
    tracker->keyHandler[SDL_SCANCODE_4] = editCommand;
    tracker->keyHandler[SDL_SCANCODE_8] = editCommand;
    tracker->keyHandler[SDL_SCANCODE_A] = editCommand;
    tracker->keyHandler[SDL_SCANCODE_F] = editCommand;

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
    tracker->keyHandler[SDL_SCANCODE_F9] = previousPatch;
    tracker->keyHandler[SDL_SCANCODE_F10] = nextPatch;
    tracker->keyHandler[SDL_SCANCODE_F12] = saveSong;

    tracker->keyHandler[SDL_SCANCODE_TAB] = previousOrNextColumn;
    tracker->keyHandler[SDL_SCANCODE_LEFT] = previousColumn;
    tracker->keyHandler[SDL_SCANCODE_RIGHT] = nextColumn;
    tracker->keyHandler[SDL_SCANCODE_RETURN] = noteOff;
    tracker->keyHandler[SDL_SCANCODE_RCTRL] = playPattern;
    tracker->keyHandler[SDL_SCANCODE_SPACE] = stopSong;
}

void tracker_close(Tracker *tracker) {
    if (NULL != tracker) {
        if (tracker->player != NULL) {
            player_close(tracker->player);
            tracker->player = NULL;
        }
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
    tracker->song.bpm = 118;
    tracker->stepping = 1;
    tracker->patch = 1;

    if (NULL == (tracker->synth = synth_init(CHANNELS))) {
        tracker_close(tracker);
        return NULL;
    }
    if (NULL == (tracker->player = player_init(tracker->synth, CHANNELS))) {
        tracker_close(tracker);
        return NULL;
    }

    return tracker;
}


int main(int argc, char* args[]) {
    SDL_Event event;

    Tracker *tracker = tracker_init();

    if (!screen_init(CHANNELS)) {
        screen_close();
        tracker_close(tracker);
        return 1;
    }

    clearSong(&tracker->song);
    initKeyHandler();
    initNotes();
    initCommandKeys();

    loadSongWithName(tracker, "song.pxm");

    //synth_test();
    //return 0;


    screen_setTrackData(0, &tracker->song.tracks[0]);
    screen_setTrackData(1, &tracker->song.tracks[1]);
    screen_setTrackData(2, &tracker->song.tracks[2]);
    screen_setTrackData(3, &tracker->song.tracks[3]);

    Instrument instr1 = {
            .attack = 0,
            .decay = 4,
            .sustain = 50,
            .release = 60,
            .waves = {
                    {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0,
                            .pwm = 5,
                            .dutyCycle = 108,
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = PWM,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr2 = {
            .attack = 0,
            .decay = 2,
            .sustain = 20,
            .release = 60,
            .waves = {
                    {
                            .waveform = LOWPASS_PULSE,
                            .note = 0,
                            .length = 0
                    },{
                            .waveform = LOWPASS_PULSE,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = LOWPASS_PULSE,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr3 = {
            .attack = 0,
            .decay = 20,
            .sustain = 100,
            .release = 20,
            .waves = {
                    {
                            .waveform = NOISE,
                            .note = 0,
                            .length = 10
                    },{
                            .waveform = PWM,
                            .note = 0,
                            .length = 30,
                            .pwm = 0,
                            .dutyCycle = 128
                    }, {
                            .waveform = NOISE,
                            .note = 0,
                            .length = 0
                    }
            }
    };

    Instrument instr4 = {
            .attack = 0,
            .decay = 1,
            .sustain = 60,
            .release = 40,
            .waves = {
                    {
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    },{
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    }, {
                            .waveform = LOWPASS_SAW,
                            .note = 0,
                            .length = 0
                    }
            }
    };


    synth_loadPatch(tracker->synth, 1, &instr1);
    synth_loadPatch(tracker->synth, 2, &instr2);
    synth_loadPatch(tracker->synth, 3, &instr3);
    synth_loadPatch(tracker->synth, 4, &instr4);

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
        if (player_isPlaying(tracker->player)) {
            screen_setRowOffset(player_getCurrentRow(tracker->player));
        } else {
            screen_setRowOffset(tracker->rowOffset);
        }
        screen_update();
        SDL_Delay(5);
    }
    stopPlayback(tracker);
    screen_close();
    tracker_close(tracker);
    return 0;
}
