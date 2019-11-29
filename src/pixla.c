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
#define MUTE_SC_1 SDL_SCANCODE_F5
#define MUTE_SC_2 SDL_SCANCODE_F6
#define MUTE_SC_3 SDL_SCANCODE_F7
#define MUTE_SC_4 SDL_SCANCODE_F8

typedef struct _Tracker Tracker;

typedef struct {
    char attack[6];
    char decay[6];
    char sustain[6];
    char release[6];
    char note[MAX_WAVESEGMENTS][6];
    char length[MAX_WAVESEGMENTS][6];
    char dutyCycle[MAX_WAVESEGMENTS][6];
    char pwm[MAX_WAVESEGMENTS][6];
    char filter[MAX_WAVESEGMENTS][6];
    char waveVolume[MAX_WAVESEGMENTS][6];
    char carrierFrequency[MAX_WAVESEGMENTS][7];
} InstrumentSettingsData;

typedef struct _Tracker {
    Synth *synth;
    Player *player;
    Keyhandler *keyhandler;
    SettingsComponent *instrumentSettings;
    InstrumentSettingsData instrumentSettingsData;
    Sint8 keyToNote[256];
    Uint8 keyToCommandCode[256];
    Uint8 stepping;
    Uint8 octave;
    Uint8 patch;
    Track trackClipboard;
    Pattern patternClipboard;

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

bool predicate_isNotInstrumentMode(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    return tracker->mode != EDIT_INSTRUMENT;
}

bool predicate_isInstrumentMode(void *userData) {
    Tracker *tracker = (Tracker*)userData;
    return tracker->mode == EDIT_INSTRUMENT;
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

    if (scancode == MUTE_SC_1) {
        bool mute = !synth_isChannelMuted(tracker->synth, 0);
        synth_muteChannel(tracker->synth, 0, mute);
        screen_setChannelMute(0, mute);
    } else if (scancode == MUTE_SC_2) {
        bool mute = !synth_isChannelMuted(tracker->synth, 1);
        synth_muteChannel(tracker->synth, 1, mute);
        screen_setChannelMute(1, mute);
    } else if (scancode == MUTE_SC_3) {
        bool mute = !synth_isChannelMuted(tracker->synth, 2);
        synth_muteChannel(tracker->synth, 2, mute);
        screen_setChannelMute(2, mute);
    } else if (scancode == MUTE_SC_4) {
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
        //printf("pos = %d\n", tracker->currentPos);
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

void clearNote(Note *note) {
    note->note = NOTE_NONE;
    note->patch = 0;
    note->command = 0;
}

void copyNote(Note *target, Note *src) {
    target->note = src->note;
    target->patch = src->patch;
    target->command = src->command;
}

void deletePreviousNote(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->rowOffset == 0) {
        return;
    }

    for (int i = tracker->rowOffset; i < TRACK_LENGTH; i++) {
        Note *target = &getCurrentTrack(tracker)->notes[i-1];
        Note *src = &getCurrentTrack(tracker)->notes[i];
        copyNote(target, src);
    }
    clearNote(&getCurrentTrack(tracker)->notes[TRACK_LENGTH-1]);
    moveUpSteps(tracker, 1);
}

void insertBeforeNote(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    for (int i = TRACK_LENGTH-1; i > tracker->rowOffset; i--) {
        Note *target = &getCurrentTrack(tracker)->notes[i];
        Note *src = &getCurrentTrack(tracker)->notes[i-1];
        copyNote(target, src);
    }
    clearNote(getCurrentNote(tracker));
    moveDownSteps(tracker, 1);
}


void clearNoteOrCommand(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    if (tracker->currentColumn == 0) {
        getCurrentNote(tracker)->note = NOTE_NONE;
        getCurrentNote(tracker)->patch = 0;
    } else {
        getCurrentNote(tracker)->command = 0;
    }
    moveDownSteps(tracker, tracker->stepping);
}

void clearNoteAndCommand(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    clearNote(getCurrentNote(tracker));
    moveDownSteps(tracker, tracker->stepping);
}

void deleteNoteAndCommand(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;

    for (int i = tracker->rowOffset+1; i < TRACK_LENGTH; i++) {
        Note *target = &getCurrentTrack(tracker)->notes[i-1];
        Note *src = &getCurrentTrack(tracker)->notes[i];
        copyNote(target, src);
    }
    clearNote(&getCurrentTrack(tracker)->notes[TRACK_LENGTH-1]);
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

void _transposeTrackDown(Track *track) {
    for (int i = 0; i < TRACK_LENGTH; i++) {
        Note *note = &track->notes[i];
        if (note->note > 0 && note->note<96) {
            note->note--;
        }
    }
}

void _transposeTrackUp(Track *track) {
    for (int i = 0; i < TRACK_LENGTH; i++) {
        Note *note = &track->notes[i];
        if (note->note < 96) {
            note->note++;
        }
    }
}

void transposeTrackDown(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    _transposeTrackDown(getCurrentTrack(tracker));
}

void transposeTrackUp(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    _transposeTrackUp(getCurrentTrack(tracker));
}

void transposePatternDown(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    for (int i = 0; i < CHANNELS; i++) {
        _transposeTrackDown(&getCurrentPattern(tracker)->tracks[i]);
    }
}

void transposePatternUp(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    for (int i = 0; i < CHANNELS; i++) {
        _transposeTrackUp(&getCurrentPattern(tracker)->tracks[i]);
    }
}



/*
 * Track clipboard operations
 */
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
    int notesToPaste = TRACK_LENGTH-tracker->rowOffset;
    printf("copy %d notes to position %d\n", notesToPaste, tracker->rowOffset);
    Track *track = getCurrentTrack(tracker);

    memcpy(&track->notes[tracker->rowOffset],
            &tracker->trackClipboard, notesToPaste*sizeof(Note)
    );
}

/*
 * Pattern clipboard operations
 */
void copyPattern(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    memcpy(&tracker->patternClipboard, getCurrentPattern(tracker), sizeof(Pattern));
}

void cutPattern(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    copyPattern(tracker, scancode, keymod);
    pattern_clear(getCurrentPattern(tracker));
}

void pastePattern(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    memcpy(getCurrentPattern(tracker), &tracker->patternClipboard, sizeof(Pattern));
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

void saveSong(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    if (persist_saveSongWithName((Song*)userData, "song.pxm")) {
        screen_setStatusMessage("Successfully saved song.pxm");
    } else {
        screen_setStatusMessage("Could not save song.pxm");
    }
}

void resetChannelParams(Synth *synth, Uint8 channel) {
    //synth_pitchOffset(synth, channel, 0);
    synth_pitchModulation(synth, channel, 0, NULL, 0);
    synth_frequencyModulation(synth, channel, 0,0);
    synth_amplitudeModulation(synth, channel, 0, 0);
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
        synth_amplitudeModulation(tracker->synth, i, 0, 0);
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

void setInstrumentMode(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    setMode(tracker, EDIT_INSTRUMENT);
}

void gotoPreviousSetting(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    settings_prev(tracker->instrumentSettings);
}

void gotoNextSetting(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    settings_next(tracker->instrumentSettings);
}

void decreaseSetting(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    settings_decrease(tracker->instrumentSettings);
    synth_loadPatch(tracker->synth, tracker->patch, &tracker->song.instruments[tracker->patch]);

}
void increaseSetting(void *userData, SDL_Scancode scancode, SDL_Keymod keymod) {
    Tracker *tracker = (Tracker*)userData;
    settings_increase(tracker->instrumentSettings);
    synth_loadPatch(tracker->synth, tracker->patch, &tracker->song.instruments[tracker->patch]);
}

Instrument *getCurrentInstrument(Tracker *tracker) {
    return &tracker->song.instruments[tracker->patch];
}

void instrDecreaseAttack(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->attack > 0) {
        instr->attack--;
    }
}

void instrIncreaseAttack(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->attack < 127) {
        instr->attack++;
    }
}

char *instrGetAttack(void *userData, int userIndex) {
    Tracker *tracker = (Tracker*)userData;
    sprintf(tracker->instrumentSettingsData.attack, "%d", tracker->song.instruments[tracker->patch].attack);
    return tracker->instrumentSettingsData.attack;
}

void instrDecreaseDecay(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->decay > 0) {
        instr->decay--;
    }
}

void instrIncreaseDecay(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->decay < 127) {
        instr->decay++;
    }
}

