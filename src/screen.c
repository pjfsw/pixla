#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "screen.h"
#include "note.h"
#include "inputfield.h"
#include "config.h"

#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300
#define SCREEN_SCALING 2
#define OCTAVE_ROW 130
#define INSTRUMENT_X 280
#define INSTRUMENT_Y 40

#define PANEL_PADDING 2
#define SONG_HIGHLIGHT_OFFSET 4
#define SONG_PANEL_X 2
#define SONG_PANEL_Y 2
#define SONG_PANEL_W 56
#define SONG_X_OFFSET SONG_PANEL_X + PANEL_PADDING
#define SONG_Y_OFFSET SONG_PANEL_Y + PANEL_PADDING
#define SONG_ROWS 12
#define PANEL_HEIGHT (SONG_ROWS * 10 + PANEL_PADDING)
#define PANEL_WIDTH 204
#define MID_PANEL_X (SONG_PANEL_X + SONG_PANEL_W) + 8
#define MID_PANEL_Y SONG_PANEL_Y
#define PANEL_X_OFFSET (MID_PANEL_X + PANEL_PADDING)
#define PANEL_Y_OFFSET (MID_PANEL_Y + PANEL_PADDING)
#define PANEL_ROWS 12
#define PANEL_COLS 25
#define STATUS_MSG_ROW  (PANEL_Y_OFFSET + 10 * (PANEL_ROWS - 1))
#define SCREEN_MAX_SONG_NAME 24
#define SCREEN_MAX_STATUS_MSG 40
#define ANALYZER_WIDTH 192
#define ANALYZER_X_SPACING (ANALYZER_WIDTH + 2)
#define ANALYZER_Y_SPACING 28
#define ANALYZER_Y_OFFSET 13
#define ANALYZER_AUDIO_SCALER (65000/ANALYZER_Y_SPACING)

typedef struct {
    Uint8 x;
    Uint8 w;
} ColumnHighlightPos;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logo;
    SDL_Texture *piano;
    SDL_Texture *noteTexture[128];
    SDL_Texture *noteBeatTexture[128];
    SDL_Texture *asciiTexture[256];
    ColumnHighlightPos columnHighlight[4];
    bool mute[16];
    int noteWidth[128];
    int noteHeight[128];
    TTF_Font *font;
    int logo_w;
    int logo_h;
    Instrument *instrument;
    Track **tracks;
    PatternPtr *arrangement;
    SettingsComponent *instrumentSettings;
    FileSelector *fileSelector;
    Inputfield *songNameField;
    Uint16 songPos;
    char rowNumbers[256][4];
    char statusMsg[SCREEN_MAX_STATUS_MSG+1];
    Uint8 rowOffset;
    Uint8 selectedTrack;
    Uint8 selectedColumn;
    Uint8 numberOfTracks;
    Uint8 stepping;
    Uint8 selectedPatch;
    Uint8 octave;
    Trackermode trackermode;
    char bpm[10];
    char songName[SCREEN_MAX_SONG_NAME+1];
    Uint32 ticks;
    Uint32 statusTimer;
    Uint32 statusOffset;
    Sint16 analyzer[TRACKS_PER_PATTERN][ANALYZER_WIDTH];
    Sint16 analyzerLastV[TRACKS_PER_PATTERN];
} Screen;

SDL_Color statusColor = {255,255,255};
SDL_Color noteBeatColor = {255,255,255};
SDL_Color noteColor = {160,160,160};
SDL_Color noteOffBeatColor = {127,127,127};
SDL_Color noteOffColor = {80,80,80};

Screen *screen = NULL;

#define FALLBACK_RESOURCE_DIR1 "../resources"
#define FALLBACK_RESOURCE_DIR2 "resources"

const char* const dirs[] = {
    RESOURCE_DIR,
    FALLBACK_RESOURCE_DIR1,
    FALLBACK_RESOURCE_DIR2,
};

