#include <SDL2/SDL.h>

#include "player.h"
#include "song.h"
#include "note.h"
#include "synth.h"

#define PLAYER_MODULATION_AMP_SCALING 3
#define PLAYER_MODULATION_FREQ_SCALING 16

#define EFFECT_ARPEGGIO 0x0
#define EFFECT_SLIDE_UP 0x1
#define EFFECT_SLIDE_DOWN 0x2
#define EFFECT_TONE_PORTAMENTO 0x3
#define EFFECT_VIBRATO 0x4

typedef struct _Player {
    Synth *synth;
    Song *song;
    SDL_TimerID timerId;
    Uint8 rowOffset;
    Uint8 playbackTick;
    Uint8 channels;
} Player;

void _player_parameterToNibbles(Uint8 parameter, Uint8 *left, Uint8 *right) {
    *left = (parameter >> 4) & 0xF;
    *right = parameter & 0xF;
}

Uint32 _player_playCallback(Uint32 interval, void *param) {
    Player *player = (Player*)param;
    Synth *synth = player->synth;

    for (int channel = 0; channel < player->channels; channel++) {
        Sint8 note = player->song->tracks[channel].notes[player->rowOffset].note;
        Uint8 patch = player->song->tracks[channel].notes[player->rowOffset].patch;
        Uint16 command = player->song->tracks[channel].notes[player->rowOffset].command;
        Uint8 effect = command >> 8;
        Uint8 parameter = command & 0xFF;

        if (player->playbackTick == 0) {
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
        }
        /* Arpeggio */
        bool isArpeggio = (effect == EFFECT_ARPEGGIO) && parameter > 0;
        if (isArpeggio && player->playbackTick == 1) {
            synth_pitchOffset(synth, channel, parameter & 0xF);
        } else if (isArpeggio && player->playbackTick == 2) {
            synth_pitchOffset(synth, channel, parameter >> 4);
        } else if (isArpeggio && player->playbackTick == 3) {
            synth_pitchOffset(synth, channel, 12);
        } else {
            synth_pitchOffset(synth, channel ,0);
        }
    }
    if (player->playbackTick == 0) {
        // TODO variable track length
        player->rowOffset = (player->rowOffset + 1) % 64;
    }
    player->playbackTick = (player->playbackTick + 1) % 4;
    return interval;
}


Player *player_init(Synth *synth, Uint8 channels) {
    Player *player = calloc(1, sizeof(Player));
    player->channels = channels;
    player->synth = synth;
    return player;
}

void player_close(Player *player) {
    player_stop(player);
    if (player != NULL) {
        free(player);
        player = NULL;
    }
}

Uint32 _player_getDelayFromBpm(int bpm) {
    return 15000/bpm;
}

void player_start(Player *player, Song *song) {
    player_stop(player);
    player->song = song;
    player->rowOffset = 0;
    player->playbackTick = 0;
    player->timerId = SDL_AddTimer(_player_getDelayFromBpm(song->bpm)/4, _player_playCallback, player);
}

Uint8 player_getCurrentRow(Player *player) {
    return player->rowOffset;
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