char *instrGetDecay(void *userData, int userIndex) {
    Tracker *tracker = (Tracker*)userData;
    sprintf(tracker->instrumentSettingsData.decay, "%d", tracker->song.instruments[tracker->patch].decay);
    return tracker->instrumentSettingsData.decay;
}


void instrDecreaseSustain(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->sustain > 0) {
        instr->sustain--;
    }
}

void instrIncreaseSustain(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->sustain < 127) {
        instr->sustain++;
    }
}

char *instrGetSustain(void *userData, int userIndex) {
    Tracker *tracker = (Tracker*)userData;
    sprintf(tracker->instrumentSettingsData.sustain, "%d", tracker->song.instruments[tracker->patch].sustain);
    return tracker->instrumentSettingsData.sustain;
}



void instrDecreaseRelease(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->release > 0) {
        instr->release--;
    }
}

void instrIncreaseRelease(void *userData, int userIndex) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->release < 127) {
        instr->release++;
    }
}

char *instrGetRelease(void *userData, int userIndex) {
    Tracker *tracker = (Tracker*)userData;
    sprintf(tracker->instrumentSettingsData.release, "%d", tracker->song.instruments[tracker->patch].release);
    return tracker->instrumentSettingsData.release;
}

void instrDecreaseWave(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].waveform == 0) {
        instr->waves[index].waveform = WAVEFORM_TYPES-1;
    } else {
        instr->waves[index].waveform--;
    }
}
void instrIncreaseWave(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);

    if (instr->waves[index].waveform == WAVEFORM_TYPES-1) {
        instr->waves[index].waveform = 0;
    } else {
        instr->waves[index].waveform++;
    }
}