void _screen_loadResources() {
     for (int i = 0; i < 3; i++) {
         const char *dir = dirs[i];
         char *path;
         if (-1 == asprintf(&path, "%s/pixla.png", dir)) {
             fprintf(stderr, "Failed to allocate memory!");
             exit(1);
         }
         screen->logo = IMG_LoadTexture(screen->renderer, path);
         free(path);
         if (screen->logo != NULL) {
              if (-1 == asprintf(&path, "%s/piano.png", dir)) {
                   fprintf(stderr, "Failed to allocate memory!");
                   exit(1);
              }
              screen->piano = IMG_LoadTexture(screen->renderer, path);
              free(path);
              if (-1 == asprintf(&path, "%s/nesfont.fon", dir)) {
                   fprintf(stderr, "Failed to allocate memory!");
                   exit(1);
              }
              screen->font = TTF_OpenFont(path, 8);
              free(path);
              break;
         }
     }
     if (screen->logo == NULL) {
         fprintf(stderr, "Failed to load resources!");
         exit(1);
     }
     SDL_QueryTexture(screen->logo, NULL, NULL, &screen->logo_w, &screen->logo_h);
}

void _screen_createAsciiTextures() {
    char chars[2];
    chars[1] = 0;

    for (int i = 0; i < 256; i++) {
        if (i < 32 || i > 127) {
            chars[0] = ' ';
        } else {
            chars[0] = i;
        }

        SDL_Surface *text = TTF_RenderText_Solid(screen->font, chars, noteColor);
        if (NULL != text) {
            screen->asciiTexture[i] = SDL_CreateTextureFromSurface(screen->renderer, text);
            SDL_FreeSurface(text);
        }
    }
}

void _screen_createNoteTextures() {
    char *noteText[] = {
            "C-","C#","D-","D#", "E-","F-","F#","G-","G#","A-","A#","B-"
    };
    char noteAndOctave[5];

    for (int i = 0; i < 128;i++) {
        if (i == NOTE_OFF) {
            sprintf(noteAndOctave, "===");
        } else if (i == NOTE_NONE) {
            sprintf(noteAndOctave, "---");
        } else if (i < 96) {
            sprintf(noteAndOctave, "%s%d", noteText[i%12], i/12);
        } else {
            sprintf(noteAndOctave, "NaN");
        }

        SDL_Surface *text1 = TTF_RenderText_Solid(screen->font, noteAndOctave, i == NOTE_NONE ? noteOffColor : noteColor);
        SDL_Surface *text2 = TTF_RenderText_Solid(screen->font, noteAndOctave, i == NOTE_NONE ? noteOffBeatColor : noteBeatColor);

        if (NULL != text1 ) {
            screen->noteTexture[i] = SDL_CreateTextureFromSurface(screen->renderer, text1);
            screen->noteWidth[i] = text1->w;
            screen->noteHeight[i] = text1->h;
            SDL_FreeSurface(text1);
        }
        if (NULL != text2) {
            screen->noteBeatTexture[i] = SDL_CreateTextureFromSurface(screen->renderer, text2);
            SDL_FreeSurface(text2);
        }
    }
}

void _screen_setupColumnHighlighters() {
    screen->columnHighlight[0].x = 0;
    screen->columnHighlight[0].w = 48;
    screen->columnHighlight[1].x = 56;
    screen->columnHighlight[1].w = 8;
    screen->columnHighlight[2].x = 64;
    screen->columnHighlight[2].w = 8;
    screen->columnHighlight[3].x = 72;
    screen->columnHighlight[3].w = 8;
}

void _screen_initArrays() {
    for (int i = 0; i < 255; i++) {
        sprintf(screen->rowNumbers[i], "%02d", i);
    }
    _screen_createNoteTextures();
    _screen_createAsciiTextures();
    _screen_setupColumnHighlighters();

}

