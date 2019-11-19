#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#include "screen.h"
#include "synth.h"
#include "track.h"
#include "pattern.h"
#include "song.h"
#include "trackermode.h"
#include "player.h"
#include "note.h"
#include "instrument.h"
#include "defaultsettings.h"
#include "persist.h"

/*
https://milkytracker.titandemo.org/docs/FT2.pdf
https://github.com/leafo/goattracker2/blob/master/morphos/goattracker.guide
https://pages.mtu.edu/~suits/NoteFreqCalcs.html
https://github.com/cmatsuoka/tracker-history/blob/master/reference/amiga/noisetracker/NoiseTracker.doc
https://en.wikipedia.org/wiki/WAV
https://www.desmos.com/calculator
http://coppershade.org/articles/More!/Topics/Protracker_Effect_Commands/
http://coppershade.org/helpers/DOCS/protracker23.readme.txt

*/

#define CHANNELS TRACKS_PER_PATTERN
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
    Track trackClipboard;

    Sint8 rowOffset;
    Uint16 currentPos;
    Uint8 currentTrack;
    Uint16 currentPattern;
    Song song;
    Trackermode mode;
    Uint8 currentColumn;
} Tracker;

Tracker *tracker;

Pattern *getCurrentPattern(Tracker *tracker) {
    return &tracker->song.patterns[tracker->currentPattern];
}

Track *getCurrentTrack(Tracker *tracker) {
    return &getCurrentPattern(tracker)->tracks[tracker->currentTrack];
}

Note *getCurrentNote(Tracker *tracker) {
    return &getCurrentTrack(tracker)->notes[tracker->rowOffset];
}


void gotoSongPos(Tracker *tracker, Uint16 pos) {
    tracker->currentPos = pos;
    if (tracker->song.arrangement[pos].pattern < 0) {
        tracker->song.arrangement[pos].pattern = 0;
    }

    tracker->currentPattern = tracker->song.arrangement[pos].pattern;
    screen_setSongPos(pos);
    for (int i = 0; i < TRACKS_PER_PATTERN; i++) {
        screen_setTrackData(i, &getCurrentPattern(tracker)->tracks[i]);
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
void moveSongPosUp(Tracker *tracker) {
    if (tracker->currentPos == 0) {
        return;
    }
    tracker->currentPos--;
    gotoSongPos(tracker, tracker->currentPos);
}


void moveSongPosOrUp(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_ALT) {
        moveSongPosUp(tracker);
        return;
    }
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

void moveSongPosDown(Tracker *tracker, SDL_Keymod keymod) {
    if (tracker->currentPos >= MAX_PATTERNS-1) {
        return;
    }
    /* Only go past last arrangement if we press shift = add pattern */
    if (tracker->song.arrangement[tracker->currentPos+1].pattern != -1 || (keymod & KMOD_SHIFT)) {
        tracker->currentPos++;
        gotoSongPos(tracker, tracker->currentPos);
    }
}


void moveDownMany(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps(tracker, 16);
}

void moveSongPosOrDown(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keymod & KMOD_ALT) {
        moveSongPosDown(tracker, keymod);
        return;
    }
    moveDownSteps(tracker, 1);
}

bool isEditMode(Tracker *tracker) {
    return tracker->mode == EDIT;
}

void setMode(Tracker *tracker, Trackermode modeToSet) {
    tracker->mode = modeToSet;
    screen_setTrackermode(tracker->mode);
};


void increaseStepping(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (keymod & KMOD_SHIFT) {
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

void editCommand(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (!isEditMode(tracker) || tracker->currentColumn < 1) {
        return;
    }
    int nibblePos = (3-tracker->currentColumn) * 4;
    Uint16 mask = 0XFFF - (0xF << nibblePos);

    Note *note = getCurrentNote(tracker);
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

void deleteSongPos(Tracker *tracker) {
    if (tracker->currentPos == 0) {
        return;
    }
    for (int i = tracker->currentPos-1 ; i < MAX_PATTERNS-1; i++) {
        tracker->song.arrangement[i].pattern = tracker->song.arrangement[i+1].pattern;
    }
    tracker->song.arrangement[MAX_PATTERNS-1].pattern = -1;
    tracker->currentPos--;
    gotoSongPos(tracker, tracker->currentPos);
}

void deleteNote(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode(tracker)) {
        if (keymod & KMOD_SHIFT) {
            printf("DERPES\n");
            getCurrentNote(tracker)->note = NOTE_NONE;
            getCurrentNote(tracker)->patch = 0;
            getCurrentNote(tracker)->command = 0;
        } else if (tracker->currentColumn == 0) {
            getCurrentNote(tracker)->note = NOTE_NONE;
            getCurrentNote(tracker)->patch = 0;
        } else if (tracker->currentColumn > 0) {
            getCurrentNote(tracker)->command = 0;
        }
        moveDownSteps(tracker, tracker->stepping);
    }
}

void deleteNoteOrSongPos(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & (KMOD_ALT | KMOD_SHIFT)) {
        deleteSongPos(tracker);
        return;
    } else {
        deleteNote(tracker, scancode, keymod);
    }
}

void noteOff(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (isEditMode(tracker)) {
        getCurrentNote(tracker)->note = NOTE_OFF;
        moveDownSteps(tracker, tracker->stepping);
    }
}

void previousOctave(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->octave == 0) {
        return;
    }
    tracker->octave--;
    screen_setOctave(tracker->octave);
}

void nextOctave(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->octave >= 6) {
        return;
    }
    tracker->octave++;
    screen_setOctave(tracker->octave);
}