char* instrGetWave(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);

    return instrument_getWaveformName(instr->waves[index].waveform);
}

void instrDecreaseNote(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].note > -128) {
        instr->waves[index].note--;
    }


}

void instrIncreaseNote(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].note < 95) {
        instr->waves[index].note++;
    }
}

char* instrGetNote(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    sprintf(tracker->instrumentSettingsData.note[index], "%d", instr->waves[index].note);
    return tracker->instrumentSettingsData.note[index];
}

void instrDecreaseLength(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].length > 0) {
        instr->waves[index].length--;
    }
}

void instrIncreaseLength(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].length < 32767) {
        instr->waves[index].length++;
    }
}

char* instrGetLength(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    sprintf(tracker->instrumentSettingsData.length[index], "%d", instr->waves[index].length);
    return tracker->instrumentSettingsData.length[index];
}

void instrDecreaseDutyCycle(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].dutyCycle > 0) {
        instr->waves[index].dutyCycle--;
    }
}

void instrIncreaseDutyCycle(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].dutyCycle < 255) {
        instr->waves[index].dutyCycle++;
    }
}

char* instrGetDutyCycle(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    sprintf(tracker->instrumentSettingsData.dutyCycle[index], "%d", instr->waves[index].dutyCycle);
    return tracker->instrumentSettingsData.dutyCycle[index];
}


void instrDecreasePWM(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].pwm > 0) {
        instr->waves[index].pwm--;
    }
}

void instrIncreasePWM(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].pwm < 127) {
        instr->waves[index].pwm++;
    }
}

