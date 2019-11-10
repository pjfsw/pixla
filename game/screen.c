#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "screen.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define COLUMNS 5
#define STATUS_ROW 130

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logo;
    SDL_Texture *noteTexture[128];
    SDL_Texture *noteBeatTexture[128];
    int noteWidth[128];
    int noteHeight[128];
    TTF_Font *font;
    int logo_w;
    int logo_h;
    Sint8 **mainColumn;
    Uint8 columnLength[COLUMNS];
    Uint8 rowOffset;
    Uint8 selectedColumn;
    /* Status flags and counters */
    char* statusMsg;
    int stepping;
    bool editMode;
} Screen;

SDL_Color noteBeatColor = {255,255,255};
SDL_Color noteColor = {157,157,157};
SDL_Color statusColor = {255,255,255};

char rowNumbers[64][4];

Screen *screen = NULL;

void _screen_loadResources() {
     screen->logo = IMG_LoadTexture(screen->renderer,"../game/pixla.png");
     if (screen->logo != NULL) {
         SDL_QueryTexture(screen->logo, NULL, NULL, &screen->logo_w, &screen->logo_h);
     }
     screen->font = TTF_OpenFont("../game/nesfont.fon", 8);
     if (screen->font == NULL) {
         fprintf(stderr, "Failed to load font\n");
     }
}

void _screen_initArrays() {
    for (int i = 0; i < 64; i++) {
        sprintf(rowNumbers[i], "%02d", i);
    }
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
            screen->noteWidth[i] = text1->w*2;
            screen->noteHeight[i] = text1->h*2;
            SDL_FreeSurface(text1);
        }
        if (NULL != text2) {
            screen->noteBeatTexture[i] = SDL_CreateTextureFromSurface(screen->renderer, text2);
            SDL_FreeSurface(text2);
        }
    }
}

bool screen_init() {
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
    screen->mainColumn = calloc(COLUMNS, sizeof(Sint8*));


    screen->window = SDL_CreateWindow(
            "Pixla",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH, SCREEN_HEIGHT,
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

    if (screen->font == NULL) {
        return;
    }
    SDL_Surface *text = TTF_RenderText_Solid(screen->font, msg, *color);
    if (NULL != text ) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(screen->renderer, text);
        SDL_Rect pos = {
                .x=x*2,
                .y=y*2,
                .w=text->w*2,
                .h=text->h*2
        };
        SDL_RenderCopy(screen->renderer, texture, NULL, &pos);
        SDL_FreeSurface(text);
        SDL_DestroyTexture(texture);
    }
}

void screen_setColumn(Uint8 column, Uint8 rows, Sint8 *mainColumn) {
    if (column >= COLUMNS) {
        return;
    }
    screen->mainColumn[column] = mainColumn;
    screen->columnLength[column] = rows;
}

void screen_setSelectedColumn(Uint8 column) {
    if (column >= COLUMNS) {
        return;
    }
    screen->selectedColumn = column;
}

void screen_setRowOffset(Sint8 rowOffset) {
    if (rowOffset < 0) {
        screen->rowOffset = 0;
    } else if (rowOffset > 63) {
        screen->rowOffset = 63;
    } else {
        screen->rowOffset = rowOffset;
    }
}


void _screen_renderLogo() {
    if (screen->logo != NULL) {
        SDL_Rect pos = {
                .x=SCREEN_WIDTH-screen->logo_w*2,
                .y=0,
                .w=screen->logo_w*2,
                .h=screen->logo_h*2
        };
        SDL_RenderCopy(screen->renderer, screen->logo, NULL, &pos);
    }
}

int getTrackRowY(int row) {
    return  140+row*10;
}

int getColumnOffset(int column) {
    return 56+column*40;
}

void _screen_setEditColor() {
    SDL_SetRenderDrawColor(screen->renderer, 255,0,255,100);
}

void _screen_renderColumns() {
    int editOffset = 8;
    for (int y = 0; y < 16; y++) {
        int offset = y + screen->rowOffset - editOffset;
        if (offset > 63) {
            break;
        }

        int screenY = getTrackRowY(y);

        if (offset >= 0) {
            bool isBeat = (offset % 4) == 0;
            screen_print(8, screenY, rowNumbers[offset],
                    isBeat ? &noteBeatColor : &noteColor
            );

            for (int x = 0; x < COLUMNS; x++) {
                if (NULL == screen->mainColumn[x]) {
                    continue;
                }
                Sint8 note = screen->mainColumn[x][offset];
                if (note >-1) {
                    SDL_Rect pos = {
                            .x=getColumnOffset(x)*2,
                            .y=screenY*2,
                            .w=screen->noteWidth[note],
                            .h=screen->noteHeight[note]
                    };
                    SDL_RenderCopy(
                            screen->renderer,
                            isBeat ? screen->noteBeatTexture[note] : screen->noteTexture[note],
                                    NULL,
                                    &pos
                    );
                }
            }
        }
    }

    SDL_Rect pos = {
            .x=0,
            .y=getTrackRowY(editOffset)*2-1,
            .w=SCREEN_WIDTH,
            .h=18
    };

    _screen_setEditColor();
    SDL_RenderFillRect(screen->renderer, &pos);


    SDL_SetRenderDrawColor(screen->renderer, 255,255,255,80);
    SDL_Rect pos2 = {
            .x=getColumnOffset(screen->selectedColumn)*2,
            .y=getTrackRowY(editOffset)*2-1,
            .w=48,
            .h=18

    };
    SDL_RenderFillRect(screen->renderer, &pos2);
}

void screen_setStepping(int stepping) {
    screen->stepping = stepping;
}

void _screen_renderDivisions() {
    SDL_SetRenderDrawColor(screen->renderer, 77,77,77,255);
    SDL_RenderDrawLine(screen->renderer,0,2*STATUS_ROW+19, 2*SCREEN_WIDTH,2*STATUS_ROW+19);
}

void _screen_renderStatus() {
    char s[3];
    sprintf(s, "%d", screen->stepping);
    screen_print(0, STATUS_ROW, s, &statusColor);
    if (screen->statusMsg != NULL) {
        screen_print(16, STATUS_ROW, screen->statusMsg, &statusColor);
    }
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

void screen_setStatusMessage(char* msg) {
    screen->statusMsg = msg;
}

void screen_setEditMode(bool isEditMode) {
    printf("editmode\n");
    screen->editMode = isEditMode;
}

void screen_update() {
    SDL_RenderClear(screen->renderer);
    _screen_renderDivisions();
    SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_ADD);
    _screen_renderLogo();
    _screen_renderColumns();
    _screen_renderStatus();
    SDL_SetRenderDrawColor(screen->renderer, 0,0,0,0);
    SDL_RenderPresent(screen->renderer);
}
