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
#include "keyhandler.h"

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
#define SUBCOLUMNS 4

typedef struct _Tracker Tracker;

typedef struct _Tracker {
    Synth *synth;
    Player *player;
    Keyhandler *keyhandler;
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

/**
 * Predicate functions
 */

bool predicate_isEditMode(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    return tracker->mode == EDIT;
}

bool predicate_isStopped(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    return tracker->mode == STOP;
}

bool predicate_isPlaying(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    return tracker->mode == PLAY;
}


bool predicate_isEditOnCommandColumn(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    if (!predicate_isEditMode(tracker) || tracker->currentColumn < 1) {
        return false;
    }
    return true;
}

bool predicate_isEditOnNoteColumn(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    if (!predicate_isEditMode(tracker) || tracker->currentColumn > 0) {
        return false;
    }
    return true;
}

bool predicate_isNotEditMode(void *userData) {
    return !predicate_isEditMode(userData);
}

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

void moveHome(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveToFirstRow((Tracker*)userData);
}

void moveEnd(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    tracker->rowOffset = TRACK_LENGTH-1;
}

void moveUpSteps(Tracker *tracker, int steps) {
    tracker->rowOffset-=steps;
    if (tracker->rowOffset < 0) {
        tracker->rowOffset += TRACK_LENGTH;
    }
}


void moveUp(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveUpSteps((Tracker*)userData, 1);
}

void moveUpMany(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    moveUpSteps((Tracker*)userData, 16);
}

void moveDownSteps(Tracker *tracker, int steps) {
    tracker->rowOffset+=steps;
    if (tracker->rowOffset > TRACK_LENGTH-1) {
        tracker->rowOffset -= TRACK_LENGTH;
    }
}


void moveDownMany(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps((Tracker*)userData, 16);
}

void moveDown(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    moveDownSteps((Tracker*)userData, 1);
}


void setMode(Tracker *tracker, Trackermode modeToSet) {
    tracker->mode = modeToSet;
    screen_setTrackermode(tracker->mode);
};


void decreaseStepping(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    if (tracker->stepping == 0) {
        return;
    }
    tracker->stepping--;
}

void increaseStepping(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->stepping > 7) {
        return;
    }
    tracker->stepping++;
}

void editCommand(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    int nibblePos = (3-tracker->currentColumn) * 4;
    Uint16 mask = 0XFFF - (0xF << nibblePos);

    Note *note = getCurrentNote(tracker);
    note->command &= mask;
    note->command |= (tracker->keyToCommandCode[scancode] << nibblePos);

    moveDownSteps(tracker, tracker->stepping);
}

void muteTrack(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

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

void playNote(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->keyToNote[scancode] > -1) {
        Sint8 note = tracker->keyToNote[scancode] + 12 * tracker->octave;
        if (note > 95) {
            return;
        }
        synth_noteTrigger(tracker->synth, tracker->currentTrack, tracker->patch, note);
        //SDL_AddTimer(100, stopJamming, tracker);
    }
}

void updateNote(void *userData, SDL_Scancode scancode,SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->keyToNote[scancode] > -1) {
        Sint8 note = tracker->keyToNote[scancode] + 12 * tracker->octave;
        if (note > 95) {
            return;
        }
        getCurrentNote(tracker)->note = note;
        getCurrentNote(tracker)->patch = tracker->patch;
        moveDownSteps(tracker, tracker->stepping);
        synth_noteTrigger(tracker->synth, tracker->currentTrack, tracker->patch, note);

    }
}

void skipRow(Tracker *tracker, SDL_Scancode scancode,SDL_Keymod keymod) {
    if (predicate_isEditMode(tracker)) {
        moveDownSteps(tracker, tracker->stepping);
    } else {
        synth_noteRelease(tracker->synth, tracker->currentTrack);
    }
}



void moveSongPosHome(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    tracker->currentPos = 0 ;
    gotoSongPos(tracker, tracker->currentPos);
}

void moveSongPosEnd(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    tracker->currentPos = -1;
    for (int i = 0; i < MAX_PATTERNS; i++) {
        if (tracker->song.arrangement[i].pattern < 0) {
            break;
        }
        tracker->currentPos++;
        printf("pos = %d\n", tracker->currentPos);
    }

    gotoSongPos(tracker, tracker->currentPos);
}