char* instrGetPWM(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    sprintf(tracker->instrumentSettingsData.pwm[index], "%d", instr->waves[index].pwm);
    return tracker->instrumentSettingsData.pwm[index];
}

void instrDecreaseFilter(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].filter > 0) {
        instr->waves[index].filter--;
    }
}

void instrIncreaseFilter(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].filter < 127) {
        instr->waves[index].filter++;
    }
}

char* instrGetFilter(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    sprintf(tracker->instrumentSettingsData.filter[index], "%d", instr->waves[index].filter);
    return tracker->instrumentSettingsData.filter[index];
}

void instrDecreaseWaveformVolume(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].volume > 0) {
        instr->waves[index].volume--;
    }
}

void instrIncreaseWaveformVolume(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].volume < 127) {
        instr->waves[index].volume++;
    }
}

char* instrGetWaveformVolume(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    if (instr->waves[index].volume == 0) {
        return "MAX";
    } else {
        sprintf(tracker->instrumentSettingsData.waveVolume[index], "%d", instr->waves[index].volume);
        return tracker->instrumentSettingsData.waveVolume[index];
    }
}

void instrDecreaseCarrier(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].carrierFrequency > 0) {
        instr->waves[index].carrierFrequency--;
    }
}

void instrIncreaseCarrier(void *userData, int index) {
    Instrument *instr = getCurrentInstrument((Tracker*)userData);
    if (instr->waves[index].carrierFrequency < 20000) {
        instr->waves[index].carrierFrequency++;
    }
}

char* instrGetCarrier(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);

    if (instr->waves[index].carrierFrequency == 0) {
        return "Chan0";
    } else {
        sprintf(tracker->instrumentSettingsData.carrierFrequency[index], "%d", instr->waves[index].carrierFrequency);
        return tracker->instrumentSettingsData.carrierFrequency[index];
    }
}


bool isWaveActive(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);
    for (int i = 0; i < MAX_WAVESEGMENTS; i++) {
        if (instr->waves[i].length == 0 && i < index) {
            return false;
        }
    }
    return true;
}

bool isPwmActive(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);
    return isWaveActive(userData, index) && instr->waves[index].waveform == PWM;
}