bool screen_init(Uint8 numberOfTracks) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        return false;
    }
    if (TTF_Init() < 0) {
        SDL_Quit();
        return false;
    }

    if (NULL != screen) {
        free(screen);
    }
    screen = calloc(1, sizeof(Screen));
    screen->numberOfTracks = numberOfTracks;
    screen->tracks = calloc(numberOfTracks, sizeof(Track*));


    screen->window = SDL_CreateWindow(
            "Pixla",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH * SCREEN_SCALING, SCREEN_HEIGHT * SCREEN_SCALING,
            SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP
    );
    if (NULL == screen->window) {
        fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        return false;
    }

    screen->renderer = SDL_CreateRenderer(screen->window, -1, 0);
    if (NULL == screen->renderer) {
        fprintf(stderr, "could not create renderer: %s\n", SDL_GetError());
        return false;
    }
    SDL_RenderSetScale(screen->renderer, SCREEN_SCALING, SCREEN_SCALING);

    _screen_loadResources();
    _screen_initArrays();
    return screen;

}

void screen_close() {
    if (NULL != screen) {
        for (int i = 0; i < 128; i++) {
            SDL_DestroyTexture(screen->noteTexture[i]);
            SDL_DestroyTexture(screen->noteBeatTexture[i]);
            screen->noteTexture[i] = NULL;
            screen->noteBeatTexture[i] = NULL;
        }
        for (int i = 0; i < 256; i++) {
            SDL_DestroyTexture(screen->asciiTexture[i]);
            screen->asciiTexture[i] = NULL;
        }
        if (NULL != screen->font) {
            TTF_CloseFont(screen->font);
            screen->font = NULL;
        }
        if (NULL != screen->logo) {
            SDL_DestroyTexture(screen->logo);
            screen->logo = NULL;
        }
        if (NULL != screen->renderer) {
            SDL_DestroyRenderer(screen->renderer);
            screen->renderer = NULL;
        }
        if (NULL != screen->window) {
            SDL_DestroyWindow(screen->window);
            screen->window = NULL;
        }
        TTF_Quit();
        SDL_Quit();
        free(screen);
        screen = NULL;
    }
}

void screen_print(int x, int y, char* msg, SDL_Color *color) {

    if (screen->font == NULL || msg == NULL || color == NULL) {
        return;
    }

    SDL_Rect pos = {
            .x=x,
            .y=y,
            .w=8,
            .h=8,
    };

    while (*msg != 0) {
        SDL_RenderCopy(screen->renderer, screen->asciiTexture[(int)*msg], NULL, &pos);
        pos.x+=8;
        msg++;

    }
}

void screen_nprint(int x, int y, char* msg, int maxchars) {

    if (screen->font == NULL || msg == NULL) {
        return;
    }

    SDL_Rect pos = {
            .x=x,
            .y=y,
            .w=8,
            .h=8,
    };

    int n = 0;
    while (*msg != 0 && n < maxchars) {
        n++;
        SDL_RenderCopy(screen->renderer, screen->asciiTexture[(int)*msg], NULL, &pos);
        pos.x+=8;
        msg++;

    }
}



void screen_setTrackData(Uint8 track, Track *trackData) {
    if (track >= screen->numberOfTracks) {
        return;
    }
    screen->tracks[track] = trackData;
}

void screen_setArrangementData(PatternPtr *arrangement) {
    screen->arrangement = arrangement;
}

void screen_setSongPos(Uint16 songPos) {
    screen->songPos = songPos;
}

void screen_setSongName(char *name) {
    strncpy(screen->songName, name, SCREEN_MAX_SONG_NAME);
}

void screen_setInstrumentSettings(SettingsComponent *instrumentSettings) {
    screen->instrumentSettings = instrumentSettings;
}

void screen_setFileSelector(FileSelector *fileSelector) {
    screen->fileSelector = fileSelector;
}

void screen_songNameField(Inputfield *inputfield) {
    screen->songNameField = inputfield;
}

void screen_setSelectedColumn(Uint8 column) {
    screen->selectedColumn = column;
}