int getSongLength(Song *song) {
    for (int i = 0; i < MAX_PATTERNS; i++) {
        if (song->arrangement[i].pattern < 0) {
            return i;
        }
    }
    return MAX_PATTERNS;
}

void moveSongPosUp(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    if (tracker->currentPos == 0) {
        return;
    }
    tracker->currentPos--;
    gotoSongPos(tracker, tracker->currentPos);
}

void moveSongPosDown(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    if (tracker->currentPos >= MAX_PATTERNS-1) {
        return;
    }
    /* Only go past last arrangement if we press shift = add pattern */
    if (tracker->currentPos < getSongLength(&tracker->song)-1 || (keymod & KMOD_SHIFT)) {
        tracker->currentPos++;
        gotoSongPos(tracker, tracker->currentPos);
    }
}

void insertSongPos(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    for (int i = MAX_PATTERNS-1 ; i > tracker->currentPos; i--) {
        tracker->song.arrangement[i].pattern = tracker->song.arrangement[i-1].pattern;
    }
    tracker->currentPos++;
    gotoSongPos(tracker, tracker->currentPos);
}

void deletePreviousSongPos(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

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

void deleteCurrentSongPos(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (getSongLength(&tracker->song) == 1) {
        tracker->song.arrangement[tracker->currentPos].pattern = 0;
        return;
    }

    for (int i = tracker->currentPos ; i < MAX_PATTERNS-1; i++) {
        tracker->song.arrangement[i].pattern = tracker->song.arrangement[i+1].pattern;
    }
    tracker->song.arrangement[MAX_PATTERNS-1].pattern = -1;
    if (tracker->currentPos >= getSongLength(&tracker->song)) {
        tracker->currentPos--;
    }
    gotoSongPos(tracker, tracker->currentPos);
}


void deleteNoteOrCommand(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->currentColumn == 0) {
        getCurrentNote(tracker)->note = NOTE_NONE;
        getCurrentNote(tracker)->patch = 0;
    } else {
        getCurrentNote(tracker)->command = 0;
    }
    moveDownSteps(tracker, tracker->stepping);
}

void deleteNoteAndCommand(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    getCurrentNote(tracker)->note = NOTE_NONE;
    getCurrentNote(tracker)->patch = 0;
    getCurrentNote(tracker)->command = 0;
    moveDownSteps(tracker, tracker->stepping);
}

void playNoteOff(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    synth_noteRelease(tracker->synth, tracker->currentTrack);
}

void insertNoteOff(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    getCurrentNote(tracker)->note = NOTE_OFF;
    moveDownSteps(tracker, tracker->stepping);
}

void previousOctave(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->octave == 0) {
        return;
    }
    tracker->octave--;
    screen_setOctave(tracker->octave);
}

void nextOctave(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->octave >= 6) {
        return;
    }
    tracker->octave++;
    screen_setOctave(tracker->octave);
}

void copyTrack(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    memcpy(&tracker->trackClipboard, getCurrentTrack(tracker), sizeof(Track));
}

void cutTrack(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    copyTrack(tracker, scancode, keymod);
    track_clear(getCurrentTrack(tracker));
}

void pasteTrack(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    memcpy(getCurrentTrack(tracker), &tracker->trackClipboard, sizeof(Track));
}


void gotoNextTrack(Tracker *tracker) {
    if (tracker->currentTrack < CHANNELS-1) {
        tracker->currentTrack++;
        tracker->currentColumn = 0;
        screen_setSelectedTrack(tracker->currentTrack);
    }
}

void nextTrack(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    gotoNextTrack((Tracker*)userData);
}


void gotoPreviousTrack(Tracker *tracker) {
    if (tracker->currentTrack > 0) {
        tracker->currentTrack--;
        tracker->currentColumn = 0;
        screen_setSelectedTrack(tracker->currentTrack);
    }
}
void previousTrack(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    gotoPreviousTrack((Tracker*)userData);
}


void previousPattern(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->song.arrangement[tracker->currentPos].pattern <= 0) {
        return;
    }
    tracker->song.arrangement[tracker->currentPos].pattern--;
    gotoSongPos(tracker, tracker->currentPos);
}

void previousColumn(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->currentColumn > 0) {
        tracker->currentColumn--;
    } else if (tracker->currentTrack > 0) {
        gotoPreviousTrack(tracker);
        tracker->currentColumn = SUBCOLUMNS-1;
    }
    screen_setSelectedColumn(tracker->currentColumn);
}