void copyTrack(Tracker *tracker) {
    memcpy(&tracker->trackClipboard, getCurrentTrack(tracker), sizeof(Track));
}

void cutTrack(Tracker *tracker) {
    copyTrack(tracker);
    track_clear(getCurrentTrack(tracker));
}

void cutData(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_SHIFT) {
        cutTrack(tracker);
    }
}

void copyData(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_SHIFT) {
        copyTrack(tracker);
    }
}

void pasteTrack(Tracker *tracker) {
    memcpy(getCurrentTrack(tracker), &tracker->trackClipboard, sizeof(Track));
}

void pasteData(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_SHIFT) {
        pasteTrack(tracker);
    }
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
    if (keymod & KMOD_SHIFT) {
        gotoPreviousTrack(tracker);
    } else {
        gotoNextTrack(tracker);
    }
}

void previousPattern(Tracker *tracker) {
    if (tracker->song.arrangement[tracker->currentPos].pattern <= 0) {
        return;
    }
    tracker->song.arrangement[tracker->currentPos].pattern--;
    gotoSongPos(tracker, tracker->currentPos);
}

void previousColumnOrPreviousPattern(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_ALT) {
        previousPattern(tracker);
        return;
    }
    if (tracker->currentColumn > 0) {
        tracker->currentColumn--;
    } else if (tracker->currentTrack > 0) {
        gotoPreviousTrack(tracker);
        tracker->currentColumn = SUBCOLUMNS-1;
    }
    screen_setSelectedColumn(tracker->currentColumn);
}

void nextPattern(Tracker *tracker) {
    if (tracker->song.arrangement[tracker->currentPos].pattern >= MAX_PATTERNS-1) {
        return;
    }
    tracker->song.arrangement[tracker->currentPos].pattern++;
    gotoSongPos(tracker, tracker->currentPos);
}

void nextColumnOrNextPattern(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_ALT) {
        nextPattern(tracker);
        return;
    }
    if (tracker->currentColumn < SUBCOLUMNS-1) {
        tracker->currentColumn++;
        screen_setSelectedColumn(tracker->currentColumn);
    } else {
        gotoNextTrack(tracker);
    }
}

void insertEntity(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (keymod & KMOD_ALT) {

    }
}

void previousPatch(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->patch <= 1) {
        tracker->patch = 255;
    } else {
        tracker->patch--;
    }
}