void screen_setSelectedTrack(Uint8 track) {
    if (track >= screen->numberOfTracks) {
        return;
    }
    screen->selectedColumn = 0;
    screen->selectedTrack = track;
}

void screen_setRowOffset(Sint8 rowOffset) {
    if (rowOffset < 0) {
        screen->rowOffset = 0;
    } else if (rowOffset > TRACK_LENGTH-1) {
        screen->rowOffset = TRACK_LENGTH-1;
    } else {
        screen->rowOffset = rowOffset;
    }
}

int getTrackRowY(int row) {
    return  140+row*10;
}

int getColumnOffset(int column) {
    return 36+column*92;
}


void screen_selectPatch(Uint8 patch, Instrument *instrument) {
   screen->selectedPatch = patch;
   screen->instrument = instrument;
}

void screen_setStepping(Uint8 stepping) {
    screen->stepping = stepping;
}

void screen_setOctave(Uint8 octave) {
    screen->octave = octave;
}


void screen_setStatusMessage(char* msg) {
    strncpy(screen->statusMsg, msg, SCREEN_MAX_STATUS_MSG);
    screen->statusTimer = 0;
    screen->statusOffset = 0;
}

void screen_setBpm(Uint16 bpm) {
    sprintf(screen->bpm, "BPM: %d", bpm);
}


void screen_setTrackermode(Trackermode trackermode) {
    screen->trackermode = trackermode;
}

void screen_setChannelMute(Uint8 track, bool mute) {
    screen->mute[track] = mute;
}

void _screen_renderLogo() {
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);
    if (screen->logo != NULL) {
        SDL_Rect pos = {
                .x=SCREEN_WIDTH-screen->logo_w,
                .y=2,
                .w=screen->logo_w,
                .h=screen->logo_h
        };
        SDL_RenderCopy(screen->renderer, screen->logo, NULL, &pos);
    }
}

void _screen_setEditColor() {
    SDL_SetRenderDrawColor(screen->renderer, 255,0,255,100);
}

void _screen_setDisabledCursorColor() {
    SDL_SetRenderDrawColor(screen->renderer, 147,40,147,100);
}

void _screen_setEnabledCursorColor() {
    SDL_SetRenderDrawColor(screen->renderer, 255,255,255,100);
}

void _screen_setWaveColor() {
    SDL_SetRenderDrawColor(screen->renderer, 135,55,135,100);
}

void _screen_setWaveBaseColor() {
    SDL_SetRenderDrawColor(screen->renderer, 35,5,35,100);
}

void screen_drawAnalyzer(Uint8 track, Sint16 *samples, Uint16 length) {
    if (screen == NULL || length == 0) {
        return;
    }
    int step = length / ANALYZER_WIDTH;
    for (int i = 0; i < length; i+=step) {
/*        Sint32 average = 0;
        for (int j = 0; j < step; j++) {
            average += samples[i+j];
        }*/
        screen->analyzer[track][i/step] = samples[i];

        //screen->analyzer[track][i/step] = (Sint32)average/(Sint32)step;
    }
}
void _screen_renderSong() {
    char txt[12];
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);
    if (screen->arrangement == NULL) {
        return;
    }
    for (int i = 0; i < SONG_ROWS; i++) {
        int songPosOffset = i + screen->songPos - SONG_HIGHLIGHT_OFFSET;
        if (songPosOffset >= 0 && songPosOffset < MAX_PATTERNS) {
            if (screen->arrangement[songPosOffset].pattern >= 0) {
                sprintf(txt, "%03d %03d", songPosOffset, screen->arrangement[songPosOffset].pattern);
                screen_print(SONG_X_OFFSET, SONG_Y_OFFSET + i * 10, txt, &statusColor);
            } else {
                screen_print(SONG_X_OFFSET, SONG_Y_OFFSET + i * 10, "--End--", &statusColor);
                break;
            }

        }
    }
    SDL_Rect pos = {
            .x=SONG_X_OFFSET-1, SONG_Y_OFFSET + 10 * SONG_HIGHLIGHT_OFFSET - 1, .w=56, .h=10
    };
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(screen->renderer, 255,255,255,100);
    SDL_RenderFillRect(screen->renderer, &pos);

}