void nextPattern(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->song.arrangement[tracker->currentPos].pattern >= MAX_PATTERNS-1) {
        return;
    }
    tracker->song.arrangement[tracker->currentPos].pattern++;
    gotoSongPos(tracker, tracker->currentPos);
}

void nextColumn(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->currentColumn < SUBCOLUMNS-1) {
        tracker->currentColumn++;
        screen_setSelectedColumn(tracker->currentColumn);
    } else {
        gotoNextTrack(tracker);
    }
}

void insertEntity(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
}

void previousPatch(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->patch <= 1) {
        tracker->patch = 255;
    } else {
        tracker->patch--;
    }
}

void nextPatch(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->patch == 255) {
        tracker->patch = 1;
    } else {
        tracker->patch++;
    }
}


void registerNote(Tracker *tracker, SDL_Scancode scancode, Sint8 note) {
    tracker->keyToNote[scancode] = note;
    keyhandler_register(tracker->keyhandler, scancode, 0, predicate_isEditOnNoteColumn, updateNote, tracker);
    keyhandler_register(tracker->keyhandler, scancode, 0, predicate_isNotEditMode, playNote, tracker);
}


void loadSong(Tracker *tracker, SDL_Scancode scancode, SDL_Keymod keymod) {
    song_clear(&tracker->song);
    if (!persist_loadSongWithName(&tracker->song, "song.pxm")) {
        screen_setStatusMessage("Could not open song.pxm");
    }

}

void saveSong(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (persist_saveSongWithName((Song*)userData, "song.pxm")) {
        screen_setStatusMessage("Successfully saved song.pxm");
    } else {
        screen_setStatusMessage("Could not save song.pxm");
    }
}

void resetChannelParams(Synth *synth, Uint8 channel) {
    synth_pitchOffset(synth, channel, 0);
    synth_frequencyModulation(synth, channel, 0,0);
}

void startEditing(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    for (int i = 0; i < CHANNELS; i++) {
        synth_noteOff(tracker->synth, i);
        resetChannelParams(tracker->synth, i);
    }
    setMode(tracker, EDIT);
}

void stopEditing(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    for (int i = 0; i < CHANNELS; i++) {
        synth_noteRelease(tracker->synth, i);
        resetChannelParams(tracker->synth, i);
    }
    setMode(tracker, STOP);
}

void stopPlayback(Tracker *tracker) {
    player_stop(tracker->player);
    for (int i = 0; i < CHANNELS; i++) {
        synth_pitchGlideReset(tracker->synth, i);
        synth_frequencyModulation(tracker->synth, i, 0, 0);
        synth_noteRelease(tracker->synth, i);
    }

    tracker->rowOffset = player_getCurrentRow(tracker->player);
}

void stopPlaying(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    stopPlayback((Tracker*)userData);
    setMode(tracker, STOP);
}

void playPattern(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    stopPlayback(tracker);
    moveToFirstRow(tracker);
    player_start(tracker->player, &tracker->song, tracker->currentPos);
    setMode(tracker, PLAY);
};