bool isRingActive(void *userData, int index) {
    Tracker *tracker = (Tracker*)userData;
    Instrument *instr = getCurrentInstrument(tracker);
    return isWaveActive(userData, index) && instr->waves[index].waveform == RING_MOD;
}



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

    keyhandler_register(kh, MUTE_SC_1, KM_SHIFT, NULL, muteTrack, tracker);
    keyhandler_register(kh, MUTE_SC_2, KM_SHIFT, NULL, muteTrack, tracker);
    keyhandler_register(kh, MUTE_SC_3, KM_SHIFT, NULL, muteTrack, tracker);
    keyhandler_register(kh, MUTE_SC_4, KM_SHIFT, NULL, muteTrack, tracker);

    keyhandler_register(kh, SDL_SCANCODE_X, KM_SHIFT, NULL, cutTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_C, KM_SHIFT, NULL, copyTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_V, KM_SHIFT, NULL, pasteTrack, tracker);

    keyhandler_register(kh, SDL_SCANCODE_X, KM_ALT, NULL, cutPattern, tracker);
    keyhandler_register(kh, SDL_SCANCODE_C, KM_ALT, NULL, copyPattern, tracker);
    keyhandler_register(kh, SDL_SCANCODE_V, KM_ALT, NULL, pastePattern, tracker);

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

    keyhandler_register(kh, SDL_SCANCODE_DELETE, 0, predicate_isEditMode, deleteNoteAndCommand, tracker);

    keyhandler_register(kh, SDL_SCANCODE_BACKSPACE, 0, predicate_isEditMode, deletePreviousNote, tracker);

    keyhandler_register(kh, SDL_SCANCODE_RETURN, 0, predicate_isEditMode, clearNoteOrCommand, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RETURN, KM_SHIFT, predicate_isEditOnNoteColumn, clearNoteAndCommand, tracker);

    keyhandler_register(kh, SDL_SCANCODE_NONUSBACKSLASH, 0, predicate_isEditOnNoteColumn, insertNoteOff, tracker);
    keyhandler_register(kh, SDL_SCANCODE_NONUSBACKSLASH, 0, predicate_isNotEditMode, playNoteOff, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RETURN, 0, predicate_isNotEditMode, playNoteOff, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F1, 0, NULL, previousOctave, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F2, 0, NULL, nextOctave, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F1, KM_SHIFT, NULL, transposeTrackDown, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F2, KM_SHIFT, NULL, transposeTrackUp, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F1, KM_ALT, NULL, transposePatternDown, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F2, KM_ALT, NULL, transposePatternUp, tracker);

    keyhandler_register(kh, SDL_SCANCODE_F9, 0, NULL, previousPatch, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F10, 0, NULL ,  nextPatch, tracker);


    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isEditMode, stopEditing, tracker);
    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isStopped, startEditing, tracker);
    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isPlaying, stopPlaying, tracker);
    keyhandler_register(kh, SDL_SCANCODE_SPACE, 0, predicate_isInstrumentMode, stopPlaying, tracker);

    keyhandler_register(kh, SDL_SCANCODE_INSERT, 0, predicate_isEditMode, insertBeforeNote, tracker);

    /** Pattern movement */
    keyhandler_register(kh, SDL_SCANCODE_UP, 0, predicate_isNotInstrumentMode, moveUp, tracker);
    keyhandler_register(kh, SDL_SCANCODE_DOWN, 0, predicate_isNotInstrumentMode, moveDown, tracker);
    keyhandler_register(kh, SDL_SCANCODE_LEFT, 0, predicate_isNotInstrumentMode, previousColumn, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RIGHT, 0, predicate_isNotInstrumentMode, nextColumn, tracker);
    keyhandler_register(kh, SDL_SCANCODE_PAGEUP, 0, NULL, moveUpMany, tracker);
    keyhandler_register(kh, SDL_SCANCODE_PAGEDOWN, 0, NULL, moveDownMany, tracker);

    keyhandler_register(kh, SDL_SCANCODE_HOME, 0, NULL, moveHome, tracker);
    keyhandler_register(kh, SDL_SCANCODE_END, 0, NULL, moveEnd, tracker);

    keyhandler_register(kh, SDL_SCANCODE_GRAVE, KM_SHIFT, NULL, decreaseStepping, tracker);
    keyhandler_register(kh, SDL_SCANCODE_GRAVE, 0, NULL, increaseStepping, tracker);

    keyhandler_register(kh, SDL_SCANCODE_TAB, KM_SHIFT, NULL, previousTrack, tracker);
    keyhandler_register(kh, SDL_SCANCODE_TAB, 0, NULL, nextTrack, tracker);

    /** Panel mode switch */
    keyhandler_register(kh, SDL_SCANCODE_F9, KM_SHIFT, predicate_isNotInstrumentMode, setInstrumentMode, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F10, KM_SHIFT, predicate_isNotInstrumentMode, setInstrumentMode, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F9, KM_SHIFT, predicate_isInstrumentMode, stopPlaying, tracker);
    keyhandler_register(kh, SDL_SCANCODE_F10, KM_SHIFT, predicate_isInstrumentMode, stopPlaying, tracker);

    /** Instrument editor */
    keyhandler_register(kh, SDL_SCANCODE_UP, 0, predicate_isInstrumentMode, gotoPreviousSetting, tracker);
    keyhandler_register(kh, SDL_SCANCODE_DOWN, 0, predicate_isInstrumentMode, gotoNextSetting, tracker);
    keyhandler_register(kh, SDL_SCANCODE_LEFT, 0, predicate_isInstrumentMode, decreaseSetting, tracker);
    keyhandler_register(kh, SDL_SCANCODE_RIGHT, 0, predicate_isInstrumentMode, increaseSetting, tracker);


}