void _screen_renderColumns() {
    char strbuf[5];

    int editOffset = 8;

    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);

    for (int y = 0; y < 16; y++) {
        int offset = y + screen->rowOffset - editOffset;
        if (offset > TRACK_LENGTH-1) {
            break;

        }
        int screenY = getTrackRowY(y);

        if (offset >= 0) {
            bool isBeat = (offset % 4) == 0;
            SDL_Color *textColor = isBeat ? &noteBeatColor : &noteColor;
            screen_print(8, screenY, screen->rowNumbers[offset], textColor);

            for (int x = 0; x < screen->numberOfTracks; x++) {
                Track *track = screen->tracks[x];
                if (NULL == track || offset >= TRACK_LENGTH) {
                    continue;
                }
                Note note = track->notes[offset];
                if (note.note >-1) {
                    SDL_Rect pos = {
                            .x=getColumnOffset(x),
                            .y=screenY,
                            .w=screen->noteWidth[note.note],
                            .h=screen->noteHeight[note.note]
                    };
                    SDL_RenderCopy(
                            screen->renderer,
                            isBeat ? screen->noteBeatTexture[note.note] : screen->noteTexture[note.note],
                                    NULL,
                                    &pos
                    );


                    sprintf(strbuf, "%02X", note.patch);
                    screen_print(getColumnOffset(x)+32, screenY, strbuf, textColor);

                    sprintf(strbuf, "%03X", note.command & 0xFFF);
                    screen_print(getColumnOffset(x)+56, screenY, strbuf, textColor);
                }
            }
        }
    }

    SDL_Rect pos = {
            .x=0,
            .y=getTrackRowY(editOffset)-1,
            .w=SCREEN_WIDTH,
            .h=10
    };

    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(screen->renderer, 0,0,0,200);
    for (int i = 0; i < screen->numberOfTracks; i++) {
        if (screen->mute[i]) {
            SDL_Rect pos2 = {
                    .x=getColumnOffset(i),
                    .y=getTrackRowY(0)-1,
                    .w=80,
                    .h=160
            };
            SDL_RenderFillRect(screen->renderer, &pos2);
        }
    }

    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);

    _screen_setEditColor();
    SDL_RenderFillRect(screen->renderer, &pos);

    if (screen->trackermode != EDIT_INSTRUMENT) {
        if (screen->trackermode == EDIT)  {
            _screen_setEnabledCursorColor();
        } else {
            _screen_setDisabledCursorColor();
        }
        SDL_Rect pos2 = {
                .x=getColumnOffset(screen->selectedTrack)+screen->columnHighlight[screen->selectedColumn].x-2,
                .y=getTrackRowY(editOffset)-2,
                .w=screen->columnHighlight[screen->selectedColumn].w+3,
                .h=12
        };

        SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
        SDL_RenderDrawRect(screen->renderer, &pos2);
    }
}


void _screen_renderDivisions() {
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(screen->renderer, 77,77,77,255);
    SDL_RenderDrawLine(screen->renderer,0,OCTAVE_ROW+8, SCREEN_WIDTH,OCTAVE_ROW+8);

    SDL_Rect pos = {
            .x = SONG_PANEL_X,
            .y = SONG_PANEL_Y,
            .w = 56 + PANEL_PADDING,
            .h = SONG_ROWS * 10 + PANEL_PADDING
    };
    SDL_RenderDrawRect(screen->renderer, &pos);

    SDL_Rect pos2 = {
            .x = MID_PANEL_X,
            .y = MID_PANEL_Y,
            .w = PANEL_WIDTH,
            .h = PANEL_HEIGHT
    };

    SDL_RenderDrawRect(screen->renderer, &pos2);

}