void nextPatch(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (tracker->patch == 255) {
        tracker->patch = 1;
    } else {
        tracker->patch++;
    }
}


void registerNote(Tracker *tracker, SDL_Scancode scancode, Sint8 note) {
    tracker->keyToNote[scancode] = note;
    tracker->keyHandler[scancode] = playOrUpdateNote;
}


void loadSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    song_clear(&tracker->song);
    if (!persist_loadSongWithName(&tracker->song, "song.pxm")) {
        screen_setStatusMessage("Could not open song.pxm");
    }

}

void saveSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (persist_saveSongWithName(&tracker->song, "song.pxm")) {
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
    player_start(tracker->player, &tracker->song, tracker->currentPos);
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

    tracker->keyHandler[SDL_SCANCODE_UP] = moveSongPosOrUp;
    tracker->keyHandler[SDL_SCANCODE_DOWN] = moveSongPosOrDown;
    tracker->keyHandler[SDL_SCANCODE_PAGEUP] = moveUpMany;
    tracker->keyHandler[SDL_SCANCODE_PAGEDOWN] = moveDownMany;
    tracker->keyHandler[SDL_SCANCODE_BACKSPACE] = deleteNoteOrSongPos;
    tracker->keyHandler[SDL_SCANCODE_DELETE] = deleteNote;
    tracker->keyHandler[SDL_SCANCODE_HOME] = moveHome;
    tracker->keyHandler[SDL_SCANCODE_END] = moveEnd;
    tracker->keyHandler[SDL_SCANCODE_GRAVE] = increaseStepping;
    tracker->keyHandler[SDL_SCANCODE_NONUSBACKSLASH] = noteOff;
    tracker->keyHandler[SDL_SCANCODE_F1] = previousOctave;
    tracker->keyHandler[SDL_SCANCODE_F2] = nextOctave;
    tracker->keyHandler[SDL_SCANCODE_F3] = cutData;
    tracker->keyHandler[SDL_SCANCODE_F4] = copyData;
    tracker->keyHandler[SDL_SCANCODE_F5] = pasteData;

    tracker->keyHandler[SDL_SCANCODE_F9] = previousPatch;
    tracker->keyHandler[SDL_SCANCODE_F10] = nextPatch;
    tracker->keyHandler[SDL_SCANCODE_F12] = saveSong;

    tracker->keyHandler[SDL_SCANCODE_TAB] = previousOrNextColumn;
    tracker->keyHandler[SDL_SCANCODE_LEFT] = previousColumnOrPreviousPattern;
    tracker->keyHandler[SDL_SCANCODE_RIGHT] = nextColumnOrNextPattern;
    tracker->keyHandler[SDL_SCANCODE_RETURN] = noteOff;
    tracker->keyHandler[SDL_SCANCODE_RCTRL] = playPattern;
    tracker->keyHandler[SDL_SCANCODE_SPACE] = stopSong;

    tracker->keyHandler[SDL_SCANCODE_INSERT] = insertEntity;
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

    song_clear(&tracker->song);
    initKeyHandler();
    initNotes();
    initCommandKeys();

    persist_loadSongWithName(&tracker->song, "song.pxm");

    screen_setArrangementData(tracker->song.arrangement);
    gotoSongPos(tracker, 0);

    //synth_test();
    //return 0;

    defaultsettings_createInstruments(tracker->song.instruments);
    for (int i = 1; i < MAX_INSTRUMENTS; i++) {
        synth_loadPatch(tracker->synth, i, &tracker->song.instruments[i]);
    }

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
            Uint16 playPos = player_getSongPos(tracker->player);
            if (playPos != tracker->currentPos) {
                tracker->currentPos = playPos;
                gotoSongPos(tracker, playPos);
            }
        } else {
            screen_setRowOffset(tracker->rowOffset);
        }
        screen_selectPatch(tracker->patch, &tracker->song.instruments[tracker->patch]);
        screen_update();
        SDL_Delay(5);
    }
    stopPlayback(tracker);
    screen_close();
    tracker_close(tracker);
    return 0;
}
