#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "screen.h"

#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300
#define SCREEN_SCALING 2
#define STATUS_ROW 130

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logo;
    SDL_Texture *piano;
    SDL_Texture *noteTexture[128];
    SDL_Texture *noteBeatTexture[128];
    SDL_Texture *asciiTexture[256];
    int noteWidth[128];
    int noteHeight[128];
    TTF_Font *font;
    int logo_w;
    int logo_h;
    Sint8 *tableToShow;
    Sint16 tableToShowCount;

    Track **tracks;
    char rowNumbers[256][4];
    char* statusMsg;
    Uint8 rowOffset;
    Uint8 selectedTrack;
    Uint8 numberOfTracks;
    Uint8 stepping;
    Uint8 selectedPatch;
    Uint8 octave;
    bool editMode;
} Screen;

SDL_Color noteBeatColor = {255,255,255};
SDL_Color noteColor = {157,157,157};
SDL_Color statusColor = {255,255,255};


Screen *screen = NULL;

#define RESOURCE_DIR "../resources/"

void _screen_loadResources() {
     screen->logo = IMG_LoadTexture(screen->renderer, "../resources/pixla.png");
     if (screen->logo != NULL) {
         SDL_QueryTexture(screen->logo, NULL, NULL, &screen->logo_w, &screen->logo_h);
     }
     screen->piano = IMG_LoadTexture(screen->renderer, "../resources/piano.png");

     screen->font = TTF_OpenFont("../resources/nesfont.fon", 8);
     if (screen->font == NULL) {
         fprintf(stderr, "Failed to load font\n");
     }
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
        if (i == 126) {
            sprintf(noteAndOctave, "===");
        } else if (i == 127) {
            sprintf(noteAndOctave, "---");
        } else if (i < 120) {
            sprintf(noteAndOctave, "%s%d", noteText[i%12], i/12);
        } else {
            sprintf(noteAndOctave, "NaN");
        }

        SDL_Surface *text1 = TTF_RenderText_Solid(screen->font, noteAndOctave, noteColor);
        SDL_Surface *text2 = TTF_RenderText_Solid(screen->font, noteAndOctave, noteBeatColor);

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

void _screen_initArrays() {
    for (int i = 0; i < 255; i++) {
        sprintf(screen->rowNumbers[i], "%02d", i);
    }
    _screen_createNoteTextures();
    _screen_createAsciiTextures();

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

void screen_setTrackData(Uint8 track, Track *trackData) {
    if (track >= screen->numberOfTracks) {
        return;
    }
    screen->tracks[track] = trackData;
}

void screen_setSelectedTrack(Uint8 track) {
    if (track >= screen->numberOfTracks) {
        return;
    }
    screen->selectedTrack = track;
}

Uint8 screen_getLongestTrackLength() {
    Uint8 length = 0;
    for (int i = 0; i < screen->numberOfTracks; i++) {
        if (screen->tracks[i] != NULL && screen->tracks[i]->length > length) {
            length = screen->tracks[i]->length;
        }
    }
    return length;
}

void screen_setRowOffset(Sint8 rowOffset) {
    if (rowOffset < 0) {
        screen->rowOffset = 0;
    } else if (rowOffset > screen_getLongestTrackLength()-1) {
        screen->rowOffset = screen_getLongestTrackLength()-1;
    } else {
        screen->rowOffset = rowOffset;
    }
}

int getTrackRowY(int row) {
    return  140+row*10;
}

int getColumnOffset(int column) {
    return 40+column*88;
}


void screen_selectPatch(Uint8 patch) {
   screen->selectedPatch = patch;
}

void screen_setStepping(Uint8 stepping) {
    screen->stepping = stepping;
}

void screen_setOctave(Uint8 octave) {
    screen->octave = octave;
}


void screen_setStatusMessage(char* msg) {
    screen->statusMsg = msg;
}

void screen_setEditMode(bool isEditMode) {
    screen->editMode = isEditMode;
}

void screen_setTableToShow(Sint8 *table, Uint8 elements) {
    screen->tableToShow = table;
    screen->tableToShowCount = elements;

}


void _screen_renderLogo() {
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

void _screen_renderColumns() {
    char strbuf[3];

    int editOffset = 8;
    Uint8 maxLength = screen_getLongestTrackLength();
    for (int y = 0; y < 16; y++) {
        int offset = y + screen->rowOffset - editOffset;
        if (offset > maxLength-1) {
            break;
        }

        int screenY = getTrackRowY(y);

        if (offset >= 0) {
            bool isBeat = (offset % 4) == 0;
            SDL_Color *textColor = isBeat ? &noteBeatColor : &noteColor;
            screen_print(8, screenY, screen->rowNumbers[offset], textColor);

            for (int x = 0; x < screen->numberOfTracks; x++) {
                Track *track = screen->tracks[x];
                if (NULL == track || offset >= track->length) {
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
                    screen_print(getColumnOffset(x)+56, screenY, "00", textColor);
                }
            }
        }
    }

    SDL_Rect pos = {
            .x=0,
            .y=getTrackRowY(editOffset)-1,
            .w=SCREEN_WIDTH,
            .h=9
    };

    _screen_setEditColor();
    SDL_RenderFillRect(screen->renderer, &pos);


    SDL_SetRenderDrawColor(screen->renderer, 255,255,255,80);
    SDL_Rect pos2 = {
            .x=getColumnOffset(screen->selectedTrack),
            .y=getTrackRowY(editOffset)-1,
            .w=24,
            .h=9

    };
    SDL_RenderFillRect(screen->renderer, &pos2);
}


void _screen_renderDivisions() {
    SDL_SetRenderDrawColor(screen->renderer, 77,77,77,255);
    SDL_RenderDrawLine(screen->renderer,0,STATUS_ROW+8, SCREEN_WIDTH,STATUS_ROW+8);
}

void _screen_renderStatusOctave() {
    for (int i = 0; i < 7; i++) {
        SDL_Rect pos = {
                .x=8+i*56,
                .y=STATUS_ROW,
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
    if (screen->editMode) {
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
    char s[3];
    sprintf(s, "%d", screen->stepping);
    screen_print(0, STATUS_ROW, s, &statusColor);
}

void _screen_renderSelectedPatch() {
    char s[3];
    sprintf(s, "%02x", screen->selectedPatch);
    screen_print(0, STATUS_ROW-8, s, &statusColor);
}

void _screen_renderStatusMessage() {
    if (screen->statusMsg != NULL) {
        screen_print(16, STATUS_ROW, screen->statusMsg, &statusColor);
    }
}

void _screen_renderStatus() {
    _screen_renderStepping();
    _screen_renderSelectedPatch();
    _screen_renderStatusMessage();
    _screen_renderIsEditMode();
    _screen_renderStatusOctave();
}

void _screen_renderGraphs() {
    if (screen->tableToShow != NULL) {
        SDL_Rect pos = {
                .x=10,
                .y=2,
                .w=130,
                .h=128
        };
        SDL_RenderDrawRect(screen->renderer, &pos);
        for (int i = 0; i < screen->tableToShowCount; i++) {
            SDL_RenderDrawPoint(screen->renderer, i+pos.x+1, (pos.y+pos.h)-(screen->tableToShow[i]));
        }

    }
}

void screen_update() {
    SDL_RenderClear(screen->renderer);
    _screen_renderDivisions();
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);
    _screen_renderLogo();
    _screen_renderColumns();
    _screen_renderStatus();
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_NONE);
    _screen_renderGraphs();
    SDL_SetRenderDrawColor(screen->renderer, 0,0,0,0);
    SDL_RenderPresent(screen->renderer);
}