void _screen_renderStatusOctave() {
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);
    for (int i = 0; i < 7; i++) {
        SDL_Rect pos = {
                .x=8+i*56,
                .y=OCTAVE_ROW,
                .w=56,
                .h=8
        };
        SDL_RenderCopy(screen->renderer, screen->piano, NULL, &pos);
        if (i == screen->octave) {
            SDL_SetRenderDrawColor(screen->renderer, 255,255,255,100);
            SDL_RenderFillRect(screen->renderer, &pos);
        }
    }
}

void _screen_renderIsEditMode() {
    if (screen->trackermode == EDIT) {
        _screen_setEditColor();
        SDL_Rect pos = {
                .x=0,
                .y=0,
                .w=SCREEN_WIDTH,
                .h=SCREEN_HEIGHT
        };
        SDL_Rect pos2 = {
                .x=1,
                .y=1,
                .w=SCREEN_WIDTH-2,
                .h=SCREEN_HEIGHT-2
        };
        SDL_RenderDrawRect(screen->renderer, &pos);
        SDL_RenderDrawRect(screen->renderer, &pos2);
    }
}

void _screen_renderStepping() {
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);

    char s[3];
    sprintf(s, "%d", screen->stepping);
    screen_print(0, OCTAVE_ROW, s, &statusColor);
}

void _screen_renderSelectedPatch() {
    char s[20];

    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);

    sprintf(s, "Patch: %02x", screen->selectedPatch);
    screen_print(INSTRUMENT_X, INSTRUMENT_Y, s, &statusColor);

    if (screen->instrument != NULL) {
        Instrument *instrument = screen->instrument;
        int adsrScaleX = 10;
        int adsrScaleY = 24;
        int adsrOffset = 10;

        /* Attack */
        int attackX = INSTRUMENT_X + instrument->attack/adsrScaleX;
        SDL_RenderDrawLine(screen->renderer,
                INSTRUMENT_X, INSTRUMENT_Y + adsrScaleY + adsrOffset - 1,
                attackX, INSTRUMENT_Y+adsrOffset);
        /* Decay */
        int decayX = attackX + instrument->decay/adsrScaleX;
        int sustainY = INSTRUMENT_Y + adsrOffset + adsrScaleY - instrument->sustain * adsrScaleY/128 -1;
        SDL_RenderDrawLine(screen->renderer,
                attackX, INSTRUMENT_Y + adsrOffset,
                decayX, sustainY);

        int graphEnd = 319;
        /* Sustain */
        int sustainX = graphEnd - instrument->release/adsrScaleX;
        if (sustainX < decayX) {
            sustainX = decayX;
        }
        SDL_RenderDrawLine(screen->renderer,
                decayX, sustainY,
                sustainX, sustainY);
        /* Release */
        SDL_RenderDrawLine(screen->renderer,
                sustainX, sustainY,
                graphEnd, INSTRUMENT_Y + adsrScaleY - 1 + adsrOffset);

        for (int i = 0; i < 3; i++) {
            Wavesegment *wav = &instrument->waves[i];
            sprintf(s, "%d: %s", i+1, instrument_getWaveformName(wav->waveform));
            screen_print(graphEnd + 8, INSTRUMENT_Y + adsrOffset + i * 10, s, &statusColor);
            if (wav->length == 0) {
                break;
            }
        }

    }
    screen_print(INSTRUMENT_X, PANEL_Y_OFFSET + 10 * (PANEL_ROWS-1), screen->bpm, &statusColor);

}

