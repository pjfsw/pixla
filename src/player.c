#include <SDL2/SDL.h>

#include "player.h"
#include "song.h"
#include "note.h"
#include "synth.h"

#define PLAYER_AMP_MODULATION_AMP_SCALING 16
#define PLAYER_AMP_MODULATION_FREQ_SCALING 2
#define PLAYER_MODULATION_AMP_SCALING 3
#define PLAYER_MODULATION_FREQ_SCALING 16

#define EFFECT_ARPEGGIO 0x0
#define EFFECT_SLIDE_UP 0x1
#define EFFECT_SLIDE_DOWN 0x2
#define EFFECT_TONE_PORTAMENTO 0x3
#define EFFECT_VIBRATO 0x4
#define EFFECT_TREMOLO 0x7
#define EFFECT_PATTERN_BREAK 0xD
#define EFFECT_TEMPO 0xF

typedef struct {
    Sint8 arpeggio[4];
} PlayerChannel;

typedef struct _Player {
    PlayerChannel *channelData;
    Synth *synth;
    Song *song;
    SDL_TimerID timerId;
    Uint16 songPos;
    Uint8 rowOffset;
    Uint8 playbackTick;
    Uint8 channels;
    Uint8 bpm;
    Sint16 patternBreak;
} Player;

void _player_parameterToNibbles(Uint8 parameter, Uint8 *left, Uint8 *right) {
    *left = (parameter >> 4) & 0xF;
    *right = parameter & 0xF;
}

Uint32 _player_getDelayFromBpm(int bpm) {
    return 30000/bpm;
}

void _player_increaseSongPos(Player *player) {
    player->songPos++;
    if (player->songPos >= MAX_PATTERNS || player->song->arrangement[player->songPos].pattern < 0) {
        player->songPos = 0;
    }
}

Uint32 _player_playCallback(Uint32 interval, void *param) {
    Player *player = (Player*)param;
    Synth *synth = player->synth;

    Uint16 patternToPlay = player->song->arrangement[player->songPos].pattern;
    if (patternToPlay < 0 || patternToPlay >= MAX_PATTERNS) {
        patternToPlay = 0;
    }

    Pattern *pattern = &player->song->patterns[patternToPlay];

    for (int channel = 0; channel < player->channels; channel++) {
        Sint8 note = pattern->tracks[channel].notes[player->rowOffset].note;
        Uint8 patch = pattern->tracks[channel].notes[player->rowOffset].patch;
        Uint16 command = pattern->tracks[channel].notes[player->rowOffset].command;
        Uint8 effect = command >> 8;
        Uint8 parameter = command & 0xFF;

        if (note == NOTE_OFF) {
            synth_noteRelease(synth, channel);
        } else if (note >= 0 && note < 97) {
            if (effect == EFFECT_TONE_PORTAMENTO) {
                synth_notePitch(synth, channel, patch, note);
            } else {
                synth_noteTrigger(synth, channel, patch, note);
            }
        }
        if (effect == EFFECT_VIBRATO) {
            Uint8 freq, amp;
            _player_parameterToNibbles(parameter, &freq, &amp);

            if (freq > 0 || amp > 0) {
                synth_frequencyModulation(
                        synth, channel,
                        PLAYER_MODULATION_FREQ_SCALING * freq,
                        PLAYER_MODULATION_AMP_SCALING * amp
                );
            }
        } else {
            synth_frequencyModulation(synth, channel, 0, 0);
        }
        if (effect == EFFECT_TREMOLO) {
            Uint8 freq, amp;
            _player_parameterToNibbles(parameter, &freq, &amp);

            if (freq > 0 || amp > 0) {
                synth_amplitudeModulation(
                        synth, channel,
                        PLAYER_AMP_MODULATION_FREQ_SCALING * freq,
                        PLAYER_AMP_MODULATION_AMP_SCALING * amp
                        );
            }
        } else {
            synth_amplitudeModulation(synth, channel, 0, 0);
        }
        if (effect == EFFECT_SLIDE_DOWN) {
            if (parameter != 0) {
                synth_pitchGlideDown(synth, channel, parameter);
            }
        } else if (effect == EFFECT_SLIDE_UP) {
            if (parameter != 0) {
                synth_pitchGlideUp(synth, channel, parameter);
            }
        } else {
            synth_pitchGlideStop(synth, channel);
        }
        if (effect == EFFECT_TEMPO && parameter > 0) {
            player->bpm = parameter;
        }
        if (effect == EFFECT_PATTERN_BREAK) {
            player->patternBreak = parameter;
        }
        bool isArpeggio = (effect == EFFECT_ARPEGGIO) && parameter > 0;
        Sint8 *arpeggio = player->channelData[channel].arpeggio;
        if (isArpeggio) {
            arpeggio[0] = 0;
            arpeggio[1] = parameter >> 4;
            arpeggio[2] = parameter & 0xF;
            arpeggio[3] = 12;
            synth_pitchModulation(synth, channel, 30, arpeggio, 4);
        } else {
            synth_pitchModulation(synth, channel, 0, arpeggio, 0);
        }
    }
    if (player->patternBreak > -1) {
        player->rowOffset = player->patternBreak % TRACK_LENGTH;
        _player_increaseSongPos(player);
    } else {
        player->rowOffset = player->rowOffset + 1;
        if (player->rowOffset >= TRACK_LENGTH) {
            player->rowOffset = 0;
            _player_increaseSongPos(player);
        }
    }
    player->patternBreak = -1;
    return _player_getDelayFromBpm(player->bpm)/4;
}


Player *player_init(Synth *synth, Uint8 channels) {
    Player *player = calloc(1, sizeof(Player));
    player->channels = channels;
    player->synth = synth;
    player->channelData = calloc(channels, sizeof(PlayerChannel));
    return player;
}

void player_close(Player *player) {
    player_stop(player);
    if (player != NULL) {
        free(player->channelData);
        free(player);
        player = NULL;
    }
}

void player_start(Player *player, Song *song, Uint16 songPos) {
    player_stop(player);
    player->song = song;
    player->songPos = songPos;
    player->rowOffset = 0;
    player->playbackTick = 0;
    player->bpm = song->bpm;
    player->patternBreak = -1;
    player->timerId = SDL_AddTimer(_player_getDelayFromBpm(song->bpm)/4, _player_playCallback, player);
}

Uint8 player_getCurrentRow(Player *player) {
    return player->rowOffset;
}

Uint16 player_getSongPos(Player *player) {
    return player->songPos;
}


void player_stop(Player *player) {
    if (player->timerId != 0) {
        SDL_RemoveTimer(player->timerId);
        player->timerId = 0;
    }
}

bool player_isPlaying(Player *player) {
    return player->timerId != 0;
}