void tracker_close(Tracker *tracker) {
    if (NULL != tracker) {
        if (tracker->instrumentSettings != NULL) {
            settings_close(tracker->instrumentSettings);
            tracker->instrumentSettings = NULL;
        }
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


void createInstrumentSettings(Tracker *tracker) {
    tracker->instrumentSettings = settings_create();
    SettingsComponent *sc = tracker->instrumentSettings;

    settings_add(sc, "Attack", instrDecreaseAttack, instrIncreaseAttack, instrGetAttack, NULL, tracker, 0);
    settings_add(sc, "Decay", instrDecreaseDecay, instrIncreaseDecay, instrGetDecay, NULL, tracker, 0);
    settings_add(sc, "Sustain", instrDecreaseSustain, instrIncreaseSustain, instrGetSustain, NULL, tracker, 0);
    settings_add(sc, "Release", instrDecreaseRelease, instrIncreaseRelease, instrGetRelease, NULL, tracker, 0);
    for (int i = 0; i < MAX_WAVESEGMENTS; i++) {
        char buf[10];
        sprintf(buf, "Wave %d", i+1);
        settings_add(sc, buf, instrDecreaseWave, instrIncreaseWave, instrGetWave, isWaveActive, tracker , i);
        settings_add(sc, " Length", instrDecreaseLength, instrIncreaseLength, instrGetLength, isWaveActive, tracker, i);
        settings_add(sc, " Note", instrDecreaseNote, instrIncreaseNote, instrGetNote, isWaveActive, tracker, i);
        settings_add(sc, " Filter", instrDecreaseFilter, instrIncreaseFilter, instrGetFilter, isWaveActive, tracker, i);
        settings_add(sc, " Volume", instrDecreaseWaveformVolume, instrIncreaseWaveformVolume, instrGetWaveformVolume, isWaveActive, tracker, i);
        settings_add(sc, " Duty Cycle", instrDecreaseDutyCycle, instrIncreaseDutyCycle, instrGetDutyCycle, isPwmActive, tracker, i);
        settings_add(sc, " PWM", instrDecreasePWM, instrIncreasePWM, instrGetPWM, isPwmActive, tracker, i);
        settings_add(sc, " Carrier Freq", instrDecreaseCarrier, instrIncreaseCarrier, instrGetCarrier, isRingActive, tracker, i);
    }
}

Tracker *tracker_init() {
    Tracker *tracker = calloc(1, sizeof(Tracker));
    tracker->song.bpm = 59;
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
    createInstrumentSettings(tracker);
    initKeyMappings(tracker);
    initNotes(tracker);
    initCommandKeys(tracker);
    tracker->mode = STOP;

    return tracker;
}

void loadSong(Tracker *tracker, char *name) {
    song_clear(&tracker->song);
    if (!persist_loadSongWithName(&tracker->song, name)) {
        screen_setStatusMessage("Could not open song.pxm");
    } else {
        screen_setStatusMessage("Successfully loaded song");
    }
    for (int i = 1; i < MAX_INSTRUMENTS; i++) {
        synth_loadPatch(tracker->synth, i, &tracker->song.instruments[i]);
    }

}

int main(int argc, char* args[]) {
    SDL_Event event;

    Tracker *tracker = tracker_init();

    if (!screen_init(CHANNELS)) {
        screen_close();
        tracker_close(tracker);
        return 1;
    }


    screen_setInstrumentSettings(tracker->instrumentSettings);
    defaultsettings_createInstruments(tracker->song.instruments);

    loadSong(tracker, "song.pxm");

    screen_setArrangementData(tracker->song.arrangement);
    gotoSongPos(tracker, 0);

    //synth_test();
    //return 0;

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
                //printf("Key %d\n", event.key.keysym.scancode);

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