void _screen_renderSongPanel() {
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
    if (strlen(screen->statusMsg) == 0) {
        screen_print(PANEL_X_OFFSET, STATUS_MSG_ROW, screen->songName, &statusColor);
        for (int i = 0; i < TRACKS_PER_PATTERN; i++) {
            int xOfs = PANEL_X_OFFSET + 3; //+(i%2)*ANALYZER_X_SPACING;
            int yOfs = PANEL_Y_OFFSET + ANALYZER_Y_OFFSET + ANALYZER_Y_SPACING * i;
            _screen_setWaveBaseColor();
            SDL_RenderDrawLine(screen->renderer, xOfs, yOfs,xOfs + ANALYZER_WIDTH, yOfs);
            _screen_setWaveColor();
            for (int s = 0; s < ANALYZER_WIDTH; s++) {
                int v = screen->analyzer[i][s]/ANALYZER_AUDIO_SCALER;
                SDL_RenderDrawLine(screen->renderer,
                        xOfs+s, yOfs + screen->analyzerLastV[i],
                        xOfs+s+1, yOfs+ v
                        );

                screen->analyzerLastV[i] = v;
            }
        }

    } else {
        int len = strlen(screen->statusMsg);
        if (len > PANEL_COLS) {
            int ticks = SDL_GetTicks() - screen->ticks;
            screen->ticks = SDL_GetTicks();
            screen->statusTimer += ticks;
            if (screen->statusTimer > 100) {
                screen->statusTimer = 0;
                screen->statusOffset++;
            }
            if (screen->statusOffset >= len) {
                screen->statusOffset = 0;
            }
        } else {
            screen->statusOffset = 0;
        }
        int charsToPrint = len-screen->statusOffset;
        if (charsToPrint > PANEL_COLS) {
            charsToPrint = PANEL_COLS;
        }
        screen_nprint(PANEL_X_OFFSET, STATUS_MSG_ROW, &screen->statusMsg[screen->statusOffset], charsToPrint);
        if (len > PANEL_COLS) {
            int charsToPrintAgain = PANEL_COLS - charsToPrint - 1;
            if (charsToPrintAgain > 0)  {
                screen_nprint(PANEL_X_OFFSET + 8 * (1+charsToPrint), STATUS_MSG_ROW, screen->statusMsg, charsToPrintAgain);
            }
        }
    }
}

void _screen_renderInstrumentPanel() {
    if (screen->instrument == NULL || screen->instrumentSettings == NULL) {
        return;
    }

    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
    settings_render(screen->instrumentSettings, screen->renderer, PANEL_X_OFFSET, PANEL_Y_OFFSET, PANEL_ROWS);

}

void _screen_renderFileSelector() {
    if (screen->fileSelector == NULL) {
        return;
    }
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
    fileSelector_render(screen->fileSelector, screen->renderer, PANEL_X_OFFSET, PANEL_Y_OFFSET, PANEL_ROWS);
}

void _screen_renderSavePanel() {
    if (screen->songNameField != NULL) {
        SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
        screen_print(PANEL_X_OFFSET, PANEL_Y_OFFSET, "Save song:", screen_getDefaultColor());
        inputfield_render(screen->songNameField, screen->renderer, PANEL_X_OFFSET, PANEL_Y_OFFSET+10, MAX_SONG_NAME);
    }
}

void _screen_renderStatus() {
    _screen_renderStepping();
    _screen_renderSelectedPatch();
    _screen_renderIsEditMode();
    _screen_renderStatusOctave();
}

void screen_update() {
    SDL_RenderClear(screen->renderer);
    _screen_renderDivisions();
    _screen_renderLogo();
    _screen_renderSong();
    _screen_renderColumns();
    switch (screen->trackermode) {
    case EDIT:
    case STOP:
    case PLAY:
    case CONFIRM_STATE:
        _screen_renderSongPanel();
        break;
    case PLAY_EDIT_INSTRUMENT:
    case EDIT_INSTRUMENT:
        _screen_renderInstrumentPanel();
        break;
    case LOAD_SONG:
        _screen_renderFileSelector();
        break;
    case SAVE_SONG:
        _screen_renderSavePanel();
        break;
    }
    _screen_renderStatus();
/*    _screen_renderGraphs();*/
    SDL_SetRenderDrawColor(screen->renderer, 0,0,0,0);
    SDL_RenderPresent(screen->renderer);
}

SDL_Color *screen_getDefaultColor() {
    return &statusColor;
}