void initNotes(Tracker *tracker) {
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

void initCommandKeys(Tracker *tracker) {
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


void initKeyMappings(Tracker *tracker) {
    Keyhandler *kh = tracker->keyhandler;

    /* Song commands */
    keyhandler_register(kh, SDL_SCANCODE_UP, KM_ALT, NULL, moveSongPosUp, tracker);
    keyhandler_register(kh, SDL_SCANCODE_DOWN, KM_ALT, NULL, moveSongPosDown, tracker);
    keyhandler_register(kh, SDL_SCANCODE_DOWN, KM_SHIFT_ALT, NULL, moveSongPosDown, tracker);
    keyhandler_register(kh, SDL_SCANCODE_HOME, KM_ALT, NULL, moveSongPosHome, tracker);
    keyhandler_register(kh, SDL_SCANCODE_END, KM_ALT, NULL, moveSongPosEnd, tracker);

    keyhandler_register(kh, SDL_SCANCODE_LEFT, KM_ALT, NULL, previousPattern, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RIGHT, KM_ALT, NULL, nextPattern, tracker);

    keyhandler_register(kh, SDL_SCANCODE_INSERT, KM_ALT, NULL, insertSongPos, tracker);
    keyhandler_register(kh, SDL_SCANCODE_BACKSPACE, KM_ALT, NULL, deletePreviousSongPos, tracker);
    keyhandler_register(kh, SDL_SCANCODE_DELETE, KM_ALT, NULL, deleteCurrentSongPos, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F12, 0, NULL, saveSong, &tracker->song);

    keyhandler_register(kh, SDL_SCANCODE_RCTRL, KM_CTRL, NULL, playPattern, tracker);

    /* Track commands */

    keyhandler_register(kh, SDL_SCANCODE_Z, KM_ALT, NULL, muteTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_X, KM_ALT, NULL, muteTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_C, KM_ALT, NULL, muteTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_V, KM_ALT, NULL, muteTrack, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F3, KM_SHIFT, predicate_isEditMode, cutTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F4, KM_SHIFT, predicate_isEditMode, copyTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F5, KM_SHIFT, predicate_isEditMode, pasteTrack, tracker);

    keyhandler_register(kh, SDL_SCANCODE_DELETE, KM_SHIFT, predicate_isEditMode, deleteNoteAndCommand, tracker);

    keyhandler_register(kh, SDL_SCANCODE_0, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_1, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_2, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_3, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_4, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_5, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_6, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_7, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_8, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_9, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_A, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_B, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_C, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_D, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_E, 0, predicate_isEditOnCommandColumn, editCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F, 0, predicate_isEditOnCommandColumn, editCommand, tracker);

    keyhandler_register(kh, SDL_SCANCODE_DELETE, 0, predicate_isEditMode, deleteNoteOrCommand, tracker);

    keyhandler_register(kh, SDL_SCANCODE_NONUSBACKSLASH, 0, predicate_isEditOnNoteColumn, insertNoteOff, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RETURN, 0, predicate_isEditOnNoteColumn, insertNoteOff, tracker);

    keyhandler_register(kh, SDL_SCANCODE_NONUSBACKSLASH, 0, predicate_isNotEditMode, playNoteOff, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RETURN, 0, predicate_isNotEditMode, playNoteOff, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F1, 0, NULL, previousOctave, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F2, 0, NULL, nextOctave, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F9, 0, NULL, previousPatch, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F10, 0, NULL ,  nextPatch, tracker);


    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isEditMode, stopEditing, tracker);
    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isStopped, startEditing, tracker);
    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isPlaying, stopPlaying, tracker);

    keyhandler_register(kh, SDL_SCANCODE_INSERT, 0, predicate_isEditMode, insertEntity, tracker);

    /** Pattern movement */
    keyhandler_register(kh, SDL_SCANCODE_UP, 0, NULL, moveUp, tracker);
    keyhandler_register(kh, SDL_SCANCODE_DOWN, 0, NULL, moveDown, tracker);
    keyhandler_register(kh, SDL_SCANCODE_PAGEUP, 0, NULL, moveUpMany, tracker);
    keyhandler_register(kh, SDL_SCANCODE_PAGEDOWN, 0, NULL, moveDownMany, tracker);
    keyhandler_register(kh, SDL_SCANCODE_LEFT, 0, NULL, previousColumn, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RIGHT, 0, NULL, nextColumn, tracker);

    keyhandler_register(kh, SDL_SCANCODE_HOME, 0, NULL, moveHome, tracker);
    keyhandler_register(kh, SDL_SCANCODE_END, 0, NULL, moveEnd, tracker);

    keyhandler_register(kh, SDL_SCANCODE_GRAVE, KM_SHIFT, NULL, decreaseStepping, tracker);
    keyhandler_register(kh, SDL_SCANCODE_GRAVE, 0, NULL, increaseStepping, tracker);

    keyhandler_register(kh, SDL_SCANCODE_TAB, KM_SHIFT, NULL, previousTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_TAB, 0, NULL, nextTrack, tracker);

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
    Tracker *tracker = calloc(1, sizeof(Tracker));
    tracker->song.bpm = 118;
    tracker->stepping = 1;
    tracker->patch = 1;

    if (
            NULL == (tracker->synth = synth_init(CHANNELS)) ||
            NULL == (tracker->player = player_init(tracker->synth, CHANNELS)) ||
            NULL == (tracker->keyhandler = keyhandler_init())
    ) {
        tracker_close(tracker);
        return NULL;
    }
    initKeyMappings(tracker);
    initNotes(tracker);
    initCommandKeys(tracker);

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
                keyhandler_handle(tracker->keyhandler, event.key.keysym.scancode, keymod);
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
